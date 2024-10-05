//++
//scancode.h - declarations for scancode.c module
//
// Copyright (C) 2006-2024 by Spare Time Gizmos.  All rights reserved.
//
// This file is part of the Spare Time Gizmos' Elf 2000 GPIO firmware.
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
// Place, Suite 330, Boston, MA  02111-1307  USA
//
//REVISION HISTORY:
// dd-mmm-yy    who     description
//  5-Feb-06	RLA	New file.
// 12-May-24	RLA	Update for SDCC.
//--
#pragma once

// Global data definitions...
extern uint8_t const __code g_abScanCodes[128][4];

// Function keys ...
#define KEY_BREAK	0x80	// PAUSE/BREAK KEY
#define KEY_F1		0x81	// F1  KEY
#define KEY_F2		0x82	// F2  KEY
#define KEY_F3		0x83	// F3  KEY
#define KEY_F4		0x84	// F4  KEY
#define KEY_F5		0x85	// F5  KEY
#define KEY_F6		0x86	// F6  KEY
#define KEY_F7		0x87	// F7  KEY
#define KEY_F8		0x88	// F8  KEY
#define KEY_F9		0x89	// F9  KEY
#define KEY_F10		0x8A	// F10 KEY
#define KEY_F11		0x8B	// F11 KEY
#define KEY_F12		0x8C	// F12 KEY
#define KEY_SCRLCK	0x8D	// SCROLL LOCK key
#define KEY_NUMLOCK	0x8E	// NUM LOCK key
//#define KEY_PSCREEN	0x8F	// PRINT SCREEN key
// Arrow keypad keys ...
#define KEY_UP		0x90	// UP    ARROW
#define KEY_DOWN	0x91	// DOWN    "
#define KEY_RIGHT	0x92	// RIGHT   "
#define KEY_LEFT	0x93	// LEFT	   "
//   The MENU key is to the right of the spacebar on a standard
// PC keyboard.  DON'T confuse it with the Windows key(s)!
#define KEY_MENU	0x95	// MENU
// Editing keypad keys ...
#define KEY_END		0x96	// END
#define KEY_HOME	0x97	// HOME
#define KEY_INSERT	0x98	// INSERT
#define KEY_PGDN	0x99	// PAGE DOWN
#define KEY_PGUP	0x9A	// PAGE UP
#define KEY_DELETE	0x9B	// DELETE
// Numeric keypad keys ...
#define KEY_KP0		0xA0	// KEYPAD 0
#define KEY_KP1		0xA1	// KEYPAD 1
#define KEY_KP2		0xA2	// KEYPAD 2
#define KEY_KP3		0xA3	// KEYPAD 3
#define KEY_KP4		0xA4	// KEYPAD 4
#define KEY_KP5		0xA5	// KEYPAD 5
#define KEY_KP6		0xA6	// KEYPAD 6
#define KEY_KP7		0xA7	// KEYPAD 7
#define KEY_KP8		0xA8	// KEYPAD 8
#define KEY_KP9		0xA9	// KEYPAD 9
#define KEY_KPDOT	0xAA	// KEYPAD .
#define KEY_KPPLUS	0xAB	// KEYPAD +
#define KEY_KPSLASH	0xAC	// KEYPAD /
#define KEY_KPSTAR	0xAD	// KEYPAD *
#define KEY_KPMINUS	0xAE	// KEYPAD -
#define KEY_KPENTER	0xAF	// KEYPAD ENTER

// Special codes not associated with keys ...
#define KEY_VERSION	0xC0
