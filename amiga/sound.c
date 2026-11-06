/* Amiga sound support routines
 * by Chris Young chris@unsatisfactorysoftware.co.uk
 *
 * This just writes all sound direct to AUDIO: and lets
 * that take care of any buffering.
 */

#if defined(SOUND_SUPPORT)

#include <stdio.h>

#include "platform.h"
#include "../armdefs.h"
#include "../arch/sound.h"
#include "../arch/ControlPane.h"
#include "../arch/dbugsys.h"
#include "../arch/displaydev.h"

BPTR audioh = 0;

static unsigned long sampleRate = 44100;

SoundFrame sound_buffer[256]; /* Must be >= Sound_BatchSize! */

static bool openaudio(void)
{
	char audiof[256];

	snprintf(audiof, 256, "AUDIO:BITS/16/C/2/F/%lu/T/SIGNED", sampleRate);

	if(!(audioh = Open(audiof,MODE_NEWFILE)))
	{
		ControlPane_Error(false,"Could not open audio: device");
		return false;
	}

	return true;
}

SoundFrame *Sound_GetHostBuffer(size_t *destavail)
{
	/* Just assume we always have enough space for the max batch size */
	*destavail = countof(sound_buffer);
	return sound_buffer;
}

void Sound_HostBuffered(SoundFrame *buffer,size_t numFrames)
{
	/* TODO - Adjust Sound_FudgeRate to fine-tune how often we receive new data */

	Write(audioh,buffer,numFrames*sizeof(SoundFrame));
}

bool
Sound_InitHost(ARMul_State *state)
{
	UNUSED_VAR(state);

	if(!openaudio())
		return false;

	/* TODO - Tweak these as necessary */
	eSound_StereoSense = Stereo_LeftRight;

	Sound_BatchSize = 256;

	Sound_HostRate = sampleRate<<10;

	return true;
}

void sound_exit(void)
{
	/* IExec->FreeVec(buffer); */
	Close(audioh);
	audioh = 0;
}

void
Sound_ShutdownHost(ARMul_State *state)
{
	UNUSED_VAR(state);

	sound_exit();
}

#endif
