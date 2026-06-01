/***************************************************************************
 *   AL-LINK CyUSB transport layer — pure C implementation                *
 *                                                                         *
 *   Talks to CyUSB.sys / CyUSB3.sys via Win32 SetupDi + DeviceIoControl. *
 *   No C++ code, no CyAPI.lib dependency.                                 *
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

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

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

static enum al_cyusb_ioctl_layout al_cyusb_alternate_layout(
		enum al_cyusb_ioctl_layout layout)
{
	return (layout == AL_CYUSB_IOCTL_LAYOUT_FX2) ?
		AL_CYUSB_IOCTL_LAYOUT_FX3 : AL_CYUSB_IOCTL_LAYOUT_FX2;
}

static DWORD al_cyusb_ioctl_code(enum al_cyusb_ioctl_layout layout,
		unsigned int fx2_base, unsigned int fx2_offset,
		unsigned int fx3_base, unsigned int fx3_offset,
		DWORD method)
{
	if (layout == AL_CYUSB_IOCTL_LAYOUT_FX2)
		return CTL_CODE(FILE_DEVICE_UNKNOWN, fx2_base + fx2_offset,
			method, FILE_ANY_ACCESS);

	return CTL_CODE(FILE_DEVICE_UNKNOWN, fx3_base + fx3_offset,
		method, FILE_ANY_ACCESS);
}

static DWORD al_cyusb_non_ep0_ioctl(enum al_cyusb_ioctl_layout layout)
{
	return al_cyusb_ioctl_code(layout,
		AL_CYUSB_IOCTL_BASE_FX2, AL_CYUSB_IOCTL_OFF_NON_EP0_FX2,
		AL_CYUSB_IOCTL_BASE_FX3, AL_CYUSB_IOCTL_OFF_NON_EP0_FX3,
		METHOD_IN_DIRECT);
}

static DWORD al_cyusb_reset_pipe_ioctl(enum al_cyusb_ioctl_layout layout)
{
	return al_cyusb_ioctl_code(layout,
		AL_CYUSB_IOCTL_BASE_FX2, AL_CYUSB_IOCTL_OFF_RESET_FX2,
		AL_CYUSB_IOCTL_BASE_FX3, AL_CYUSB_IOCTL_OFF_RESET_FX3,
		METHOD_BUFFERED);
}

static const char *al_cyusb_layout_name(enum al_cyusb_ioctl_layout layout)
{
	return (layout == AL_CYUSB_IOCTL_LAYOUT_FX2) ? "CyUSB" : "CyUSB3";
}

static int al_cyusb_device_ioctl(al_cyusb_handle_t *h,
		DWORD (*ioctl_selector)(enum al_cyusb_ioctl_layout layout),
		void *in_buf, DWORD in_size,
		void *out_buf, DWORD out_size,
		DWORD *bytes_returned, DWORD *last_error)
{
	enum al_cyusb_ioctl_layout preferred = h->ioctl_layout;
	enum al_cyusb_ioctl_layout alternate;

	if (preferred == AL_CYUSB_IOCTL_LAYOUT_UNKNOWN)
		preferred = AL_CYUSB_IOCTL_LAYOUT_FX3;
	alternate = al_cyusb_alternate_layout(preferred);

	for (unsigned int attempt = 0; attempt < 2; attempt++) {
		enum al_cyusb_ioctl_layout layout = (attempt == 0) ? preferred : alternate;
		DWORD local_bytes_returned = 0;
		BOOL ok;
		DWORD error;

		ok = DeviceIoControl(
			h->dev,
			ioctl_selector(layout),
			in_buf, in_size,
			out_buf, out_size,
			&local_bytes_returned, NULL);

		if (ok) {
			if (bytes_returned)
				*bytes_returned = local_bytes_returned;
			if (h->ioctl_layout != layout) {
				h->ioctl_layout = layout;
				LOG_INFO("AL-LINK CyUSB: using %s IOCTL layout",
					 al_cyusb_layout_name(layout));
			}
			return 0;
		}

		error = GetLastError();
		if (last_error)
			*last_error = error;
		if (error != ERROR_INVALID_PARAMETER)
			break;
	}

	if (bytes_returned)
		*bytes_returned = 0;
	return -1;
}

/* ------------------------------------------------------------------ */
/*  Device enumeration & open                                          */
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
	h->ioctl_layout = AL_CYUSB_IOCTL_LAYOUT_UNKNOWN;

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

		/* Found a matching device — open it.
		 * Use synchronous I/O: CyUSB.sys returns ERROR_INVALID_PARAMETER (87)
		 * if the handle is opened with FILE_FLAG_OVERLAPPED but DeviceIoControl
		 * is called with a NULL OVERLAPPED pointer. */
		HANDLE hdev = CreateFileA(
			detail->DevicePath,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			0,
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
/*  Pipe management                                                    */
/* ------------------------------------------------------------------ */

int al_cyusb_reset_pipe(al_cyusb_handle_t *h, uint8_t ep_addr)
{
	AL_SINGLE_TRANSFER xfer;
	DWORD bytes_returned = 0;
	DWORD last_error = ERROR_SUCCESS;

	if (!h || !h->is_open)
		return -1;

	memset(&xfer, 0, sizeof(xfer));
	xfer.ucEndpointAddress = ep_addr;

	if (al_cyusb_device_ioctl(h, al_cyusb_reset_pipe_ioctl,
			&xfer, sizeof(xfer), &xfer, sizeof(xfer),
			&bytes_returned, &last_error) != 0) {
		LOG_WARNING("AL-LINK CyUSB: reset pipe 0x%02x failed (error %lu)",
			    ep_addr, last_error);
		return -1;
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/*  Bulk transfers                                                     */
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
			   int *transferred, bool is_read)
{
	DWORD xfer_buf_size;
	uint8_t *xfer_buf = NULL;
	AL_SINGLE_TRANSFER *hdr;
	DWORD bytes_returned = 0;
	DWORD last_error = ERROR_SUCCESS;

	if (!h || !h->is_open)
		return -1;

	xfer_buf_size = (DWORD)(sizeof(AL_SINGLE_TRANSFER) + length);
	xfer_buf = (uint8_t *)calloc(1, xfer_buf_size);
	if (!xfer_buf)
		return -1;

	hdr = (AL_SINGLE_TRANSFER *)xfer_buf;
	hdr->ucEndpointAddress = ep_addr;
	hdr->BufferOffset = sizeof(AL_SINGLE_TRANSFER);
	hdr->BufferLength = (uint32_t)length;

	/* For OUT transfers, copy data into the buffer after the header */
	if (!is_read && length > 0)
		memcpy(xfer_buf + sizeof(AL_SINGLE_TRANSFER), data, length);

	if (al_cyusb_device_ioctl(h, al_cyusb_non_ep0_ioctl,
			xfer_buf, xfer_buf_size, xfer_buf, xfer_buf_size,
			&bytes_returned, &last_error) != 0) {
		LOG_ERROR("AL-LINK CyUSB: bulk %s on EP 0x%02x failed (error %lu)",
			  is_read ? "read" : "write", ep_addr, last_error);
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
	(void)timeout_ms; /* CyUSB DeviceIoControl timeout is driver-level */
	return cyusb_bulk_xfer(h, ep_addr,
			       (uint8_t *)data, length, transferred, false);
}

int al_cyusb_bulk_read(al_cyusb_handle_t *h, uint8_t ep_addr,
		       uint8_t *data, int length,
		       int *transferred, unsigned int timeout_ms)
{
	(void)timeout_ms;
	return cyusb_bulk_xfer(h, ep_addr,
			       data, length, transferred, true);
}

#endif /* _WIN32 */
