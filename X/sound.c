#if defined(SOUND_SUPPORT)

#include <stdio.h>
#include <stdlib.h>

#include <linux/soundcard.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "../armdefs.h"
#include "../arch/ControlPane.h"
#include "../arch/sound.h"
#include "../arch/dbugsys.h"
#include "../arch/displaydev.h"

static uint32_t format = AFMT_S16_LE;
static uint32_t channels = 2;
static uint32_t sampleRate = 44100;

static int soundDevice;

SoundFrame sound_buffer[256]; /* Must be >= Sound_BatchSize! */

SoundFrame *Sound_GetHostBuffer(size_t *destavail)
{
  /* Just assume we always have enough space for the max batch size */
  *destavail = countof(sound_buffer);
  return sound_buffer;
}

void Sound_HostBuffered(SoundFrame *buffer,size_t numFrames)
{
  audio_buf_info buf;
  if (ioctl(soundDevice, SOUND_PCM_GETOSPACE, &buf) != -1) {
    /* Adjust fudge rate based around how much buffer space is available
       We aim for the buffer to be somewhere between 1/4 and 3/4 full, but don't
       explicitly set the buffer size, so we're at the mercy of the sound system
       in terms of how much lag there'll be */
    int32_t bufsize = buf.fragsize*buf.fragstotal;
    int32_t buffree = buf.bytes/sizeof(SoundFrame);
    int32_t used = (bufsize-buf.bytes)/sizeof(SoundFrame);
    int32_t stepsize = Sound_DMARate>>2;
    bufsize /= sizeof(SoundFrame);
    if((int32_t)numFrames > buffree)
    {
      warn_sound("*** sound overflow! %"PRId32" %d %d %d ***\n",(int32_t)numFrames-buffree,ARMul_EmuRate,Sound_FudgeRate,Sound_DMARate);
      numFrames = buffree; /* We could block until space is available, but I'm woried we'd get stuck blocking forever because the FudgeRate increase wouldn't compensate for the ARMul cycles lost due to blocking */
      if(Sound_FudgeRate < -stepsize)
        Sound_FudgeRate = Sound_FudgeRate/2;
      else
        Sound_FudgeRate+=stepsize;
    }
    else if(!used)
    {
      warn_sound("*** sound underflow! %d %d %d ***\n",ARMul_EmuRate,Sound_FudgeRate,Sound_DMARate);
      if(Sound_FudgeRate > stepsize)
        Sound_FudgeRate = Sound_FudgeRate/2;
      else
        Sound_FudgeRate-=stepsize;
    }
    else if(used < bufsize/4)
    {
      Sound_FudgeRate-=stepsize>>4;
    }
    else if(buffree < bufsize/4)
    {
      Sound_FudgeRate+=stepsize>>4;
    }
    else if(Sound_FudgeRate)
    {
      /* Bring the fudge value back towards 0 until we go out of the comfort zone */
      Sound_FudgeRate += (Sound_FudgeRate>0?-1:1);
    }
  }
  
  write(soundDevice,buffer,numFrames*sizeof(SoundFrame));
}

bool
Sound_InitHost(ARMul_State *state)
{
  audio_buf_info buf;

  UNUSED_VAR(state);

  if ((soundDevice = open("/dev/dsp", O_WRONLY )) < 0) {
    ControlPane_Error(false,"Could not open audio device /dev/dsp");
    return false;
  }

  if (ioctl(soundDevice, SOUND_PCM_RESET, 0) == -1) {
    ControlPane_Error(false,"Could not reset PCM");
    return false;
  }

  if (ioctl(soundDevice, SOUND_PCM_SYNC, 0) == -1) {
    ControlPane_Error(false,"Could not sync PCM");
    return false;
  }

  if (ioctl(soundDevice, SOUND_PCM_SETFMT, &format) == -1) {
    ControlPane_Error(false,"Could not set PCM format");
    return false;
  }

  if (ioctl(soundDevice, SOUND_PCM_WRITE_CHANNELS, &channels) == -1) {
    ControlPane_Error(false,"Could not set to 2 channel stereo");
    return false;
  }

  if (ioctl(soundDevice, SOUND_PCM_WRITE_RATE, &sampleRate) == -1) {
    ControlPane_Error(false,"Could not set sample rate");
    return false;
  }

  if (ioctl(soundDevice, SOUND_PCM_READ_RATE, &sampleRate) == -1) {
    ControlPane_Error(false,"Could not read sample rate");
    return false;
  }

  /* Check that GETOSPACE is supported */
  if (ioctl(soundDevice, SOUND_PCM_GETOSPACE, &buf) == -1) {
    ControlPane_Error(false,"Could not read output space");
    return false;
  }
  warn_sound("Sound buffer params: frags %d total %d size %d bytes %d\n",buf.fragments,buf.fragstotal,buf.fragsize,buf.bytes);

  eSound_StereoSense = Stereo_LeftRight;

  /* Use a decent batch size */
  Sound_BatchSize = 256;

  Sound_HostRate = sampleRate<<10;

  return true;
}

void
Sound_ShutdownHost(ARMul_State *state)
{
  /* TODO */
  UNUSED_VAR(state);
}

#endif
