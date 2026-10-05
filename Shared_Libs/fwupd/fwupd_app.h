/*
 * fwupd_app.h — firmware update support inside a module application (roll call, reboot into bootloader).
 *
 * Usage in the application:
 *   FWUPD_APP_HEADER(FWUPD_TYPE_xxx, board_rev, version);   // once, at file scope
 *   FwupdApp_Init(send_fn, FWUPD_TYPE_xxx, board_rev, version);
 *   FwupdApp_OnRx(...) from the FDCAN RX callback for frames of that bus;
 *   FwupdApp_Poll() from the main loop.
 *
 * The application project compiles fwupd_app.c (one-line wrapper in Core/Src) and its linker script must:
 *   - start FLASH at the application start of the chip layout;
 *   - place KEEP(*(.fw_header)) at FWUPD_HEADER_OFFSET inside .isr_vector;
 *   - define _fw_image_end and _fwupd_shared (see Firmware_Locomotive/Application linker script).
 */
#ifndef FWUPD_APP_H
#define FWUPD_APP_H

#include <stdint.h>
#include "main.h"
#include "fwupd_proto.h"

/* Queues one extended-ID frame on the update bus; must be safe to call from the main loop. */
typedef void (*FwupdAppSendFn)(uint32_t ext_id, const uint8_t data[8]);

extern uint32_t _fw_image_end;

#define FWUPD_APP_HEADER(type, board, version)                                  \
  __attribute__((section(".fw_header"), used)) const FwupdHeader g_fw_header = { \
    .magic = FWUPD_HEADER_MAGIC, .header_ver = 1u, .module_type = (type),       \
    .board_rev = (board), .fw_version = (version),                              \
    .image_end = (uint32_t)&_fw_image_end }

void FwupdApp_Init(FwupdAppSendFn send, uint8_t module_type, uint8_t board_rev, uint16_t version);

/* Call for every received frame of the update bus (interrupt context is fine). */
void FwupdApp_OnRx(uint32_t id, uint8_t is_ext, const uint8_t *data, uint32_t len);

/* Periodic roll call, start confirmation, reboot into the bootloader on CONNECT. */
void FwupdApp_Poll(void);

#endif /* FWUPD_APP_H */
