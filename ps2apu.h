//++
//gpio.h
//
// Copyright (C) 2005-2024 by Spare Time Gizmos.  All rights reserved.
//
// This file is part of the Spare Time Gizmos'??????? firmware.
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
// DESCRIPTION
//   This file contans mostly hardware related defintions (e.g. I/O ports and
// bits, clock speeds, memory sizes, etc) for the PS2 keyboard to parallel project.
//
// REVISION HISTORY:
// dd-mmm-yy    who     description
//  4-Feb-06    RLA     New file.
// 11-May-24	RLA	Add CPUCLOCK and STROBE_ACT_LVL.
//			Make ROMSIZE and checksum optional.
//--
#pragma once

// System configuration parameters...
#ifndef VERSION
#define VERSION		4		// version number of this firmware
#endif
#ifndef CPUCLOCK
#define CPUCLOCK	11059200	// CPU clock frequency (in Hz!)
#endif
#ifndef STROBE_ACT_LVL
#define STROBE_ACT_LVL  1		// data ready strobe active level
#endif

// Status LED ...
#define LED_BIT		P3_5
#define LED_ON	{LED_BIT = 0;}
#define LED_OFF	{LED_BIT = 1;}

// External options jumpers...
//   In the non-DEBUG version, P3.0 can be used to connect an external
// jumper to control the SWAP CAPSLOCK and CONTROL feature.  In the DEBUG
// version P3.1 and P3.0 are TXD and RXD for the serial port, and can't be
// used for jumpers.
//
//   In the DEBUG version you can define (with -D... on the SDCC command line)
// the SWAP_CAPSLOCK_AND_CONTROL symbol to get any default behavior you want.
// In the non-DEBUG version these definitions will be ignored and only the
// jumper settings matters.
#ifndef DEBUG
// Ignore any command line definitions in the non-DEBUG version ...
#undef SWAP_CAPSLOCK_AND_CONTROL
// JP4 - caps lock/control mode (active low!)
#define SWAP_CAPSLOCK_AND_CONTROL	(P3_0 == 0)
#else
// Set the defaults for the DEBUG version (you can override with -D!) ...
#ifndef SWAP_CAPSLOCK_AND_CONTROL
#define SWAP_CAPSLOCK_AND_CONTROL false
#endif
#endif

// Handshaking flags...
#define SET_KEY_DATA_RDY P3_4		// set the KEY_DATA_RDY flip-flop
#define KEY_DATA_RDY	 P3_3		// byte is waiting for the host

//   These macros are used mostly for documentation purposes to indicate that
// a variable of a function is to be visible within the current module only
// (PRIVATE) or to other modules as well (PUBLIC)...
#define PRIVATE	static
#define PUBLIC

// Assemble and disassemble words and longwords...
#define LOBYTE(x) 	((uint8_t)  ((x) & 0xFF))
#define HIBYTE(x) 	((uint8_t)  (((x) >> 8) & 0xFF))
#define MKWORD(h,l)	((uint16_t) ((((h) & 0xFF) << 8) | ((l) & 0xFF)))

// Turn the interrupt system on and off...
#define INT_ON		EA = 1
#define INT_OFF		EA = 0

// Stop execution ...
//  (well, what else can we do in an embedded system??)
#define HALT {INT_OFF;  while (1) ;}

// Public variables in the main module...
extern char const __code g_szFirmware[];
extern char const __code g_szCopyright[];
#ifdef ROMSIZE
#define g_wROMChecksum (*((PCWORD) (ROMSIZE-2)))
#endif
