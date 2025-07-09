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
#include <stdbool.h>

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

static int read_section(uint8_t *header, struct anlogic_bit_file* bit_file)
{
    uint8_t *current = header;
    int section_idx = 0;
    int max_sections = SECTION_COUNT * 2; // 防御性上限，防止死循环

    // 按照bit_file字段顺序填充
    uint8_t **fields[SECTION_COUNT] = {
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

    while (section_idx < max_sections && section_idx < SECTION_COUNT) {
        if (strncmp((char *)current, "# ", 2) != 0)
            break;

        current += 2; // 跳过 "# "
        char *end = strstr((char *)current, "\n");
        if (!end) {
            // 最后一个段（如 user_code）可能没有换行，直接到末尾
            end = (char *)header + strlen((char *)header);
        }

        size_t len = end - (char *)current;
        *fields[section_idx] = (uint8_t *)malloc(len + 1);
        if (!*fields[section_idx]) {
            LOG_ERROR("Memory allocation failed for section: %d", section_idx);
            return -1;
        }
        memcpy(*fields[section_idx], current, len);
        (*fields[section_idx])[len] = '\0';

        section_idx++;
        if (*end == '\0')
            break;
        current = (uint8_t *)end + 1;
    }

    if (section_idx >= max_sections) {
        LOG_ERROR("read_section: section parse loop exceeded max_sections, possible malformed header");
        return -1;
    }

    // 剩余的 section 置为 NULL
    for (int i = section_idx; i < SECTION_COUNT; i++) {
        *fields[i] = NULL;
    }

    return 0;
}

int anlogic_read_bit_file(struct anlogic_bit_file *bit_file, const char *filename)
{
    FILE *input_file;
    uint8_t *header = NULL;
    uint8_t *data = NULL;
    long data_len = 0;

    // 确保bit_file结构体内容初始化为0，防止野指针
    memset(bit_file, 0, sizeof(*bit_file));

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

    if (read_section(header, bit_file) != 0) {
        free(header);
        free(data);
        return -1;
    }

    LOG_INFO("bitfile start: %s", bit_file->start);
    LOG_INFO("bitfile version: %s", bit_file->version);
    LOG_INFO("bitfile design_name: %s", bit_file->design_name);    
    LOG_INFO("bitfile architecture: %s", bit_file->architecture);

        // 保存 data 段和 data_len
    bit_file->data = data;
    bit_file->data_len = data_len;

    #undef SAFE_LOG_STR

    free(header);

    // 添加调试信息
    return 0;
}

int anlogic_check_architecture(struct anlogic_bit_file* bit_file, const char *drv_name)
{
    const char *arch = (const char*)bit_file->architecture;
    bool match = false;

    // 跳过前缀"Architecture:"和空白
    if (arch && strncmp(arch, "Architecture:", 13) == 0) {
        arch += 13;
        while (*arch == ' ' || *arch == '\t') arch++;
    }

    // 检查架构和驱动名称是否匹配
    if (arch && drv_name) {
        // 直接匹配
        if (strcmp(arch, drv_name) == 0) {
            match = true;
        }
        // 特殊情况：dr1_90 驱动也可以用于 dr1_300p 架构
        else if (strcmp(drv_name, "dr1_90") == 0 && strcmp(arch, "dr1_300p") == 0) {
            match = true;
        }
    }

    if (!match) {
        LOG_ERROR("Bitfile architecture '%s' does not match driver name '%s'",
            arch ? arch : "(null)",
            drv_name ? drv_name : "(null)");
        // return ERROR_FAIL;
    }
    return ERROR_OK;
}