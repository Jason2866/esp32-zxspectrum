/*=====================================================================
  z80.c -> Header file related to the Z80 emulation code.

   Please read documentation files to know how this works :)

   Thanks to Marat Fayzullin for writing the "How to write a Computer
   Eemulator" HOWTO. This emulator is based on his tutorials and the
   code organization (very readable!) of his "Z80 Portable Emulator".
   I've learnt a lot from it, and I've taken some ideas of his code
   to write this emulator.I think that almost all of the undocumented
   Z80 opcodes are covered on this emulator. I also asked Marat
   Fayzullin (by email) about ideas and so on (his Z80 emulator is
   quite good, so go check it :-).

   Of course, I can't forget Raúl Gomez (he answered me thousands
   of email questions) and Philip Kendall. Whitout his ___kind___
   people surely you won't be reading this file now...

   "Programming the Z80" (from Rodnay Zaks) and the comp.sys.sinclair
   FAQ were another excelent sources of info!

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 Copyright (c) 2000 Santiago Romero Iglesias.
 Email: sromero@escomposlinux.org
 ======================================================================*/  
#ifndef Z80_H
#define Z80_H
  
//#define DEBUG
//#define _DEV_DEBUG_                         /* development debugging */
#define LOW_ENDIAN
/*#define HI_ENDIAN */ 
  

/*=== Some common standard data types: ==============================*/ 
typedef uint8_t  byte;
//typedef uint16_t word; // arduino allready define it
typedef uint32_t dword;
typedef int8_t   offset;

/*--- Thanks to Philip Kendall for it's help using the flags --------*/ 
extern byte halfcarry_add_table[];
extern byte halfcarry_sub_table[];
extern byte overflow_add_table[];
extern byte overflow_sub_table[];
extern byte sz53_table[];
extern byte sz53p_table[];
extern byte parity_table[];
extern byte ioblock_inc1_table[];
extern byte ioblock_dec1_table[];
extern byte ioblock_2_table[];

/*=====================================================================
   Z80 Flag Register:       ---------------------------------
                            | 7   6   5   4   3   2   1   0 |
                            ---------------------------------
                            | S   Z   x   H   x  O/P  N   C |
                            ---------------------------------
   If (1) means that:           S   =  Negative result.
                                Z   =  Zero result.
                                x   =  special cases (by opcode)
                                H   =  Halfcarry/borrow.
                                O/P =  Overflow/Parity Flag.
                                N   =  Substraction.
                                C   =  Carry/borrow.
 ====================================================================*/ 
#define  S_FLAG    0x80
#define  Z_FLAG    0x40
#define  H_FLAG    0x10
#define  P_FLAG    0x04
#define  O_FLAG    0x04
#define  N_FLAG    0x02
#define  C_FLAG    0x01
  
/*
 Defines for interrupts and special Z80Hardware() codes:
=======================================================================
 INT_QUIT   =    Exit the emulation (for Z80Run())
 INT_NOINT  =    No interrupt required
 INT_IRQ    =    Standard RST 38h interrupt
 INT_NMI    =    Non-maskerable interrupt
*/ 
#define  INT_QUIT   0xFFFE
#define  INT_NOINT  0xFFFF
#define  INT_IRQ    0x0038
#define  INT_NMI    0x0066
  

/*=== A register is defined as follows: =============================*/ 
#ifdef LOW_ENDIAN
typedef union {
  struct { byte l, h; } B;
  uint16_t W;
} eword;
#else
typedef union {
  struct { byte h, l; } B;
  uint16_t W;
} eword;
#endif //ifdef LOW_ENDIAN

#define  WE_ARE_ON_DD  1
#define  WE_ARE_ON_FD  2
  
/*=== Now we define the Z80 registers using the previous definition =*/ 
typedef struct {
  // use this to stash a pointer to our own data
  void *userInfo;
  /* general and shadow z80 registers */ 
  eword AF,  BC,  DE,  HL, IX, IY, PC, SP, R; 
  eword AFs, BCs, DEs, HLs;
  /* IFF and I registers, used on interrupts. */ 
  byte IFF1, IFF2, I, halted;
  char IM;
  uint16_t IRequest;
  int we_are_on_ddfd;
  /* the following is to take care of cycle counting */ 
  int cycles;
  /* DecodingErrors = set this to 1 for debugging purposes in order
   *    to trap undocumented or non implemented opcodes.
   *    Trace          = set this to 1 to start tracing. It's also set
   *   when PC reaches TrapAddress value.             
   */ 
   byte DecodingErrors;
//   word TrapAddress;
//   byte Trace, 
     byte dobreak;
//   byte BorderColor;
} Z80Regs;

/*====================================================================
   Function declarations, read the .c file to know what they do.
 ===================================================================*/ 
void     Z80Reset (Z80Regs * regs);
void     Z80Interrupt (Z80Regs *, uint16_t);
uint16_t Z80Run (Z80Regs *, int);
void     Z80Patch (Z80Regs *);
byte     Z80Debug (Z80Regs *);
void     Z80FlagTables (void);
uint16_t ParseOpcode (char *, char *, char *, uint16_t, Z80Regs *);
uint16_t Z80Dissasembler (Z80Regs *, char *, char *);

#endif  // #ifdef Z80_H
