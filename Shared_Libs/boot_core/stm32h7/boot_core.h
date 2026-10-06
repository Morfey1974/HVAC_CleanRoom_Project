/*
 * boot_core.h - HVAC CAN bootloader core for STM32H7 single-bank parts (protocol: fwupd/fwupd_proto.h).
 *
 * The module bootloader project provides boot_config.h with:
 *   BOOT_MODULE_TYPE   FWUPD_TYPE_*
 *   BOOT_BOARD_REV     board revision accepted from the image header
 *   BOOT_VERSION       bootloader version, 1..255
 *   BOOT_APP_START     application start address, sector aligned
 *   BOOT_APP_END       first address after the application area, sector aligned
 *   BOOT_META_ADDR     boot metadata sector (outside the application area)
 *   BOOT_APP_VTOR      vector table of the application (BOOT_APP_START + 0x400)
 * and links the shared RAM block _fwupd_shared (see its linker script).
 *
 * Application image layout (the H7 vector table is longer than FWUPD_HEADER_OFFSET):
 *   +0x000  initial SP and reset handler (section .boot_vec)
 *   +0x100  FwupdHeader (section .fw_header)
 *   +0x400  vector table (section .isr_vector), VTOR is set here before the jump
 *
 * Flash is programmed in 32-byte flash words; the first flash word of the image is written last.
 * The core runs with the FDCAN already initialised (not started) and never returns.
 */
#ifndef BOOT_CORE_H
#define BOOT_CORE_H

#include "main.h"

void Boot_Run(FDCAN_HandleTypeDef *hfdcan);

#endif /* BOOT_CORE_H */
