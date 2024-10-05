//++
//debug.c - 8051 serial port debugging I/O
//
// Copyright (C) 2006-2024 by Spare Time Gizmos.  All rights reserved.
//
// This file is part of the Spare Time Gizmos' VT1802 and VIS1802 firmware.
//
// This firmware is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc., 59 Temple
// Place, Suite 330, Boston, MA  02111-1307  USA.
//
// WARNING:
//   The serial port uses timer 1 for baud rate generation!
//
//REVISION HISTORY:
// dd-mmm-yy    who     description
//  4-Feb-06    RLA     New file.
// 12-May-24	RLA	Add putchar() and getchar() to this file.
//			Add USE_SMOD to control setting the SMOD bit.
//			putchar() should add a <CR> to every <LF>.
//--

// Include files...
#include <stdio.h>		// needed so DBGOUT(()) can find printf!
#include <stdint.h>		// uint8_t, et al ...
#include "at89x051.h"		// register definitions for the AT89C2051
#include "ps2apu.h"		// declarations for this project
#include "debug.h"		// declarations for this module


#ifdef DEBUG
PUBLIC void InitializeDebugSerial (void)
{
  //++
  //   This routine will initialize the 8051's internal UART.  This interface
  // is used only for debugging purposes, so the baud rate is fixed.  Timer 1 is
  // used to generate the baud rate clock, and interrupts are _not_ enabled for
  // the UART!
  //--
  SCON = 0x52;			// select mode 1 - 8 bit UART, set REN, TI
  TMOD = (TMOD & 0x0F) | 0x20;	// timer 1 mode 2 - 8 bit auto reload
  TH1 = T1RELOAD;		// set the divisor for the baud rate
#if (USE_SMOD == 0)
  PCON &= 0x7F;			// use SMOD=0 for the baud rate
#else
  PCON |= 0x80;			// use SMOD=1 for the baud rate
#endif
  TR1 = 1;			// and start the timer running
  TI = 1;			// enable the transmitter
//REN = 1;			// enable the receiver
}


PUBLIC int putchar (int c)
{
  //++
  //   This routine outputs a character to the terminal and it's called by
  // SDCC's library printf_tiny() ...
  //
  //   Note that if the current character being output is a newline, then
  // we want to automatically output a carriage return first.  You might
  // think we could do this just be recursively calling putchar(), but
  // remember that MCS51 functions in SDCC are not automatically reentrant.
  // We could make putchar() reentrant, but that has some other consequences
  // that I don't want to deal with.  Easier to just brute force it!
  //--
  if (LOBYTE(c) == '\n') {
    // Output a carriage return first ...
    while (!TI) ;
    SBUF = '\r';  TI = 0;
  }

  // Now output the original character ...
  while (!TI) ;
  SBUF = c;  TI = 0;
  return c;
}


PUBLIC int getkey (void)
{
  //++
  //   This function waits for a character to be received on the 8051's serial
  // port.  The character received is returned as the function's value.
  //--
  while (!RI) ;
  char c = SBUF;
  RI = 0;
  return c;
}
#endif	// #ifdef DEBUG ...
