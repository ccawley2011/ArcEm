/* Modified by DAG to remove memory leak */

/*  armsupp.c -- ARMulator support code:  ARM6 Instruction Emulator.
    Copyright (C) 1994 Advanced RISC Machines Ltd.
 
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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */

#include "armdefs.h"
#include "armemu.h"
#include "arch/fastmap.h"

/***************************************************************************\
* Given a processor mode, this routine returns the register bank that       *
* will be accessed in that mode.                                            *
\***************************************************************************/

static inline ARMword ModeToBank(ARMword mode) {
    return(mode&3);
}

/***************************************************************************\
* This routine controls the saving and restoring of registers across mode   *
* changes.  The regbank matrix is largely unused, only rows 13 and 14 are   *
* used across all modes, 8 to 14 are used for FIQ, all others use the USER  *
* column.  It's easier this way.  old and new parameter are modes numbers.  *
* Notice the side effect of changing the Bank variable.                     *
\***************************************************************************/

ARMword ARMul_SwitchMode(ARMul_State *state,ARMword oldmode, ARMword newmode)
{unsigned i;

 oldmode = ModeToBank(oldmode);
 state->Bank = ModeToBank(newmode);
 if (oldmode != state->Bank) { /* really need to do it */
    switch (oldmode) { /* save away the old registers */
       case USERBANK  :
       case IRQBANK   :
       case SVCBANK   : if (state->Bank == FIQBANK)
                           for (i = 8; i < 13; i++)
                              state->RegBank[USERBANK][i] = state->Reg[i];
                        state->RegBank[oldmode][13] = state->Reg[13];
                        state->RegBank[oldmode][14] = state->Reg[14];
                        break;
       case FIQBANK   : for (i = 8; i < 15; i++)
                           state->RegBank[FIQBANK][i] = state->Reg[i];
                        break;

       }
    switch (state->Bank) { /* restore the new registers */
       case USERBANK  :
       case IRQBANK   :
       case SVCBANK   : if (oldmode == FIQBANK)
                           for (i = 8; i < 13; i++)
                              state->Reg[i] = state->RegBank[USERBANK][i];
                        state->Reg[13] = state->RegBank[state->Bank][13];
                        state->Reg[14] = state->RegBank[state->Bank][14];
                        break;
       case FIQBANK  : for (i = 8; i < 15; i++)
                           state->Reg[i] = state->RegBank[FIQBANK][i];
                        break;
       } /* switch */
    } /* if */
    return(newmode);
}

#ifndef FASTMAP_INLINE
#include "arch/fastmap.c"
#endif
