//++
//keyboard.h - declarations for keyboard.a51 module
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
// 12-May-24	RLA	Use stdint and update for SDCC.
//--
#pragma once

#define KEYBOARD_ERROR_BITS	0xF0

// Function prototypes...
extern void InitializeKeyboard (void);
extern int GetKey (void);
extern void KEYBOARD_BIT (void) __interrupt (0);
extern void KEYBOARD_TIMEOUT (void) __interrupt (1);

// Global data definitions...
extern volatile uint8_t __data g_bKeyFlags;
