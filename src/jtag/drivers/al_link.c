#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <jtag/adapter.h>
#include <jtag/interface.h>
#include <jtag/commands.h>

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "al_link_cyusb.h"

/*
 * AL-LINK driver
 *
 * This file is structured in three layers:
 * 1. USB transport: open/close/configure the device and move bytes over USB.
 *    The transport uses the CyUSB kernel driver (CyUSB.sys / CyUSB3.sys) via
 *    pure Win32 DeviceIoControl calls.  See al_link_cyusb.c for details.
 * 2. AL-LINK protocol: encode one logical JTAG cycle into one USB byte and
 *    decode the matching TDO sample from the response byte.
 * 3. OpenOCD queue execution: translate queued JTAG operations into the byte
 *    stream expected by the adapter.
 *
 * The queue handling is modeled after bitbang.c. The old AL-LINK behaves like
 * a USB-carried bitbang adapter, so the safest approach is to preserve
 * standard OpenOCD TAP semantics and only replace the physical pin toggling
 * backend with AL-LINK byte encoding.
 */

#define AL_LINK_MAX_USB_IDS 8

#define AL_LINK_DEFAULT_VID 0x336C
#define AL_LINK_DEFAULT_PID 0x1001

#define AL_LINK_EP_DATA_IN  0x82
#define AL_LINK_EP_DATA_OUT 0x06
#define AL_LINK_EP_CFG_OUT  0x08

/*
 * Ground truth from a real, successful Anlogic vendor-tool capture
 * (Wireshark/USBPcap on the same AL-Link probe, VID:PID 336C:1001): the EP8
 * mode-config packet it sends is { 0x01, speed_byte }, not { 0x04, speed_byte
 * }. The 0x04 "direct I/O" assumption was reverse-engineered from an
 * unrelated STM32-based reference firmware and was never verified against
 * real FX2LP traffic. Name kept for compatibility even though the value no
 * longer matches the old "direct I/O" description.
 */
#define AL_LINK_MODE_DIRECT 0x01

#define AL_LINK_SPEED_12M   0x00
#define AL_LINK_SPEED_6M    0x04
#define AL_LINK_SPEED_4M    0x08
#define AL_LINK_SPEED_2M    0x14
#define AL_LINK_SPEED_1200K 0x24
#define AL_LINK_SPEED_800K  0x38
#define AL_LINK_SPEED_500K  0x5c
#define AL_LINK_SPEED_200K  0x80
#define AL_LINK_SPEED_180K  0xff

/* FX2LP USB2.0 high-speed bulk endpoints use 512-byte packets. */
#define AL_LINK_DEFAULT_PACKET_SIZE 512

/* IEEE 1149.1 requires at least five TMS=1 clocks to reach TLR. */
#define AL_LINK_TLR_RESET_CYCLES 5

/*
 * USB timeout in milliseconds.
 *
 * Matches the old LIBUSB_TIMEOUT_MS value (6000 ms) from the OpenOCD libusb
 * helper.  USB hosts may stall for several seconds when a halted target causes
 * the bus to go unresponsive, so a generous timeout is required.
 */
#define AL_LINK_USB_TIMEOUT_MS 6000

/*
 * User-visible driver configuration.
 *
 * Most values are given defaults so the driver can be brought up with minimal
 * target configuration, but every field that is still uncertain for older
 * hardware can be overridden from the OpenOCD config layer.
 */
struct al_link_config {
	uint16_t vid[AL_LINK_MAX_USB_IDS];
	uint16_t pid[AL_LINK_MAX_USB_IDS];
	uint8_t ep_in;
	uint8_t ep_out;
	uint8_t ep_cfg;
	uint8_t interface_num;
	uint16_t packet_size;
	uint8_t speed_byte;
	bool sample_high_nibble;
	bool speed_byte_overridden;
	/* Temporary diagnostic: histogram every raw EP2 response bit instead of
	 * only trusting the configured TDO sample position. Off by default. */
	bool dump_raw_rx;
};

/* Transport state owned by the driver instance. */
struct al_link_usb {
	al_cyusb_handle_t cyusb;
	bool is_open;
	bool io_failed;
};

/* Single global context, following the pattern used by many existing drivers. */
struct al_link_context {
	struct al_link_config config;
	struct al_link_usb usb;
};

static struct al_link_context al_link = {
	.config = {
		.vid = {AL_LINK_DEFAULT_VID, 0},
		.pid = {AL_LINK_DEFAULT_PID, 0},
		.ep_in = AL_LINK_EP_DATA_IN,
		.ep_out = AL_LINK_EP_DATA_OUT,
		.ep_cfg = AL_LINK_EP_CFG_OUT,
		.interface_num = 0,
		.packet_size = AL_LINK_DEFAULT_PACKET_SIZE,
		.speed_byte = AL_LINK_SPEED_2M,
		.sample_high_nibble = true,
	},
};

struct al_link_speed_entry {
	int khz;
	uint8_t speed_byte;
};

static const struct al_link_speed_entry al_link_speed_table[] = {
	{ 12000, AL_LINK_SPEED_12M },
	{  6000, AL_LINK_SPEED_6M },
	{  4000, AL_LINK_SPEED_4M },
	{  2000, AL_LINK_SPEED_2M },
	{  1200, AL_LINK_SPEED_1200K },
	{   800, AL_LINK_SPEED_800K },
	{   500, AL_LINK_SPEED_500K },
	{   200, AL_LINK_SPEED_200K },
	{   180, AL_LINK_SPEED_180K },
};

static int al_link_init(void);
static int al_link_quit(void);
static int al_link_reset(int srst, int trst);
static int al_link_speed(int speed);
static int al_link_khz(int khz, int *jtag_speed);
static int al_link_speed_div(int speed, int *khz);
static int al_link_execute_queue(void);

static int al_link_usb_open(void);
static void al_link_usb_close(void);
static void al_link_usb_drain_ep2(void);
static int al_link_usb_set_speed(uint8_t speed_byte);
static int al_link_usb_xfer(const uint8_t *tx, size_t tx_len,
	uint8_t *rx, size_t rx_len, uint8_t idle_byte);

static int al_link_execute_stableclocks(struct jtag_command *cmd);
static int al_link_execute_runtest(struct jtag_command *cmd);
static int al_link_execute_scan(struct jtag_command *cmd);
static int al_link_execute_pathmove(struct jtag_command *cmd);
static int al_link_execute_tms(struct jtag_command *cmd);
static int al_link_execute_statemove(size_t skip);
static int al_link_execute_tlr_reset(void);
static int al_link_execute_reset(struct jtag_command *cmd);

static int al_link_end_state(tap_state_t state);

/*
 * Extract one bit from a little-endian OpenOCD bit buffer.
 *
 * OpenOCD scan buffers are bit-addressed LSB-first, so every place that maps
 * scan data to protocol bytes should use a single helper to avoid subtle
 * off-by-one or bit-endianness mistakes.
 */
static inline int al_link_get_bit(const uint8_t *buf, unsigned int bit)
{
	return (buf[bit / 8] >> (bit % 8)) & 0x1;
}

/*
 * Write one bit into a little-endian OpenOCD bit buffer.
 *
 * This is used when a scan operation reads TDO back from the adapter and the
 * resulting bits must be written into the same format that jtag_read_buffer()
 * expects.
 */
static inline void al_link_set_bit(uint8_t *buf, unsigned int bit, int value)
{
	uint8_t mask = 1u << (bit % 8);

	if (value)
		buf[bit / 8] |= mask;
	else
		buf[bit / 8] &= ~mask;
}

/*
 * Encode one 4-bit AL-LINK frame.
 *
 * The old AL-LINK direct-control protocol uses one nibble per GPIO snapshot:
 *   bit0 = TMS
 *   bit1 = TDI
 *   bit2 = TCK
 *   bit3 = unused
 */
static inline uint8_t al_link_frame(bool tck, bool tdi, bool tms)
{
	return (tms ? 0x01 : 0) |
		(tdi ? 0x02 : 0) |
		(tck ? 0x04 : 0);
}

static inline uint8_t al_link_pack(uint8_t low_frame, uint8_t high_frame)
{
	return (low_frame & 0x0f) | ((high_frame & 0x0f) << 4);
}

/*
 * Encode one logical JTAG clock into one USB byte.
 *
 * The chosen model is:
 *   low nibble  = setup half-step,  TCK=0
 *   high nibble = clock half-step,  TCK=1
 *
 * This gives the driver a useful transport invariant: one logical JTAG bit
 * becomes one USB byte on EP6. Only scan/readback transfers consume matching
 * EP2 response bytes; write-only transfers skip EP2 entirely.
 */
static inline uint8_t al_link_encode_cycle(int tms, int tdi)
{
	return al_link_pack(al_link_frame(false, tdi, tms),
		al_link_frame(true, tdi, tms));
}

/*
 * Encode a padding/no-op cycle used only to fill out packet-aligned transfers.
 *
 * Ground truth from a real, successful Anlogic vendor-tool capture (Bus Hound
 * / USBPcap on the same AL-Link probe, VID:PID 336C:1001) shows that its tail
 * padding bytes are a distinct 0x44 sentinel (both nibbles encode tck=1),
 * never the tck=0 setup/clock split used by real cycles (see
 * al_link_encode_cycle). Real cycles always have tck=0 in the low nibble, so
 * a tck=1 low nibble is otherwise unreachable and lets the adapter tell "this
 * byte is padding, do not clock it" apart from "this byte is a real TMS/TDI
 * cycle". Our previous idle encoding used tck=false in both nibbles, which is
 * indistinguishable from a real cycle's low nibble and was likely being
 * executed as an extra, unwanted clock by the adapter.
 */
static inline uint8_t al_link_encode_idle(int tms, int tdi)
{
	uint8_t frame = al_link_frame(true, tdi, tms);

	return al_link_pack(frame, frame);
}

/*
 * Decode the effective TDO sample for one logical cycle.
 *
 * The initial assumption is that the high-nibble sample aligns better with the
 * actual JTAG clock edge, because it corresponds to the second half-step where
 * TCK is high. This is configurable because real hardware may prove that the
 * low-nibble sample is the correct one.
 */
static inline int al_link_decode_tdo(uint8_t rx_byte)
{
	if (al_link.config.sample_high_nibble)
		return (rx_byte >> 4) & 0x1;

	return rx_byte & 0x1;
}

/*
 * Convert an OpenOCD speed request in kHz to the EP8 speed byte.
 *
 * The mapping follows the legacy speed table taken from the original AL-LINK
 * tooling:
 *   12000 kHz -> 0x00
 *    6000 kHz -> 0x04
 *    4000 kHz -> 0x08
 *    2000 kHz -> 0x14
 *    1200 kHz -> 0x24
 *     800 kHz -> 0x38
 *     500 kHz -> 0x5c
 *     200 kHz -> 0x80
 *     180 kHz -> 0xff
 *
 * The selected entry never exceeds the requested speed. That matches the usual
 * OpenOCD expectation that adapter speed is an upper bound.
 */
static uint8_t al_link_map_speed_byte(int speed)
{
	size_t i;

	if (al_link.config.speed_byte_overridden)
		return al_link.config.speed_byte;

	if (speed <= 0)
		return AL_LINK_SPEED_2M;

	for (i = 0; i < ARRAY_SIZE(al_link_speed_table); i++) {
		if (speed >= al_link_speed_table[i].khz)
			return al_link_speed_table[i].speed_byte;
	}

	return AL_LINK_SPEED_180K;
}

/* Translate an internal speed byte back to the nominal kHz value. */
static int al_link_map_khz(uint8_t speed_byte)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(al_link_speed_table); i++) {
		if (speed_byte == al_link_speed_table[i].speed_byte)
			return al_link_speed_table[i].khz;
	}

	return 2000;
}

/*
 * Allocate parallel TX/RX buffers for a sequence of logical cycles.
 *
 * Each logical cycle maps to one outgoing byte. Allocating the optional RX
 * buffer here keeps scan readback paths straightforward, while non-scan paths
 * can pass readback=false and avoid waiting for EP2 data.
 */
static int al_link_alloc_cycles(size_t num_cycles, uint8_t **tx_buf, uint8_t **rx_buf)
{
	*tx_buf = NULL;
	*rx_buf = NULL;

	if (num_cycles == 0)
		return ERROR_OK;

	if (num_cycles > (size_t)INT_MAX)
		return ERROR_FAIL;

	*tx_buf = malloc(num_cycles);
	*rx_buf = calloc(num_cycles, 1);
	if (!*tx_buf || !*rx_buf) {
		free(*tx_buf);
		free(*rx_buf);
		*tx_buf = NULL;
		*rx_buf = NULL;
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

/*
 * Ship an already-encoded sequence of cycles to the adapter.
 *
 * Real AL-LINK (FX2LP) direct IO mode only produces EP2 data for cycles that
 * are actually sampled; write-only traffic (TLR reset, statemove, pathmove,
 * tms, stableclocks) returns nothing on EP2, and reading it there blocks until
 * the 6000 ms timeout. So readback gates the EP2 read: scans pass rx_buf with a
 * non-zero rx_len; write-only callers pass NULL/0 and the transport skips EP2.
 */
static int al_link_send_cycles(uint8_t *tx_buf, uint8_t *rx_buf,
	size_t num_cycles, bool readback, int idle_tms, int idle_tdi)
{
	uint8_t idle_byte = al_link_encode_idle(idle_tms, idle_tdi);

	if (num_cycles == 0)
		return ERROR_OK;

	return al_link_usb_xfer(tx_buf, num_cycles,
		readback ? rx_buf : NULL, readback ? num_cycles : 0, idle_byte);
}

/*
 * Force the hardware TAP into Test-Logic-Reset.
 *
 * The adapter does not yet expose dedicated TRST wiring, so when OpenOCD asks
 * for a TAP-affecting reset we must drive the standard TMS reset sequence
 * explicitly instead of only updating the software state tracker.
 */
static int al_link_execute_tlr_reset(void)
{
	uint8_t *tx_buf;
	uint8_t *rx_buf;
	int retval;

	retval = al_link_alloc_cycles(AL_LINK_TLR_RESET_CYCLES, &tx_buf, &rx_buf);
	if (retval != ERROR_OK)
		return retval;

	for (size_t index = 0; index < AL_LINK_TLR_RESET_CYCLES; index++)
		tx_buf[index] = al_link_encode_cycle(1, 0);

	retval = al_link_send_cycles(tx_buf, rx_buf,
		AL_LINK_TLR_RESET_CYCLES, false, 1, 0);
	free(tx_buf);
	free(rx_buf);
	if (retval != ERROR_OK)
		return retval;

	tap_set_state(TAP_RESET);
	return ERROR_OK;
}

/*
 * Session byte-stream accounting for EP2 readback alignment.
 *
 * The FX2LP firmware returns exactly one EP2 byte for every EP6 byte it
 * receives (real cycles, idle padding, and write-only traffic alike). Because
 * write-only operations never read their EP2 bytes back, those bytes pile up in
 * the device FIFO and would otherwise displace real readback data. stream_w
 * counts every byte written to EP6, stream_r every byte read from EP2; both are
 * reset to zero when a fresh session opens (after the EP2 drain).
 */
static uint64_t al_link_stream_w;
static uint64_t al_link_stream_r;

/*
 * Open the adapter via the CyUSB driver and reset endpoint pipes.
 *
 * The driver opens a CyUSB device interface directly, so there is no explicit
 * per-interface claim step in this code path.
 */
static int al_link_usb_open(void)
{
	int retval;

	if (al_link.usb.is_open)
		return ERROR_OK;

	retval = al_cyusb_open(al_link.config.vid, al_link.config.pid,
			       &al_link.usb.cyusb);
	if (retval != 0) {
		LOG_ERROR("AL-LINK: unable to open CyUSB device");
		return ERROR_FAIL;
	}

	/* Clear stalled pipes (best-effort, non-fatal) */
	al_cyusb_reset_pipe(&al_link.usb.cyusb, al_link.config.ep_in);
	al_cyusb_reset_pipe(&al_link.usb.cyusb, al_link.config.ep_out);
	al_cyusb_reset_pipe(&al_link.usb.cyusb, al_link.config.ep_cfg);

	/*
	 * A host-side pipe reset does not guarantee the FX2LP device itself
	 * discarded bytes already sitting in its EP2 IN FIFO from a previous
	 * session. Drain them now so the first real scan of this session never
	 * observes stale data.
	 */
	al_link_usb_drain_ep2();

	/* Fresh session: the device FIFO was just drained, so our byte-stream
	 * accounting starts from zero. */
	al_link_stream_w = 0;
	al_link_stream_r = 0;

	al_link.usb.is_open = true;
	al_link.usb.io_failed = false;
	return ERROR_OK;
}

/*
 * Best-effort drain of any stale bytes left in the EP2 IN FIFO from a
 * previous session.
 *
 * Uses a short per-read timeout so an already-empty pipe returns quickly
 * instead of blocking for the full 6000 ms USB timeout. Bounded to a handful
 * of rounds so a genuinely stuck endpoint cannot hang device open.
 */
static void al_link_usb_drain_ep2(void)
{
	uint8_t scratch[512];
	int drained_total = 0;
	int rounds = 0;

	while (rounds < 8) {
		int transferred = 0;
		int retval = al_cyusb_bulk_read(&al_link.usb.cyusb, al_link.config.ep_in,
			scratch, (int)sizeof(scratch), &transferred, 50);

		if (retval != 0 || transferred <= 0)
			break;

		drained_total += transferred;
		rounds++;
	}

	if (drained_total > 0)
		LOG_INFO("AL-LINK: drained %d stale byte(s) from EP2 before session start",
			drained_total);
}

/* Close the CyUSB device handle. */
static void al_link_usb_close(void)
{
	if (!al_link.usb.is_open)
		return;

	al_cyusb_close(&al_link.usb.cyusb);
	al_link.usb.is_open = false;
	al_link.usb.io_failed = false;
}

/*
 * Send the 2-byte EP8 mode configuration packet.
 *
 * Packet format:
 *   byte0 = mode selector
 *   byte1 = speed byte
 *
 * Verified 2026-07-07 against a real Anlogic vendor-tool USB capture: the
 * real mode byte is 0x01 (see AL_LINK_MODE_DIRECT), not the previously
 * assumed 0x04.
 */
static int al_link_usb_set_speed(uint8_t speed_byte)
{
	uint8_t packet[2] = { AL_LINK_MODE_DIRECT, speed_byte };
	int transferred = 0;
	int retval;

	retval = al_cyusb_bulk_write(&al_link.usb.cyusb, al_link.config.ep_cfg,
		packet, sizeof(packet), &transferred, AL_LINK_USB_TIMEOUT_MS);
	if (retval != 0)
		return ERROR_FAIL;
	if (transferred != (int)sizeof(packet)) {
		LOG_ERROR("AL-LINK: short EP8 write, expected %u bytes, wrote %d",
			(unsigned int)sizeof(packet), transferred);
		return ERROR_FAIL;
	}

	al_link.config.speed_byte = speed_byte;
	return ERROR_OK;
}

static int al_link_usb_mark_failed(const char *where)
{
	LOG_ERROR("AL-LINK: USB transfer failed during %s; resetting pipes and aborting remaining queued JTAG work",
		where);
	al_link.usb.io_failed = true;
	al_cyusb_reset_pipe(&al_link.usb.cyusb, al_link.config.ep_in);
	al_cyusb_reset_pipe(&al_link.usb.cyusb, al_link.config.ep_out);
	al_cyusb_reset_pipe(&al_link.usb.cyusb, al_link.config.ep_cfg);
	return ERROR_FAIL;
}


/*
 * Transfer a logical byte stream using the AL-LINK direct-mode framing.
 *
 * Write-only callers pass rx_len == 0 and are sent at their real length with no
 * EP2 read. Scan/readback callers pass rx_len > 0 and are transferred in
 * packet_size-aligned EP6/EP2 pairs. The write is padded with idle_byte up to a
 * packet_size boundary so every readback byte lands inside a released EP2 batch;
 * bytes belonging to preceding write-only ops (and this scan's padding) are read
 * and discarded, keeping the EP2 stream aligned with EP6 (see al_link_stream_w).
 */
static int al_link_usb_xfer(const uint8_t *tx, size_t tx_len,
	uint8_t *rx, size_t rx_len, uint8_t idle_byte)
{
	size_t offset = 0;
	uint8_t *tx_packet = NULL;
	uint8_t *rx_packet = NULL;
	int transferred = 0;
	int retval;
	/* Temporary diagnostic accumulators; only populated when
	 * al_link_dump_raw_rx is enabled. See the histogram dump right before
	 * this function returns in the rx_len > 0 path. */
	size_t dbg_bit_ones[8] = {0};
	size_t dbg_bit_total = 0;
	uint8_t dbg_first_bytes[16];
	size_t dbg_first_bytes_len = 0;

	if (!al_link.usb.is_open)
		return ERROR_FAIL;

	if (al_link.usb.io_failed) {
		LOG_ERROR("AL-LINK: previous USB transfer failed; re-run init to reopen the adapter");
		return ERROR_FAIL;
	}

	if (tx_len > (size_t)INT_MAX || rx_len > (size_t)INT_MAX) {
		LOG_ERROR("AL-LINK: transfer too large for CyUSB API (tx=%u rx=%u)",
			(unsigned int)tx_len, (unsigned int)rx_len);
		return ERROR_FAIL;
	}

	if (rx_len > tx_len) {
		LOG_ERROR("AL-LINK: EP2 readback length exceeds EP6 transfer length");
		return ERROR_FAIL;
	}
	if (rx_len > 0 && !rx) {
		LOG_ERROR("AL-LINK: EP2 readback requested without a destination buffer");
		return ERROR_FAIL;
	}

	if (tx_len == 0)
		return ERROR_OK;

	if (rx_len == 0) {
		while (offset < tx_len) {
			size_t chunk = tx_len - offset;

			if (chunk > al_link.config.packet_size)
				chunk = al_link.config.packet_size;

			transferred = 0;
			LOG_DEBUG("AL-LINK: EP6 write begin ep=0x%02x offset=%u chunk=%u logical_tx=%u logical_rx=%u packet_size=%u",
				al_link.config.ep_out,
				(unsigned int)offset,
				(unsigned int)chunk,
				(unsigned int)tx_len,
				(unsigned int)rx_len,
				(unsigned int)al_link.config.packet_size);
			retval = al_cyusb_bulk_write(&al_link.usb.cyusb, al_link.config.ep_out,
				tx + offset, (int)chunk, &transferred, AL_LINK_USB_TIMEOUT_MS);
			LOG_DEBUG("AL-LINK: EP6 write done retval=%d transferred=%d",
				retval, transferred);
			if (retval != 0)
				return al_link_usb_mark_failed("EP6 write");
			if (transferred != (int)chunk) {
				LOG_ERROR("AL-LINK: short EP6 write, expected %u bytes, wrote %d",
					(unsigned int)chunk, transferred);
				return al_link_usb_mark_failed("EP6 write");
			}

			al_link_stream_w += (uint64_t)chunk;
			offset += chunk;
		}

		return ERROR_OK;
	}

	size_t ps = al_link.config.packet_size;

	/*
	 * Padding can push a single write up to just under two packets, so the TX
	 * scratch buffer must hold 2 * packet_size. EP2 is drained one packet_size
	 * batch at a time into rx_packet.
	 */
	tx_packet = malloc(ps * 2);
	rx_packet = malloc(ps);
	if (!tx_packet || !rx_packet) {
		free(tx_packet);
		free(rx_packet);
		LOG_ERROR("AL-LINK: out of memory allocating aligned USB packets");
		return ERROR_FAIL;
	}

	while (offset < tx_len) {
		size_t real_chunk = tx_len - offset;
		size_t keep = offset < rx_len ? rx_len - offset : 0;
		uint64_t data_start = al_link_stream_w;
		uint64_t end;
		size_t write_len;
		size_t pad_len;

		if (real_chunk > ps)
			real_chunk = ps;
		if (keep > real_chunk)
			keep = real_chunk;

		/*
		 * Pad the write up to the next packet_size boundary so this scan's
		 * bytes all fall inside released EP2 batches (no straddle). The
		 * padding is appended after the scan has already exited to a stable
		 * end state, so the extra idle cycles are harmless.
		 */
		end = data_start + real_chunk;
		if (end % ps)
			end += ps - (end % ps);
		write_len = (size_t)(end - data_start);
		pad_len = write_len - real_chunk;

		memset(tx_packet, idle_byte, write_len);
		memcpy(tx_packet, tx + offset, real_chunk);

		transferred = 0;
		LOG_DEBUG("AL-LINK: EP6 write begin ep=0x%02x offset=%u real_chunk=%u write_len=%u pad_len=%u data_start=%u logical_tx=%u logical_rx=%u",
			al_link.config.ep_out,
			(unsigned int)offset,
			(unsigned int)real_chunk,
			(unsigned int)write_len,
			(unsigned int)pad_len,
			(unsigned int)data_start,
			(unsigned int)tx_len,
			(unsigned int)rx_len);
		retval = al_cyusb_bulk_write(&al_link.usb.cyusb, al_link.config.ep_out,
			tx_packet, (int)write_len, &transferred, AL_LINK_USB_TIMEOUT_MS);
		LOG_DEBUG("AL-LINK: EP6 write done retval=%d transferred=%d",
			retval, transferred);
		if (retval != 0) {
			free(tx_packet);
			free(rx_packet);
			return al_link_usb_mark_failed("EP6 write");
		}
		if (transferred != (int)write_len) {
			LOG_ERROR("AL-LINK: short EP6 write, expected %u bytes, wrote %d",
				(unsigned int)write_len, transferred);
			free(tx_packet);
			free(rx_packet);
			return al_link_usb_mark_failed("EP6 write");
		}

		al_link_stream_w = end;

		/*
		 * Drain every full batch the device has now released (up to end),
		 * copying only the bytes that belong to this scan's readback window
		 * [data_start, data_start + keep). Leading bytes are the unread EP2
		 * responses of preceding write-only operations and are discarded.
		 */
		while (al_link_stream_r < end) {
			uint64_t batch_start = al_link_stream_r;
			uint64_t batch_end = batch_start + ps;

			transferred = 0;
			LOG_DEBUG("AL-LINK: EP2 read begin ep=0x%02x batch_start=%u read_len=%u keep=%u",
				al_link.config.ep_in,
				(unsigned int)batch_start,
				(unsigned int)ps,
				(unsigned int)keep);
			retval = al_cyusb_bulk_read(&al_link.usb.cyusb, al_link.config.ep_in,
				rx_packet, (int)ps, &transferred, AL_LINK_USB_TIMEOUT_MS);
			LOG_DEBUG("AL-LINK: EP2 read done retval=%d transferred=%d",
				retval, transferred);
			if (retval != 0) {
				free(tx_packet);
				free(rx_packet);
				return al_link_usb_mark_failed("EP2 read");
			}
			if (transferred != (int)ps) {
				LOG_ERROR("AL-LINK: short EP2 read, expected %u bytes, got %d",
					(unsigned int)ps, transferred);
				free(tx_packet);
				free(rx_packet);
				return al_link_usb_mark_failed("EP2 read");
			}

			al_link_stream_r = batch_end;

			if (keep > 0) {
				uint64_t want_start = data_start;
				uint64_t want_end = data_start + keep;
				uint64_t ov_start = want_start > batch_start ? want_start : batch_start;
				uint64_t ov_end = want_end < batch_end ? want_end : batch_end;

				if (ov_start < ov_end) {
					size_t n = (size_t)(ov_end - ov_start);
					size_t dst = offset + (size_t)(ov_start - want_start);
					size_t src = (size_t)(ov_start - batch_start);

					memcpy(rx + dst, rx_packet + src, n);

					if (al_link.config.dump_raw_rx) {
						for (size_t i = 0; i < n; i++) {
							uint8_t byte = rx_packet[src + i];

							for (int bit = 0; bit < 8; bit++) {
								if ((byte >> bit) & 0x1)
									dbg_bit_ones[bit]++;
							}
							dbg_bit_total++;

							if (dbg_first_bytes_len < sizeof(dbg_first_bytes))
								dbg_first_bytes[dbg_first_bytes_len++] = byte;
						}
					}
				}
			}
		}

		if (al_link.config.dump_raw_rx)
			LOG_INFO("AL-LINK: readback pkt data_start=%u real=%u keep=%u write_len=%u stream_w=%u stream_r=%u",
				(unsigned int)data_start,
				(unsigned int)real_chunk,
				(unsigned int)keep,
				(unsigned int)write_len,
				(unsigned int)al_link_stream_w,
				(unsigned int)al_link_stream_r);

		offset += real_chunk;
	}

	if (al_link.config.dump_raw_rx && dbg_bit_total > 0) {
		LOG_INFO("AL-LINK: raw EP2 bit histogram over %u valid bytes: "
			"bit0=%u bit1=%u bit2=%u bit3=%u bit4=%u bit5=%u bit6=%u bit7=%u (ones out of %u)",
			(unsigned int)dbg_bit_total,
			(unsigned int)dbg_bit_ones[0], (unsigned int)dbg_bit_ones[1],
			(unsigned int)dbg_bit_ones[2], (unsigned int)dbg_bit_ones[3],
			(unsigned int)dbg_bit_ones[4], (unsigned int)dbg_bit_ones[5],
			(unsigned int)dbg_bit_ones[6], (unsigned int)dbg_bit_ones[7],
			(unsigned int)dbg_bit_total);

		char hex[sizeof(dbg_first_bytes) * 3 + 1];
		size_t hex_pos = 0;

		for (size_t i = 0; i < dbg_first_bytes_len; i++)
			hex_pos += (size_t)snprintf(hex + hex_pos, sizeof(hex) - hex_pos,
				"%02x ", dbg_first_bytes[i]);

		LOG_INFO("AL-LINK: raw EP2 first %u bytes: %s",
			(unsigned int)dbg_first_bytes_len, hex);
	}

	free(tx_packet);
	free(rx_packet);

	return ERROR_OK;
}

/*
 * Validate and record the desired end state for a queued JTAG operation.
 *
 * The helper exists because several queue commands use exactly the same policy:
 * reject non-stable end states early and leave the global TAP helper state in a
 * form that statemove logic can consume later.
 */
static int al_link_end_state(tap_state_t state)
{
	if (!tap_is_state_stable(state)) {
		LOG_ERROR("BUG: %s is not a valid stable end state", tap_state_name(state));
		return ERROR_JTAG_QUEUE_FAILED;
	}

	tap_set_end_state(state);
	return ERROR_OK;
}

/*
 * Execute a generic TAP state move using the canonical OpenOCD TMS path.
 *
 * The skip parameter is used when the last scan bit already generated the first
 * state-transitioning TMS=1 edge and only the remainder of the exit path must
 * be emitted.
 */
static int al_link_execute_statemove(size_t skip)
{
	uint8_t tms_scan = tap_get_tms_path(tap_get_state(), tap_get_end_state());
	int tms_count = tap_get_tms_path_len(tap_get_state(), tap_get_end_state());
	int last_tms = 0;
	uint8_t *tx_buf;
	uint8_t *rx_buf;
	int retval;

	if ((size_t)tms_count < skip)
		return ERROR_FAIL;

	retval = al_link_alloc_cycles((size_t)tms_count - skip, &tx_buf, &rx_buf);
	if (retval != ERROR_OK)
		return retval;

	for (int index = (int)skip; index < tms_count; index++) {
		last_tms = (tms_scan >> index) & 0x1;
		tx_buf[index - (int)skip] = al_link_encode_cycle(last_tms, 0);
	}

	retval = al_link_send_cycles(tx_buf, rx_buf,
		(size_t)tms_count - skip, false, last_tms, 0);
	free(tx_buf);
	free(rx_buf);
	if (retval != ERROR_OK)
		return retval;

	tap_set_state(tap_get_end_state());
	return ERROR_OK;
}

/*
 * Execute an explicit JTAG_TMS bit sequence.
 *
 * Unlike statemove/pathmove, JTAG_TMS comes from the queue as a raw bitstream,
 * so this function forwards the bits directly without trying to infer TAP state
 * transitions.
 */
static int al_link_execute_tms(struct jtag_command *cmd)
{
	unsigned int num_bits = cmd->cmd.tms->num_bits;
	const uint8_t *bits = cmd->cmd.tms->bits;
	int last_tms = 0;
	uint8_t *tx_buf;
	uint8_t *rx_buf;
	int retval;

	retval = al_link_alloc_cycles(num_bits, &tx_buf, &rx_buf);
	if (retval != ERROR_OK)
		return retval;

	for (unsigned int bit = 0; bit < num_bits; bit++) {
		last_tms = al_link_get_bit(bits, bit);
		tx_buf[bit] = al_link_encode_cycle(last_tms, 0);
	}

	retval = al_link_send_cycles(tx_buf, rx_buf, num_bits, false, last_tms, 0);
	free(tx_buf);
	free(rx_buf);
	return retval;
}

/*
 * Generate clocks while holding the TAP in its current stable state.
 *
 * In TAP_RESET the line must keep TMS high; in the normal stable states it must
 * keep TMS low. This matches the standard OpenOCD stableclocks semantics.
 */
static int al_link_execute_stableclocks(struct jtag_command *cmd)
{
	int tms;
	int retval;
	uint8_t *tx_buf;
	uint8_t *rx_buf;
	int num_cycles = cmd->cmd.stableclocks->num_cycles;

	switch (tap_get_state()) {
	case TAP_RESET:
		tms = 1;
		break;
	case TAP_IDLE:
	case TAP_DRSHIFT:
	case TAP_DRPAUSE:
	case TAP_IRSHIFT:
	case TAP_IRPAUSE:
		tms = 0;
		break;
	default:
		LOG_ERROR("AL-LINK: JTAG_STABLECLOCKS in non-stable state %s",
			tap_state_name(tap_get_state()));
		return ERROR_JTAG_QUEUE_FAILED;
	}

	retval = al_link_alloc_cycles((size_t)num_cycles, &tx_buf, &rx_buf);
	if (retval != ERROR_OK)
		return retval;

	for (int index = 0; index < num_cycles; index++)
		tx_buf[index] = al_link_encode_cycle(tms, 0);

	retval = al_link_send_cycles(tx_buf, rx_buf,
		(size_t)num_cycles, false, tms, 0);
	free(tx_buf);
	free(rx_buf);
	return retval;
}

/*
 * Implement OpenOCD runtest by moving to IDLE, clocking the requested number
 * of cycles there, and then optionally moving to the requested end state.
 */
static int al_link_execute_runtest(struct jtag_command *cmd)
{
	tap_state_t requested_end_state = cmd->cmd.runtest->end_state;
	int retval;

	retval = al_link_end_state(requested_end_state);
	if (retval != ERROR_OK)
		return retval;

	if (tap_get_state() != TAP_IDLE) {
		tap_set_end_state(TAP_IDLE);
		retval = al_link_execute_statemove(0);
		if (retval != ERROR_OK)
			return retval;
		tap_set_end_state(requested_end_state);
	}

	retval = al_link_execute_stableclocks(&(struct jtag_command) {
		.type = JTAG_STABLECLOCKS,
		.cmd.stableclocks = &(struct stableclocks_command) {
			.num_cycles = cmd->cmd.runtest->num_cycles,
		},
	});
	if (retval != ERROR_OK)
		return retval;

	if (tap_get_state() != tap_get_end_state())
		return al_link_execute_statemove(0);

	return ERROR_OK;
}

/*
 * Follow an explicit TAP path supplied by OpenOCD.
 *
 * The entire path is encoded first and then sent in one USB transaction. The
 * local TAP state is only updated after the transfer completes successfully, so
 * the software state remains conservative if the USB operation fails.
 */
static int al_link_execute_pathmove(struct jtag_command *cmd)
{
	int num_states = cmd->cmd.pathmove->num_states;
	tap_state_t *path = cmd->cmd.pathmove->path;
	tap_state_t current_state = tap_get_state();
	int last_tms = 0;
	uint8_t *tx_buf;
	uint8_t *rx_buf;
	int retval;

	retval = al_link_alloc_cycles((size_t)num_states, &tx_buf, &rx_buf);
	if (retval != ERROR_OK)
		return retval;

	for (int index = 0; index < num_states; index++) {
		int tms;

		if (path[index] == tap_state_transition(current_state, false))
			tms = 0;
		else if (path[index] == tap_state_transition(current_state, true))
			tms = 1;
		else {
			LOG_ERROR("BUG: %s -> %s is not a valid TAP transition",
				tap_state_name(current_state),
				tap_state_name(path[index]));
			free(tx_buf);
			free(rx_buf);
			return ERROR_JTAG_QUEUE_FAILED;
		}

		last_tms = tms;
		tx_buf[index] = al_link_encode_cycle(tms, 0);
		current_state = path[index];
	}

	retval = al_link_send_cycles(tx_buf, rx_buf,
		(size_t)num_states, false, last_tms, 0);
	free(tx_buf);
	free(rx_buf);
	if (retval != ERROR_OK)
		return retval;

	tap_set_state(current_state);
	tap_set_end_state(current_state);
	return ERROR_OK;
}

/*
 * Execute one IR or DR scan.
 *
 * Flow:
 * 1. Move into the correct shift state if needed.
 * 2. Ask OpenOCD to build the outgoing TDI buffer.
 * 3. Encode each scan bit into one AL-LINK byte.
 * 4. Read the matching response bytes and decode TDO if required.
 * 5. Let jtag_read_buffer() distribute the captured bits back to the original
 *    scan fields.
 * 6. If the scan exits shift state, finish the remaining TAP path.
 */
static int al_link_execute_scan(struct jtag_command *cmd)
{
	enum scan_type type = jtag_scan_type(cmd->cmd.scan);
	tap_state_t shift_state = cmd->cmd.scan->ir_scan ? TAP_IRSHIFT : TAP_DRSHIFT;
	tap_state_t saved_end_state = cmd->cmd.scan->end_state;
	uint8_t *buffer = NULL;
	uint8_t *tx_buf = NULL;
	uint8_t *rx_buf = NULL;
	bool readback = type != SCAN_OUT;
	int last_tms = 0;
	int last_tdi = 0;
	int scan_size;
	size_t tx_index;
	int retval;

	retval = al_link_end_state(saved_end_state);
	if (retval != ERROR_OK)
		return retval;

	if (tap_get_state() != shift_state) {
		tap_set_end_state(shift_state);
		retval = al_link_execute_statemove(0);
		if (retval != ERROR_OK)
			return retval;
		tap_set_end_state(saved_end_state);
	}

	scan_size = jtag_build_buffer(cmd->cmd.scan, &buffer);
	retval = al_link_alloc_cycles((size_t)scan_size, &tx_buf, &rx_buf);
	if (retval != ERROR_OK)
		goto out;

	tx_index = 0;
	for (int bit = 0; bit < scan_size; bit++) {
		int tms = (bit == scan_size - 1 && saved_end_state != shift_state) ? 1 : 0;
		int tdi = type != SCAN_IN ? al_link_get_bit(buffer, (unsigned int)bit) : 0;
		last_tms = tms;
		last_tdi = tdi;
		tx_buf[tx_index++] = al_link_encode_cycle(tms, tdi);
	}

	retval = al_link_send_cycles(tx_buf, rx_buf,
		(size_t)scan_size,
		readback, last_tms, last_tdi);
	if (retval != ERROR_OK)
		goto out;

	if (saved_end_state != shift_state) {
		retval = al_link_execute_statemove(1);
		if (retval != ERROR_OK)
			goto out;
	}

	if (readback) {
		for (int bit = 0; bit < scan_size; bit++)
			al_link_set_bit(buffer, (unsigned int)bit, al_link_decode_tdo(rx_buf[bit]));
	}

	retval = jtag_read_buffer(buffer, cmd->cmd.scan);
	if (retval != ERROR_OK)
		goto out;

out:
	free(buffer);
	free(tx_buf);
	free(rx_buf);
	return retval;
}

/*
 * Handle queue-level reset requests.
 *
 * Dedicated SRST/TRST wiring is still deferred until the electrical behavior
 * is confirmed on hardware. When OpenOCD requests a TAP-affecting reset, we
 * therefore emulate it with the standard TMS reset sequence so the hardware
 * and software TAP states stay aligned.
 */
static int al_link_execute_reset(struct jtag_command *cmd)
{
	bool srst_requested = cmd->cmd.reset->srst == 1;
	bool trst_requested = cmd->cmd.reset->trst == 1;
	bool tap_reset_requested = trst_requested ||
		(srst_requested &&
		(jtag_get_reset_config() & RESET_SRST_PULLS_TRST));
	int retval = ERROR_OK;

	if (tap_reset_requested)
		retval = al_link_execute_tlr_reset();
	if (retval != ERROR_OK)
		return retval;
	if (tap_reset_requested)
		tap_set_end_state(TAP_RESET);

	if (srst_requested && !tap_reset_requested)
		LOG_WARNING("AL-LINK: SRST was requested but the adapter has no dedicated SRST line; the system reset request was ignored");

	return ERROR_OK;
}

/*
 * Top-level queue dispatcher.
 *
 * This function stays intentionally simple: each queue item is translated by a
 * specialized helper and the first failure aborts the whole batch. That keeps
 * early bring-up behavior explicit and easy to debug.
 */
static int al_link_execute_queue(void)
{
	struct jtag_command *cmd = jtag_command_queue;

	while (cmd) {
		int retval;

		switch (cmd->type) {
		case JTAG_RESET:
			retval = al_link_execute_reset(cmd);
			break;
		case JTAG_RUNTEST:
			retval = al_link_execute_runtest(cmd);
			break;
		case JTAG_TLR_RESET:
			retval = al_link_end_state(cmd->cmd.statemove->end_state);
			if (retval != ERROR_OK)
				break;

			retval = al_link_execute_tlr_reset();
			if (retval != ERROR_OK)
				break;

			if (tap_get_end_state() != TAP_RESET)
				retval = al_link_execute_statemove(0);
			break;
		case JTAG_PATHMOVE:
			retval = al_link_execute_pathmove(cmd);
			break;
		case JTAG_SCAN:
			retval = al_link_execute_scan(cmd);
			break;
		case JTAG_SLEEP:
			jtag_sleep(cmd->cmd.sleep->us);
			retval = ERROR_OK;
			break;
		case JTAG_STABLECLOCKS:
			retval = al_link_execute_stableclocks(cmd);
			break;
		case JTAG_TMS:
			retval = al_link_execute_tms(cmd);
			break;
		default:
			LOG_ERROR("BUG: unknown JTAG command type encountered: %d", cmd->type);
			retval = ERROR_JTAG_QUEUE_FAILED;
			break;
		}

		if (retval != ERROR_OK)
			return retval;

		cmd = cmd->next;
	}

	return ERROR_OK;
}

/*
 * Driver initialization.
 *
 * The minimal contract here is:
 * 1. open the USB device,
 * 2. reset/prepare the endpoint pipes,
 * 3. switch the adapter into the legacy 0x04 bit-bang mode via EP8.
 *
 * A future smoke-test step can be inserted here once the hardware is available,
 * for example one fully padded 512-byte traditional-mode batch.
 */
static int al_link_init(void)
{
	int retval = al_link_usb_open();

	if (retval != ERROR_OK)
		return ERROR_JTAG_INIT_FAILED;

	retval = al_link_usb_set_speed(al_link.config.speed_byte);
	if (retval != ERROR_OK) {
		al_link_usb_close();
		return ERROR_JTAG_INIT_FAILED;
	}

	LOG_INFO("AL-LINK: opened USB device VID:PID %04x:%04x, interface %u, endpoints in/out/cfg 0x%02x/0x%02x/0x%02x",
		al_link.config.vid[0], al_link.config.pid[0],
		al_link.config.interface_num,
		al_link.config.ep_in, al_link.config.ep_out, al_link.config.ep_cfg);

	return ERROR_OK;
}

/* Driver teardown. */
static int al_link_quit(void)
{
	al_link_usb_close();
	return ERROR_OK;
}

/*
 * Adapter-level reset hook expected by OpenOCD.
 *
 * It simply reuses the queue helper implementation to keep reset semantics in
 * one place.
 */
static int al_link_reset(int srst, int trst)
{
	struct reset_command reset = {
		.srst = srst,
		.trst = trst,
	};
	struct jtag_command cmd = {
		.type = JTAG_RESET,
		.cmd.reset = &reset,
	};

	return al_link_execute_reset(&cmd);
}

/*
 * Update the active speed policy.
 *
 * If the adapter is already open the new EP8 speed byte is pushed immediately;
 * otherwise it is only recorded and will be applied during init.
 */
static int al_link_speed(int speed)
{
	uint8_t speed_byte = al_link.config.speed_byte_overridden ?
		al_link.config.speed_byte : (uint8_t)speed;

	al_link.config.speed_byte = speed_byte;
	if (al_link.usb.is_open)
		return al_link_usb_set_speed(speed_byte);

	return ERROR_OK;
}

/*
 * For now we expose a direct identity mapping between OpenOCD kHz requests and
 * the driver's abstract speed value. Accurate frequency reporting will need
 * real hardware measurements.
 */
static int al_link_khz(int khz, int *jtag_speed)
{
	*jtag_speed = al_link_map_speed_byte(khz);
	return ERROR_OK;
}

/* Reverse side of the EP8 speed-byte mapping used above. */
static int al_link_speed_div(int speed, int *khz)
{
	*khz = al_link_map_khz((uint8_t)speed);
	return ERROR_OK;
}

/* Report the currently configured transport and protocol settings. */
COMMAND_HANDLER(al_link_handle_info_command)
{
	command_print(CMD,
		"AL-LINK transport=cyusb vid:pid=%04x:%04x interface=%u(unused) ep_in=0x%02x ep_out=0x%02x ep_cfg=0x%02x packet_size=%u speed_byte=0x%02x sample=%s",
		al_link.config.vid[0], al_link.config.pid[0],
		al_link.config.interface_num,
		al_link.config.ep_in, al_link.config.ep_out, al_link.config.ep_cfg,
		al_link.config.packet_size, al_link.config.speed_byte,
		al_link.config.sample_high_nibble ? "high" : "low");
	return ERROR_OK;
}

/*
 * Configure one or more VID/PID pairs.
 *
 * The driver reuses the common OpenOCD convention of a NULL-terminated array of
 * IDs, so the last successfully parsed pair is followed by a terminating zero.
 */
COMMAND_HANDLER(al_link_handle_vid_pid_command)
{
	unsigned int index;

	if (CMD_ARGC > AL_LINK_MAX_USB_IDS * 2) {
		LOG_WARNING("ignoring extra IDs in al_link_vid_pid (maximum is %u pairs)",
			AL_LINK_MAX_USB_IDS);
		CMD_ARGC = AL_LINK_MAX_USB_IDS * 2;
	}
	if (CMD_ARGC < 2 || (CMD_ARGC & 1))
		return ERROR_COMMAND_SYNTAX_ERROR;

	for (index = 0; index < (unsigned int)CMD_ARGC; index += 2) {
		COMMAND_PARSE_NUMBER(u16, CMD_ARGV[index], al_link.config.vid[index >> 1]);
		COMMAND_PARSE_NUMBER(u16, CMD_ARGV[index + 1], al_link.config.pid[index >> 1]);
	}

	al_link.config.vid[index >> 1] = 0;
	al_link.config.pid[index >> 1] = 0;
	return ERROR_OK;
}

/* Override endpoint addresses for adapters that do not use the defaults. */
COMMAND_HANDLER(al_link_handle_endpoints_command)
{
	if (CMD_ARGC != 3)
		return ERROR_COMMAND_SYNTAX_ERROR;

	COMMAND_PARSE_NUMBER(u8, CMD_ARGV[0], al_link.config.ep_in);
	COMMAND_PARSE_NUMBER(u8, CMD_ARGV[1], al_link.config.ep_out);
	COMMAND_PARSE_NUMBER(u8, CMD_ARGV[2], al_link.config.ep_cfg);
	return ERROR_OK;
}

/*
 * Record a USB interface number for compatibility with older configs.
 *
 * The current CyUSB transport does not explicitly claim interfaces, so this
 * value is informational only unless a future backend starts using it again.
 */
COMMAND_HANDLER(al_link_handle_interface_command)
{
	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	COMMAND_PARSE_NUMBER(u8, CMD_ARGV[0], al_link.config.interface_num);
	return ERROR_OK;
}

/*
 * Set the maximum chunk size used by the transport layer.
 *
 * This is transport metadata rather than protocol metadata; the queue compiler
 * itself does not care whether the underlying endpoint size is 64 bytes or 512
 * bytes.
 */
COMMAND_HANDLER(al_link_handle_packet_size_command)
{
	uint16_t packet_size;

	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	COMMAND_PARSE_NUMBER(u16, CMD_ARGV[0], packet_size);
	if (packet_size == 0)
		return ERROR_COMMAND_SYNTAX_ERROR;

	al_link.config.packet_size = packet_size;
	return ERROR_OK;
}

/* Force a specific EP8 speed byte instead of using the bucketed mapping. */
COMMAND_HANDLER(al_link_handle_speed_byte_command)
{
	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	COMMAND_PARSE_NUMBER(u8, CMD_ARGV[0], al_link.config.speed_byte);
	al_link.config.speed_byte_overridden = true;
	return ERROR_OK;
}

/* Choose whether TDO is decoded from the low or high nibble response sample. */
COMMAND_HANDLER(al_link_handle_sample_nibble_command)
{
	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	if (strcmp(CMD_ARGV[0], "high") == 0)
		al_link.config.sample_high_nibble = true;
	else if (strcmp(CMD_ARGV[0], "low") == 0)
		al_link.config.sample_high_nibble = false;
	else
		return ERROR_COMMAND_SYNTAX_ERROR;

	return ERROR_OK;
}

/*
 * Temporary bring-up diagnostic: instead of trusting the configured TDO
 * sample position, histogram every bit of every raw EP2 response byte across
 * a whole scan/readback transfer. This helps tell whether some other bit
 * (not the currently assumed bit0/bit4) is the one that actually changes.
 */
COMMAND_HANDLER(al_link_handle_dump_raw_rx_command)
{
	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	if (strcmp(CMD_ARGV[0], "on") == 0)
		al_link.config.dump_raw_rx = true;
	else if (strcmp(CMD_ARGV[0], "off") == 0)
		al_link.config.dump_raw_rx = false;
	else
		return ERROR_COMMAND_SYNTAX_ERROR;

	return ERROR_OK;
}

/* AL-LINK-specific command namespace. */
static const struct command_registration al_link_subcommand_handlers[] = {
	{
		.name = "info",
		.handler = al_link_handle_info_command,
		.mode = COMMAND_EXEC,
		.help = "show the current AL-LINK transport configuration",
		.usage = "",
	},
	COMMAND_REGISTRATION_DONE,
};

/*
 * Public command set exposed by the driver.
 *
 * The commands focus on bring-up flexibility: they allow the transport details
 * and the most important protocol assumption, the TDO sample point, to be tuned
 * from a config file without rebuilding OpenOCD.
 */
static const struct command_registration al_link_command_handlers[] = {
	{
		.name = "al_link",
		.mode = COMMAND_ANY,
		.help = "perform AL-LINK management",
		.usage = "<cmd>",
		.chain = al_link_subcommand_handlers,
	},
	{
		.name = "al_link_vid_pid",
		.handler = al_link_handle_vid_pid_command,
		.mode = COMMAND_CONFIG,
		.help = "set one or more AL-LINK USB VID/PID pairs",
		.usage = "(vid pid)+",
	},
	{
		.name = "al_link_endpoints",
		.handler = al_link_handle_endpoints_command,
		.mode = COMMAND_CONFIG,
		.help = "set AL-LINK endpoints as ep_in ep_out ep_cfg",
		.usage = "ep_in ep_out ep_cfg",
	},
	{
		.name = "al_link_interface",
		.handler = al_link_handle_interface_command,
		.mode = COMMAND_CONFIG,
		.help = "record a USB interface number for compatibility (unused by current CyUSB transport)",
		.usage = "interface_num",
	},
	{
		.name = "al_link_packet_size",
		.handler = al_link_handle_packet_size_command,
		.mode = COMMAND_CONFIG,
		.help = "set the transport packet size used for chunking",
		.usage = "packet_size",
	},
	{
		.name = "al_link_speed_byte",
		.handler = al_link_handle_speed_byte_command,
		.mode = COMMAND_CONFIG,
		.help = "override EP8 direct-mode speed byte",
		.usage = "speed_byte",
	},
	{
		.name = "al_link_sample_nibble",
		.handler = al_link_handle_sample_nibble_command,
		.mode = COMMAND_CONFIG,
		.help = "choose which returned nibble is treated as TDO sample",
		.usage = "(high | low)",
	},
	{
		.name = "al_link_dump_raw_rx",
		.handler = al_link_handle_dump_raw_rx_command,
		.mode = COMMAND_ANY,
		.help = "temporary diagnostic: histogram all 8 bits of every raw EP2 response byte",
		.usage = "(on | off)",
	},
	COMMAND_REGISTRATION_DONE,
};

/* JTAG capability declaration for the adapter core. */
static struct jtag_interface al_link_interface = {
	.supported = DEBUG_CAP_TMS_SEQ | DEBUG_CAP_TDI_SEQ,
	.execute_queue = al_link_execute_queue,
};

/* Adapter driver descriptor that will be registered once the build wiring is added. */
struct adapter_driver al_link_adapter_driver = {
	.name = "al_link",
	.transports = jtag_only,
	.commands = al_link_command_handlers,
	.init = al_link_init,
	.quit = al_link_quit,
	.reset = al_link_reset,
	.speed = al_link_speed,
	.khz = al_link_khz,
	.speed_div = al_link_speed_div,
	.jtag_ops = &al_link_interface,
};
