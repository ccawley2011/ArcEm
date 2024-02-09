/*
  arch/stdsounddev.c

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
*/

#define MAX_BATCH_SIZE 1024

struct SSD_Name(SoundInfo) {
    int BatchSize; /* How many 16*2 sample batches to attempt to deliver to the platform code at once */
    CycleCount DMARate; /* How many cycles between DMA fetches */
    Sound_StereoSense StereoSense;
    CycleDiff FudgeRate; /* Extra fudge factor applied to Sound_DMARate */

    uint32_t HostRate; /* Rate of host sound system, in 1/1024 Hz. Must be set by host on init. */

    SSD_SoundData soundTable[256];
    ARMword channelAmount[8][2];

    SSD_SoundData soundBuffer[16*2*MAX_BATCH_SIZE];
    uint32_t soundBufferAmt; /* Number of stereo pairs buffered */
#define TIMESHIFT 9 /* Bigger values make the mixing more accurate. But 9 is the biggest value possible to avoid overflows in the 32bit accumulators. */
    uint32_t soundTime; /* Offset into 1st sample pair of buffer */
    uint32_t soundTimeStep; /* How many source samples per dest sample, fixed point with TIMESHIFT fraction bits */
    uint32_t soundScale; /* Output scale factor, 16.16 fixed point */
};

#ifdef SI
#undef SI
#endif
#define SI (*((struct SSD_Name(SoundInfo) *) state->Sound))

static void SSD_Name(UpdateDMARate)(ARMul_State *state)
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

static void SSD_Name(InitTable)(ARMul_State *state)
{
  unsigned i;

  for (i = 0; i < 256; i++) {
    /*
     * (not VIDC1, whoops...)
     * VIDC2:
     * 0 Sign
     * 4,3,2,1 Point on chord
     * 7,6,5 Chord select
     */

    uint32_t chordSelect = (i & 0xE0) >> 5;
    uint32_t pointSelect = (i & 0x1E) >> 1;
    uint32_t sign = (i & 1);

    uint32_t stepSize;

    const uint32_t scale = (0xFFFF / (247 * 2));
    uint32_t chordBase;
    int32_t sample;

    switch (chordSelect) {
      case 0: chordBase = 0;
              stepSize = scale / 16;
              break;
      case 1: chordBase = scale;
              stepSize = (2 * scale) / 16;
              break;
      case 2: chordBase = 3*scale;
              stepSize = (4 * scale) / 16;
              break;
      case 3: chordBase = 7*scale;
              stepSize = (8 * scale) / 16;
              break;
      case 4: chordBase = 15*scale;
              stepSize = (16 * scale) / 16;
              break;
      case 5: chordBase = 31*scale;
              stepSize = (32 * scale) / 16;
              break;
      case 6: chordBase = 63*scale;
              stepSize = (64 * scale) / 16;
              break;
      case 7: chordBase = 127*scale;
              stepSize = (128 * scale) / 16;
              break;
      /* End of chord 7 is 247 * scale. */

      default: chordBase = 0;
               stepSize = 0;
               break;
    }

    sample = chordBase + stepSize * pointSelect;

    if (sign == 1) { /* negative */
      SI.soundTable[i] = -sample;
    } else {
      SI.soundTable[i] = sample;
    }
  }
}

static void SSD_Name(StereoUpdated)(ARMul_State *state)
{
  int i = 0;

  for (i = 0; i < 8; i++) {
    uint8_t reg = VIDC.StereoImageReg[i];
    if(SI.StereoSense == Stereo_RightLeft)
      reg = 8-reg; /* Swap stereo */
    switch (reg) {
      /* Centre */
      case 4: SI.channelAmount[i][0] = (ARMword) (0.5*65536);
              SI.channelAmount[i][1] = (ARMword) (0.5*65536);
              break;

      /* Left 100% */
      case 1: SI.channelAmount[i][0] = (ARMword) (1.0*65536);
              SI.channelAmount[i][1] = (ARMword) (0.0*65536);
              break;
      /* Left 83% */
      case 2: SI.channelAmount[i][0] = (ARMword) (0.83*65536);
              SI.channelAmount[i][1] = (ARMword) (0.17*65536);
              break;
      /* Left 67% */
      case 3: SI.channelAmount[i][0] = (ARMword) (0.67*65536);
              SI.channelAmount[i][1] = (ARMword) (0.33*65536);
              break;

      /* Right 100% */
      case 7: SI.channelAmount[i][1] = (ARMword) (1.0*65536);
              SI.channelAmount[i][0] = (ARMword) (0.0*65536);
              break;
      /* Right 83% */
      case 6: SI.channelAmount[i][1] = (ARMword) (0.83*65536);
              SI.channelAmount[i][0] = (ARMword) (0.17*65536);
              break;
      /* Right 67% */
      case 5: SI.channelAmount[i][1] = (ARMword) (0.67*65536);
              SI.channelAmount[i][0] = (ARMword) (0.33*65536);
              break;

      /* Bad setting - just mute it */
      default: SI.channelAmount[i][0] = SI.channelAmount[i][1] = 0;
    }
  }
}

static void SSD_Name(SoundFreqUpdated)(ARMul_State *state)
{
  /* Do nothing for now */
}

static void SSD_Name(Log2Lin)(ARMul_State *state, const uint8_t *in,SSD_SoundData *out,int32_t avail)
{
  /* Convert the source log data to linear. Note that no mixing is done here. */
  avail *= 2;
  while(avail--)
  {
#ifdef HOST_BIGENDIAN
    /* Byte accesses must be endian swapped.
       This makes sure the stereo positions are correct, and that the samples
       come through in the right order for the mixing algorithm to work. */
    SSD_SoundData val0 = SI.soundTable[in[3]];
    SSD_SoundData val1 = SI.soundTable[in[2]];
    SSD_SoundData val2 = SI.soundTable[in[1]];
    SSD_SoundData val3 = SI.soundTable[in[0]];
    *out++ = (SI.channelAmount[0][0] * val0)>>16;
    *out++ = (SI.channelAmount[0][1] * val0)>>16;
    *out++ = (SI.channelAmount[1][0] * val1)>>16;
    *out++ = (SI.channelAmount[1][1] * val1)>>16;
    *out++ = (SI.channelAmount[2][0] * val2)>>16;
    *out++ = (SI.channelAmount[2][1] * val2)>>16;
    *out++ = (SI.channelAmount[3][0] * val3)>>16;
    *out++ = (SI.channelAmount[3][1] * val3)>>16;
    val0 = SI.soundTable[in[7]];
    val1 = SI.soundTable[in[6]];
    val2 = SI.soundTable[in[5]];
    val3 = SI.soundTable[in[4]];
    *out++ = (SI.channelAmount[4][0] * val0)>>16;
    *out++ = (SI.channelAmount[4][1] * val0)>>16;
    *out++ = (SI.channelAmount[5][0] * val1)>>16;
    *out++ = (SI.channelAmount[5][1] * val1)>>16;
    *out++ = (SI.channelAmount[6][0] * val2)>>16;
    *out++ = (SI.channelAmount[6][1] * val2)>>16;
    *out++ = (SI.channelAmount[7][0] * val3)>>16;
    *out++ = (SI.channelAmount[7][1] * val3)>>16;
    in += 8;
#else
    int i;
    for(i=0;i<8;i++)
    {
      SSD_SoundData val = SI.soundTable[*in++];
      *out++ = (SI.channelAmount[i][0] * val)>>16;
      *out++ = (SI.channelAmount[i][1] * val)>>16;
    }
#endif
  }
}

static int32_t SSD_Name(Mix)(ARMul_State *state,SSD_SoundData *out,int32_t destavail)
{
  /* This mixing function performs two roles:
  
     1. Mixing together the N-channel source data to produce 2-channel output
     2. Resampling the data to match the host sample rate

     The algorithm works by treating each source sample as if it has a duration
     of 8 times the sample period. I.e. as if the source data is 8-channel, but
     with each channel offset by a different amount. So if no sample rate
     conversion was in effect, the value of destination sample N would be as
     follows:

        sum(N-7,N-6,N-5,N-4,N-3,N-2,N-1,N)/8
        
     This provides a rough approximation of the time division multiplexing that
     the Arc hardware uses to simulate the presence of multiple sound channels.

     Sample rate conversion is performed by evaluating the above equation for
     M adjacent samples, weighting each result, and calculating the sum. The
     lack of linear interpolation does make the sound a little rough, but it
     helps to keep the algorithm simple, as it becomes trivial to calculate
     the amount by which each source sample contributes to the output.

     Effectively, for each destination sample, the algorithm just calculates
     the (scaled) sum of the area marked in the below diagram:

     D                                             XXXXXXXX
     T       0   1   2   3   4   5   6   7   8   9  10  11  12
     S0      XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
     S1          XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
     S2              XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX*
     S3                  XXXXXXXXXXXXXXXXXXXXXXXXXX*****
     S4                      XXXXXXXXXXXXXXXXXXXXXX********X
     S5                          XXXXXXXXXXXXXXXXXX********XXXXX
     S6                              XXXXXXXXXXXXXX********XXXXX
     S7                                  XXXXXXXXXX********XXXXX
     S8                                      XXXXXX********XXXXX
     S9                                          XX********XXXXX
     S10                                            *******XXXXX
     S11                                                ***XXXXX
     S12                                                    XXXX

     (D=destination sample, T=source time, Sn = source samples)

     Note that the area of the marked region is 8*R (where R is the ratio of
     the two sample rates).

     In the below code, 'amt' is used for two purposes:

     1. Keeping track of the contribution of a source sample
     2. Keeping track of the current time (as an offset from the start time)

     In both situations, 'amt' stores the its as the number of source clock
     ticks (shifted by TIMESHIFT). 
  */
     
  const SSD_SoundData *in = SI.soundBuffer;
  int32_t srcavail = SI.soundBufferAmt;
  uint32_t time = SI.soundTime;
  const int32_t timestep = SI.soundTimeStep;
  const uint32_t scale = SI.soundScale;

  /* We can only generate a destination sample if all the required source
     samples are present. Bias the source sample count by a suitable amount
     so we don't have to worry about this in the main loop. */
  srcavail -= 10+(timestep>>TIMESHIFT);

  if(timestep > 8<<TIMESHIFT)
  {
    /* Big downmix factor. This factor is so big that, unlike in the above
       diagram, no source sample will ever have both of its ends cropped off
       by the width of the sampling window.

       Note that we're using two sets of accumulators to avoid overflow
       First set will count to 32768*8*8<<TIMESHIFT
       Second set will count to significantly less due to lack of multipliers */
    while((srcavail > 0) && (destavail > 0))
    {
      int32_t lacc=0,racc=0;
      int32_t lacc2=0,racc2=0;
      const SSD_SoundData *oldin = in;
      int32_t amt = (1<<TIMESHIFT)-time;
      /* Calculate the sum of the first few samples
         'amt' is being used to store the contribution factor */ 
      while(amt<(8<<TIMESHIFT))
      {
        lacc += *in++*amt;
        racc += *in++*amt;
        amt += 1<<TIMESHIFT;
      }
      /* Calculate the sum of the middle bit (using the 2nd accumulator set)
         'amt' is being used to store the time (contriubtion factor is fixed
         at 8 source ticks) */
      while(amt<=timestep)
      {
        lacc2 += *in++;
        racc2 += *in++;
        amt += 1<<TIMESHIFT;
      }
      /* Calculate the sum of the tail end
         'amt' is being used to store the contribution factor */
      amt = (8<<TIMESHIFT)-(amt-timestep);
      while(amt>0)
      {
        lacc += *in++*amt;
        racc += *in++*amt;
        amt -= 1<<TIMESHIFT;
      }
      lacc2 += lacc>>(3+TIMESHIFT);
      racc2 += racc>>(3+TIMESHIFT);
      *out++ = (lacc2*scale)>>16;
      *out++ = (racc2*scale)>>16;
      destavail--;
      time += timestep;
      amt = time>>TIMESHIFT;
      time &= (1<<TIMESHIFT)-1;
      in = oldin+amt*2;
      srcavail -= amt;
    }
  }
  else
  {
    /* Small downmix factor
       Accumulators will reach max of 32768*8*timestep */
    while((srcavail > 0) && (destavail > 0))
    {
      int32_t lacc=0,racc=0;
      int32_t amt = (8<<TIMESHIFT)-time;
      /* Work backwards from the last sample that has both its start and end
         cropped by the 'sides' of the sampling window. This corresponds to
         S4-S8 in the diagram above.
         'amt' is being used to store the time (contribution factor is fixed
         at 'timestep') */
      in += 16;
      while(amt > timestep)
      {
        racc += *--in;
        lacc += *--in;
        amt -= 1<<TIMESHIFT;
      }
      lacc *= timestep;
      racc *= timestep;
      /* Calculate the sum of the first and last few samples. This corresponds
         to S2, S3, S9, and S10 in the diagram above
         'amt' is being used to store the contribution factor of the first
         few samples; for the last few it's just (timestep-amt). */
      while(amt > 0)
      {
        in -= 2;
        lacc += in[0]*amt + in[16]*(timestep-amt);
        racc += in[1]*amt + in[17]*(timestep-amt);
        amt -= 1<<TIMESHIFT;
      }
      lacc = lacc>>(3+TIMESHIFT);
      racc = racc>>(3+TIMESHIFT);
      *out++ = (lacc*scale)>>16;
      *out++ = (racc*scale)>>16;
      destavail--;
      time += timestep;
      amt = time>>TIMESHIFT;
      time &= (1<<TIMESHIFT)-1;
      in += amt*2;
      srcavail -= amt;
    }
  }

  /* Update globals */
  srcavail += 10+(timestep>>TIMESHIFT);
  memmove(SI.soundBuffer,in,srcavail*sizeof(SSD_SoundData)*2); /* TODO - Improve this. Should only memmove() once we're near the end of the buffer. */
  SI.soundBufferAmt = srcavail;
  SI.soundTime = time;

  /* Return remaining output space */
  return destavail;
}

static void SSD_Name(DoMix)(ARMul_State *state)
{
  int32_t destavail;
  SSD_SoundData *out;
  if(SI.soundBufferAmt <= 10+(SI.soundTimeStep>>TIMESHIFT))
    return;
  /* Get host buffer params */
  out = SSD_Name(GetHostBuffer)(state,&destavail);
  if(destavail)
  {
    /* Mix into host buffer */
    int32_t remain = SSD_Name(Mix)(state,out,destavail);
    /* Tell the host */
    SSD_Name(HostBuffered)(state,out,destavail-remain);
  }
}

static void SSD_Name(Process)(ARMul_State *state,int32_t avail)
{
  /* Recalc soundTimeStep */
  static uint8_t oldsoundfreq=0;
  static uint8_t oldioebcr=0;
  static uint32_t oldhostrate=0;
  if((VIDC.SoundFreq != oldsoundfreq) || (ioc.IOEBControlReg != oldioebcr) || (SI.HostRate != oldhostrate))
  {
    uint32_t clockin;
    uint64_t a, b;
    oldsoundfreq = VIDC.SoundFreq;
    oldioebcr = ioc.IOEBControlReg;
    oldhostrate = SI.HostRate;
    /* Arc sample rate has most likely changed; process as much of the existing buffer as possible (using the current step values) */
    SSD_Name(DoMix)(state);
    clockin = DisplayDev_GetVIDCClockIn();
    /* Arc sound runs at a rate of (clockin*1024)/(24*(VIDC.SoundFreq+2)) in 1/1024Hz units
       We need that divided by SI.HostRate, and the reciprocal */
    a = ((uint64_t) clockin)*1024;
    b = ((uint64_t) SI.HostRate)*24*(VIDC.SoundFreq+2);
    SI.soundTimeStep = (uint32_t)((a<<TIMESHIFT)/b);
    SI.soundScale = (uint32_t)((b<<16)/a);
    warn_vidc("New sample period %d (VIDC %dMHz) host %dHz -> timestep %08x scale %08x\n",VIDC.SoundFreq+2,clockin/1000000,SI.HostRate>>10,SI.soundTimeStep,SI.soundScale);
    SI.soundTime = 0;
  }
  if(avail)
  {
    /* Log -> lin conversion */
    SSD_Name(Log2Lin)(state,((uint8_t *) MEMC.PhysRam) + MEMC.Sptr,SI.soundBuffer+(SI.soundBufferAmt<<1),avail);
    SI.soundBufferAmt += avail<<4;
  }
  /* Process this new data */
  SSD_Name(DoMix)(state);
}

static void SSD_Name(DMAEvent)(ARMul_State *state,CycleCount nowtime)
{
  int32_t srcbatchsize, avail;
  int32_t bufspace;
  CycleCount next;
  SSD_Name(UpdateDMARate)(state);
  /* Work out how many source samples are required to generate SI.BatchSize dest samples */
  srcbatchsize = (SI.BatchSize*SI.soundTimeStep)>>TIMESHIFT;
  if(!srcbatchsize)
    srcbatchsize = 1;
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
    bufspace = (16*MAX_BATCH_SIZE-SI.soundBufferAmt)>>4;
    if(avail > bufspace)
      avail = bufspace;
  }
  /* Process data first, so host can adjust fudge rate */
  SSD_Name(Process)(state,avail);
  /* Work out when to reschedule the event
     TODO - This is wrong; there's no guarantee the host accepted all the data we wanted to give him */
  next = SI.DMARate*(avail?avail:srcbatchsize)+SI.FudgeRate;
  /* Clamp to a safe minimum value */
  if(next < 100)
    next = 100;
  EventQ_RescheduleHead(state,nowtime+next,SSD_Name(DMAEvent));
  /* Update DMA stuff */
  MEMC.Sptr += avail<<4;
}

static int SSD_Name(Init)(ARMul_State *state)
{
  state->Sound = calloc(sizeof(struct SSD_Name(SoundInfo)),1);
  if(!state->Sound) {
    warn_vidc("Failed to allocate SoundInfo\n");
    return -1;
  }

  SI.BatchSize = 1; /* How many 16*2 sample batches to try to do at once */
  SI.StereoSense = Stereo_LeftRight;
  SI.FudgeRate = 0;
  SI.soundBufferAmt = 0; /* Number of stereo pairs buffered */
  SI.soundTime = 0; /* Offset into 1st sample pair of buffer */

  SSD_Name(InitTable)(state);
  SSD_Name(UpdateDMARate)(state);
  EventQ_Insert(state,ARMul_Time+SI.DMARate, SSD_Name(DMAEvent));
  return SSD_Name(InitHost)(state);
}

static void SSD_Name(Shutdown)(ARMul_State *state)
{
  int idx = EventQ_Find(state,SSD_Name(DMAEvent));
  if(idx >= 0)
    EventQ_Remove(state,idx);
  else
  {
    ControlPane_Error(EXIT_FAILURE,"Couldn't find SSD_Name(DMAEvent)!\n");
  }
  SSD_Name(QuitHost)(state);
  free(state->Sound);
  state->Sound = NULL;
}

const SoundDev SSD_SoundDev = {
  SSD_Name(Init),
  SSD_Name(Shutdown),
  SSD_Name(SoundFreqUpdated),
  SSD_Name(StereoUpdated),
};

/*

  The end

*/
