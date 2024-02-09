/* Amiga sound support routines
 * by Chris Young chris@unsatisfactorysoftware.co.uk
 *
 * This just writes all sound direct to AUDIO: and lets
 * that take care of any buffering.
 */

#include "platform.h"
#include "../armdefs.h"
#include "../armemu.h"
#include "../arch/sound.h"
#include "../arch/ControlPane.h"
#include "../arch/dbugsys.h"
#include "../arch/displaydev.h"

#define SSD_Name(x) ssd_ ## x
#define SSD_SoundData int16_t

BPTR audioh = 0;

static unsigned long sampleRate = 44100;

SSD_SoundData sound_buffer[256*2]; /* Must be >= 2*Sound_BatchSize! */

static SSD_SoundData *SSD_Name(GetHostBuffer)(ARMul_State *state,int32_t *destavail);
static void SSD_Name(HostBuffered)(ARMul_State *state,SSD_SoundData *buffer, int32_t numSamples);
static int SSD_Name(InitHost)(ARMul_State *state);
static void SSD_Name(QuitHost)(ARMul_State *state);

#include "stdsounddev.c"

static int openaudio(void)
{
	char audiof[256];

	sprintf(audiof, "AUDIO:BITS/16/C/2/F/%lu/T/SIGNED\0", sampleRate);

	if(!(audioh = Open(audiof,MODE_NEWFILE)))
	{
		fprintf(stderr, "Could not open audio: device\n");
		return -1;
	}

	return 0;
}

static SSD_SoundData *SSD_Name(GetHostBuffer)(ARMul_State *state,int32_t *destavail)
{
	/* Just assume we always have enough space for the max batch size */
	*destavail = sizeof(sound_buffer)/(sizeof(SSD_SoundData)*2);
	return sound_buffer;
}

static void SSD_Name(HostBuffered)(ARMul_State *state,SSD_SoundData *buffer,int32_t numSamples)
{
	numSamples *= 2;

	/* TODO - Adjust Sound_FudgeRate to fine-tune how often we receive new data */

	Write(audioh,buffer,numSamples*sizeof(SSD_SoundData));
}

static int SSD_Name(InitHost)(ARMul_State *state)
{
	if(openaudio() == -1)
		return -1;

	/* TODO - Tweak these as necessary */
	SI.StereoSense = Stereo_LeftRight;

	SI.BatchSize = 256;

	SI.HostRate = sampleRate<<10;

	return 0;
}

static void SSD_Name(QuitHost)(ARMul_State *state)
{
	sound_exit();
}

/*-----------------------------------------------------------------------------*/
int
SoundDev_Init(ARMul_State *state)
{
	/* Setup sound device */
	return SoundDev_Set(state, &SSD_SoundDev);
}

void sound_exit(void)
{
//	IExec->FreeVec(buffer);
	Close(audioh);
}
