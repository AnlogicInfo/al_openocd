#include <flash/nor/dwcssi/dwcssi.h>
#define ZETTA_CMD_WRITE_REGISTER               0x01
#define ZETTA_CMD_WRITE_REGISTER_BYTE1         0x31

#define ZETTA_CMD_READ_STATUS_BYTE0            0x05
#define ZETTA_CMD_READ_STATUS_BYTE1            0x35

#define ZETTA_CMD_ENABLE_QPI                   0x38
#define ZETTA_CMD_DISABLE_QPI                  0xFF

#define ZETTA_EP_FAIL(x)                   ((x>>2) & 0x1)

int zetta_zd25q_quad_en(struct flash_bank *bank)
{
    // uint32_t sr_byte1 = 0;
    uint8_t sr[2];
    uint8_t quad_en_seq[3] = {ZETTA_CMD_WRITE_REGISTER, 0, 0};
    uint8_t quad_en;

    dwcssi_rd_flash_reg(bank,sr, ZETTA_CMD_READ_STATUS_BYTE1, 1);
    quad_en_seq[2] = sr[0] | 0x2;
    dwcssi_wr_flash_reg(bank, quad_en_seq, 3, STANDARD_SPI_MODE);

    dwcssi_rd_flash_reg(bank,sr, ZETTA_CMD_READ_STATUS_BYTE1, 1);
    // LOG_INFO("zd quad en sr %x", sr[0]);

    quad_en = (sr[0]>>1) & 0x1;
    if(quad_en != 1)
    {
        LOG_ERROR("zd quad en failed sr %x", sr[0]);
        return ERROR_FAIL;
    }

    return ERROR_OK;
}

int zetta_zd25q_quad_dis(struct flash_bank *bank)
{
    uint8_t sr[2] = {0};
    uint8_t quad_dis_seq[3] = {ZETTA_CMD_WRITE_REGISTER, 0, 0};
    uint8_t quad_en;

    // LOG_INFO("zd quad dis");

    dwcssi_rd_flash_reg(bank,sr, ZETTA_CMD_READ_STATUS_BYTE1, 1);
    // LOG_INFO("before sr byte0 %x", sr[0]);
    quad_dis_seq[2] = sr[0] & 0xFD;
    dwcssi_wr_flash_reg(bank, quad_dis_seq, 2, STANDARD_SPI_MODE);
    dwcssi_rd_flash_reg(bank,sr, ZETTA_CMD_READ_STATUS_BYTE1, 1);
    // LOG_INFO("after sr byte0 %x", sr[0]);

    quad_en = (sr[0]>>1) & 0x1;
    if(quad_en == 1)
    {
        LOG_ERROR("zd quad dis failed wr %x reset sr %x", quad_dis_seq[1], sr[0]);
        return ERROR_FAIL;
    }

    return ERROR_OK;
}


const flash_ops_t zetta_zd25q_ops = {
    .qread_cmd = 0x6B,
    .qprog_cmd = 0x32,
    .clk_div = 8,
    .wait_cycle = 8,

    .quad_rd_config = general_spi_quad_rd_config,
    .quad_en   = zetta_zd25q_quad_en,
    .quad_dis  = zetta_zd25q_quad_dis
};
