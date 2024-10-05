//++
//host.c - convert PS/2 scan codes to ASCII and send to host
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
//DESCRIPTION:
//   The ConvertKeys() routine in this module is an endless loop that extracts
// scan codes from the keyboard buffer, converts them to ASCII, and sends them
// to the host CPU.
//
// PS2 to ASCII translation notes
// ------------------------------
//   All printing characters send their corresponding ASCII codes, as do TAB
// (0x09), ENTER (0x0D), BACKSPACE (0x08), and ESC (0x1B).
//
//   The SHIFT (both left and right), CTRL (_left_ control only!) and CAPS LOCK
// keys work as you would expect.  If the SWAP_CAPSLOCK_AND_CONTROL" option is
// enabled,  then these two functions are swapped.  Note that CAPS LOCK is a
// "CAPS LOCK", _not_ a "SHIFT LOCK" (i.e. it affects only letters).  This is
// standard for PCs, but not all ASCII terminals behaved this way.
//
//   In this version of the firmware (which differs from the original Elf2K
// GPIO version!) all "special" keys, including all numeric keypad keys,
// function keys, arrow keys and editing keypad keys, send special codes in
// the range 0x80..0xFF.  It's left up to the host to translate these keys
// into VT52 (or whatever) escape sequences. This allows the host to implement
// the VT52 keypad application mode switch.  It also allows the VIS/VT1802
// firmware to implement setup mode, local menus, run the built in BASIC,
// send an RS-232 long break, etc ...
//
//   The right CTRL (if your keyboard has one), ALT keys (both left and right),
// and NUMLOCK key do nothing.
//
//   The current hardware and also keyboard.asm implement one way communication
// only with the keyboard, and so the keyboard LEDs are not used, including the
// CAPS LOCK LED.
//
//REVISION HISTORY:
// dd-mmm-yy    who     description
//  5-Feb-06	RLA	New file.
//  7-May-06	RLA	Convert APPLICATION_KEYPAD and SWAP_CAPSLOCK_AND_CONTROL
//			  #ifdef options into external hardware jumpers that can
//			  be changed at runtime.
//			Don't call putchar() unless DEBUG is defined!
// 12-May-24	RLA	Update for SDCC.
//			Change m_bKeyFlags to m_bShiftFlags to avoid confusion
//			  with the g_bKeyFlags in keyboard.asm
// 15-May-24	RLA	Add (non-VT52) escape sequences for the function keys,
//			  keypad editing keys, MENU and PAUSE/BREAK.
// 22-May-24	RLA	Change my mind and make ALL the special keys, including
//			  numeric keypad keys, send codes 0x80..0xFF.  This
//			  allows the host to implement application mode.
//		        Remove the APPLICATION_KEYPAD flag entirely, but the
//			  SWAP_CAPSLOCK_AND_CONTROL remains.
// 29-SEP-24	RLA	Invert the sense of the LED - it's normally ON now, and
//			  turns off when the buffer is full.
//--
#include <stdio.h>		// needed so DBGOUT(()) can find printf!
#include <stdint.h>		// uint8_t, et al ...
#include <stdbool.h>		// bool, true, false ...
#include "at89x051.h"		// register definitions for the AT89C2051
#include "ps2apu.h"		// definitions for this project
#include "debug.h"		// debuging (serial port output) routines
#include "keyboard.h"		// low level keyboard serial I/O functions
#include "scancode.h"		// PS2 scan codes to ASCII translation table
#include "host.h"		// prototypes and options for this module

// These are simplified versions of islower() and toupper() from ctype.h ...
#define toupper(c) ((c)&=0xDF)
#define islower(c) ((unsigned char) c >= (unsigned char) 'a' \
		 && (unsigned char) c <= (unsigned char) 'z')


// Keyboard status bits ...
//   This uses the bit addressable memory of the 8051.  m_bShiftFlags is the
// entire byte and m_fXXX variables are individual bits within that byte.
PRIVATE __data uint8_t __at 0x0021 m_bShiftFlags = 0;
__bit __at 0x8	m_fLeftShiftDown;  //  -> left shift key is pressed now
__bit __at 0x9	m_fRightShiftDown; //  -> right  "    "   "  "   "   "
__bit __at 0xA	m_fControlDown;    //  -> control key     "  "   "   "
__bit __at 0xB	m_fCapsLockOn;	   //  -> CAPS LOCK mode is on


//++
//   This routine returns a scan code from the keyboard buffer.  If the
// buffer is empty, it waits (forever if necessary) until one shows up.
//--
PRIVATE uint8_t WaitKey (void)
{
  int nKey;
  while (true) {
    if ((nKey = GetKey()) != -1) {
      DBGOUT(("KBD: GetKey() returned 0x%x\n", nKey));
      return LOBYTE(nKey);
    }
    if ((g_bKeyFlags & KEYBOARD_ERROR_BITS) != 0) {
      DBGOUT(("KBD: Keyboard re-initialized (0x%x) !!\n", g_bKeyFlags));
      InitializeKeyboard();
    }
  }
}


//++
//   This routine will send one ASCII character to the host CPU.  If the
// buffer isn't free (because the host hasn't yet read the last character)
// then it will wait (forever, if necessary) for the host.  
//--
PUBLIC void SendHost (uint8_t ch)
{
  //   Put the new data on the port pins, turn off the LED as an activity
  // indicator, and then assert the KEY DATA READY strobe...
  P1 = ch;
  LED_OFF;  SET_KEY_DATA_RDY = STROBE_ACT_LVL;
  DBGOUT(("KBD: sending 0x%x to host\n", ch));

  //   When the host reads the data it will reset the KEY DATA READY signel.
  // When we see that happen, then we know it's OK to proceed ...
  while (KEY_DATA_RDY == 0) ;

  // Turn on the LED and deassert the SET KEY DATA READY ...
  SET_KEY_DATA_RDY = !STROBE_ACT_LVL;  LED_ON;
}


//++
//   This routine sends an escape character to the host followed by a one or
// more characters.  In the unlikely event that the serial port buffer is full,
// this routine does NOT swap tasks with the ScreenTask() - this prevents two
// escape sequences from becoming intermixed (for example, suppose the host sent
// an IDENTIFY (ESC-Z) sequence at the same time the user presses the keypad
// ENTER key - if we swap tasks, the two escape sequences, ESC ? M for ENTER,
// and ESC / A for inquire, could get mixed).
//--
//PRIVATE void SendEscape (char const __code *pszEscape)
//{
//  DBGOUT(("KBD: Send Escape %s\n", pszEscape));
//  SendHost('\033');
//  while (*pszEscape != '\0')  SendHost(*pszEscape++);
//}


//++
//   This routine handles "special" key codes, such as 0xAA ("Self Test Pass"),
// 0xFF ("Error") and so on.  It will return TRUE if it processes the key and
// FALSE if the key code is not one of the "special" ones.  Note that these
// messages never have a release or extended byte associated with them!
//--
PRIVATE bool DoSpecial (uint8_t bKey)
{
  switch (bKey) {
    case 0xFA:  DBGOUT(("KBD: ACKNOWLEDGE\n"));		  break;
    case 0xAA:  DBGOUT(("KBD: SELF TEST PASSED\n"));	  break;
    case 0xEE:  DBGOUT(("KBD: ECHO\n"));		  break;
    case 0xFE:  DBGOUT(("KBD: RESEND\n"));		  break;
    case 0x00:  DBGOUT(("KBD: ERROR/OVERFLOW\n"));	  break;
    case 0xFF:  DBGOUT(("KBD: ERROR/OVERFLOW\n"));	  break;
    default: return false;
  }
  return true;
}


//++
//   This routine handles the various "shift" keys - left/right shift, left/
// right control, caps lock, left/right alt, and the infamous "Windows" keys.
// Some of these keys are just ignored, and the ones that aren't either set or
// clear bits in the g_bKeyFlags byte.  The fRelease parameter indicates
// whether a release code (0xF0) was seen before this key code.  TRUE is
// returned if the key is handled, and FALSE is returned if the code is not a
// "shift" key...
//
//   This routine is also called to handle entended key codes for the right hand
// ALT and CONTROL keys.  Convenienty the right side control and alt keys have
// the same keycode as their left hand counterparts, but with the extended
// prefix...
//
//   All these keys are processed even while Setup mode is active just so that
// we can keep track of the keyboard state, but none of them actually does
// anything in Setup mode.
//--
PRIVATE bool DoShift (uint8_t bKey, bool fRelease, bool fExtended)
{
  // A small "hack" to swap the CAPS LOCK and CONTROL keys on the keyboard...
  if (SWAP_CAPSLOCK_AND_CONTROL) {
    if (bKey == 0x58)
      bKey = 0x14;
    else if (bKey == 0x14)
      bKey = 0x58;
  }

  switch (bKey) {
    // Left shift, right shift...
    case 0x12:  m_fLeftShiftDown = !fRelease;  return true;
    case 0x59:  m_fRightShiftDown = !fRelease;  return true;

    // Control key...
    case 0x14:
      // The right control key is not currently implemented...
      if (fExtended) return false;
      m_fControlDown = !fRelease;  return true;

    // CAPS LOCK key...
    case 0x58:  
      if (fRelease) break;
      m_fCapsLockOn = !m_fCapsLockOn;  return true;

    // Alt key (ignored)...
    case 0x11:  return true;

    // Windows keys...
    case 0x1F:	// WINDOWS key (left)
    case 0x27:	// WINDOWS key (right)
      if (fExtended) {
        if (!fRelease) DBGOUT(("KBD: Windows key pressed 0x%x\n", bKey));
      }
      return false;

    // Everything else ...
    default:
      return false;
  }

  //   We can never get here, but SDCC isn't smart enough to figure that out
  // so we need this to keep SDCC from complaining...
  return false;
}


//++
//   This routine will handle the numeric keypad keys which don't send extended
// codes (that's all of them, except "/" and ENTER).  These keys always send
// unique single byte key codes in the range 0x80..0xFF.  It's up to the host
// to translate these into escape sequences.
//--
PRIVATE bool DoKeypad (uint8_t bKey, bool fRelease)
{
  switch (bKey) {
    case 0x70:	// "0"
      if (!fRelease) SendHost(KEY_KP0);
      return true;
    case 0x69:	// "1"
      if (!fRelease) SendHost(KEY_KP1);
      return true;
    case 0x72:	// "2"
      if (!fRelease) SendHost(KEY_KP2);
      return true;
    case 0x7A:	// "3"
      if (!fRelease) SendHost(KEY_KP3);
      return true;
    case 0x6B:	// "4"
      if (!fRelease) SendHost(KEY_KP4);
      return true;
    case 0x73:	// "5"
      if (!fRelease) SendHost(KEY_KP5);
      return true;
    case 0x74:	// "6"
      if (!fRelease) SendHost(KEY_KP6);
      return true;
    case 0x6C:	// "7"
      if (!fRelease) SendHost(KEY_KP7);
      return true;
    case 0x75:	// "8"
      if (!fRelease) SendHost(KEY_KP8);
      return true;
    case 0x7D:	// "9"
      if (!fRelease) SendHost(KEY_KP9);
      return true;
    case 0x71:	// "."
      if (!fRelease) SendHost(KEY_KPDOT);
      return true;
    case 0x7C:	// "*"
      if (!fRelease) SendHost(KEY_KPSTAR);
      return true;
    case 0x7B:	// "-"
      if (!fRelease) SendHost(KEY_KPMINUS);
      return true;
    case 0x79:	// "+"
      if (!fRelease) SendHost(KEY_KPPLUS);
      return true;

    // The NUM LOCK key is ignored ...
    case 0x77:	// NUM LOCK
      return true;

    // All other keys are unknown...
    default:
      return false;
  }
}


//++
//   This routine handles an "extended" key code (i.e. one of the new keys that
// were added to the PC/AT keyboard!).  It's called whenever the 0xE0 "extended"
// prefix is found, so there's no need for it to return TRUE or FALSE - we
// already know that an extended code is coming...
//
//   Note that most of the numeric keypad keys are NOT extended, with the
// notable exceptions of the keypad "/" (upper row, 2nd from the left) and
// keypad ENTER.  The arrow keys, however, are ALL extended keys as are the
// DEL/END/HOME/PAGE UP/PAGE DN/INS etc keys.
//--
PRIVATE void DoExtended (void)
{
  uint8_t bExtended = WaitKey();  bool fRelease = false;
  if (bExtended == 0xF0) {
    fRelease = true;  bExtended = WaitKey();
  }

  switch (bExtended) {

    // Arrow keys...
    case 0x75:	// UP ARROW
      if (!fRelease) SendHost(KEY_UP);
      break;
    case 0x72:	// DOWN ARROW
      if (!fRelease) SendHost(KEY_DOWN);
      break;
    case 0x74:	// RIGHT ARROW
      if (!fRelease) SendHost(KEY_RIGHT);
      break;
    case 0x6B:	// LEFT ARROW
      if (!fRelease) SendHost(KEY_LEFT);
      break;

    // Editing keys...
    case 0x69:	// END
      if (!fRelease) SendHost(KEY_END);
      break;
    case 0x6C:	// HOME
      if (!fRelease) SendHost(KEY_HOME);
      break;
    case 0x70:	// INSERT
      if (!fRelease) SendHost(KEY_INSERT);
      break;
    case 0x71:	// DELETE
      if (!fRelease) SendHost(KEY_DELETE);
      break;
    case 0x7A:	// PAGE DOWN
      if (!fRelease) SendHost(KEY_PGDN);
      break;
    case 0x7D:	// PAGE UP
      if (!fRelease) SendHost(KEY_PGUP);
      break;

    // Other keypad keys...
    case 0x5A:	// KEYPAD ENTER
      if (!fRelease) SendHost(KEY_KPENTER);
      break;
    case 0x4A: // KEYPAD "/"
      if (!fRelease) SendHost(KEY_KPSLASH);
      break;

    // Right ALT and right CONTROL keys...
    case 0x11:  case 0x14:
      DoShift(bExtended, fRelease, true);
      break;

    // MENU key ...
    case 0x2F:
      if (!fRelease) SendHost(KEY_MENU);
      break;

    // Windows keys...
    case 0x1F:	// WINDOWS key (left)
    case 0x27:	// WINDOWS key (right)
      DoShift(bExtended, fRelease, true);
      break;

    //   The PRINT SCREEN key is a little bizzare - when pressed, it sends
    // _two_ extended key sequences, E0 12 followed by E0 7C.  These are treated
    // like two keys that are both ignored!  BTW, when it's released, PRINT
    // SCREEN sends E0 F0 12 and then E0 F0 7C (which is what you'd expect).
    case 0x12:  case 0x7C:
      if (!fRelease) DBGOUT(("KBD: PRINT SCREEN pressed 0x%x\n", bExtended));
      break;

    default:
      DBGOUT(("KBD: unknown extended key code E0 0x%x\n", bExtended));
  }
}


//++
// Handle the function (F1..F12) keys...  All are currently ignored...
//--
PRIVATE bool DoFunction (uint8_t bKey, bool fRelease)
{
  switch (bKey) {
    // Function keys F1..F12 (all are currently ignored).
    case 0x05:  if (!fRelease) SendHost(KEY_F1);   return true;  // F1
    case 0x06:  if (!fRelease) SendHost(KEY_F2);   return true;  // F2
    case 0x04:  if (!fRelease) SendHost(KEY_F3);   return true;  // F3
    case 0x0C:  if (!fRelease) SendHost(KEY_F4);   return true;  // F4
    case 0x03:  if (!fRelease) SendHost(KEY_F5);   return true;  // F5
    case 0x0B:  if (!fRelease) SendHost(KEY_F6);   return true;  // F6
    case 0x83:  if (!fRelease) SendHost(KEY_F7);   return true;  // F7
    case 0x0A:  if (!fRelease) SendHost(KEY_F8);   return true;  // F8
    case 0x01:  if (!fRelease) SendHost(KEY_F9);   return true;  // F9
    case 0x09:  if (!fRelease) SendHost(KEY_F10);  return true;  // F10
    case 0x78:  if (!fRelease) SendHost(KEY_F11);  return true;  // F11
    case 0x07:  if (!fRelease) SendHost(KEY_F12);  return true;  // F12
    default:
      return false;
  }
}


//++
//   And lastly this routine will attempt to translate the key code into an
// ASCII character.  If it's successful then it will return TRUE and send the
// ASCII code to the serial port, and if it's unsuccessful it returns FALSE.
// Note that the ASCII code generated depends on some of the m_bShiftFlags
// (e.g. shift, control, caps lock) flags.  Also note that ASCII keys only care
// about the down event and never the release, so ASCII characters are sent
// to the host only if fRelease == FALSE;
//--
PRIVATE bool DoASCII (uint8_t bKey, bool fRelease)
{
  uint8_t bASCII, bShift;
  bShift = (m_fLeftShiftDown | m_fRightShiftDown) ? 1 : 0;
  if (m_fControlDown) bShift |= 2;
  bASCII = g_abScanCodes[bKey][bShift];
  if (bASCII == 0) return false;
  if (fRelease) return true;
  bASCII &= 0x7F;
  if (m_fCapsLockOn && islower(bASCII))
    bASCII = toupper(bASCII);
  SendHost(bASCII);
  return true;
}


//++
//   This routine is the keyboard "task" - it's an endless loop that runs
// forever reading bytes from the keyboard, converting them to ASCII, and
// sending them to the host.  It never returns ...
//--
PUBLIC void ConvertKeys (void)
{
  uint8_t bKey;  bool fRelease;
  m_bShiftFlags = 0;
  while (true) {
    bKey = WaitKey();  fRelease = false;

    if (DoSpecial(bKey)) continue;
    if (bKey == 0xE0) {
      DoExtended();  continue;
    }
    if (bKey == 0xE1) {
      //   When pressed, the PAUSE/BREAK key sends the absolutely bizzare
      // sequence E1 14 77 E1 F0 14 F0 77.  We don't do anything with this
      // key, but we need to read and throw away these bytes so that they
      // don't get misinterpreted as something else.  BTW, what does PAUSE/
      // BREAK send when it's released ??  Answer: Absolutely nothing!
      if (WaitKey() != 0x14) continue;
      if (WaitKey() != 0x77) continue;
      if (WaitKey() != 0xE1) continue;
      if (WaitKey() != 0xF0) continue;
      if (WaitKey() != 0x14) continue;
      if (WaitKey() != 0xF0) continue;
      if (WaitKey() != 0x77) continue;
      DBGOUT(("KBD: PAUSE/BREAK pressed\n"));
      //   This key sends 0x80 to the host, which (if you strip the 8th bit)
      // would be a NULL and ignored.  The VT1802/VIS1802 can check for this
      // if it wants to though, and trigger a break condition on the UART.
      SendHost(KEY_BREAK);
      continue;
    }
    if (bKey == 0xF0) {
      fRelease = true;  bKey = WaitKey();
    }
    if (bKey == 0x77) {
      if (!fRelease){
	DBGOUT(("KBD: NUM LOCK pressed\n"));
	SendHost(KEY_NUMLOCK);
      }
      continue;
    }
    if (bKey == 0x7E) {
      if (!fRelease) {
	DBGOUT(("KBD: SCROLL LOCK pressed\n"));
	SendHost(KEY_SCRLCK);
      }
      continue;
    }
    if (DoShift(bKey, fRelease, false)) continue;
    if (DoFunction (bKey, fRelease)) continue;
    if (DoKeypad(bKey, fRelease)) continue;
    if (DoASCII(bKey, fRelease)) continue;

    DBGOUT(("KBD: unknown scan code 0x%x\n", bKey));
  }
}

