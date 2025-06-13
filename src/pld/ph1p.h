#ifndef OPENOCD_FPGA_PH1P_H
#define OPENOCD_FPGA_PH1P_H

#include <jtag/jtag.h>

struct ph1p_fpga_device {
    struct jtag_tap *tap;
};

#endif /* OPENOCD_FPGA_DR1_H */