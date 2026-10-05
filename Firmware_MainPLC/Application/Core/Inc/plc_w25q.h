/*
 * plc_w25q.h — W25Q128 SPI NOR flash on OCTOSPI1 (single-line SPI commands, indirect mode).
 *
 * 16 MB, 4 KB sectors, 64 KB blocks, 256 B pages. All calls are thread-safe (one mutex)
 * and must be made from RTOS threads after the scheduler started.
 */
#ifndef PLC_W25Q_H
#define PLC_W25Q_H

#include <stdint.h>

#define W25Q_SIZE           (16u * 1024u * 1024u)
#define W25Q_SECTOR_SIZE    4096u
#define W25Q_BLOCK_SIZE     65536u
#define W25Q_PAGE_SIZE      256u

/* Creates the mutex. Call before the scheduler starts. */
void W25q_Setup(void);

/* Reads the JEDEC ID; returns 1 if a 16 MB Winbond-compatible chip answers. */
uint8_t W25q_Probe(void);
uint32_t W25q_JedecId(void);

uint8_t W25q_Read(uint32_t addr, void *buf, uint32_t len);
/* Programs any length; the area must be erased. */
uint8_t W25q_Write(uint32_t addr, const void *buf, uint32_t len);
/* Erases [addr, addr + len), rounded out to 4 KB sectors; uses 64 KB blocks where aligned. */
uint8_t W25q_Erase(uint32_t addr, uint32_t len);

#endif /* PLC_W25Q_H */
