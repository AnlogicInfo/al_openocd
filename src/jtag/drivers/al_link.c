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

/* Mode byte verified against a successful Anlogic vendor-tool USB capture. */
#define AL_LINK_MODE_DIRECT 0x01

#define AL_LINK_SPEED_16M   0x00
#define AL_LINK_SPEED_6M    0x04
#define AL_LINK_SPEED_4M    0x08
#define AL_LINK_SPEED_2M    0x14
#define AL_LINK_SPEED_1200K 0x24
#define AL_LINK_SPEED_800K  0x38
#define AL_LINK_SPEED_500K  0x5c
#define AL_LINK_SPEED_200K  0xe8 //0x80
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
	{ 16000, AL_LINK_SPEED_16M },
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
static int al_link_usb_set_speed(uint8_t speed_byte);
static int al_link_usb_xfer(const uint8_t *tx, size_t tx_len, uint8_t idle_byte);

struct al_link_capture {
	size_t stream_offset;
	int num_bits;
	uint8_t *scan_buffer;
	struct scan_command *scan;
	bool readback;
	bool owns_buffer;
};

struct al_link_batch {
	uint8_t *tx;
	size_t tx_len;
	size_t tx_capacity;
	struct al_link_capture *captures;
	size_t capture_count;
	size_t capture_capacity;
	uint8_t idle_byte;
};

static struct al_link_batch al_link_batch;
static int al_link_batch_append(const uint8_t *tx, size_t tx_len, uint8_t idle_byte);
static int al_link_batch_add_capture(size_t stream_offset, int num_bits,
	uint8_t *scan_buffer, struct scan_command *scan, bool readback,
	bool owns_buffer);
static int al_link_batch_flush(void);
static void al_link_batch_clear(void);

static int al_link_execute_stableclocks(struct jtag_command *cmd);
static int al_link_execute_runtest(struct jtag_command *cmd);
static int al_link_execute_scan(struct jtag_command *cmd);
static int al_link_execute_tdi(struct jtag_command *cmd);
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
 * becomes one USB byte on EP6 and produces one matching sample byte on EP2.
 * The bridge has no JTAG-state awareness, so every EP2 byte must be consumed;
 * callers only decide whether the sample is retained or discarded.
 */
static inline uint8_t al_link_encode_cycle(int tms, int tdi)
{
	return al_link_pack(al_link_frame(false, tdi, tms),
		al_link_frame(true, tdi, tms));
}

/*
 * Encode a padding/no-op cycle used only to fill out packet-aligned transfers.
 *
 * Vendor-tool captures use a distinct sentinel with TCK set in both nibbles.
 * This cannot collide with a real cycle, whose low-nibble TCK is always clear.
 */
static inline uint8_t al_link_encode_idle(int tms, int tdi)
{
	uint8_t frame = al_link_frame(true, tdi, tms);

	return al_link_pack(frame, frame);
}

/*
 * Decode the effective TDO sample for one logical cycle.
 *
 * High-nibble sampling is verified on current hardware. The configurable low
 * sample remains available for compatibility with other adapter revisions.
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
 *   16000 kHz -> 0x00
 *    6000 kHz -> 0x04
 *    4000 kHz -> 0x08
 *    2000 kHz -> 0x14
 *    1200 kHz -> 0x24
 *     800 kHz -> 0x38
 *     500 kHz -> 0x5c
 *     200 kHz -> 0xe8
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

/* Allocate the encoded TX buffer for a sequence of logical cycles. */
static int al_link_alloc_cycles(size_t num_cycles, uint8_t **tx_buf)
{
	*tx_buf = NULL;

	if (num_cycles == 0)
		return ERROR_OK;

	if (num_cycles > (size_t)INT_MAX)
		return ERROR_FAIL;

	*tx_buf = malloc(num_cycles);
	if (!*tx_buf)
		return ERROR_FAIL;

	return ERROR_OK;
}

/*
 * Ship an already-encoded sequence of cycles to the adapter.
 *
 * AL-LINK returns one EP2 sample for every EP6 byte. Capture windows determine
 * which samples are retained when the accumulated batch is transferred.
 */
static int al_link_send_cycles(const uint8_t *tx_buf, size_t num_cycles,
	int idle_tms, int idle_tdi)
{
	uint8_t idle_byte = al_link_encode_idle(idle_tms, idle_tdi);

	if (num_cycles == 0)
		return ERROR_OK;

	return al_link_batch_append(tx_buf, num_cycles, idle_byte);
}

static int al_link_batch_reserve_tx(size_t extra)
{
	if (extra > SIZE_MAX - al_link_batch.tx_len)
		return ERROR_FAIL;

	size_t needed = al_link_batch.tx_len + extra;
	if (needed <= al_link_batch.tx_capacity)
		return ERROR_OK;

	size_t capacity = al_link_batch.tx_capacity ? al_link_batch.tx_capacity :
		AL_LINK_DEFAULT_PACKET_SIZE;
	while (capacity < needed) {
		if (capacity > SIZE_MAX / 2) {
			capacity = needed;
			break;
		}
		capacity *= 2;
	}

	uint8_t *new_tx = realloc(al_link_batch.tx, capacity);
	if (!new_tx)
		return ERROR_FAIL;

	al_link_batch.tx = new_tx;
	al_link_batch.tx_capacity = capacity;
	return ERROR_OK;
}

static int al_link_batch_append(const uint8_t *tx, size_t tx_len, uint8_t idle_byte)
{
	int retval = al_link_batch_reserve_tx(tx_len);
	if (retval != ERROR_OK)
		return retval;

	memcpy(al_link_batch.tx + al_link_batch.tx_len, tx, tx_len);
	al_link_batch.tx_len += tx_len;
	al_link_batch.idle_byte = idle_byte;
	return ERROR_OK;
}

static int al_link_batch_add_capture(size_t stream_offset, int num_bits,
	uint8_t *scan_buffer, struct scan_command *scan, bool readback,
	bool owns_buffer)
{
	if (al_link_batch.capture_count == al_link_batch.capture_capacity) {
		size_t capacity = al_link_batch.capture_capacity ?
			al_link_batch.capture_capacity * 2 : 8;
		struct al_link_capture *captures = realloc(al_link_batch.captures,
			capacity * sizeof(*captures));
		if (!captures)
			return ERROR_FAIL;
		al_link_batch.captures = captures;
		al_link_batch.capture_capacity = capacity;
	}

	struct al_link_capture *capture =
		&al_link_batch.captures[al_link_batch.capture_count++];
	capture->stream_offset = stream_offset;
	capture->num_bits = num_bits;
	capture->scan_buffer = scan_buffer;
	capture->scan = scan;
	capture->readback = readback;
	capture->owns_buffer = owns_buffer;
	return ERROR_OK;
}

static void al_link_batch_clear(void)
{
	for (size_t i = 0; i < al_link_batch.capture_count; i++) {
		if (al_link_batch.captures[i].owns_buffer)
			free(al_link_batch.captures[i].scan_buffer);
	}

	free(al_link_batch.tx);
	free(al_link_batch.captures);
	memset(&al_link_batch, 0, sizeof(al_link_batch));
}

static int al_link_batch_flush(void)
{
	if (al_link_batch.tx_len == 0)
		return ERROR_OK;

	int retval = al_link_usb_xfer(al_link_batch.tx, al_link_batch.tx_len,
		al_link_batch.idle_byte);
	if (retval == ERROR_OK) {
		for (size_t i = 0; i < al_link_batch.capture_count; i++) {
			struct al_link_capture *capture = &al_link_batch.captures[i];

			if (!capture->scan)
				continue;

			retval = jtag_read_buffer(capture->scan_buffer, capture->scan);
			if (retval != ERROR_OK)
				break;
		}
	}

	al_link_batch_clear();
	return retval;
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
	int retval;

	retval = al_link_alloc_cycles(AL_LINK_TLR_RESET_CYCLES, &tx_buf);
	if (retval != ERROR_OK)
		return retval;

	for (size_t index = 0; index < AL_LINK_TLR_RESET_CYCLES; index++)
		tx_buf[index] = al_link_encode_cycle(1, 0);

	retval = al_link_send_cycles(tx_buf, AL_LINK_TLR_RESET_CYCLES, 1, 0);
	free(tx_buf);
	if (retval != ERROR_OK)
		return retval;

	tap_set_state(TAP_RESET);
	return ERROR_OK;
}

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

	al_link.usb.is_open = true;
	al_link.usb.io_failed = false;
	return ERROR_OK;
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
 * Every logical operation is padded to a USB2 high-speed packet boundary. Each
 * 512-byte EP6 write is immediately followed by one 512-byte EP2 read. Only the
 * samples inside registered SHIFT-IR/DR capture windows are decoded directly;
 * write-only, SCAN_OUT, and padding samples are consumed and discarded.
 */
static int al_link_usb_xfer(const uint8_t *tx, size_t tx_len, uint8_t idle_byte)
{
	size_t offset = 0;
	size_t ps = al_link.config.packet_size;
	uint8_t *tx_packet;
	uint8_t *rx_packet;
	size_t packet_count = 0;

	if (!al_link.usb.is_open)
		return ERROR_FAIL;

	if (al_link.usb.io_failed) {
		LOG_ERROR("AL-LINK: previous USB transfer failed; re-run init to reopen the adapter");
		return ERROR_FAIL;
	}

	if (tx_len > (size_t)INT_MAX) {
		LOG_ERROR("AL-LINK: transfer too large for CyUSB API (tx=%u)",
			(unsigned int)tx_len);
		return ERROR_FAIL;
	}

	if (tx_len == 0)
		return ERROR_OK;
	if (ps == 0 || ps > (size_t)INT_MAX) {
		LOG_ERROR("AL-LINK: invalid USB packet size %u", (unsigned int)ps);
		return ERROR_FAIL;
	}

	tx_packet = malloc(ps);
	rx_packet = malloc(ps);
	if (!tx_packet || !rx_packet) {
		free(tx_packet);
		free(rx_packet);
		LOG_ERROR("AL-LINK: out of memory allocating aligned USB packets");
		return ERROR_FAIL;
	}

	while (offset < tx_len) {
		size_t real_chunk = tx_len - offset;
		int transferred = 0;
		int retval;

		if (real_chunk > ps)
			real_chunk = ps;
		memset(tx_packet, idle_byte, ps);
		memcpy(tx_packet, tx + offset, real_chunk);

		retval = al_cyusb_bulk_write(&al_link.usb.cyusb, al_link.config.ep_out,
			tx_packet, (int)ps, &transferred, AL_LINK_USB_TIMEOUT_MS);
		if (retval != 0) {
			free(tx_packet);
			free(rx_packet);
			return al_link_usb_mark_failed("EP6 write");
		}
		if (transferred != (int)ps) {
			LOG_ERROR("AL-LINK: short EP6 write, expected %u bytes, wrote %d",
				(unsigned int)ps, transferred);
			free(tx_packet);
			free(rx_packet);
			return al_link_usb_mark_failed("EP6 write");
		}

		transferred = 0;
		retval = al_cyusb_bulk_read(&al_link.usb.cyusb, al_link.config.ep_in,
			rx_packet, (int)ps, &transferred, AL_LINK_USB_TIMEOUT_MS);
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

		for (size_t capture_index = 0;
			capture_index < al_link_batch.capture_count; capture_index++) {
			struct al_link_capture *capture =
				&al_link_batch.captures[capture_index];
			size_t capture_start = capture->stream_offset;
			size_t capture_end = capture_start + (size_t)capture->num_bits;
			size_t frame_end = offset + real_chunk;
			size_t overlap_start = capture_start > offset ? capture_start : offset;
			size_t overlap_end = capture_end < frame_end ? capture_end : frame_end;

			if (!capture->readback || overlap_start >= overlap_end)
				continue;

			for (size_t stream_index = overlap_start;
				stream_index < overlap_end; stream_index++) {
				size_t source_index = stream_index - offset;
				size_t bit_index = stream_index - capture_start;
				uint8_t sample = rx_packet[source_index];

				al_link_set_bit(capture->scan_buffer, (unsigned int)bit_index,
					al_link_decode_tdo(sample));
			}
		}

		offset += real_chunk;
		packet_count++;
	}

	LOG_DEBUG("AL-LINK: transferred %u logical byte(s) in %u packet(s)",
		(unsigned int)tx_len, (unsigned int)packet_count);

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
	int retval;

	if ((size_t)tms_count < skip)
		return ERROR_FAIL;

	retval = al_link_alloc_cycles((size_t)tms_count - skip, &tx_buf);
	if (retval != ERROR_OK)
		return retval;

	for (int index = (int)skip; index < tms_count; index++) {
		last_tms = (tms_scan >> index) & 0x1;
		tx_buf[index - (int)skip] = al_link_encode_cycle(last_tms, 0);
	}

	retval = al_link_send_cycles(tx_buf, (size_t)tms_count - skip, last_tms, 0);
	free(tx_buf);
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
	int retval;

	retval = al_link_alloc_cycles(num_bits, &tx_buf);
	if (retval != ERROR_OK)
		return retval;

	for (unsigned int bit = 0; bit < num_bits; bit++) {
		last_tms = al_link_get_bit(bits, bit);
		tx_buf[bit] = al_link_encode_cycle(last_tms, 0);
	}

	retval = al_link_send_cycles(tx_buf, num_bits, last_tms, 0);
	free(tx_buf);
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

	retval = al_link_alloc_cycles((size_t)num_cycles, &tx_buf);
	if (retval != ERROR_OK)
		return retval;

	for (int index = 0; index < num_cycles; index++)
		tx_buf[index] = al_link_encode_cycle(tms, 0);

	retval = al_link_send_cycles(tx_buf, (size_t)num_cycles, tms, 0);
	free(tx_buf);
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
	int retval;

	retval = al_link_alloc_cycles((size_t)num_states, &tx_buf);
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
			return ERROR_JTAG_QUEUE_FAILED;
		}

		last_tms = tms;
		tx_buf[index] = al_link_encode_cycle(tms, 0);
		current_state = path[index];
	}

	retval = al_link_send_cycles(tx_buf, (size_t)num_states, last_tms, 0);
	free(tx_buf);
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
 * 4. Record the scan's byte-stream window for deferred EP2 extraction.
 * 5. If the scan exits shift state, append the remaining TAP path.
 * 6. Decode TDO and call jtag_read_buffer() when the queue batch is flushed.
 */
static int al_link_execute_scan(struct jtag_command *cmd)
{
	enum scan_type type = jtag_scan_type(cmd->cmd.scan);
	tap_state_t shift_state = cmd->cmd.scan->ir_scan ? TAP_IRSHIFT : TAP_DRSHIFT;
	tap_state_t saved_end_state = cmd->cmd.scan->end_state;
	uint8_t *buffer = NULL;
	uint8_t *tx_buf = NULL;
	bool readback = type != SCAN_OUT;
	int last_tms = 0;
	int last_tdi = 0;
	int scan_size;
	size_t stream_offset;
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
	retval = al_link_alloc_cycles((size_t)scan_size, &tx_buf);
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

	stream_offset = al_link_batch.tx_len;
	retval = al_link_send_cycles(tx_buf, (size_t)scan_size, last_tms, last_tdi);
	if (retval != ERROR_OK)
		goto out;

	retval = al_link_batch_add_capture(stream_offset, scan_size, buffer,
		cmd->cmd.scan, readback, true);
	if (retval != ERROR_OK)
		goto out;
	buffer = NULL;

	if (saved_end_state != shift_state) {
		retval = al_link_execute_statemove(1);
		if (retval != ERROR_OK)
			goto out;
	}

out:
	free(buffer);
	free(tx_buf);
	return retval;
}

/*
 * Execute a raw TDI sequence without adding TAP state transitions.
 *
 * The caller owns the TAP path. TMS stays low for the sequence except when
 * the requested end state requires the final clock to leave SHIFT or PAUSE.
 * TDO, when requested, is decoded directly into the caller's input buffer.
 */
static int al_link_execute_tdi(struct jtag_command *cmd)
{
	struct scan_field *field = cmd->cmd.scan->fields;
	tap_state_t end_state = cmd->cmd.scan->end_state;
	uint8_t *tx_buf = NULL;
	bool exit_on_last;
	int last_tms = 0;
	int last_tdi = 0;
	int retval;

	if (field->num_bits == 0)
		return ERROR_OK;
	if (field->num_bits < 0 || !field->out_value) {
		LOG_ERROR("AL-LINK: invalid raw TDI sequence");
		return ERROR_JTAG_QUEUE_FAILED;
	}

	exit_on_last = end_state == TAP_DREXIT1 || end_state == TAP_IREXIT1 ||
		end_state == TAP_DREXIT2 || end_state == TAP_IREXIT2;

	retval = al_link_alloc_cycles((size_t)field->num_bits, &tx_buf);
	if (retval != ERROR_OK)
		return retval;

	for (int bit = 0; bit < field->num_bits; bit++) {
		last_tms = exit_on_last && bit == field->num_bits - 1;
		last_tdi = al_link_get_bit(field->out_value, (unsigned int)bit);
		tx_buf[bit] = al_link_encode_cycle(last_tms, last_tdi);
	}

	size_t stream_offset = al_link_batch.tx_len;
	retval = al_link_send_cycles(tx_buf, (size_t)field->num_bits,
		last_tms, last_tdi);
	if (retval == ERROR_OK && field->in_value)
		retval = al_link_batch_add_capture(stream_offset, field->num_bits,
			field->in_value, NULL, true, false);

	free(tx_buf);
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
 * Each queue item is translated by a specialized helper, and the first failure
 * aborts and clears the accumulated batch.
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
		case JTAG_TDI:
			retval = al_link_execute_tdi(cmd);
			break;
		case JTAG_SLEEP:
			retval = al_link_batch_flush();
			if (retval == ERROR_OK)
				jtag_sleep(cmd->cmd.sleep->us);
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

		if (retval != ERROR_OK) {
			al_link_batch_clear();
			return retval;
		}

		cmd = cmd->next;
	}

	return al_link_batch_flush();
}

/*
 * Driver initialization.
 *
 * Initialization performs the adapter transport setup:
 * 1. open the USB device,
 * 2. reset/prepare the endpoint pipes,
 * 3. configure mode 0x01 and the selected speed through EP8.
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
	al_link_batch_clear();
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

	int retval = al_link_execute_reset(&cmd);
	if (retval != ERROR_OK) {
		al_link_batch_clear();
		return retval;
	}

	return al_link_batch_flush();
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
 * Map an OpenOCD kHz request to the closest supported adapter speed byte.
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
 * Real AL-Link transfers are fixed to one USB2 high-speed packet per EP6/EP2
 * operation. Keep the command for configuration compatibility, but reject any
 * value other than 512.
 */
COMMAND_HANDLER(al_link_handle_packet_size_command)
{
	uint16_t packet_size;

	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	COMMAND_PARSE_NUMBER(u16, CMD_ARGV[0], packet_size);
	if (packet_size != AL_LINK_DEFAULT_PACKET_SIZE) {
		LOG_ERROR("AL-LINK: packet size is fixed at %u bytes",
			AL_LINK_DEFAULT_PACKET_SIZE);
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}

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

/* Choose the TDO sample position for compatibility with adapter revisions. */
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
 * The commands expose transport settings and compatibility overrides that may
 * vary between adapter revisions.
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
	COMMAND_REGISTRATION_DONE,
};

/* JTAG capability declaration for the adapter core. */
static struct jtag_interface al_link_interface = {
	.supported = DEBUG_CAP_TMS_SEQ | DEBUG_CAP_TDI_SEQ,
	.execute_queue = al_link_execute_queue,
};

/* Adapter driver descriptor registered by the OpenOCD adapter core. */
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
