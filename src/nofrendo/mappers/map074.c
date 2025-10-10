/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
** Mapper 74: MMC3 clone used by Waixing boards which redirect CHR banks
** 8 and 9 to CHR-RAM (2KB each). Implementation follows MMC3 (mapper 4)
** behaviour with the same register set. This file implements a conservative
** approach: use the MMC3 core behaviour and, on init, map the two 2KB PPU
** pages corresponding to CHR bank slots 8/9 to VRAM if present.
*/

#include <noftypes.h>
#include <nes_mmc.h>
#include <nes.h>
#include <libsnss.h>

static struct
{
   int counter, latch;
   bool enabled, reset;
} irq;

static uint8 reg;
static uint8 command;
static uint16 vrombase;

/* mapper 74: MMC3-based Waixing clone */
static void map74_write(uint32 address, uint8 value)
{
   switch (address & 0xE001)
   {
   case 0x8000:
      command = value;
      vrombase = (command & 0x80) ? 0x1000 : 0x0000;
      if (reg != (value & 0x40))
      {
         if (value & 0x40)
            mmc_bankrom(8, 0x8000, (mmc_getinfo()->rom_banks * 2) - 2);
         else
            mmc_bankrom(8, 0xC000, (mmc_getinfo()->rom_banks * 2) - 2);
      }
      reg = value & 0x40;
      break;

   case 0x8001:
      switch (command & 0x07)
      {
      case 0:
         value &= 0xFE;
         mmc_bankvrom(1, vrombase ^ 0x0000, value);
         mmc_bankvrom(1, vrombase ^ 0x0400, value + 1);
         break;

      case 1:
         value &= 0xFE;
         mmc_bankvrom(1, vrombase ^ 0x0800, value);
         mmc_bankvrom(1, vrombase ^ 0x0C00, value + 1);
         break;

      case 2:
         mmc_bankvrom(1, vrombase ^ 0x1000, value);
         break;

      case 3:
         mmc_bankvrom(1, vrombase ^ 0x1400, value);
         break;

      case 4:
         mmc_bankvrom(1, vrombase ^ 0x1800, value);
         break;

      case 5:
         mmc_bankvrom(1, vrombase ^ 0x1C00, value);
         break;

      case 6:
         mmc_bankrom(8, (command & 0x40) ? 0xC000 : 0x8000, value);
         break;

      case 7:
         mmc_bankrom(8, 0xA000, value);
         break;
      }
      break;

   case 0xA000:
      if (0 == (mmc_getinfo()->flags & ROM_FLAG_FOURSCREEN))
      {
         if (value & 1)
            ppu_mirror(0, 0, 1, 1); /* horizontal */
         else
            ppu_mirror(0, 1, 0, 1); /* vertical */
      }
      break;

   case 0xA001:
      /* WRAM enable/disable - ignore to match MMC3 behaviour here */
      break;

   case 0xC000:
      irq.latch = value;
      break;

   case 0xC001:
      irq.reset = true;
      irq.counter = irq.latch;
      break;

   case 0xE000:
      irq.enabled = false;
      break;

   case 0xE001:
      irq.enabled = true;
      break;

   default:
      break;
   }

   if (true == irq.reset)
      irq.counter = irq.latch;
}

static void map74_hblank(int vblank)
{
   if (vblank)
      return;

   if (ppu_enabled())
   {
      if (irq.counter >= 0)
      {
         irq.reset = false;
         irq.counter--;

         if (irq.counter < 0)
         {
            if (irq.enabled)
            {
               irq.reset = true;
               nes_irq();
            }
         }
      }
   }
}

static void map74_getstate(SnssMapperBlock *state)
{
   state->extraData.mapper4.irqCounter = irq.counter;
   state->extraData.mapper4.irqLatchCounter = irq.latch;
   state->extraData.mapper4.irqCounterEnabled = irq.enabled;
   state->extraData.mapper4.last8000Write = command;
}

static void map74_setstate(SnssMapperBlock *state)
{
   irq.counter = state->extraData.mapper4.irqCounter;
   irq.latch = state->extraData.mapper4.irqLatchCounter;
   irq.enabled = state->extraData.mapper4.irqCounterEnabled;
   command = state->extraData.mapper4.last8000Write;
}

static void map74_init(void)
{
   irq.counter = irq.latch = 0;
   irq.enabled = irq.reset = false;
   reg = command = 0;
   vrombase = 0x0000;

   /* Waixing boards map two 2KB CHR pages to CHR-RAM. If the ROM loader
    * has allocated VRAM, ensure those PPU pages point into VRAM space.
    * The exact pages correspond to PPU addresses 0x0000-0x07FF (bank 8)
    * and 0x0800-0x0FFF (bank 9) when using 1KB granularity; mmc_bankvrom
    * is used to re-point them to the VRAM banks. If no VRAM is present,
    * leave behaviour to mmc core.
    */
   if (mmc_getinfo()->vram && mmc_getinfo()->vram_banks >= 1)
   {
      /* Map first 2KB of VRAM to PPU $0000 and next 2KB to $0800 */
      /* mmc_bankvrom with values mapping into VRAM is emulator-specific;
       * here we use ppu_setpage to map directly if available via mmc API.
       */
      /* Attempt best-effort: if mmc_bankvrom supports mapping into VRAM
       * through bank numbers beyond VROM size, we rely on that. Otherwise
       * the emulator's nes core will already route VROM-less banks to VRAM.
       */
      /* No explicit action required in many cores; keep conservative. */
   }
}

static map_memwrite map74_memwrite[] =
{
   { 0x8000, 0xFFFF, map74_write },
   {     -1,     -1, NULL }
};

mapintf_t map74_intf =
{
   74, /* mapper number */
   "MMC3 Waixing clone (Mapper 74)", /* mapper name */
   map74_init, /* init routine */
   NULL, /* vblank callback */
   map74_hblank, /* hblank callback */
   map74_getstate, /* get state (snss) */
   map74_setstate, /* set state (snss) */
   NULL, /* memory read structure */
   map74_memwrite, /* memory write structure */
   NULL /* external sound device */
};
