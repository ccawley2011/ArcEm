#ifndef ARCH_JOYSTICK_H
#define ARCH_JOYSTICK_H

#include "../armdefs.h"

#define MAX_AXES 2
#define MAX_BUTTONS 32
#define MAX_JOYSTICKS 2

struct arch_joystick {
	struct stick {
		int axis[MAX_AXES];
		uint32_t buttons;

		int naxes;
		int nbuttons;
	} stick[MAX_JOYSTICKS];

	int njoysticks;
};

#define JOY (*(state->Joy))

void joystick_axis_changed(struct arch_joystick* joy, int id, int axis, int position);
void joystick_button_changed(struct arch_joystick* joy, int id, int button, bool up);
void joystick_add(struct arch_joystick* joy, int buttons, int axes);

void Joy_Init(ARMul_State* state);
ARMword RTFM_Read(ARMul_State* state, ARMword offset);

/* Internal function; just exposed so the profiling code can mess with it */
void Joystick_Poll(ARMul_State* state, CycleCount nowtime);

/* Frontend must implement this */
int Joy_PollHostJoy(ARMul_State* state);

#endif

