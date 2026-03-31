dwcssi_write 基于 Ping-Pong FIFO 的 Loader-ELF 统一写入方案（设计稿）

1. 目标

- 将 dwcssi 的写入数据通道从“环回 FIFO(wp/rp 环形缓冲)”统一为 “Ping-Pong FIFO(pp fifo)”。
- 复用现有 target 层 pp fifo 传输框架（target_run_async_algorithm_ping_pong / target_ping_pong_trans_data），并在 dwcssi 调用前设置 mailbox sections，保证 pp 机制可用且语义明确。
- 完全迁移：dwcssi 写入仅保留 pp 写入路径，不保留环回 FIFO 旧路径。

相关代码位置：
- src/flash/nor/dwcssi/dwcssi.c: dwcssi_write_loader 等写入路径
- src/flash/loader_io.h, src/flash/loader_io.c: loader 框架、ELF 加载、写入入口
- src/target/target.h, src/target/target.c: pp fifo 初始化与传输、mailbox sections
- contrib/loaders/flash/qspi/dwcssi/: dwcssi loader 工程与 ELF 产物


2. 现状与问题

2.1 dwcssi 当前写入（环回 FIFO）

- Host 启动 Loader 后，通过 async ring fifo 将 data 推送到目标端共享缓冲；
- 目标端 loader 通过 wp/rp 消费数据，并按页写入 QSPI Flash；
- 协议实现较轻，但与 dwcmshc 的 pp 写、nuspi 的同步写并存，维护成本高。

2.2 pp fifo 的现状（dwcmshc 已使用）

- dwcmshc 在写入前会设置 mailbox sections（即便只有 1 个 section），使得 pp fifo 传输函数可读取段信息并传递给 loader；
- pp fifo 传输当前依赖 g_mailbox（全局状态），未设置会导致传输层无法获得段边界与起始地址。

结论：
- 若要统一为 pp 写，必须把“mailbox 的设置/清理”固化为 dwcssi 写入流程的一部分（至少设置 1 个 section）。


3. 统一原则与边界

3.1 统一点

- 统一“数据通道”为 pp fifo：host 只负责填充 ping/pong 半缓冲并置位 flag，loader 消费后清 flag。
- 统一“段信息传递”为 mailbox：host 通过 mailbox 提供 section_start_addr 与 size_blocks；pp 传输层负责写入共享区并通过 ctrl_flag 通知 loader。

3.2 不强制统一点

- section_start_addr 的单位不强制统一：由具体 loader 解释（dwcmshc 可用 block address；dwcssi 建议使用 byte offset）。
- block_size 不强制统一：由各控制器性能与写入约束决定；dwcssi 初期建议以 page_size 为 block_size，后续可优化为更大块并在 loader 内拆分成页写。

3.3 兼容策略

- 不做回退：dwcssi 写入统一切换为 pp 写入，删除/移除旧的环回 FIFO 写入路径与对应 loader 产物，避免后续清理工作。


4. Host↔Target 协议定义（pp fifo + mailbox）

4.1 mailbox sections（Host 设置）

- 统一约定：dwcssi 走 pp 写时，始终设置 mailbox sections，至少 1 个 section。
- 单段写入（最常见）建议设置：
  - section_start_addr：dwcssi flash 写入起始 offset（单位：byte offset，供 dwcssi loader 解释）
  - size_blocks：DIV_ROUND_UP(image_size, block_size)
  - buf_offset_blocks：0（当前 pp 传输层不使用该字段，保留供未来扩展）

完成写入后必须清理：
- target_set_mailbox_sections(NULL, NULL, NULL, 0)

4.2 pp fifo 控制块与缓冲区（由 target 层初始化）

- pp fifo 的共享内存布局由 target_async_algorithm_init_ping_pong_fifo 统一初始化，host/loader 不应自行硬编码偏移。
- pp 传输时序（host 侧）：
  - remain_section_blks == 0 时，host 将 mailbox 当前 section 的 start/size 写入共享区，并置位 ctrl_flag；
  - 等待 loader 清除 ctrl_flag（ack）后，开始填充 ping/pong buffer；
  - 写入 buffer 后置位 buf_flag（写入块数），等待 loader 清零表示消费完成；
  - section 结束时，host 等待最后一个 buf_flag 清零，避免段切换覆盖。

4.3 loader 侧协议（dwcssi pp write loader 必须遵循）

必须行为：
- 看到 ctrl_flag==1 时，读取 section_start_ptr/section_size_ptr，并清 ctrl_flag 为 0；
- 当 buf0_flag 或 buf1_flag 为非 0 值时：
  - 将对应半缓冲的 blocks 数据写入 flash（按 dwcssi 页写规则处理）；
  - 完成后将对应 buf_flag 清为 0；
- 看到 buf_flag 为 0xFFFFFFFF（host abort）时，尽快退出。


5. dwcssi pp write loader（Target 端）设计

5.1 ELF 产物命名与选择策略

- 建议新增前缀：
  - flash_async_pp_x1_riscv_64.elf
  - flash_async_pp_x1_riscv_32.elf（如需要）
  - flash_async_pp_x1_aarch_64.elf
- ELF 选择策略沿用 dwcssi_try_load_elf_code_src：
  - 若配置 elf_dir，优先从 elf_dir 加载；
  - 否则使用 contrib/loaders/flash/qspi/dwcssi/build/ 默认目录。

5.2 写入粒度与页边界处理

- 输入数据以 block_size 为单位进入 pp 缓冲；dwcssi loader 内部需要保证 flash 的 program 操作符合页约束：
  - 若 block_size > page_size：loader 内部按页拆分（页对齐、跨页拆写）。
- 重要约束（由现有 pp fifo 共享布局决定）：
  - target_async_algorithm_init_ping_pong_fifo 在 buffer_start 起始区域占用 0x0..0x1BF 作为控制块（buf flags/ctrl/mailbox ptr/buf start ptr 等）。
  - 数据区起始地址当前实现为 buffer_start + block_size，因此必须满足 block_size >= 0x200，避免与控制块重叠。
  - 对 dwcssi（常见 page_size=256），建议 block_size 固定选择 512（0x200），loader 内按 256 页写拆分。
- 尾块对齐（Host 侧必须处理）：
  - pp 传输按“块数”搬运，Host 侧会按 image_block_cnt * block_size 发送数据；
  - 若 image_size 不是 block_size 的整数倍，Host 必须构造一个对齐到 block_size 的 staging buffer 并用 0xFF 填充尾部，否则会发生 host 侧越界读取。

5.3 参数传递（建议）

- 段起始地址与段大小通过 section_start_ptr/section_size_ptr 提供。
- dwcssi 仍需要少量“设备/命令参数”，建议两种策略二选一：
  - 策略 A（推荐）：复用现有 loader 参数页（loader_shmem_map.params），补齐 dwcssi program 需要的 prog_cmd/addr_size/page_size 等字段；
  - 策略 B：全部通过寄存器参数传递（与现有 dwcmshc 类似），但需要在各架构的 loader 入口 ABI 上完全一致。

本方案推荐策略 A，以便与现有 dwcssi loader 写入框架保持一致，并减少寄存器参数个数与架构差异。


6. Host 侧实现落点（dwcssi 驱动）

6.1 新增/调整的写入路径

- 将 dwcssi_write_loader 的写入流程切换为 pp 唯一路径：
  - 加载 flash_async_pp_x1_* ELF；
  - 设置 mailbox sections（单段）；
  - 调用 pp 写入入口（建议封装成 dwcssi_flash_write_pp，内部调用 target_run_async_algorithm_ping_pong）；
  - 写完清理 mailbox。

6.2 mailbox 的生命周期与清理

- mailbox 是全局状态，必须做到每次写入都成对 set/clear。
- 推荐把 set/clear 与 pp 写入调用封装在同一层函数中，避免驱动层遗漏清理导致后续命令异常。


7. 分阶段迁移计划

实施步骤（完全迁移）：
- 新增/替换 dwcssi 的 pp write loader ELF（Target 端），实现 pp 协议消费循环与页写拆分逻辑。
- Host 侧 dwcssi 写入改为 pp 唯一路径：
  - block_size 固定为 512（或更大，但必须 >= 0x200）；
  - 对 image_size 做 block_size 对齐的 staging buffer（尾部 0xFF padding）；
  - 写前设置 mailbox（单段），写后清理 mailbox。
- 删除/移除旧环回 FIFO 写入相关代码路径与旧 loader 产物，统一后续维护面。


9. master_sdk dwcssi loader 改动方案（al_spi_nor_loader.c）

当前 dwcssi loader 位于：
- z:/work/master_sdk/fpsoc/driver/ps_driver/spi-nor/example/al_spi_nor_loader.c

9.1 现状（环回 FIFO）

- Page0 参数区（offset/count/DevId）+ Page1 wp/rp + ring data buffer；
- loader 通过 wait_fifo(wp_ptr, rp_ptr) 等待 host 写入，并按 256B 页写到 flash。

9.2 目标（pp 协议消费端）

- 不再使用 wp/rp ring；
- loader 改为：
  - 等待并 ack ctrl_flag，读取 section_start/section_size；
  - 轮询 buf0_flag/buf1_flag 获取“可消费块数”，消费后清零；
  - 在内部按页拆写 AlSpiNor_Write。

9.3 建议的内存布局（以 LOADER_BUF_START 为基址）

- Page0（0x0000~0x00FF）：保留参数区（256B）
  - +0x0000：offset（uint32_t，flash byte offset，仅用于日志/兼容；实际写入起点以 section_start 为准）
  - +0x0040：count（int，bytes to write，精确字节数）
  - +0x0080：DevId（uint32_t）
- Page1 起（+0x0100）：pp 控制块与 pp buffer
  - pp_base = LOADER_BUF_START + 0x0100
  - +0x000：buf0_flag（uint32_t，0=空闲；>0=该 buffer 内 blocks 有效；0xFFFFFFFF=abort）
  - +0x040：buf1_flag（同上）
  - +0x080：ctrl_flag（uint32_t，1=段信息就绪；loader 读完置 0）
  - +0x0C0：section_start_ptr（uint32_t，flash byte offset）
  - +0x100：section_size_ptr（uint32_t，blocks）
  - +0x140：buf0_start_addr（uint32_t，host 写入，表示 buf0 的数据地址）
  - +0x180：buf1_start_addr（uint32_t，host 写入，表示 buf1 的数据地址）

说明：
- 上述偏移与 OpenOCD 侧 target_async_algorithm_init_ping_pong_fifo 一致；
- 为避免与控制块重叠，OpenOCD 侧必须保证 block_size >= 0x200（建议 512）。

9.4 loader 主循环伪代码（消费 pp 数据并页写）

- 初始化：
  - 从 Page0 读取 count/DevId，初始化 AlSpiNor；
  - 计算 pp_base，并取出各控制字段地址；
- 等待段信息：
  - while (*ctrl_flag == 0)；
  - section_offset = *section_start_ptr；
  - section_blocks  = *section_size_ptr；
  - *ctrl_flag = 0（ack）；
- 写入循环：
  - remaining_bytes = count；
  - prod_idx = 0；
  - while (remaining_bytes > 0)：
    - flag_ptr = (prod_idx==0) ? buf0_flag : buf1_flag；
    - buf_addr_ptr = (prod_idx==0) ? buf0_start_addr : buf1_start_addr；
    - while ((*flag_ptr) == 0)；
    - if (*flag_ptr == 0xFFFFFFFF) 退出；
    - blocks = *flag_ptr；
    - buf = (uint8_t *)(*buf_addr_ptr)；
    - available_bytes = blocks * block_size；
    - to_write = min(remaining_bytes, available_bytes)；
    - 对 [section_offset, section_offset+to_write) 执行页拆分写：
      - page_offset = section_offset & (PAGE_SIZE - 1)
      - 必要时拆成两段写，或循环写满页
    - section_offset += to_write；
    - remaining_bytes -= to_write；
    - *flag_ptr = 0（ack 消费完成）；
    - prod_idx ^= 1；

9.5 Host/loader 侧对齐策略（与 pp 块传输匹配）

- Host 侧：
  - image_block_cnt = DIV_ROUND_UP(image_size, block_size)
  - mailbox 的 size_blocks 使用 image_block_cnt
  - 发送数据前构造 staging buffer（长度 image_block_cnt * block_size，尾部 0xFF padding）
- loader 侧：
  - 以 Page0 的 count 作为“精确写入字节数”，即便 host 发送了 padding，也不对 padding 区域做 program 操作。


8. 验证与回归关注点

- 正确性：
  - 与旧写入路径对比：相同镜像写入后校验（read back 或 checksum）一致；
  - 覆盖不同 offset（页对齐/非对齐）、不同 size（小于/等于/大于 pp 半缓冲）。
- 鲁棒性：
  - 写入中断/abort 时 loader 能退出，host 不死等；
  - mailbox set/clear 成对执行，连续多次写命令无交叉污染。
- 性能：
  - 记录吞吐（KB/s）与失败率，确认 pp 写不引入明显退化。
