# DWCSSI Read Loader 实现设计文档

## 1. 背景与目标
当前 OpenOCD 中对 `dwcssi_read` 的实现是 Host 侧通过寄存器直接控制 Target 的 SPI 控制器来进行 Flash 数据的读取。这种方式会产生大量的 JTAG/SWD 交互开销，导致读取速度较慢。
为了提升 Flash 读取性能，计划参考现有的 `loader_flash_write_pp` 写入机制，实现基于 Loader 的 Ping-Pong（乒乓）读取机制。
核心思想是：将一段独立运行的 Loader 程序下载到 Target 的内存中，Loader 负责高速从 Flash 读取数据并填充到内存中的共享 Buffer；Host 侧只需监控标志位，通过批量内存读取（Memory Read）将数据取回并拼接。

## 2. 共享内存布局设计 (Shared Memory Layout)
为了实现 Host 与 Target 的协同，需要定义内存交互协议。参考 `al_spi_nor_loader.c` 的实现，共享内存 (Shared Memory) 的布局定义如下：

```text
  +------------------------------+  +0x0000
  | Page0: Loader params (256B)  |
  |  - offset (buf_start[0])     |  +0x0000 (uint32_t, flash 起始地址)
  |  - count (buf_start[16])     |  +0x0040 (int, 需要读取的总字节数)
  |  - DevId (buf_start[32])     |  +0x0080 (uint32_t, SpiNor 设备ID)
  |  - reserved                  |
  +------------------------------+  +0x0100 (PAGE_SIZE)
  | Page1: PP control block      |  +0x0100
  |  - buf0_flag                 |  +0x0100 (0: Buffer空/Host已读; >0: Buffer内有效数据块数; 0xFFFFFFFF: 异常)
  |  - buf1_flag                 |  +0x0140 (同上)
  |  - ctrl_flag                 |  +0x0180 (控制同步标志)
  |  - section_start_ptr         |  +0x01C0 (当前读取段的 Flash 偏移)
  |  - section_size_ptr          |  +0x0200 (当前读取段的块数)
  |  - buf0_start_addr           |  +0x0240 (Buffer0 的数据区起始地址)
  |  - buf1_start_addr           |  +0x0280 (Buffer1 的数据区起始地址)
  +------------------------------+  +0x0300
  | Data Buffer 0                |
  +------------------------------+
  | Data Buffer 1                |
  +------------------------------+
```

## 3. Host 侧 (OpenOCD) 实现方案

### 3.1 扩展 `loader_io.c`
在 `loader_io.c` 中新增 `loader_flash_read_pp` 接口：
```c
int loader_flash_read_pp(struct flash_loader *loader, struct code_src *srcs, uint8_t *data, target_addr_t addr, int image_size, int bank_number);
```
**执行流程：**
1. **初始化环境**：调用 `loader_init_arch` 和 `loader_init_code_area`，分配工作区并将 Loader ELF 代码写入 Target RAM。
2. **设置参数**：计算并设置 Shared Memory 中的参数（`offset`, `count`, `DevId`），并初始化控制块参数（`buf0_start_addr`, `buf1_start_addr`, `buf0_flag=0`, `buf1_flag=0`, `ctrl_flag=1`）。
3. **启动 Target**：通过 `target_start_algorithm` 异步启动 Loader。
4. **乒乓读取 (Ping-Pong Read)**：
   - Host 侧维护一个指针 `prod_idx = 0`。
   - 轮询读取当前 Buffer 的 `flag`（`buf0_flag` 或 `buf1_flag`）。
   - 如果 `flag > 0`，说明 Target 已经将数据准备好，读取的字节数为 `flag * BLOCK_SIZE`（注意处理最后一块的边界情况）。
   - 使用 `target_read_buffer` 将数据从 Target 的 `buf_start_addr` 读回 Host，并拷贝到最终的 `data` 缓存中。
   - 将对应的 `flag` 写为 `0`，通知 Target 缓存已被 Host 消耗完毕。
   - 切换 `prod_idx ^= 1`，重复上述过程直到所有 `image_size` 读取完成。
5. **退出清理**：等待 Target 算法结束（`target_wait_algorithm`），释放分配的工作区并清理环境。

### 3.2 修改 `dwcssi.c`
在 `dwcssi_read` 函数中接入新机制：
1. 检查是否存在配置好的 Loader 环境（`driver_priv->elf_dir` 或有效路径）。
2. 如果存在，并且读取数据量较大，则优先调用 `loader_flash_read_pp` 进行读取。
3. 若 Loader 不可用或读取小块数据，回退至原有的 `dwcssi_read_x4` / `dwcssi_read_x1` 逻辑。

## 4. Target 侧 (Loader) 实现方案

在 `al_spi_nor_loader.c` 的基础上派生一个专门用于 Read 的 Loader（如 `al_spi_nor_read_loader.c`）：

**执行流程：**
1. **读取参数**：Loader 启动后，从 Page 0 处读取 `offset`（Flash 起始地址）和 `count`（需要读取的总字节数），以及 `DevId`。
2. **硬件初始化**：调用 `AlSpiNor_Init` 初始化 SPI 控制器和 Flash。
3. **同步握手**：等待 Host 设置 `ctrl_flag`（或通知 Host 已就绪）。
4. **乒乓读取数据**：
   - 维护 `prod_idx = 0`。
   - 判断剩余需要读取的 `remaining_bytes`。
   - 检查当前对应的 `flag`（`buf0_flag` 或 `buf1_flag`）。如果 `flag == 0`，说明该 Buffer 是空的（Host 已读取）。
   - 计算本次可以读取的数据量 `to_read`（通常为 Buffer 大小，如果是最后一次则为 `remaining_bytes`）。
   - 调用 `AlSpiNor_Read` 将数据从 Flash 读取到对应的 `buf_start_addr` 缓存中。
   - 更新 Flash 地址偏移和 `remaining_bytes`。
   - 将对应的 `flag` 设置为写入的块数或字节数（通知 Host 数据已就绪），执行内存屏障（如 `DSB()`）确保可见性。
   - 切换 `prod_idx ^= 1`。
5. **结束**：当所有数据读取完成并传递给 Host 后，Loader 返回或进入停机状态，等待 Host 结束运行。

## 5. 异常处理
- **超时机制**：Host 侧在轮询 `flag` 时应加入超时判断。如果在一定时间内（例如 2-3 秒）Flag 一直没有更新，需中止读取操作，返回 `ERROR_TARGET_TIMEOUT`。
- **状态异常**：如果 Target 检测到 Flash 读取失败，可将 `flag` 置为特定错误码（如 `0xFFFFFFFF`），Host 侧检测到此值后中止读取。
- **缓存一致性**：Target 侧读写 Flag 和数据 Buffer 前后，需要保证 Cache 一致性（如关闭 D-Cache 或调用 Invalid/Flush 操作，以及 `DSB` 指令）。
