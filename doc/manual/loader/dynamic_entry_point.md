# Loader 动态入口点解析方案

## 1. 背景与问题
当前 OpenOCD loader 框架中，`target_run_algorithm` 等执行接口的 `entry_point` 参数通常被硬编码为分配的工作区首地址 (`loader->copy_area->address`)。
这要求编译出的 Loader 固件的可执行代码必须严格位于文件的绝对起始位置。如果 Loader 链接脚本的开头包含中断向量表或其他非执行数据，或者 `_start` 入口并未放置在文件最开头，Loader 将从错误的位置开始执行，从而导致运行异常甚至 CPU 跑飞。

## 2. 目标
通过解析 ELF 文件的头部信息，动态获取真实的 `entry_point` (例如 `_start` 符号的地址)，并将其转化为相对代码段起始位置的偏移量。在运行 Loader 算法时应用此偏移，使得 Loader 可以兼容带有中断向量表或入口地址不固定的固件结构。

## 3. 修改计划

### 3.1 扩展数据结构
在 `src/flash/loader_io.h` 中的 `struct code_src` 结构体中新增 `entry_offset` 字段，用于保存入口点相对于代码首地址的偏移量：
```c
struct code_src {
    const uint8_t *bin;
    int size;
    uint32_t entry_offset; // 新增：入口点相对基址的偏移量
};
```

### 3.2 在 ELF 加载阶段计算偏移量
在 `src/flash/loader_io.c` 的 `loader_load_elf` 函数中，利用 OpenOCD 已有的 `struct image` 解析能力获取入口地址。
OpenOCD 的 `image_open` 函数会自动解析 ELF 的 `e_entry` 并将其保存在 `image.start_address` 中。
我们需要在获取到有效 `min_base` 后，计算偏移量并赋值给 `src`：
```c
// 在 loader_load_elf 的末尾添加计算逻辑
if (img.start_address_set && img.start_address >= min_base) {
    src->entry_offset = img.start_address - min_base;
    LOG_DEBUG("loader_load_elf: entry offset %" PRIx32, src->entry_offset);
} else {
    src->entry_offset = 0; // 兜底：如果没有入口信息或入口非法，默认从头开始
}
```

### 3.3 在执行阶段应用偏移量
修改 `src/flash/loader_io.c` 中所有调用算法的地方，将 `entry_point` 参数加上上面计算的偏移量。
涉及修改的函数包括但不限于：
- `loader_flash_write_sync`
- `loader_flash_write_async_pp`
- `loader_flash_write`
- `loader_flash_write_pp`
- `loader_flash_read_pp`
- `loader_flash_crc_mem_parm`
- `loader_flash_write_async`
- `loader_flash_crc`

将原本传递给 `target_run_algorithm` 或 `target_run_async_algorithm` 及同系列函数的入口地址参数由：
```c
loader->copy_area->address
```
修改为：
```c
loader->copy_area->address + loader->code_src->entry_offset
```

## 4. 潜在风险与注意事项

1. **64位地址截断风险**：当前 `struct image` 中的 `start_address` 字段被声明为 `uint32_t`。对于 64 位架构（如 AArch64、RV64），如果链接的绝对入口地址超出 32 位寻址空间，OpenOCD 的 `image_open` 可能会发生截断。后续如有 64 位绝对高地址运行的需求，需要将 `image.h` 中的 `start_address` 升级为 `target_addr_t` 或 `uint64_t`。
2. **纯 Bin 文件兼容性**：加载非 ELF 的 `.bin` 文件时（如果没有额外指定入口），`start_address_set` 为 `false`，本方案会默认 `entry_offset = 0`，与现有逻辑完全兼容，不会引入副作用。
3. **架构强相关限制**：部分架构的 CPU 在启动时有固定的取指行为。应用该改动时需要确认硬件和目标配置允许通过 PC 寄存器重定向方式跳过文件头并安全执行代码。
