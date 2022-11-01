/*
 * File: dwcmshc.c
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-10-31
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-10-31
 */


#include "dwc_mshc.h"

const struct emmc_flash_controller dwcmshc_emmc = {
    .name = "dwcmshc"
    // .emmc_device_command = ,
    // .init = ,
    // .reset = ,
    // .write_block_data = ,
    // .read_block_data = ,
    // .emmc_ready      = 
};
