#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "anlogic_bit.h"
#include "pld.h"
#include <helper/log.h>
#include <sys/stat.h>
#include <helper/system.h>
#include <string.h>
#include <stdlib.h>

#define MAX_SECTION_LEN 1024
#define SECTION_COUNT 11

static int anlogic_split_bit_file(FILE *input_file, uint8_t **header, uint8_t **data, long *data_len)
{
    uint8_t buffer[MAX_SECTION_LEN];
    size_t file_size, read_size;
    long split_pos = -1;

    // 获取文件大小
    fseek(input_file, 0, SEEK_END);
    file_size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);

    // 读取文件内容并查找连续两个 0x0a 的位置
    size_t offset = 0;
    while ((read_size = fread(buffer, 1, sizeof(buffer), input_file)) > 0) {
        for (size_t i = 1; i < read_size; i++) {
            if (buffer[i - 1] == 0x0a && buffer[i] == 0x0a) {
                split_pos = offset + i - 1;
                break;
            }
        }
        if (split_pos != -1)
            break;
        offset += read_size;
    }

    if (split_pos == -1) {
        LOG_ERROR("Failed to find split position in bit file");
        return -1;
    }

    // 分割文件
    fseek(input_file, 0, SEEK_SET);
    *header = (uint8_t *)malloc(split_pos + 1);
    if (!*header) {
        LOG_ERROR("Memory allocation failed for header");
        return -1;
    }
    fread(*header, 1, split_pos, input_file);
    (*header)[split_pos] = '\0';

    *data_len = file_size - split_pos - 2; // 跳过两个 0x0a
    *data = (uint8_t *)malloc(*data_len + 1);
    if (!*data) {
        LOG_ERROR("Memory allocation failed for data");
        free(*header);
        return -1;
    }
    fseek(input_file, split_pos + 2, SEEK_SET);
    fread(*data, 1, *data_len, input_file);
    (*data)[*data_len] = '\0';

    return 0;
}

static int read_section(uint8_t *header, uint8_t **sections)
{
    uint8_t *current = header;

    for (int i = 0; i < SECTION_COUNT; i++) {
        if (strncmp((char *)current, "# ", 2) != 0) {
            LOG_ERROR("Invalid header format at section: %d", i);
            return -1;
        }
        current += 2; // 跳过 "# "

        char *end;
        if (i == SECTION_COUNT - 1) {
            // 最后一个段 user_code 从当前位置到 header 的末尾
            end = (char *)header + strlen((char *)header);
        } else {
            end = strstr((char *)current, "\n");
            if (!end) {
                LOG_ERROR("Failed to find end of section: %d", i);
                return -1;
            }
        }

        size_t len = end - (char *)current;
        sections[i] = (uint8_t *)malloc(len + 1);
        if (!sections[i]) {
            LOG_ERROR("Memory allocation failed for section: %d", i);
            return -1;
        }
        memcpy(sections[i], current, len);
        sections[i][len] = '\0';

        current = (uint8_t *)end + 1; // 跳到下一段
    }

    return 0;
}

int anlogic_read_bit_file(struct anlogic_bit_file *bit_file, const char *filename)
{
    FILE *input_file;
    uint8_t *header = NULL;
    uint8_t *data = NULL;
    long data_len = 0;

    input_file = fopen(filename, "rb");
    if (!input_file) {
        LOG_ERROR("Failed to open file: %s", filename);
        return -1;
    }

    // 调用 anlogic_split_bit_file 分割文件
    if (anlogic_split_bit_file(input_file, &header, &data, &data_len) != 0) {
        fclose(input_file);
        return -1;
    }
    fclose(input_file);

    // 解析 header
    uint8_t **sections[] = {
        &bit_file->start,
        &bit_file->version,
        &bit_file->design_name,
        &bit_file->architecture,
        &bit_file->package,
        &bit_file->date,
        &bit_file->golbal_crc,
        &bit_file->transfer_crc,
        &bit_file->file_format,
        &bit_file->spi_feature,
        &bit_file->user_code
    };

    if (read_section(header, (uint8_t **)sections) != 0) {
        free(header);
        free(data);
        return -1;
    }

    // 保存 data 段和 data_len
    bit_file->data = data;
    bit_file->data_len = data_len;

    free(header);

    // 添加调试信息
    return 0;
}