//++
//ps2apu.c - 89C2051 source for PS/2 Keyboard Interface ...
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
//REVISION HISTORY:
// dd-mmm-yy    who     description
//  4-Feb-06    RLA     New file.
// 11-May-24	RLA	Make ROMSIZE and checksum optional.
//			Update copyright.
//--
#include <stdio.h>		// needed so DBGOUT(()) can find printf!
#include <stdint.h>		// uint8_t, et al ...
#include <stdbool.h>		// bool, true, false ...
#include "at89x051.h"		// register definitions for the AT89C2051
#include "ps2apu.h"		// definitions for this project
#include "debug.h"		// debuging (serial port output) routines
#include "keyboard.h"		// low level keyboard serial I/O functions
#include "scancode.h"		// PS2 scan codes to ASCII translation table
#include "host.h"		// convert scan codes to ASCII and send to host


//   This is the copyright notice, version, and date for the software in plain
// ASCII.  Even though this only gets printed out in the debug version, it's
// always included to identify the ROM's contents...
PUBLIC char const __code g_szFirmware[] =
  "PS2 Keyboard Interface " __DATE__ " " __TIME__;
PUBLIC char const __code g_szCopyright[] =
  "Copyright (C) 2006-2024 by Spare Time Gizmos. All rights reserved.";


//++
// System initialization and startup...
//--
void main (void)
{
  // Reset the key data ready strobe to the inactive level ...
  SET_KEY_DATA_RDY = !STROBE_ACT_LVL;

  //   If debugging is enabled, initialize the serial port and print the
  // copyright notice.
#ifdef DEBUG
  InitializeDebugSerial();
  DBGOUT(("\n\n%s V%d\n%s\n", g_szFirmware, VERSION, g_szCopyright));
  DBGOUT(("Swap=%d, Strobe=%d\n\n", SWAP_CAPSLOCK_AND_CONTROL, STROBE_ACT_LVL));
#endif

  // Initialize the PS/2 keyboard interface and enable interrupts ...
  InitializeKeyboard();
  INT_ON;  LED_ON;

  // Whenever the APU is restarted we always send our version number.
  SendHost(KEY_VERSION|VERSION);

  // And then convert PS/2 keys to ASCII and send them to the host.
  ConvertKeys();

  // We should never get here, but ...
  HALT;
}

