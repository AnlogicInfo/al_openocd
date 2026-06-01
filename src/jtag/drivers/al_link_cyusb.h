/***************************************************************************
 *   AL-LINK CyUSB transport layer — pure C wrapper over CyUSB.sys IOCTLs *
 *                                                                         *
 *   This file provides a libusb-free path to the AL-LINK adapter on       *
 *   Windows, talking directly to the Cypress CyUSB kernel driver via      *
 *   standard Win32 DeviceIoControl calls.  No CyAPI.lib (C++) needed.     *
 *                                                                         *
 *   The IOCTL codes and SINGLE_TRANSFER layout are taken from the         *
 *   publicly distributed Cypress/Infineon EZ-USB SDK headers              *
 *   (cyioctl.h / CyUSB30_def.h).                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef OPENOCD_JTAG_DRIVERS_AL_LINK_CYUSB_H
#define OPENOCD_JTAG_DRIVERS_AL_LINK_CYUSB_H

#ifdef _WIN32

#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Default CyUSB device-interface GUID.
 *
 * This is the GUID that the standard CyUSB.sys / CyUSB3.sys INF file
 * registers.  If your AL-LINK INF uses a custom GUID, update this constant
 * or make it configurable in a higher layer before init.
 */
DEFINE_GUID(CYUSB_DRIVER_GUID,
	0xAE18AA60, 0x7F6A, 0x11D4,
	0x97, 0xDD, 0x00, 0x01, 0x02, 0x29, 0xB9, 0x59);

/* ------------------------------------------------------------------ */
/*  CyUSB IOCTL definitions (from Cypress SDK cyioctl.h)              */
/* ------------------------------------------------------------------ */

/*
 * IOCTL codes differ between the two Cypress drivers commonly bound to
 * AL-LINK class devices:
 *
 *   CyUSB3.sys (FX3 / SuperSpeed driver)  base 0x0800, EP0=+3 NON_EP0=+5 RESET=+8
 *   CyUSB.sys  (FX2 / classic driver)     base 0x0000, EP0=+22 NON_EP0=+24 RESET=+27
 *
 * Anlogic's anlocyusb.sys is a Cypress-derived driver whose version resource
 * reports OriginalFilename=CyUsb3.sys, but older installations may still bind
 * the classic CyUSB.sys layout. The transport therefore probes both layouts at
 * runtime and caches the one that succeeds.
 */
#define AL_CYUSB_IOCTL_BASE_FX3          0x0800
#define AL_CYUSB_IOCTL_OFF_EP0_FX3       3
#define AL_CYUSB_IOCTL_OFF_NON_EP0_FX3   5
#define AL_CYUSB_IOCTL_OFF_RESET_FX3     8
#define AL_CYUSB_IOCTL_OFF_ABORT_FX3     15

#define AL_CYUSB_IOCTL_BASE_FX2          0x0000
#define AL_CYUSB_IOCTL_OFF_EP0_FX2       22
#define AL_CYUSB_IOCTL_OFF_NON_EP0_FX2   24
#define AL_CYUSB_IOCTL_OFF_RESET_FX2     27
#define AL_CYUSB_IOCTL_OFF_ABORT_FX2     28

enum al_cyusb_ioctl_layout {
	AL_CYUSB_IOCTL_LAYOUT_UNKNOWN = 0,
	AL_CYUSB_IOCTL_LAYOUT_FX2,
	AL_CYUSB_IOCTL_LAYOUT_FX3,
};

/* ------------------------------------------------------------------ */
/*  SINGLE_TRANSFER — the per-transfer header sent via DeviceIoControl */
/* ------------------------------------------------------------------ */

/*
 * Layout must exactly match Cypress SDK SINGLE_TRANSFER (cyioctl.h).
 * Default (natural) alignment is used — the driver does NOT use pack(1);
 * sizeof must be 40 bytes on Windows (3-byte tail padding after
 * ucEndpointAddress so NtStatus is 4-byte aligned).
 *
 * IMPORTANT: field order below is the canonical one. Reordering or
 * repacking causes DeviceIoControl to return ERROR_INVALID_PARAMETER (87)
 * because the driver reads BufferOffset/BufferLength from wrong offsets.
 */
typedef struct _AL_SINGLE_TRANSFER {
	/* EP0 setup packet (zeroed for bulk transfers) */
	uint8_t  bmRequest;
	uint8_t  bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
	uint32_t ulTimeOut;

	uint32_t IsoPacketOffset;    /* isochronous only (0 for bulk) */
	uint32_t IsoPacketLength;    /* isochronous only (0 for bulk) */
	uint32_t BufferOffset;       /* offset from struct start to data */
	uint32_t BufferLength;       /* data payload length */

	uint8_t  ucEndpointAddress;  /* target endpoint */
	/* 3 bytes of implicit padding here (natural alignment) */

	uint32_t NtStatus;           /* filled by driver on return */
	uint32_t UsbdStatus;         /* filled by driver on return */
} AL_SINGLE_TRANSFER;

/* ------------------------------------------------------------------ */
/*  Transport handle                                                   */
/* ------------------------------------------------------------------ */

typedef struct al_cyusb_handle {
	HANDLE dev;               /* handle returned by CreateFile */
	bool   is_open;
	enum al_cyusb_ioctl_layout ioctl_layout;
	char   dev_path[512];     /* device path for diagnostics */
} al_cyusb_handle_t;

/* ------------------------------------------------------------------ */
/*  Public API consumed by al_link.c                                   */
/* ------------------------------------------------------------------ */

/**
 * Open the first CyUSB device matching any of the given VID/PID pairs.
 *
 * @param vids  NULL-terminated array of Vendor IDs.
 * @param pids  NULL-terminated array of Product IDs (same length as vids).
 * @param h     Output handle, filled on success.
 * @return 0 on success, -1 on failure.
 */
int al_cyusb_open(const uint16_t vids[], const uint16_t pids[],
		  al_cyusb_handle_t *h);

/** Close an open CyUSB handle. */
void al_cyusb_close(al_cyusb_handle_t *h);

/**
 * Reset (clear halt on) a specific endpoint pipe.
 *
 * @return 0 on success, -1 on failure.
 */
int al_cyusb_reset_pipe(al_cyusb_handle_t *h, uint8_t ep_addr);

/**
 * Synchronous bulk write.
 *
 * @param ep_addr  OUT endpoint address (e.g. 0x06).
 * @param data     Pointer to data to send.
 * @param length   Number of bytes to send.
 * @param transferred  Output: actual bytes sent.
 * @param timeout_ms   Timeout in milliseconds (currently advisory).
 * @return 0 on success, -1 on failure.
 */
int al_cyusb_bulk_write(al_cyusb_handle_t *h, uint8_t ep_addr,
			const uint8_t *data, int length,
			int *transferred, unsigned int timeout_ms);

/**
 * Synchronous bulk read.
 *
 * @param ep_addr  IN endpoint address (e.g. 0x82).
 * @param data     Buffer to receive data.
 * @param length   Maximum number of bytes to read.
 * @param transferred  Output: actual bytes received.
 * @param timeout_ms   Timeout in milliseconds (currently advisory).
 * @return 0 on success, -1 on failure.
 */
int al_cyusb_bulk_read(al_cyusb_handle_t *h, uint8_t ep_addr,
		       uint8_t *data, int length,
		       int *transferred, unsigned int timeout_ms);

#endif /* _WIN32 */
#endif /* OPENOCD_JTAG_DRIVERS_AL_LINK_CYUSB_H */
