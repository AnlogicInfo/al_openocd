#include "dwcssi.h"

int flash_dwcssi(volatile uint32_t *ctrl_base, int32_t page_size, int count,
	uint32_t *buf_start, uint32_t *buf_end, uint32_t offset, uint32_t qprog_cmd, uint32_t spictrl)
{
	uint8_t *rp, *second_wr_rp;
	uint32_t page_offset;
	uint32_t cur_count, first_wr_count, second_wr_count;
	uint32_t second_wr_offset;
	uint32_t retval = 0;

	while (count > 0) {
		rp = (uint8_t *) wait_fifo(buf_start);

		if (count < page_size)
			cur_count = count;
		else
			cur_count = page_size;

		if (page_offset + cur_count > page_size) {
			first_wr_count = page_size - page_offset;
			second_wr_rp = rp + first_wr_count;
			second_wr_offset = offset + first_wr_count;
			second_wr_count = cur_count - first_wr_count;

			dwcssi_write_buffer(ctrl_base, rp, offset, first_wr_count, qprog_cmd, spictrl);
			dwcssi_write_buffer(ctrl_base, second_wr_rp, second_wr_offset, second_wr_count, qprog_cmd, spictrl);
		} else {
			retval = dwcssi_write_buffer(ctrl_base, rp, offset, cur_count, qprog_cmd, spictrl);
		}

		rp += cur_count;
		if (rp == buf_end)
			rp = buf_start + 2;
		*(buf_start + 1) = (uint32_t) rp;

		offset += cur_count;
		count  -= cur_count;
	}

	return retval;
}
