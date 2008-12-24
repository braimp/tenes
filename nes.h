#ifndef NES_H
#define NES_H

#include <assert.h>
#include "M6502/M6502.h"
#include "global.h"
#include "rom.h"

/***  PPU registers  ***/

#define ppu_cr1         0x2000
#define ppu_cr2         0x2001
#define ppu_status      0x2002
#define ppu_bgscroll    0x2005   /* double write */
#define ppu_addr        0x2006   /* double write */
#define ppu_data        0x2007   /* RW */


/** Sprite registers **/

#define spr_addr   0x2003
#define spr_data   0x2004        /* RW */
#define spr_dma    0x4014

/**  Control pad registers  **/ 

#define joypad_1   0x4016        /* RW */
#define joypad_2   0x4017        /* RW */

/***  Sound registers (pAPU) ***/

#define snd_p1c1  0x4000 /* Pulse channel 1 */
#define snd_p1c2  0x4001
#define snd_p1f1  0x4002
#define snd_p1f2  0x4003

#define snd_p2c1  0x4004 /* Pulse channel 2 */
#define snd_p2c2  0x4005
#define snd_p2f1  0x4006
#define snd_p2f2  0x4007

#define snd_tric1 0x4008 /* Triangle channel */
#define snd_tric2 0x4009
#define snd_trif1 0x400A
#define snd_trif2 0x400B

#define snd_nc1   0x400C /* Noise channel */
#define snd_nc2   0x400D
#define snd_nf1   0x400E
#define snd_nf2   0x400F

#define snd_dmc_control  0x4010 /* DMC (PCM) channel */
#define snd_dmc_volume   0x4011
#define snd_dmc_address  0x4012
#define snd_dmc_datalen  0x4013

#define snd_status 0x4015 /* status/control register */


/* Hardware abstractions */

#define PPUWRITE_HORIZ	1   /* VRAM write mode - horizontal or vertical */
#define PPUWRITE_VERT	32

#define DUAL_HIGH	0       /* dual write register mode */
#define DUAL_LOW	1

typedef int dualmode;

struct ppu_struct
{
    byte vram[0x4000];
    byte spriteram[0x100];
    byte sprite_address;	
    
    word v, t;
    byte x;

    dualmode ppu_addr_mode;
    int ppu_writemode; /* horizontal or vertical write pattern */
    
    byte read_latch;
    
    byte control1,control2;
    
    int vblank_flag;
    int hit_flag;
    int spritecount_flag; /* more than 8 sprites on current scanline ? */
   
};

struct sound_struct
{
   byte regs[0x17]; /* from 0x4000-0x4015, ignoring 0x4014 which isn't sound */
};

struct joypad_info
{
   int pad[4][8];
   int connected;
   int state[2];
};


struct nes_machine
{
  struct nes_rom rom;
  M6502 cpu;
  void *mapper_data;  /* private to code for current mapper */
  struct ppu_struct ppu;
  struct sound_struct snd;
  byte ram[0x800];  /* first 8 pages, mirrored 4 times to fill to 0x1FFF */
  int scanline;
  
  struct mapper_functions *mapper;
  struct joypad_info joypad;
};

#ifndef global_c
extern
#endif
struct nes_machine nes;

struct scanline_info
{
    byte control1, control2, x;
    word v, t;
};

#define LOG_LENGTH 256
extern struct scanline_info lineinfo[LOG_LENGTH];

void init_nes(struct nes_machine *nes);
void shutdown_nes(struct nes_machine *nes);
void reset_nes(struct nes_machine *nes);
void nes_runframe(void);

static inline word ppu_mirrored_nt_addr (word paddr)
{
    //word paddr_hbit = (paddr & 0x400) >> 10;
    word paddr_vbit = (paddr & 0x800) >> 11;    
    
    switch (nes.rom.mirror_mode) {

    case MIRROR_HORIZ: 
        return (paddr & 0xF3FF) | (paddr_vbit << 10);

    case MIRROR_VERT: 
        return paddr & 0xF7FF;

    case MIRROR_NONE:                      
        return paddr;

    case MIRROR_ONESCREEN:
        /* FIXME: Totally broken. Should mask out 0xC00
         * and OR a value from the mapper. */
        return paddr;

    default:
        assert(0);
        return 0x2000;
    }        
}

static inline word ppu_mirrored_addr (word paddr)
{
    paddr &= 0x3FFF;
    if ((paddr >= 0x2000) && (paddr < 0x3000)) return ppu_mirrored_nt_addr(paddr);
    else if (paddr >= 0x3F00) {
        paddr &= 0x3F1F;
        if (paddr == 0x3F10) return 0x3F00;
        else return paddr;
    } else return paddr;
}

#endif
