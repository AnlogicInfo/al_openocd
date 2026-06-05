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
 * The public Cypress CyAPI sources use the following CyUSB IOCTL function
 * numbers.  These match the programmer's reference for CyUSB3.sys and the
 * user-mode library implementation in CyAPI.cpp / cyioctl.h.
 */
#define AL_CYUSB_IOCTL_INDEX                    0x0000
#define AL_CYUSB_IOCTL_OFF_EP0                 8
#define AL_CYUSB_IOCTL_OFF_NON_EP0             9
#define AL_CYUSB_IOCTL_OFF_RESET               11
#define AL_CYUSB_IOCTL_OFF_ABORT               17
#define AL_CYUSB_IOCTL_OFF_NON_EP0_DIRECT      18

#define AL_CYUSB_IOCTL_EP0_TRANSFER \
	CTL_CODE(FILE_DEVICE_UNKNOWN, AL_CYUSB_IOCTL_INDEX + AL_CYUSB_IOCTL_OFF_EP0, \
		METHOD_BUFFERED, FILE_ANY_ACCESS)

#define AL_CYUSB_IOCTL_NON_EP0_TRANSFER \
	CTL_CODE(FILE_DEVICE_UNKNOWN, AL_CYUSB_IOCTL_INDEX + AL_CYUSB_IOCTL_OFF_NON_EP0, \
		METHOD_BUFFERED, FILE_ANY_ACCESS)

#define AL_CYUSB_IOCTL_RESET_PIPE \
	CTL_CODE(FILE_DEVICE_UNKNOWN, AL_CYUSB_IOCTL_INDEX + AL_CYUSB_IOCTL_OFF_RESET, \
		METHOD_BUFFERED, FILE_ANY_ACCESS)

#define AL_CYUSB_IOCTL_ABORT_PIPE \
	CTL_CODE(FILE_DEVICE_UNKNOWN, AL_CYUSB_IOCTL_INDEX + AL_CYUSB_IOCTL_OFF_ABORT, \
		METHOD_BUFFERED, FILE_ANY_ACCESS)

#define AL_CYUSB_IOCTL_NON_EP0_DIRECT \
	CTL_CODE(FILE_DEVICE_UNKNOWN, AL_CYUSB_IOCTL_INDEX + AL_CYUSB_IOCTL_OFF_NON_EP0_DIRECT, \
		METHOD_NEITHER, FILE_ANY_ACCESS)

/* ------------------------------------------------------------------ */
/*  SINGLE_TRANSFER — the per-transfer header sent via DeviceIoControl */
/* ------------------------------------------------------------------ */

/*
 * SINGLE_TRANSFER in the Cypress SDK is byte-packed and starts with a union of
 * SETUP_PACKET / ISO_ADV_PARAMS.  Field order matters: a reordered or naturally
 * aligned definition is rejected by the kernel driver with error 87.
 */
#pragma pack(push, 1)
typedef struct _AL_SETUP_PACKET {
	uint8_t  bmRequest;
	uint8_t  bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
	uint32_t ulTimeOut;
} AL_SETUP_PACKET;

typedef struct _AL_ISO_ADV_PARAMS {
	uint16_t isoId;
	uint16_t isoCmd;
	uint32_t ulParam1;
	uint32_t ulParam2;
} AL_ISO_ADV_PARAMS;

typedef struct _AL_SINGLE_TRANSFER {
	union {
		AL_SETUP_PACKET SetupPacket;
		AL_ISO_ADV_PARAMS IsoParams;
	};
	uint8_t  reserved;
	uint8_t  ucEndpointAddress;
	uint32_t NtStatus;
	uint32_t UsbdStatus;
	uint32_t IsoPacketOffset;
	uint32_t IsoPacketLength;
	uint32_t BufferOffset;
	uint32_t BufferLength;
} AL_SINGLE_TRANSFER;
#pragma pack(pop)

/* ------------------------------------------------------------------ */
/*  Transport handle                                                   */
/* ------------------------------------------------------------------ */

typedef struct al_cyusb_handle {
	HANDLE dev;               /* handle returned by CreateFile */
	bool   is_open;
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
 * Blocking bulk write with a caller-controlled timeout.
 *
 * @param ep_addr  OUT endpoint address (e.g. 0x06).
 * @param data     Pointer to data to send.
 * @param length   Number of bytes to send.
 * @param transferred  Output: actual bytes sent.
 * @param timeout_ms   Timeout in milliseconds. Use 0 to wait indefinitely.
 * @return 0 on success, -1 on failure.
 */
int al_cyusb_bulk_write(al_cyusb_handle_t *h, uint8_t ep_addr,
			const uint8_t *data, int length,
			int *transferred, unsigned int timeout_ms);

/**
 * Blocking bulk read with a caller-controlled timeout.
 *
 * @param ep_addr  IN endpoint address (e.g. 0x82).
 * @param data     Buffer to receive data.
 * @param length   Maximum number of bytes to read.
 * @param transferred  Output: actual bytes received.
 * @param timeout_ms   Timeout in milliseconds. Use 0 to wait indefinitely.
 * @return 0 on success, -1 on failure.
 */
int al_cyusb_bulk_read(al_cyusb_handle_t *h, uint8_t ep_addr,
		       uint8_t *data, int length,
		       int *transferred, unsigned int timeout_ms);

#endif /* _WIN32 */
#endif /* OPENOCD_JTAG_DRIVERS_AL_LINK_CYUSB_H */
