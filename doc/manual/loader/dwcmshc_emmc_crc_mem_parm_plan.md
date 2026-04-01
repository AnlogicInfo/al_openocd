dwcmshc eMMC CRC 校验统一到 loader_flash_crc_mem_parm 的实现计划

1. 目标

- eMMC(dwcmschc) 的校验路径从 `loader_flash_crc` 切换为 `loader_flash_crc_mem_parm`。
- 与 QSPI(dwcssi) 的校验路径保持一致（同一 Host 侧入口、同一“共享内存参数页”传参模型）。
- 明确并固化 CRC Loader 的 Host↔Target ABI，避免依赖“寄存器残留值”的隐式行为。

相关代码位置：
- [dwcmshc_subs.c](file:///d:/work/projs/al_openocd/src/flash/emmc/dwcmshc_subs.c#L939-L971): dwcmshc_checksum（当前使用 loader_flash_crc）
- [dwcssi.c](file:///d:/work/projs/al_openocd/src/flash/nor/dwcssi/dwcssi.c#L1429-L1456): dwcssi_checksum_x1（当前使用 loader_flash_crc_mem_parm）
- [loader_io.c](file:///d:/work/projs/al_openocd/src/flash/loader_io.c#L589-L670): loader_flash_crc_mem_parm / loader_flash_crc
- [loader_io.h](file:///d:/work/projs/al_openocd/src/flash/loader_io.h#L123-L130): 接口声明与 loader_shmem_map
- eMMC CRC Loader（计划采用的基线实现）： [al_mmc_sdma_loader_crc.c](file:///z:/work/master_sdk/fpsoc/driver/ps_driver/mmc/example/al_mmc_sdma_loader_crc.c)
- QSPI CRC Loader： [al_spi_nor_loader_crc.c](file:///z:/work/master_sdk/fpsoc/driver/ps_driver/spi-nor/example/al_spi_nor_loader_crc.c)


2. 现状与问题

2.1 Host 侧差异

- eMMC：`dwcmshc_checksum()` 走 `loader_flash_crc()`，参数通过寄存器传递（ctrl_base / block_size / image_size / addr）。
- QSPI：`dwcssi_checksum_x1()` 走 `loader_flash_crc_mem_parm()`，参数通过 `loader_shmem_map.params` 传递（flash_addr / image_size / bank_id）。

2.2 目标侧（Loader-ELF）潜在不一致

- `loader_flash_crc_mem_parm()` 当前只设置 1 个寄存器参数用于回传 CRC（a0/x0），并通过 `loader_set_mem_parms()` 写共享内存参数页。
- OpenOCD 现有 eMMC CRC loader（历史实现）以寄存器入参模型为主（例如 `emmc_dwcmshc(ctrl_base, block_size, count, block_addr)`），若 Host 切换到 mem_parm 而 loader 仍读取寄存器入参，会出现“依赖寄存器残留值”的风险（不可控、不可复现、跨架构不稳定）。

补充：eMMC 侧计划采用 master_sdk 的 [al_mmc_sdma_loader_crc.c](file:///z:/work/master_sdk/fpsoc/driver/ps_driver/mmc/example/al_mmc_sdma_loader_crc.c) 作为 loader 基线实现，其输入参数同样从共享内存 Page0 读取（`buf_start[0]`/`buf_start[16]`/`buf_start[32]` 对应 +0x00/+0x40/+0x80），并在完成后触发断点（riscv: ebreak；aarch64: hlt）返回。

补充：QSPI 侧 loader 基线实现来自 master_sdk 的 [al_spi_nor_loader_crc.c](file:///z:/work/master_sdk/fpsoc/driver/ps_driver/spi-nor/example/al_spi_nor_loader_crc.c)，其输入参数从共享内存 Page0 读取（`buf_start[0]`/`buf_start[16]`/`buf_start[32]` 对应 +0x00/+0x40/+0x80），并在完成后触发断点（riscv: ebreak；aarch64: hlt）返回。


3. 统一方案（推荐）

3.1 统一 Host 侧入口

- eMMC checksum 统一调用：
  - `loader_flash_crc_mem_parm(loader, local_srcs, block_addr, driver_priv->io_location, crc);`
- 保持 eMMC 地址语义不变：
  - `addr` 仍传 block address（`addr / block_size`），与现有写入路径一致。
- 保持共享内存参数页语义（与 write pp 同一套）：
  - params.flash_addr = block_addr（单位：block）
  - params.image_size = count（单位：byte）
  - params.bank_id    = io_location（用于选择 MMC 实例/ctrl_base）

3.2 统一 Target 侧 CRC Loader ABI

定义一个明确的 ABI（适用于 eMMC 与 QSPI 两类 CRC Loader）：

- 共享内存参数页（由 Host 写入）：
  - base = “CRC loader 可访问的 shared memory base”
  - flash_addr @ +0x00（写入起始地址，单位由 loader 定义：eMMC=block，QSPI=byte）
  - image_size @ +0x40（精确字节数）
  - bank_id    @ +0x80（设备/实例编号）
- 寄存器返回值：
  - a0/x0：CRC 结果（与现有 loader_flash_crc_mem_parm 回读一致）

关键点：CRC Loader 必须只依赖共享内存参数页获取输入参数，不依赖寄存器残留输入。


4. 实施步骤（分阶段）

4.1 第 1 阶段：Host 侧切换调用点（eMMC）

改动点：[dwcmshc_checksum](file:///d:/work/projs/al_openocd/src/flash/emmc/dwcmshc_subs.c#L939-L971)

- 将
  - `retval = loader_flash_crc(loader, local_srcs, block_addr, crc);`
  替换为
  - `retval = loader_flash_crc_mem_parm(loader, local_srcs, block_addr, driver_priv->io_location, crc);`
- 保留并确保以下字段正确：
  - `loader->work_mode = CRC_CHECK`
  - `loader->block_size = emmc->device->block_size`
  - `loader->image_size = count`
- 处理/清理无效设置（可选，但建议在第 3 阶段做一致性整理）：
  - `loader->param_cnt` 在 mem_parm 路径中会被覆盖为 1，调用点不应再硬编码 4。

4.2 第 2 阶段：Target 侧 eMMC CRC Loader 改造

改动点：
- 以 master_sdk 的 [al_mmc_sdma_loader_crc.c](file:///z:/work/master_sdk/fpsoc/driver/ps_driver/mmc/example/al_mmc_sdma_loader_crc.c) 为基线修改/维护 eMMC CRC loader，并在 master_sdk 目录下完成编译产出 ELF（riscv_64/aarch_64 均需覆盖）。
- 将生成的 ELF 复制到 OpenOCD 的 contrib 目录，供 `dwcmshc_try_load_elf_code_src()` 默认路径加载：
  - contrib/loaders/flash/emmc/dwcmshc/build/emmc_crc_{riscv_64,aarch_64}.elf

目标行为：
- CRC Loader 启动后，从共享内存参数页读取：
  - flash_addr / image_size / bank_id
- 使用 bank_id 作为 device_id 传入 `AlMmc_Hal_Init()`（与 master_sdk loader 基线一致），不再依赖寄存器传入 ctrl_base。
- 计算 CRC 并通过 a0/x0 返回。

实现要点（建议）：
- 引入与 Host 侧一致的 `loader_shmem_map` 结构（最小子集即可）：
  - 只需要 params 区的 3 个字段和它们的偏移。
- bank_id/device_id 语义对齐：
  - Host 侧传入 `loader_flash_crc_mem_parm(..., bank_number, ...)` 的 bank_number 需要与 `AlMmc_Hal_Init(&Handle, device_id, ...)` 的 device_id 语义一致。
  - 建议：直接使用 `driver_priv->io_location` 作为 bank_id/device_id（前提是该字段在平台上确实就是 MMC 的 device_id/instance id）。

4.3 第 3 阶段：统一性清理（dwcssi checksum 与 loader_io）

原因：当前 dwcssi checksum 调用点存在“设置 param_cnt/set_params_priv 但实际不生效”的代码味道（mem_parm 会覆盖 param_cnt 并且不会调用 set_params_priv）。

建议清理点：
- [dwcssi_checksum_x1](file:///d:/work/projs/al_openocd/src/flash/nor/dwcssi/dwcssi.c#L1429-L1456)
  - 移除对 `loader->param_cnt = 6` 与 `loader->set_params_priv = ...` 的依赖性设置，避免误导维护者。
- [loader_flash_crc_mem_parm](file:///d:/work/projs/al_openocd/src/flash/loader_io.c#L589-L626)
  - 明确 ABI：仅 a0/x0 作为 CRC 返回值；输入参数全部来自共享内存参数页。
  - 若后续需要为 CRC loader 额外提供参数，优先扩展共享内存参数页而不是扩展寄存器入参；对 QSPI master_sdk loader 基线（[al_spi_nor_loader_crc.c](file:///z:/work/master_sdk/fpsoc/driver/ps_driver/spi-nor/example/al_spi_nor_loader_crc.c)）而言，读取路径走 AlSpiNor_Read，通常不需要额外的 read_cmd/addr_size 入参。


5. 风险点与规避

- Shared memory base 可见性：
  - CRC Loader 必须能够可靠定位到共享内存参数页（与 write pp / read pp 使用同一片共享内存）。
  - master_sdk 的 eMMC/QSPI loader 示例使用固定的 `LOADER_BUF_START`（例如 0x6102D000）。而 OpenOCD 的 `loader_set_mem_parms()` 写入基址来自 `exec_target->loader_buf_start`。
  - 必须保证两者一致：要么让 loader 工程使用与 OpenOCD 一致的 `LOADER_BUF_START`（编译期常量），要么在 OpenOCD target 配置中把 `loader_buf_start` 配到与 loader 常量一致，否则会出现“Host 写入参数页与 loader 读取参数页不在同一地址”的隐蔽错误。
- bank_id 映射：
  - 若 eMMC loader 改为 `AlMmc_Hal_Init(device_id=bank_id)` 路径，则 Host 的 `io_location` 必须与 HAL 的 device_id 编号一致，否则会初始化到错误实例导致 CRC 错误或超时。
- image_size 的精确性：
  - CRC 按 byte 计算；读取 eMMC 时仍以 block 为粒度，但最后一块仅按 image_size 的剩余字节计入 CRC（与现有实现保持一致）。


6. 验证用例（回归清单）

- 基础功能：
  - eMMC：dwcmshc_checksum 返回值与 `image_calculate_checksum()` 对比一致（注意 ~crc 对比规则是否沿用现有 verify 逻辑）。
  - QSPI：dwcssi_checksum_x1 行为不变。
- 边界覆盖：
  - count 非 block_size 整数倍（例如 1、511、513、4097）
  - 不同 bank_id/io_location（MMC0/MMC1）
  - 连续多次 checksum 调用（验证不依赖寄存器残留）
- 异常路径：
  - target 超时/中止（loader 主动返回异常值或 host 超时）时，Host 能正确报错并退出。
