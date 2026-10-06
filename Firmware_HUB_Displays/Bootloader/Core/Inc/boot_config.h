/*
 * boot_config.h — HUB of displays bootloader configuration for Shared_Libs/boot_core/stm32g0.
 * Flash layout must match both linker scripts (Bootloader and Application).
 */
#ifndef BOOT_CONFIG_H
#define BOOT_CONFIG_H

#include "fwupd/fwupd_proto.h"

#define BOOT_MODULE_TYPE   FWUPD_TYPE_HUB_DISPLAYS
#define BOOT_BOARD_REV     1u
#define BOOT_VERSION       1u

#define BOOT_APP_START     0x08004000u
#define BOOT_APP_END       0x0801F800u
#define BOOT_META_ADDR     0x0801F800u

#endif /* BOOT_CONFIG_H */
