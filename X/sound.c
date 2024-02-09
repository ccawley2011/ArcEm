#include <stdio.h>
#include <stdlib.h>

#include <linux/soundcard.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <pthread.h>

#include "../armdefs.h"
#include "../arch/sound.h"
#include "../arch/ControlPane.h"
#include "../arch/dbugsys.h"
#include "../arch/displaydev.h"
#include "../armemu.h"

#define SSD_Name(x) ssd_ ## x
#define SSD_SoundData int16_t

static uint32_t format = AFMT_S16_LE;
static uint32_t channels = 2;
static uint32_t sampleRate = 44100;

static int soundDevice;

/* Threading currently doesn't work very well - no priority control is in place, so the sound thread hardly ever gets any CPU time */
//#define SOUND_THREAD

#ifdef SOUND_THREAD
static pthread_t thread;
static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

#define BUFFER_SAMPLES (16384) /* 8K stereo pairs */

SSD_SoundData sound_buffer[BUFFER_SAMPLES];
volatile int32_t sound_buffer_in=BUFFER_SAMPLES; /* Number of samples we've placed in the buffer */
volatile int32_t sound_buffer_out=0; /* Number of samples read out by the sound thread */
static const int32_t sound_buff_mask=BUFFER_SAMPLES-1;
#else
SSD_SoundData sound_buffer[256*2]; /* Must be >= 2*Sound_BatchSize! */
#endif

static SSD_SoundData *SSD_Name(GetHostBuffer)(ARMul_State *state,int32_t *destavail);
static void SSD_Name(HostBuffered)(ARMul_State *state,SSD_SoundData *buffer, int32_t numSamples);
static int SSD_Name(InitHost)(ARMul_State *state);
static void SSD_Name(QuitHost)(ARMul_State *state);

#include "stdsounddev.c"

static SSD_SoundData *SSD_Name(GetHostBuffer)(ARMul_State *state,int32_t *destavail)
{
#ifdef SOUND_THREAD
  /* Work out how much space is available until next wrap point, or we start overwriting data */
  pthread_mutex_lock(&mut);
  int32_t local_buffer_in = sound_buffer_in;
  int32_t used = local_buffer_in-sound_buffer_out;
  pthread_mutex_unlock(&mut);
  int32_t ofs = local_buffer_in & sound_buff_mask;
  int32_t buffree = BUFFER_SAMPLES-MAX(ofs,used);
  *destavail = buffree>>1;
  return sound_buffer + ofs;
#else
  /* Just assume we always have enough space for the max batch size */
  *destavail = sizeof(sound_buffer)/(sizeof(SSD_SoundData)*2);
  return sound_buffer;
#endif
}

static void SSD_Name(HostBuffered)(ARMul_State *state,SSD_SoundData *buffer, int32_t numSamples)
{
  numSamples <<= 1;
#ifdef SOUND_THREAD
  pthread_mutex_lock(&mut);
  int32_t local_buffer_in = sound_buffer_in;
  int32_t used = local_buffer_in-sound_buffer_out;
  pthread_mutex_unlock(&mut);
  int32_t ofs = local_buffer_in & sound_buff_mask;

  local_buffer_in += numSamples;
  
  if(buffree == numSamples)
  {
    fprintf(stderr,"*** sound overflow! ***\n");
    if(SI.FudgeRate < -10)
      SI.FudgeRate = SI.FudgeRate/2;
    else
      SI.FudgeRate+=10;
  }
  else if(!used)
  {
    fprintf(stderr,"*** sound underflow! ***\n");
    if(SI.FudgeRate > 10)
      SI.FudgeRate = SI.FudgeRate/2;
    else
      SI.FudgeRate-=10;
  }
  else if(used < BUFFER_SAMPLES/4)
  {
    SI.FudgeRate--;
  }
  else if(buffree < BUFFER_SAMPLES/4)
  {
    SI.FudgeRate++;
  }
  else if(SI.FudgeRate)
  {
    /* Bring the fudge value back towards 0 until we go out of the comfort zone */
    SI.FudgeRate += (SI.FudgeRate>0?-1:1);
  }

  pthread_mutex_lock(&mut);
  sound_buffer_in = local_buffer_in;
  pthread_mutex_unlock(&mut);
  pthread_cond_broadcast(&cond);

  pthread_yield();
#else
  audio_buf_info buf;
  if (ioctl(soundDevice, SOUND_PCM_GETOSPACE, &buf) != -1) {
    /* Adjust fudge rate based around how much buffer space is available
       We aim for the buffer to be somewhere between 1/4 and 3/4 full, but don't
       explicitly set the buffer size, so we're at the mercy of the sound system
       in terms of how much lag there'll be */
    int32_t bufsize = buf.fragsize*buf.fragstotal;
    int32_t buffree = buf.bytes/sizeof(SSD_SoundData);
    int32_t used = (bufsize-buf.bytes)/sizeof(SSD_SoundData);
    int32_t stepsize = SI.DMARate>>2;
    bufsize /= sizeof(SSD_SoundData);
    if(numSamples > buffree)
    {
      fprintf(stderr,"*** sound overflow! %d %d %d %d ***\n",numSamples-buffree,ARMul_EmuRate,SI.FudgeRate,SI.DMARate);
      numSamples = buffree; /* We could block until space is available, but I'm woried we'd get stuck blocking forever because the FudgeRate increase wouldn't compensate for the ARMul cycles lost due to blocking */
      if(SI.FudgeRate < -stepsize)
        SI.FudgeRate = SI.FudgeRate/2;
      else
        SI.FudgeRate+=stepsize;
    }
    else if(!used)
    {
      fprintf(stderr,"*** sound underflow! %d %d %d ***\n",ARMul_EmuRate,SI.FudgeRate,SI.DMARate);
      if(SI.FudgeRate > stepsize)
        SI.FudgeRate = SI.FudgeRate/2;
      else
        SI.FudgeRate-=stepsize;
    }
    else if(used < bufsize/4)
    {
      SI.FudgeRate-=stepsize>>4;
    }
    else if(buffree < bufsize/4)
    {
      SI.FudgeRate+=stepsize>>4;
    }
    else if(SI.FudgeRate)
    {
      /* Bring the fudge value back towards 0 until we go out of the comfort zone */
      SI.FudgeRate += (SI.FudgeRate>0?-1:1);
    }
  }
  
  write(soundDevice,buffer,numSamples*sizeof(SSD_SoundData));
#endif
}

#ifdef SOUND_THREAD
static void *
sound_writeThread(void *arg)
{
  int32_t local_buffer_out = sound_buffer_out;
  for (;;) {
    int32_t avail;

    pthread_mutex_lock(&mut);
    sound_buffer_out = local_buffer_out;
    avail = sound_buffer_in-local_buffer_out;
    pthread_mutex_unlock(&mut);

    printf("%d\n",avail);
    if (avail) {
      int32_t ofs = local_buffer_out & sound_buff_mask;

      if(ofs + avail > BUFFER_SAMPLES) {
        /* We're about to wrap */
        avail = BUFFER_SAMPLES-ofs;
      }

      write(soundDevice, sound_buffer + ofs,
            avail * sizeof(SSD_SoundData));

      local_buffer_out += avail;
    } else {
      pthread_mutex_lock(&mut);
      pthread_cond_wait(&cond, &mut);
      pthread_mutex_unlock(&mut);
    }
  }

  return NULL;
}
#endif

static int SSD_Name(InitHost)(ARMul_State *state)
{
  if ((soundDevice = open("/dev/dsp", O_WRONLY )) < 0) {
    fprintf(stderr, "Could not open audio device /dev/dsp\n");
    return -1;
  }

  if (ioctl(soundDevice, SOUND_PCM_RESET, 0) == -1) {
    fprintf(stderr, "Could not reset PCM\n");
    return -1;
  }

  if (ioctl(soundDevice, SOUND_PCM_SYNC, 0) == -1) {
    fprintf(stderr, "Could not sync PCM\n");
    return -1;
  }

  if (ioctl(soundDevice, SOUND_PCM_SETFMT, &format) == -1) {
    fprintf(stderr, "Could not set PCM format\n");
    return -1;
  }

  if (ioctl(soundDevice, SOUND_PCM_WRITE_CHANNELS, &channels) == -1) {
    fprintf(stderr,"Could not set to 2 channel stereo\n");
    return -1;
  }

  if (ioctl(soundDevice, SOUND_PCM_WRITE_RATE, &sampleRate) == -1) {
    fprintf(stderr, "Could not set sample rate\n");
    return -1;
  }

  if (ioctl(soundDevice, SOUND_PCM_READ_RATE, &sampleRate) == -1) {
    fprintf(stderr, "Could not read sample rate\n");
    return -1;
  }

  /* Check that GETOSPACE is supported */
  audio_buf_info buf;
  if (ioctl(soundDevice, SOUND_PCM_GETOSPACE, &buf) == -1) {
    fprintf(stderr,"Could not read output space\n");
    return -1;
  }
  fprintf(stderr,"Sound buffer params: frags %d total %d size %d bytes %d\n",buf.fragments,buf.fragstotal,buf.fragsize,buf.bytes);

  SI.StereoSense = Stereo_LeftRight;

  /* Use a decent batch size */
  SI.BatchSize = 256;

  SI.HostRate = sampleRate<<10;

#ifdef SOUND_THREAD
  pthread_create(&thread, NULL, sound_writeThread, 0);
#endif

  return 0;
}

static void SSD_Name(QuitHost)(ARMul_State *state)
{
  /* TODO */
}

/*-----------------------------------------------------------------------------*/
int
SoundDev_Init(ARMul_State *state)
{
	/* Setup sound device */
	return SoundDev_Set(state, &SSD_SoundDev);
}
