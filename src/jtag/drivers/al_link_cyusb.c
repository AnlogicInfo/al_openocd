/***************************************************************************
 *   AL-LINK CyUSB transport layer — pure C implementation                 *
 *                                                                         *
 *   Talks to CyUSB.sys / CyUSB3.sys via Win32 SetupDi + DeviceIoControl.  *
 *                                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _WIN32   /* CyUSB transport is Windows-only */

#include "al_link_cyusb.h"

#include <helper/log.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

/*
 * For MSVC builds, pull in setupapi.lib for the SetupDi* APIs.
 * MinGW/autotools builds should add the library in the build system.
 */
#ifdef _MSC_VER
#pragma comment(lib, "setupapi.lib")
#endif

#define AL_CYUSB_PIPE_TIMEOUT_MS 1000u

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                  */
/* ------------------------------------------------------------------ */
static bool cyusb_transfer_status_ok(const AL_SINGLE_TRANSFER *hdr)
{
	return hdr->NtStatus == 0 && hdr->UsbdStatus == 0;
}

static void cyusb_log_transfer_status_failure(uint8_t ep_addr, bool is_read,
		const AL_SINGLE_TRANSFER *hdr, DWORD bytes_returned, int length)
{
	LOG_ERROR("AL-LINK CyUSB: bulk %s on EP 0x%02x failed in driver status (NtStatus=0x%08lx, UsbdStatus=0x%08lx, bytes_returned=%lu, requested=%d, buffer_length=%lu)",
		is_read ? "read" : "write", ep_addr,
		(unsigned long)hdr->NtStatus, (unsigned long)hdr->UsbdStatus,
		(unsigned long)bytes_returned, length,
		(unsigned long)hdr->BufferLength);
}

typedef BOOL (WINAPI *cyusb_cancel_io_ex_fn_t)(HANDLE, LPOVERLAPPED);

static bool cyusb_cancel_io(HANDLE dev, OVERLAPPED *ov)
{
	static cyusb_cancel_io_ex_fn_t cancel_io_ex_fn;
	static bool looked_up;

	if (!looked_up) {
		HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
		if (kernel32) {
			union {
				FARPROC raw;
				cyusb_cancel_io_ex_fn_t typed;
			} cancel_io_ex = {0};

			cancel_io_ex.raw = GetProcAddress(kernel32, "CancelIoEx");
			cancel_io_ex_fn = cancel_io_ex.typed;
		}
		looked_up = true;
	}

	if (cancel_io_ex_fn)
		return cancel_io_ex_fn(dev, ov) != 0;

	return CancelIo(dev) != 0;
}

static int cyusb_device_io_control(al_cyusb_handle_t *h, DWORD ioctl_code,
		void *in_buf, DWORD in_size, void *out_buf, DWORD out_size,
		DWORD *bytes_returned, unsigned int timeout_ms,
		const char *operation, uint8_t ep_addr)
{
	OVERLAPPED ov;
	HANDLE event_handle;
	DWORD local_bytes = 0;
	DWORD wait_ms = timeout_ms ? (DWORD)timeout_ms : INFINITE;
	BOOL ok;
	DWORD error;

	if (!h || !h->is_open)
		return -1;

	memset(&ov, 0, sizeof(ov));
	event_handle = CreateEventA(NULL, TRUE, FALSE, NULL);
	if (!event_handle) {
		LOG_ERROR("AL-LINK CyUSB: CreateEvent failed for %s on EP 0x%02x (error %lu)",
			operation, ep_addr, GetLastError());
		if (bytes_returned)
			*bytes_returned = 0;
		return -1;
	}
	ov.hEvent = event_handle;

	ok = DeviceIoControl(h->dev, ioctl_code,
		in_buf, in_size,
		out_buf, out_size,
		&local_bytes, &ov);
	if (!ok) {
		error = GetLastError();
		if (error != ERROR_IO_PENDING) {
			LOG_ERROR("AL-LINK CyUSB: %s on EP 0x%02x failed (error %lu)",
				operation, ep_addr, error);
			CloseHandle(event_handle);
			if (bytes_returned)
				*bytes_returned = 0;
			return -1;
		}

		switch (WaitForSingleObject(event_handle, wait_ms)) {
		case WAIT_OBJECT_0:
			ok = GetOverlappedResult(h->dev, &ov, &local_bytes, FALSE);
			if (!ok) {
				LOG_ERROR("AL-LINK CyUSB: %s on EP 0x%02x completion failed (error %lu)",
					operation, ep_addr, GetLastError());
				CloseHandle(event_handle);
				if (bytes_returned)
					*bytes_returned = 0;
				return -1;
			}
			break;

		case WAIT_TIMEOUT:
			LOG_ERROR("AL-LINK CyUSB: %s on EP 0x%02x timed out after %u ms",
				operation, ep_addr, timeout_ms);
			if (!cyusb_cancel_io(h->dev, &ov)) {
				DWORD cancel_error = GetLastError();
				if (cancel_error != ERROR_NOT_FOUND) {
					LOG_WARNING("AL-LINK CyUSB: cancel failed for %s on EP 0x%02x (error %lu)",
						operation, ep_addr, cancel_error);
				}
			}

			ok = GetOverlappedResult(h->dev, &ov, &local_bytes, TRUE);
			if (!ok) {
				DWORD completion_error = GetLastError();
				if (completion_error != ERROR_OPERATION_ABORTED) {
					LOG_WARNING("AL-LINK CyUSB: timed-out %s on EP 0x%02x completed with error %lu",
						operation, ep_addr, completion_error);
				}
			}

			CloseHandle(event_handle);
			if (bytes_returned)
				*bytes_returned = 0;
			return -1;

		default:
			LOG_ERROR("AL-LINK CyUSB: wait failed for %s on EP 0x%02x (error %lu)",
				operation, ep_addr, GetLastError());
			CloseHandle(event_handle);
			if (bytes_returned)
				*bytes_returned = 0;
			return -1;
		}
	}

	CloseHandle(event_handle);
	if (bytes_returned)
		*bytes_returned = local_bytes;
	return 0;
}

/*
 * Parse VID and PID from a Windows device-interface path.
 *
 * Typical path fragment: "usb#vid_0547&pid_1002#..."
 * The function is case-insensitive.  Returns true on successful parse.
 */
static bool parse_vid_pid_from_path(const char *path,
				    uint16_t *vid_out, uint16_t *pid_out)
{
	const char *p;
	unsigned int vid, pid;

	/* Find "vid_XXXX" (case-insensitive) */
	for (p = path; *p; p++) {
		if ((p[0] == 'v' || p[0] == 'V') &&
		    (p[1] == 'i' || p[1] == 'I') &&
		    (p[2] == 'd' || p[2] == 'D') &&
		    p[3] == '_') {
			if (sscanf(p + 4, "%4x", &vid) != 1)
				continue;

			/* Now find "pid_XXXX" after the VID */
			const char *q;
			for (q = p + 8; *q; q++) {
				if ((q[0] == 'p' || q[0] == 'P') &&
				    (q[1] == 'i' || q[1] == 'I') &&
				    (q[2] == 'd' || q[2] == 'D') &&
				    q[3] == '_') {
					if (sscanf(q + 4, "%4x", &pid) == 1) {
						*vid_out = (uint16_t)vid;
						*pid_out = (uint16_t)pid;
						return true;
					}
				}
			}
		}
	}
	return false;
}

/*
 * Check whether a parsed VID/PID matches any entry in the given arrays.
 *
 * Arrays are NULL-terminated (trailing zero entry).
 */
static bool match_vid_pid(uint16_t vid, uint16_t pid,
			  const uint16_t vids[], const uint16_t pids[])
{
	for (unsigned int i = 0; vids[i]; i++) {
		if (vid == vids[i] && pid == pids[i])
			return true;
	}
	return false;
}

/* ------------------------------------------------------------------ */
/*  Device enumeration & open                                         */
/* ------------------------------------------------------------------ */

int al_cyusb_open(const uint16_t vids[], const uint16_t pids[],
		  al_cyusb_handle_t *h)
{
	HDEVINFO dev_info;
	SP_DEVICE_INTERFACE_DATA iface_data;
	PSP_DEVICE_INTERFACE_DETAIL_DATA_A detail = NULL;
	DWORD required_size = 0;
	DWORD idx;
	int result = -1;

	memset(h, 0, sizeof(*h));
	h->dev = INVALID_HANDLE_VALUE;

	dev_info = SetupDiGetClassDevsA(
		(const GUID *)&CYUSB_DRIVER_GUID,
		NULL, NULL,
		DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if (dev_info == INVALID_HANDLE_VALUE) {
		LOG_ERROR("AL-LINK CyUSB: SetupDiGetClassDevs failed (error %lu)",
			  GetLastError());
		return -1;
	}

	iface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	for (idx = 0; ; idx++) {
		if (!SetupDiEnumDeviceInterfaces(dev_info, NULL,
				(const GUID *)&CYUSB_DRIVER_GUID,
				idx, &iface_data))
			break;   /* no more interfaces */

		/* First call to get required buffer size */
		SetupDiGetDeviceInterfaceDetailA(dev_info, &iface_data,
						 NULL, 0, &required_size, NULL);

		detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)malloc(required_size);
		if (!detail) {
			LOG_ERROR("AL-LINK CyUSB: out of memory");
			break;
		}
		detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

		if (!SetupDiGetDeviceInterfaceDetailA(dev_info, &iface_data,
						      detail, required_size,
						      NULL, NULL)) {
			free(detail);
			detail = NULL;
			continue;
		}

		/* Match VID/PID from the device path string */
		uint16_t dev_vid = 0, dev_pid = 0;
		if (!parse_vid_pid_from_path(detail->DevicePath,
					     &dev_vid, &dev_pid)) {
			free(detail);
			detail = NULL;
			continue;
		}

		if (!match_vid_pid(dev_vid, dev_pid, vids, pids)) {
			free(detail);
			detail = NULL;
			continue;
		}

		/* Found a matching device — open it. */
		HANDLE hdev = CreateFileA(
			detail->DevicePath,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
			NULL);

		if (hdev == INVALID_HANDLE_VALUE) {
			LOG_ERROR("AL-LINK CyUSB: CreateFile failed for '%s' (error %lu)",
				  detail->DevicePath, GetLastError());
			free(detail);
			detail = NULL;
			continue;
		}

		/* Success */
		h->dev = hdev;
		h->is_open = true;
		strncpy(h->dev_path, detail->DevicePath, sizeof(h->dev_path) - 1);
		h->dev_path[sizeof(h->dev_path) - 1] = '\0';

		LOG_INFO("AL-LINK CyUSB: opened device VID:PID %04x:%04x path=%s",
			 dev_vid, dev_pid, h->dev_path);

		free(detail);
		detail = NULL;
		result = 0;
		break;
	}

	SetupDiDestroyDeviceInfoList(dev_info);

	if (result != 0)
		LOG_ERROR("AL-LINK CyUSB: no matching device found");

	return result;
}

void al_cyusb_close(al_cyusb_handle_t *h)
{
	if (!h || !h->is_open)
		return;

	CloseHandle(h->dev);
	h->dev = INVALID_HANDLE_VALUE;
	h->is_open = false;
}

/* ------------------------------------------------------------------ */
/*  Pipe management                                                   */
/* ------------------------------------------------------------------ */

int al_cyusb_reset_pipe(al_cyusb_handle_t *h, uint8_t ep_addr)
{
	DWORD bytes_returned = 0;

	if (!h || !h->is_open)
		return -1;

	if (cyusb_device_io_control(h, AL_CYUSB_IOCTL_RESET_PIPE,
			&ep_addr, sizeof(ep_addr), NULL, 0,
			&bytes_returned, AL_CYUSB_PIPE_TIMEOUT_MS,
			"reset pipe", ep_addr) != 0) {
		LOG_WARNING("AL-LINK CyUSB: reset pipe 0x%02x failed", ep_addr);
		return -1;
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/*  Bulk transfers                                                    */
/* ------------------------------------------------------------------ */

/*
 * Common bulk-transfer helper.
 *
 * CyUSB expects a buffer laid out as:
 *   [ AL_SINGLE_TRANSFER header ][ payload data ]
 *
 * For OUT transfers the payload is copied in before the IOCTL.
 * For IN  transfers the payload is copied out after the IOCTL.
 */
static int cyusb_bulk_xfer(al_cyusb_handle_t *h, uint8_t ep_addr,
			   uint8_t *data, int length,
			   int *transferred, bool is_read,
			   unsigned int timeout_ms)
{
	DWORD xfer_buf_size;
	uint8_t *xfer_buf = NULL;
	AL_SINGLE_TRANSFER *hdr;
	DWORD bytes_returned = 0;
	const char *operation = is_read ? "bulk read" : "bulk write";

	if (!h || !h->is_open)
		return -1;

	xfer_buf_size = (DWORD)(sizeof(AL_SINGLE_TRANSFER) + length);
	xfer_buf = (uint8_t *)calloc(1, xfer_buf_size);
	if (!xfer_buf)
		return -1;

	hdr = (AL_SINGLE_TRANSFER *)xfer_buf;
	hdr->ucEndpointAddress = ep_addr;
	hdr->IsoPacketOffset = 0;
	hdr->IsoPacketLength = 0;
	hdr->BufferOffset = sizeof(AL_SINGLE_TRANSFER);
	hdr->BufferLength = (uint32_t)length;

	/* For OUT transfers, copy data into the buffer after the header */
	if (!is_read && length > 0)
		memcpy(xfer_buf + sizeof(AL_SINGLE_TRANSFER), data, length);

	if (cyusb_device_io_control(h, AL_CYUSB_IOCTL_NON_EP0_TRANSFER,
			xfer_buf, xfer_buf_size,
			xfer_buf, xfer_buf_size,
			&bytes_returned, timeout_ms,
			operation, ep_addr) != 0) {
		free(xfer_buf);
		if (transferred)
			*transferred = 0;
		return -1;
	}

	if (!cyusb_transfer_status_ok(hdr)) {
		cyusb_log_transfer_status_failure(ep_addr, is_read, hdr,
			bytes_returned, length);
		free(xfer_buf);
		if (transferred)
			*transferred = 0;
		return -1;
	}

	if (is_read) {
		/* Returned bytes include the header; payload starts after it */
		DWORD payload_bytes = 0;
		if (bytes_returned > sizeof(AL_SINGLE_TRANSFER))
			payload_bytes = bytes_returned - (DWORD)sizeof(AL_SINGLE_TRANSFER);
		if (payload_bytes > (DWORD)length)
			payload_bytes = (DWORD)length;
		if (payload_bytes > 0)
			memcpy(data, xfer_buf + sizeof(AL_SINGLE_TRANSFER), payload_bytes);
		if (transferred)
			*transferred = (int)payload_bytes;
	} else {
		/* For OUT, bytes_returned may or may not include the header */
		if (transferred) {
			if (bytes_returned > sizeof(AL_SINGLE_TRANSFER))
				*transferred = (int)(bytes_returned - sizeof(AL_SINGLE_TRANSFER));
			else
				*transferred = length; /* assume success */
		}
	}

	free(xfer_buf);
	return 0;
}

int al_cyusb_bulk_write(al_cyusb_handle_t *h, uint8_t ep_addr,
			const uint8_t *data, int length,
			int *transferred, unsigned int timeout_ms)
{
	return cyusb_bulk_xfer(h, ep_addr,
			       (uint8_t *)data, length, transferred, false,
			       timeout_ms);
}

int al_cyusb_bulk_read(al_cyusb_handle_t *h, uint8_t ep_addr,
		       uint8_t *data, int length,
		       int *transferred, unsigned int timeout_ms)
{
	return cyusb_bulk_xfer(h, ep_addr,
			       data, length, transferred, true,
			       timeout_ms);
}

#endif /* _WIN32 */
