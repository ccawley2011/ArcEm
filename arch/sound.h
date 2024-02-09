#ifndef SOUND_H
#define SOUND_H

#include "../c99.h"

typedef enum {
  Stereo_LeftRight, /* Data is ordered with left channel first */
  Stereo_RightLeft, /* Data is ordered with right channel first */
} Sound_StereoSense;

typedef struct {
	int (*Init)(ARMul_State *state); /* Initialise sound device, return nonzero on failure */
	void (*Shutdown)(ARMul_State *state); /* Shutdown sound device */
	void (*SoundFreqUpdated)(ARMul_State *state); /* Called whenever the VIDC frequency register is updated */
	void (*StereoUpdated)(ARMul_State *state); /* Called whenever the VIDC stereo image registers are updated */
} SoundDev;

extern int SoundDev_Init(ARMul_State *state); /* Switch to indicated sound device, returns nonzero on failure */

/* Host must provide this function to initialize the default display device */
extern int SoundDev_Set(ARMul_State *state, const SoundDev *dev);

/**
 * Sound_SoundFreqUpdated
 *
 * Called whenever the VIDC frequency register is updated.
 */
extern void Sound_SoundFreqUpdated(ARMul_State *state);

/**
 * Sound_StereoUpdated
 *
 * Called whenever the VIDC stereo image registers are updated so that the
 * channelAmount array can be recalculated and the new number of channels can
 * be figured out.
 */
extern void Sound_StereoUpdated(ARMul_State *state);

#endif
