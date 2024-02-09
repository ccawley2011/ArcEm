/*
  arch/nullsounddev.c

  (c) 2011 Jeffrey Lee <me@phlamethrower.co.uk>
  Based on original sound code by Daniel Clarke

  Part of Arcem released under the GNU GPL, see file COPYING
  for details.

  This is the core of the sound system and is used to drive the platform-specifc
  frontend code. At intervals approximating the correct DMA interval, data is
  fetched from memory and converted from the Arc 8-bit log format to 16-bit
  linear. The sample stream is then downmixed in a manner that roughly
  approximates the behaviour of the hardware, in order to convert the N-channel
  source data to a single stereo stream. The converted data is then fed to the
  platform code for output to the sound hardware.

  Even if SOUND_SUPPORT is disabled, this code will still request data from the
  core at the correct intervals, so emulated code that relies on sound IRQs
  should run correctly.
*/

struct NSD_Name(SoundInfo) {
    CycleCount DMARate; /* How many cycles between DMA fetches */
};

#ifdef SI
#undef SI
#endif
#define SI (*((struct NSD_Name(SoundInfo) *) state->Sound))

static void NSD_Name(UpdateDMARate)(ARMul_State *state)
{
  /* Calculate a new value for how often we should trigger a sound DMA fetch
     Relies on:
     VIDC.SoundFreq - the rate of the sound system we're trying to emulate
     ARMul_EmuRate - roughly how many EventQ clock cycles occur per second
     ioc.IOEBControlReg - the VIDC clock source */
  static uint8_t oldsoundfreq = 0;
  static uint32_t oldemurate = 0;
  static uint8_t oldioebcr = 0;
  if((VIDC.SoundFreq == oldsoundfreq) && (ARMul_EmuRate == oldemurate) && (ioc.IOEBControlReg == oldioebcr))
    return;
  oldsoundfreq = VIDC.SoundFreq;
  oldemurate = ARMul_EmuRate;
  oldioebcr = ioc.IOEBControlReg;
  /* DMA fetches 16 bytes, at a rate of 1000000/(16*(VIDC.SoundFreq+2)) Hz, for a 24MHz VIDC clock
     So for a variable clock, and taking into account ARMul_EmuRate, we get:
     SI.DMARate = ARMul_EmuRate*16*(VIDC.SoundFreq+2)*24/VIDC_clk
 */
  SI.DMARate = (((uint64_t) ARMul_EmuRate)*(16*24)*(VIDC.SoundFreq+2))/DisplayDev_GetVIDCClockIn();
//  warn_vidc("UpdateDMARate: f %d r %u -> %u\n",VIDC.SoundFreq,ARMul_EmuRate,SI.DMARate);
}

static void NSD_Name(StereoUpdated)(ARMul_State *state)
{
  /* Do nothing for now */
}

static void NSD_Name(SoundFreqUpdated)(ARMul_State *state)
{
  /* Do nothing for now */
}

static void NSD_Name(DMAEvent)(ARMul_State *state,CycleCount nowtime)
{
  int32_t srcbatchsize, avail;
  CycleCount next;
  NSD_Name(UpdateDMARate)(state);
  srcbatchsize = 4;
  /* How many DMA fetches are possible? */
  avail = 0;
  if(MEMC.ControlReg & (1 << 11))
  {
    /* Trigger any pending buffer swap */
    if(MEMC.Sptr > MEMC.SendC)
    {
      /* Have the next buffer addresses been written? */
      if (MEMC.NextSoundBufferValid == 1) {
        /* Yes, so change to the next buffer */
        ARMword swap;
  
        MEMC.Sptr = MEMC.Sstart;
        MEMC.SstartC = MEMC.Sstart;
  
        swap = MEMC.SendC;
        MEMC.SendC = MEMC.SendN;
        MEMC.SendN = swap;
  
        ioc.IRQStatus |= IRQB_SIRQ; /* Take sound interrupt on */
        IO_UpdateNirq(state);
  
        MEMC.NextSoundBufferValid = 0;
      } else {
        /* Otherwise wrap to the beginning of the buffer */
        MEMC.Sptr = MEMC.SstartC;
      }
    }
    avail = ((MEMC.SendC+16)-MEMC.Sptr)>>4;
    if(avail > srcbatchsize)
      avail = srcbatchsize;
  }
  /* Process data first, so host can adjust fudge rate */
  /* Work out when to reschedule the event
     TODO - This is wrong; there's no guarantee the host accepted all the data we wanted to give him */
  next = SI.DMARate*(avail?avail:srcbatchsize);
  /* Clamp to a safe minimum value */
  if(next < 100)
    next = 100;
  EventQ_RescheduleHead(state,nowtime+next,NSD_Name(DMAEvent));
  /* Update DMA stuff */
  MEMC.Sptr += avail<<4;
}

static int NSD_Name(Init)(ARMul_State *state)
{
  state->Sound = calloc(sizeof(struct NSD_Name(SoundInfo)),1);
  if(!state->Sound) {
    warn_vidc("Failed to allocate SoundInfo\n");
    return -1;
  }

  NSD_Name(UpdateDMARate)(state);
  EventQ_Insert(state,ARMul_Time+SI.DMARate, NSD_Name(DMAEvent));
  return 0;
}

static void NSD_Name(Shutdown)(ARMul_State *state)
{
  int idx = EventQ_Find(state,NSD_Name(DMAEvent));
  if(idx >= 0)
    EventQ_Remove(state,idx);
  else
  {
    ControlPane_Error(EXIT_FAILURE,"Couldn't find NSD_Name(DMAEvent)!\n");
  }
  free(state->Sound);
  state->Sound = NULL;
}

const SoundDev NSD_SoundDev = {
  NSD_Name(Init),
  NSD_Name(Shutdown),
  NSD_Name(SoundFreqUpdated),
  NSD_Name(StereoUpdated),
};

/*

  The end

*/
