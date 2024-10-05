	.title	PS/2 Keyboard Serial Protocol
	.module	Keyboard

;++
; keyboard.asm
;
; Copyright (C) 2006-2024 by Spare Time Gizmos.  All rights reserved.
;
; This file is part of the Spare Time Gizmos' Elf 2000 GPIO firmware.
;
; This firmware is free software; you can redistribute it and/or modify it
; under the terms of the GNU General Public License as published by the Free
; Software Foundation; either version 2 of the License, or (at your option)
; any later version.
;
; This program is distributed in the hope that it will be useful, but WITHOUT
; ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
; FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
; more details.
;
; You should have received a copy of the GNU General Public License along with
; this program; if not, write to the Free Software Foundation, Inc., 59 Temple
; Place, Suite 330, Boston, MA  02111-1307  USA
;
; DESCRIPTION:
;   This module contains the code to handle the bit serial protocol used by
; the traditional PC/AT keyboard entirely in software, without any special
; hardware support.  This is harder than it sounds because the PC/AT standard
; allows the keyboard to generate a clock as fast as 30KHz, which is a little
; over 33us per bit.  Remember that a standard 8051 with a 11.0592MHz crystal
; needs a little over 2us to execute most inst ructions, and you can see that
; the keyboard data rate is pretty fast for software bit banging!
;
;   The "Standard" (if there is such a thing) is for the keyboard data clock to
; be somewhere in the range of 10 to 30kHz.  That's between 33 to 100us per
; bit, but remember that we're only allowed to sample the data bit while the
; clock is low (the keyboard is changing the data bit while the clock is high).
; So in reality, worst case, we have only about 16us from the negative clock
; edge to sample the data bit before it goes away.
;
;   The receiver is completely interrupt driven - every negative transition
; on the KEYBOARD_CLOCK input causes an interrupt.  The ISR implements a
; simple state machine which advances thru the received message one bit at a
; time until a complete byte can be assembled, and the final state (for the
; stop bit) then adds the byte to the end of the keyboard circular buffer.
;
;   This implementation depends on speed for success.  The ISR for each state
; must complete _very_ quickly, hopefully in no more than a "handful" of 8051
; instructions, so that we can return to the background in time to capture the
; next bit.
;
;    One problem with this implementation is that it's always possible that a
; spurious bit of noise on the CLOCK signal will start our state machine
; running.  Or we could miss a clock edge due to some glitch.  Or the user
; could plug or unplug a keyboard in the middle of a transmission, causing
; some bits to be dropped.  The Bad Problem about all these possibilities is
; that once our keyboard state machine gets out of sync with the real
; keyboard, there's no mechanism to ever get them back into sync again.  It'll
; never work again!
;
;   The solution is to use timer 0 to implement a time out function.  Timer 0
; is programmed for a delay of approximately 2ms (about twice as long as the
; longest keyboard transmission should ever take), and the timer is started
; when we receive the start bit from the keyboard and stopped when we receive
; the stop bit.  If all is well timer 0 will never overflow, but if something
; goes wrong then the timer 0 interrupt routine will post a timeout error and
; reset the keyboard state machine back to the idle state.
;
;REVISION HISTORY:
; dd-mmm-yy	who     description
;  5-Feb-06	RLA	New file.
; 28-Apr-19	TAF	Ported to sdas8051 distributed with sdcc
;--

	.globl	_InitializeKeyboard, _GetKey, _g_bKeyFlags
	.globl	_KEYBOARD_BIT, _KEYBOARD_TIMEOUT


;   These are the physical I/O bits that are connected to the PS/2 keyboard.
; Note that the code assumes that the keyboard clock is connected to INT0,
; but the keyboard data may be any pin...
KEYBOARD_CLOCK	.equ	P3.2		; keyboard clock (must be INT0)
KEYBOARD_DATA	.equ	P3.7		; keyboard data

;   The size of the keyboard buffer (stored in internal RAM!) must agree
; with the same constant in keyboard.h...
KEYBUFLEN	.equ	16		; MUST BE A POWER OF TWO!!

; This constant is an approximately 2ms time out for timer 0.
TIMEOUT_COUNT	.equ	-1842		; 2ms with a clock of 11.0592MHz 
T1_MASK		.equ	0xF0		; TMOD mask to clear T0 bits
T0_M0		.equ	0x01		; mode bits for timer 0

;--------------------------------------------------------
; overlayable register banks
;--------------------------------------------------------
        .area REG_BANK_0        (REL,OVR,DATA)
        .ds 8

; Keyboard status bits ...
;   This uses the bit addressable memory of the 8051.  m_bKeyFlags is the
; entire byte, which is shared with the keyboard.asm assembly code, and
;; m_fXXX variables are individual bits within that byte.
	.area	IABS	(ABS,DATA)
	.org	0x0020
_g_bKeyFlags:	.ds	1	; keyboard flags
	.area	BSEG	(BIT)
m_fKeyBusy	.equ	0x0000	;  keyboard is busy receiving
m_fKeyOverflow	.equ	0x0004	;  buffer overflow
m_fKeyParity	.equ	0x0005	;  wrong parity bit read from keyboard
m_fKeyFraming	.equ	0x0006	;  data framing error (bad start or stop bit)
m_fKeyTimeout	.equ	0x0007	;  transmission didn't complete in time

;; Internal RAM (directly addressable) segment...
	.area	DSEG	(DATA)
m_bKeyState:	.ds	1      		; current keyboard "state"
m_bKeyData:	.ds	1      		; last data byte read from the keyboard
m_bKeyGet:	.ds	1      		; "get" pointer to circular buffer
m_bKeyPut:	.ds	1   		; "put"  "   "   "   "   "     "
m_abKeyBuffer:	.ds	KEYBUFLEN	; circular buffer for bytes received

        AR7 = 0x07
        AR6 = 0x06
        AR5 = 0x05
        AR4 = 0x04
        AR3 = 0x03
        AR2 = 0x02
        AR1 = 0x01
        AR0 = 0x00


;   The Keyboard interrupt vectors are generated auto-magically by SDCC via
; prototypes in keyboard.h

; And finally, the code segment...
	.area	CSEG	(CODE)


;++
; InitializeKeyboard
;
; DESCRIPTION:
;   This routine will intialize the keyboard interface.  Remember that the
; KEYBOARD_CLOCK is connected to the INT0 input, and this pin is initialized
; to interrupt on every negative edge.
;--
_InitializeKeyboard:
; Initialize any internal data and variables...
	MOV	A, #0	       	; clear the circular buffer
	MOV	m_bKeyGet, A   	; ...
	MOV	m_bKeyPut, A	; ...
	MOV	m_bKeyState, A	; reset the state to idle
	MOV	_g_bKeyFlags, A	; and clear all the errors/flags

; Initialize timer 0 as a timeout counter...
	ANL	TMOD, #T1_MASK	; clear the timer 0 mode bits
	ORL	TMOD, #T0_M0	; select mode 1 (sixteen bit auto reload) timer
	CLR	TR0		; make sure the timer's not running for now
	SETB	ET0		; enable timer 0 interrupts
	
; Setup the external hardware for INT0...
	SETB	KEYBOARD_CLOCK	; be sure both keyboard and data
	SETB	KEYBOARD_DATA   ;  ... signals are free
	SETB	IT0		; make INT0 edge triggered
	SETB	EX0		; enable INT0 interrupts and we're done
	RET		       	; ...


;++
; GetKey
;
; DESCRIPTION:
;   This routine will return the next scan code from the keyboard buffer,
; which will be an 8 bit byte 0..0xFF, or 16 bit the value -1 if the buffer
; is currently empty.  We need to handle the interrupt system carefully here -
; to avoid race conditions with the KEYBOARD_BIT ISR we have to disable
; interrupts when checking the buffer...
;--
_GetKey:CLR	EX0		; disable keyboard interrupts
	MOV	A, m_bKeyGet	; get the circular buffer pointer
	CJNE	A,m_bKeyPut,GETK1; jump if there's something in the buffer
	SETB	EX0		; re-enable keyboard interrupts
	MOV	DPL, #0xFF	; and the buffer is empty - return -1
	MOV	DPH, #0xFF	; (in 16 bits)
	RET			; all done

; Here if there's something in the buffer...
GETK1:	INC	A		; increment the "get" pointer
	ANL	A, #KEYBUFLEN-1	; wrap around at the end of the buffer
	MOV	m_bKeyGet, A	; and update the buffer pointer
	ADD	A,#m_abKeyBuffer; add in the buffer index
	MOV	R0, A		; set up R0 with the buffer pointer
	MOV	A, @R0		; fetch the byte from the buffer
	SETB	EX0		; re-enable keyboard interrupts
	MOV	DPL, A		; and return the 16 bit value in DPH:DPL
	MOV	DPH, #0		;  (with zero fill!)
	RET			; ...


;++
; PutKey
;
; DESCRIPTION:
;   This routine will store the keyboard byte from m_bKeyData and store it
; in the keyboard circular buffer.  If the buffer is full, then the 
; m_fKeyOverflow bit is set in the error status and the key code is
; discarded.  This routine is called at interrupt level, directly from the
; KEYBOARD_BIT ISR routine...
;--
PutKey:	MOV	A, m_bKeyPut	; get the current buffer pointer
	INC	A		; increment it
	ANL	A, #KEYBUFLEN-1	; allow for wrap around
	CJNE	A,m_bKeyPut,SAVEIT; is there room in the buffer??
	AJMP	NOROOM		; nope - just discard this byte

; Add this byte to the circular buffer...
SAVEIT:	MOV	m_bKeyPut, A	; update the buffer pointer
	ADD	A,#m_abKeyBuffer; index into the keyboard buffer
	MOV	R0, A		; ...
	MOV	A, m_bKeyData	; get the original data byte back again
	MOV	@R0, A 		; and store it in the buffer
	RET

; Here if there is no room left in the buffer....
NOROOM:	SETB	m_fKeyOverflow	; set the overflow error bit
	RET			; and just discard the data byte


;++
; KEYBOARD_BIT
;
; DESCRIPTION:
;--
_KEYBOARD_BIT:
	PUSH	PSW		; save only the PSW
	PUSH	ACC		; and the ACC
	PUSH	AR0		; ...
	PUSH	DPH		; ...
	PUSH	DPL		; ...
	MOV	DPTR, #KEYTAB	; point to the state table
	MOV	A, m_bKeyState	; get the current keyboard state
	RL	A		; multiply the state by two
	JMP	@A+DPTR		; branch to the right state routine
 
; Dispatch table for PS/2 keyboard states...
KEYTAB:	AJMP	START		; state 0 - start bit
	AJMP	SDATA		; state 1 - data bit
	AJMP	SDATA		; state 2 - data bit
	AJMP	SDATA		; state 3 - data bit
	AJMP	SDATA		; state 4 - data bit
	AJMP	SDATA		; state 5 - data bit
	AJMP	SDATA		; state 6 - data bit
	AJMP	SDATA		; state 7 - data bit
	AJMP	SDATA		; state 8 - data bit
	AJMP	SPARITY		; state 9 - parity bit
	AJMP	STOPB		; state 10 - stop bit
	AJMP	KEYRET		; state 11 - error (just return!)

; Here for the start bit...
START:	JB	KEYBOARD_DATA,FRAERR	; data must be zero for a valid start
	SETB	m_fKeyBusy		; set tbe busy flag
	MOV	m_bKeyData, #0		; clear the data accumulator
	MOV	TH0,#>(TIMEOUT_COUNT)	; initialize the timeout timer
	MOV	TL0,#<(TIMEOUT_COUNT)	; ...
	CLR	TF0			; make sure the timer flag is cleared
	SETB	TR0			; and start it running
	AJMP	KEYNXT			; move to the next state and return

; Here for a framing error (bad start or stop bit)...
FRAERR:	SETB	m_fKeyFraming		; set the framing error bit
	MOV	m_bKeyState, #11	; go to (and stay in!) state 11
	AJMP	KEYRET			; ...

; Here for a data bit...
SDATA:	MOV	A, m_bKeyData		; get the current data byte
	MOV	C, KEYBOARD_DATA	; sample the data bit now
	RRC	A			; and shift it into A	
	MOV	m_bKeyData, A		; save it for next time
	AJMP	KEYNXT			; move to the next state and return

; Here for the parity bit, which should match the parity of the data byte...
SPARITY:MOV	A, m_bKeyData		; put the data byte in the AC
	JB	PSW.0, SPAR1		; if the accumulator parity is ODD
	JNB	KEYBOARD_DATA, BADPAR	; then this bit must be one
	AJMP	KEYNXT          	; yes - good parity
SPAR1:	JNB	KEYBOARD_DATA, KEYNXT 	; else this bit must be zero

; Here if the parity bit is bad....
BADPAR:	SETB	m_fKeyParity		; set the parity error flag
	MOV	m_bKeyState, #11	; then goto and stay in state 11
	AJMP	KEYRET			; ...

; And here for the stop bit...
STOPB:	CLR	m_fKeyBusy		; clear the busy flag
	CLR	TR0			; and stop the timeout timer
	JNB	KEYBOARD_DATA, FRAERR	; the stop bit must be a one
	ACALL	PutKey			; store the key in the buffer

; We're all done with this byte - back to state zero...
	MOV	m_bKeyState, #0		; back to state zero for the next byte
	AJMP	KEYRET			; and return

; Here to return from the interrupt...
KEYNXT:	INC	m_bKeyState		; on to the next state	
KEYRET:	POP	DPL			; restore the original context
	POP	DPH			; ...
	POP	AR0			; ...
	POP	ACC			; ...
	POP	PSW			; ...
	RETI				; return from the interrupt


;++
; KEYBOARD_TIMEOUT
;
; DESCRIPTION:
;   We get here if the timeout timer goes off before we find the stop bit in a
; byte from the keyboard.  Something bad must have happened - set the timeout
; error flag, clear the keyboard busy flag, and reset the keyboard back to 
; state 0 (idle)...
;--
_KEYBOARD_TIMEOUT:
	SETB	m_fKeyTimeout		; set the timeout flag
	CLR	m_fKeyBusy		; clear the busy flag
	MOV	m_bKeyState, #0		; and reset to state zero
	RETI				; and return

