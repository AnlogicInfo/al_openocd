#ifndef OPENOCD_FPGA_DR1_H
#define OPENOCD_FPGA_DR1_H

#include <jtag/jtag.h>

struct dr1_fpga_device {
    struct jtag_tap *tap;
};

#endif /* OPENOCD_FPGA_DR1_H */