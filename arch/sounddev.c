/*
  arch/newsound.c

  (c) 2011 Jeffrey Lee <me@phlamethrower.co.uk>
  Based on original sound code by Daniel Clarke

  Part of Arcem released under the GNU GPL, see file COPYING
  for details.

  Utility functions for bridging the gap between the host sound code and the
  emulator core
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../armdefs.h"
#include "arch/armarc.h"
#include "arch/ControlPane.h"
#include "arch/dbugsys.h"
#include "arch/displaydev.h"
#include "arch/sound.h"
#include "../armemu.h"

static const SoundDev *SoundDev_Current = NULL;

int SoundDev_Set(ARMul_State *state, const SoundDev *dev)
{
    if (SoundDev_Current)
    {
        (SoundDev_Current->Shutdown)(state);
        SoundDev_Current = NULL;
    }
    if (dev)
    {
        int ret = (dev->Init)(state);
        if (ret)
            return ret;
        SoundDev_Current = dev;
    }
    return 0;
}

void Sound_SoundFreqUpdated(ARMul_State *state)
{
    SoundDev_Current->SoundFreqUpdated(state);
}

void Sound_StereoUpdated(ARMul_State *state)
{
    SoundDev_Current->StereoUpdated(state);
}

#ifndef SOUND_SUPPORT
#define NSD_Name(x) nsd_ ## x
#include "nullsounddev.c"

/*-----------------------------------------------------------------------------*/
int
SoundDev_Init(ARMul_State *state)
{
    /* Setup a null sound device */
    return SoundDev_Set(state, &NSD_SoundDev);
}
#endif
