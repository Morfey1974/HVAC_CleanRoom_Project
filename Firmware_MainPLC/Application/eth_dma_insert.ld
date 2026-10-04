/*
 * Second linker script, inserted into STM32H723ZGTX_FLASH_MMT_TEMPLATE.ld
 * (Debug: Linker > Miscellaneous > Other flags: -Wl,-T,../eth_dma_insert.ld).
 * Kept separate because CubeMX regenerates the MMT template on every Generate Code.
 *
 * ETH DMA descriptors and the RX pool (sections named in LWIP/Target/ethernetif.c)
 * go to the start of D2 SRAM (non-cacheable by MPU region 3). Fixed address instead
 * of the MMT region name: this script is parsed before the MEMORY block.
 * The lwIP heap (LWIP_RAM_HEAP_POINTER in lwipopts.h) starts at 0x30005000.
 */
SECTIONS
{
  .eth_dma 0x30000000 (NOLOAD) :
  {
    . = ALIGN(32);
    *(.RxDescripSection)
    *(.RxDecripSection)
    . = ALIGN(32);
    *(.TxDescripSection)
    *(.TxDecripSection)
    . = ALIGN(32);
    *(.Rx_PoolSection)
    . = ALIGN(32);
    _eth_dma_end = .;
  }
}
INSERT AFTER .bss;

ASSERT(_eth_dma_end <= 0x30005000, "ETH DMA area overlaps lwIP heap at 0x30005000")
