#++
# Makefile - SDCC Makefile for the PS/2 Keyboard Interface
#
# Copyright (C) 2024 by Spare Time Gizmos.  All rights reserved.
#
# This file is part of the Spare Time Gizmos' VT1802 and VIS1802 firmware.
#
# This firmware is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 59 Temple
# Place, Suite 330, Boston, MA  02111-1307  USA
#
#DESCRIPTION:
#   This Makefile will build the PS/2 keyboard APU firmware image using SDCC
# compiler tool chain, and the result is a file PS2APU.HEX (Intel HEX) which
# can be programmed directly into the flash for an AT89C4051 or AT89C2051 MCU.
#
#IMPORTANT!
#   The debug version of this code requires approximately 3,800 bytes of
# program memory and will require an AT89C4051 (4K flash) part to work.
#
#   The non-debug version only needs about ????? bytes of program memory and
# will work in either an AT89C4051 or the 2K flash AT89C2051.
#
#TARGETS:
#  make all	- rebuild PS2APU.HEX
#  make clean	- delete all generated files EXCEPT PS2APU.HEX
#  make depend	- regenerate source file dependencies
#
# REVISION HISTORY:
# dd-mmm-yy	who     description
# 12-May-24	RLA	New file.
# 22-May-24	RLA	Remove the APPLICATION_KEYPAD option.
#--

# Tool paths - you can change these as necessary...
SDCC   = c:/sdcc/bin/sdcc	# SDCC compiler, v4.4.1
AS8051 = c:/sdcc/bin/sdas8051	# AS8051 cross assembler
LINK   = c:/sdcc/bin/sdld	# SDCC linker
PACK   = c:/sdcc/bin/packihx	# convert .IHX file to .HEX

# General build options...
DEBUG		= #-DDEBUG	# uncomment to build the debug version
CPUCLOCK      	= 14318180UL	# CPU cyrstal/clock frequency
#CPUCLOCK	= 12000000UL	# CPU cyrstal/clock frequency
STROBE_ACT_LVL	= 0		# SET_KBD_DATA_RDY strobe active state (0 or 1)
SCANCODE	= scancode_us.c	# either "us" or "uk" to select keyboard layout
#   You can uncomment the following option to enable the "swap CAPS LOCK and
# CONTROL" feature IN DEBUG MODE.  If DEBUG is NOT defined above, then this
# option DOES NOTHING and the swap CAPSLOCK-CONTROL option is controlled at
# runtime by a jumper on the P3.0 port pin.
SWAP_CAPSLOCK_AND_CONTROL = true	# true or false

# Compiler and assembler options...
CFLAGS  = -mmcs51 --model-small $(DEBUG) \
	  -DCPUCLOCK=$(CPUCLOCK) -DSTROBE_ACT_LVL=$(STROBE_ACT_LVL) \
	  -DSWAP_CAPSLOCK_AND_CONTROL=$(SWAP_CAPSLOCK_AND_CONTROL)
AFLAGS  = -los
LFLAGS  = -Wl -bBSEG=0x0020

# Files - C source, assembly source, and object files...
TARGET  = ps2apu
CSOURCES= ps2apu.c host.c debug.c $(SCANCODE)
INCLUDES= ps2apu.h host.h keyboard.h scancode.h debug.h
OBJECTS = $(CSOURCES:.c=.rel) keyboard.rel


# The default target builds everything, naturally...
all:	$(TARGET).hex

# Make the flash image from the linker's .IHX file...
$(TARGET).hex: $(TARGET).ihx
	$(PACK) $(TARGET).ihx > $(TARGET).hex

# Link the .REL files to make PS2APU.IHX ....
$(TARGET).ihx: $(OBJECTS)
	$(SDCC) $(LFLAGS) $(OBJECTS) -o $@

# Assemble the keyboard.asm file ...
keyboard.rel: keyboard.asm
	$(AS8051) $(AFLAGS) $<

# Compile a C file...
#   Note that this is a bit lame - it makes every C file depend on ALL the
# header files, regardless of whether this particular file actually uses
# that header.  Unfortunately SDCC doesn't seem to support the -M option to
# generate a Makefile dependency list, so this will have to do.  Fortunately
# it's a small project!
%.rel: %.c $(INCLUDES)
	$(SDCC) -c $(CFLAGS) $< -o $@

#   Remove all generated files from the directory.  DO NOT, DO NOT, DO NOT
# be tempted to do a "rm *.asm" !!!!!
clean:
	rm -f *.lst *.rel *.sym *.lk *.rst
	rm -f $(CSOURCES:.c=.asm)
	rm -f $(TARGET).ihx $(TARGET).mem $(TARGET).map
