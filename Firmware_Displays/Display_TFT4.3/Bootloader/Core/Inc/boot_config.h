/*
 * boot_config.h - Display TFT 4.3 bootloader configuration for Shared_Libs/boot_core/stm32h7.
 * Flash layout must match both linker scripts (Bootloader and Application):
 *   0x08000000..0x0801FFFF  bootloader (sector 0)
 *   0x08020000..0x080DFFFF  application (sectors 1..6, 768K)
 *   0x080E0000..0x080FFFFF  boot metadata (sector 7)
 */
#ifndef BOOT_CONFIG_H
#define BOOT_CONFIG_H

#include "fwupd/fwupd_proto.h"

#define BOOT_MODULE_TYPE   FWUPD_TYPE_DISPLAY_TFT43
#define BOOT_BOARD_REV     1u
#define BOOT_VERSION       1u

#define BOOT_APP_START     0x08020000u
#define BOOT_APP_END       0x080E0000u
#define BOOT_META_ADDR     0x080E0000u
#define BOOT_APP_VTOR      (BOOT_APP_START + 0x400u)

#endif /* BOOT_CONFIG_H */
