/*
 * File: driver.h
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-10-31
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-10-31
 */

#ifndef OPENOCD_FLASH_EMMC_DRIVER_H
#define OPENOCD_FLASH_EMMC_DRIVER_H

struct emmc_device;
#define __EMMC_DEVICE_COMMAND(name) \
    COMMAND_HELPER(name, struct emmc_device *emmc)

/**
 * Interface for EMMC flash controllers. 
 * 
 * 
 */

struct emmc_flash_controller {
    const char *name;
    const char *usage;

    const struct command_registration *commands;

	/** EMMC device command called when driver is instantiated during configuration. */
	__EMMC_DEVICE_COMMAND((*emmc_device_command));

	/** Initialize the EMMC device. */
	int (*init)(struct emmc_device *emmc, uint32_t* in_field);

	/** Reset the EMMC device. */
	int (*reset)(struct emmc_device *emmc);

	int (*command)(struct emmc_device *emmc, uint8_t command, uint32_t argument);

	// int (*read_resp)(struct emmc_device *emmc, uint8_t resp_len, uint32_t* resp_buf);

	/** Write a block of data to the EMMC device. */
	int (*write_image)(struct emmc_device *emmc, uint8_t *data, uint32_t addr, int size);	

	/** Read a block of data from the EMMC device. */
	int (*read_block_data)(struct emmc_device *emmc, uint32_t *data, uint32_t addr);

	int (*verify_image)(struct emmc_device *emmc, const uint8_t *data, uint32_t addr, uint32_t count);

	/** Check if the EMMC device is ready for more instructions with timeout. */
	int (*emmc_ready)(struct emmc_device *emmc, int timeout);
};

#define EMMC_DEVICE_COMMAND_HANDLER(name) static __EMMC_DEVICE_COMMAND(name)
/**
 * Find a EMMC flash controller by name.
 * @param name Identifies the EMMC controller to find.
 * @returns The emmc_flash_controller named @c name, or NULL if not found.
 */
struct emmc_flash_controller *emmc_driver_find_by_name(const char *name);

/** Signature for callback functions passed to emmc_driver_walk */
typedef int (*emmc_driver_walker_t)(struct emmc_flash_controller *c, void *);
/**
 * Walk the list of drivers, encapsulating the data structure type.
 * Application state/context can be passed through the @c x pointer.
 * @param f The callback function to invoke for each function.
 * @param x For use as private data storage, passed directly to @c f.
 * @returns ERROR_OK if successful, or the non-zero return value of @c f.
 * This allows a walker to terminate the loop early.
 */
int emmc_driver_walk(emmc_driver_walker_t f, void *x);


#endif