dwcmshc_emmc_async_write_image 基于 loader_flash_write_pp 的统一 Ping-Pong FIFO Loader 方案（实现文档）

1. 目标

- 将 eMMC(dwcmschc) 的镜像写入路径从 loader_flash_write_async_pp 迁移到 loader_flash_write_pp。
- 与 QSPI dwcssi 的 pp 写入方案对齐，复用 target_run_async_algorithm_ping_pong 的“mailbox + ping-pong buffer”传输协议。
- 在完成 dwcmshc 与 dwcssi 的迁移后，删除/下线冗余的 loader_flash_write_async_pp 入口与其专用协议实现，降低维护成本。

相关代码位置：
- [dwcmshc_subs.c](file:///d:/work/projs/al_openocd/src/flash/emmc/dwcmshc_subs.c): dwcmshc_emmc_async_write_image（当前调用 loader_flash_write_async_pp）
- [loader_io.h](file:///d:/work/projs/al_openocd/src/flash/loader_io.h): loader_flash_write_pp/loader_flash_write_async_pp 声明、loader_shmem_map 协议
- [loader_io.c](file:///d:/work/projs/al_openocd/src/flash/loader_io.c): loader_flash_write_pp/loader_flash_write_async_pp 实现
- [target.c](file:///d:/work/projs/al_openocd/src/target/target.c): target_run_async_algorithm_ping_pong 与 mailbox/pp fifo 共享内存布局
- [dwcssi.c](file:///d:/work/projs/al_openocd/src/flash/nor/dwcssi/dwcssi.c#L1514-L1562): dwcssi_write_loader（已按 pp 写入路径落地的参考实现）
- [al_mmc_sdma_write_loader.c](file:///z:/work/master_sdk/fpsoc/driver/ps_driver/mmc/example/al_mmc_sdma_write_loader.c): 当前 eMMC loader（pp 协议消费端示例/实现基线）


2. 现状（问题点）

2.1 Host 侧（OpenOCD 驱动）

- dwcmshc_emmc_async_write_image 当前调用 loader_flash_write_async_pp。
- 该入口依赖“寄存器参数 + buf_start/next/ctrl 的专用布局”，并通过 target_run_async_algorithm_ping_pong 进行传输，但控制块含义与 loader_flash_write_pp（mailbox + loader_shmem_map）不一致。
- 结果是：dwcssi/dwcmshc 两套写入通道并存，无法统一协议与清理共享状态（mailbox）。

2.2 Target 侧（Loader-ELF）

- 当前 eMMC loader 以 master_sdk 的 [al_mmc_sdma_write_loader.c](file:///z:/work/master_sdk/fpsoc/driver/ps_driver/mmc/example/al_mmc_sdma_write_loader.c) 为基线实现：
  - 使用 pp flag + ctrl_flag + section_start_ptr 的消费模型
  - 与 loader_flash_write_pp 的“共享内存参数页 + pp 控制块 + mailbox”协议一致，但字段使用上存在差异（见第 6 节）


3. 统一方案概览

3.1 Host 侧统一入口

- 写入统一走 loader_flash_write_pp（pp 写入）。
- 写入前设置 mailbox sections（至少 1 个 section），写入后清理 mailbox，保证 g_mailbox 生命周期成对。
- 针对 pp 传输按“块数”搬运的特性，对数据进行 block_size 对齐的 staging buffer（尾部 padding），避免 host 侧越界读取。

3.2 Target 侧统一协议

- eMMC async loader 改造为 pp 消费模型：
  - 读取 loader_shmem_map.params（至少 image_size/bank_id；flash_addr 可选，取决于 loader 实现是否使用）
  - 轮询并 ack ctrl_flag，读取 section_start_ptr（必需）/section_size_ptr（可选）
  - 轮询 buf0_flag/buf1_flag 获取“可消费块数”，消费后清零
  - 遵循 0xFFFFFFFF abort 约定快速退出

说明：
- 对齐统一的是“传输协议与 Host 侧入口”，不是“地址单位”。dwcssi 使用 byte offset；dwcmshc 使用 block address（保持现有行为即可）。


4. Host↔Target 协议定义（pp fifo + loader_shmem_map + mailbox）

4.1 loader_shmem_map（Page0 参数区）

OpenOCD 侧由 loader_flash_write_pp 写入（见 loader_set_mem_parms）：
- params.flash_addr：写入起始地址
  - dwcmshc 建议继续使用 block address（即 addr / block_size）
- params.image_size：精确写入字节数（bytes）
- params.bank_id：设备/实例编号
  - dwcmshc 建议用于区分控制器实例（例如 SDIO1/SDIO2），由 loader 侧映射到 ctrl_base

参数布局定义见 [loader_io.h](file:///d:/work/projs/al_openocd/src/flash/loader_io.h) 的 loader_shmem_map：
- flash_addr @ +0x00
- image_size @ +0x40
- bank_id    @ +0x80
- fifo_buffer 起始 @ +0x100

4.2 ping-pong 控制块（位于 fifo_buffer 起始处）

target_async_algorithm_init_ping_pong_fifo 会将控制块放在 fifo_start（即 loader_buf_start + 0x100）开始处（见 [target.c](file:///d:/work/projs/al_openocd/src/target/target.c#L1027-L1067)）：
- +0x000：buf0_flag（0=空闲；>0=有效 blocks；0xFFFFFFFF=abort）
- +0x040：buf1_flag（同上）
- +0x080：ctrl_flag（1=段信息就绪；loader 读完置 0）
- +0x0C0：section_start_ptr（本段起始地址，单位由 loader 定义）
- +0x100：section_size_ptr（本段 blocks 数）
- +0x140：buf0_start_addr（host 填充数据的目标地址）
- +0x180：buf1_start_addr
- +0x1C0：buf2_start_addr（当前用于 ddr_en 场景的额外缓冲起始，按现实现状保留）

数据缓冲区起点由 target_async_algorithm_init_ping_pong_fifo 计算：
- 非 ddr_en：buf0_start = fifo_start + block_size
- ddr_en：buf0_start = 0（依赖 target 侧额外机制，eMMC 迁移时建议先走非 ddr_en 路径）

4.3 mailbox sections（Host 设置）

Host 在调用 loader_flash_write_pp 前设置 mailbox sections（示例见 [dwcssi.c](file:///d:/work/projs/al_openocd/src/flash/nor/dwcssi/dwcssi.c#L1547-L1557)）：
- section_start_addr：dwcmshc 建议填写 block_addr
- size_blocks：DIV_ROUND_UP(image_size, block_size)
- buf_offset_blocks：0（当前未使用，保留）

写入后必须清理：
- target_set_mailbox_sections(NULL, NULL, NULL, 0)


5. Host 侧改造落点（dwcmshc_emmc_async_write_image）

5.1 建议的 Host 侧调用流程

- 计算 block_addr = addr / emmc->device->block_size
- 固定 block_size = emmc->device->block_size（通常 512），并设置：
  - loader->block_size = block_size
  - loader->image_size = image_size
- 加载新的（或改造后的）pp 版本 eMMC async loader ELF：
  - 仍可沿用现有命名 emmc_async_{riscv_64,aarch_64}.elf，但其内部实现需改为 pp 协议
- 构造 staging buffer（必要）：
  - image_block_cnt = DIV_ROUND_UP(image_size, block_size)
  - total_bytes = image_block_cnt * block_size
  - malloc(total_bytes)，用 0xFF 填充尾部，再 memcpy(image_size)
  - 原因：target_ping_pong_trans_data 以“块”为单位搬运，会读取到对齐后的 total_bytes
- 设置 mailbox（单段）：
  - section_start_addr = block_addr
  - size_blocks = image_block_cnt
- 调用 loader_flash_write_pp：
  - addr 参数建议传 block_addr（与 params.flash_addr 对齐）
  - bank_number 建议传 emmc 控制器实例编号（例如 driver_priv->io_location），供 loader 侧选择 ctrl_base
- 清理 mailbox 与释放 staging buffer

5.2 地址语义（dwcmshc）

- params.flash_addr / mailbox.section_start_addr：建议均采用 block address。
- params.image_size：严格的 byte 数，loader 必须以此作为“精确写入长度”，不要写入 padding 区域。


6. Target 侧实现落点（master_sdk eMMC loader）

6.1 现状与对齐点

- 当前 eMMC loader 采用 master_sdk 的 [al_mmc_sdma_write_loader.c](file:///z:/work/master_sdk/fpsoc/driver/ps_driver/mmc/example/al_mmc_sdma_write_loader.c)，其行为要点：
  - 从 Page0 参数区读取：
    - count = buf_start[16]（对应 +0x40，语义等价于 params.image_size）
    - device_id = buf_start[32]（对应 +0x80，语义等价于 params.bank_id）
  - 通过 ctrl_flag/section_start_ptr 获取本段起始 block_addr，并用 blocks 循环消费 buf0/buf1 数据进行写入
  - 当前实现未读取 section_size_ptr（Host 侧仍建议按 1 段设置 mailbox，size_blocks 用于 Host 侧传输边界控制）

对齐建议：
- 保持 params.image_size 与 params.bank_id 的写入语义不变（由 OpenOCD 写入 +0x40/+0x80），master_sdk loader 侧无需改动即可读取。
- 若需要让 loader 侧完全遵循“多段 section”语义，则需要在 master_sdk loader 中额外读取 section_size_ptr 并按 section_blocks 控制段边界（当前实现以 remaining_blocks=count/512 为准，等价于单段）。

6.2 与 OpenOCD pp 控制块偏移的对应关系

- 以 [al_mmc_sdma_write_loader.c](file:///z:/work/master_sdk/fpsoc/driver/ps_driver/mmc/example/al_mmc_sdma_write_loader.c) 的索引写法为例（buf_start 为 uint32_t*，每个 index 代表 4 字节）：
  - Page0（loader_shmem_map.params）：
    - buf_start[16] -> +0x40（params.image_size/count）
    - buf_start[32] -> +0x80（params.bank_id/device_id）
  - Page1（pp 控制块，起点为 fifo_buffer，即 +0x100）：
    - buf_start[64]  -> +0x100（buf0_flag）
    - buf_start[80]  -> +0x140（buf1_flag）
    - buf_start[96]  -> +0x180（ctrl_flag）
    - buf_start[112] -> +0x1C0（section_start_ptr）
    - buf_start[144] -> +0x240（buf0_start_addr_ptr）
    - buf_start[160] -> +0x280（buf1_start_addr_ptr）


7. 删除/下线 loader_flash_write_async_pp（迁移后清理）

7.1 删除条件

- dwcmshc_emmc_async_write_image 与 dwcssi_write_loader 均使用 loader_flash_write_pp（或 loader_flash_write 统一入口）完成写入。
- 目标侧对应的 loader ELF 已全部切换到 pp 协议，且不再依赖 loader_flash_write_async_pp 的专用控制块。

7.2 清理点

- 删除 [loader_io.h](file:///d:/work/projs/al_openocd/src/flash/loader_io.h) 中 loader_flash_write_async_pp 声明。
- 删除 [loader_io.c](file:///d:/work/projs/al_openocd/src/flash/loader_io.c) 中 loader_flash_write_async_pp 实现（以及其依赖的特殊 buf_start 偏移写入逻辑）。
- 更新所有调用点（当前主要为 [dwcmshc_subs.c](file:///d:/work/projs/al_openocd/src/flash/emmc/dwcmshc_subs.c)）。


8. 回归验证建议

- 功能正确性：
  - 写入后使用已有 checksum/读回接口验证（eMMC 可对比 dwcmshc_checksum 路径；QSPI 可对比 dwcssi verify）
- 边界覆盖：
  - image_size 非 block_size 整数倍（验证 staging/padding 与“精确写入”控制）
  - 连续两次写入（验证 mailbox set/clear 成对，g_mailbox 不泄露）
  - abort/超时（验证 host 置 0xFFFFFFFF 后 loader 能退出、host 不死等）
