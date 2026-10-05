/*
 * boot_core.h — HVAC CAN bootloader core for STM32G0 (protocol: fwupd/fwupd_proto.h).
 *
 * The module bootloader project provides boot_config.h with:
 *   BOOT_MODULE_TYPE   FWUPD_TYPE_*
 *   BOOT_BOARD_REV     board revision accepted from the image header
 *   BOOT_VERSION       bootloader version, 1..255
 *   BOOT_APP_START     application start address (vector table), page aligned
 *   BOOT_APP_END       first address after the application area, page aligned
 *   BOOT_META_ADDR     boot metadata page (outside the application area)
 * and links the shared RAM block _fwupd_shared (see its linker script).
 *
 * The core runs with the FDCAN already initialised by CubeMX (not started) and never returns:
 * it either starts the application or serves update commands.
 */
#ifndef BOOT_CORE_H
#define BOOT_CORE_H

#include "main.h"

void Boot_Run(FDCAN_HandleTypeDef *hfdcan);

#endif /* BOOT_CORE_H */
