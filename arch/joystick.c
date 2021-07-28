#include "armarc.h"
#include "joystick.h"

#define DEBUG_JOYSTICK 0
#if DEBUG_JOYSTICK
#define DBG(a) fprintf a
#else
#define DBG(a)
#endif

/* ------------------------------------------------------------------ */

void joystick_axis_changed(struct arch_joystick* joy, int id, int axis, int position) {
    DBG((stderr, "joystick_button_changed(joy, %d, %d, %d)\n", id, button, up));

    if (id >= joy->njoysticks) {
        fprintf(stderr, "joystick_button_changed: invalid joystick id %d\n", id);
        return;
    }

    if (id >= joy->stick[id].naxes) {
        fprintf(stderr, "joystick_button_changed: invalid axis %d for joystick %d\n", axis, id);
        return;
    }

    joy->stick[id].axis[axis] = position;
}

void joystick_button_changed(struct arch_joystick* joy, int id, int button, bool up) {
    DBG((stderr, "joystick_button_changed(joy, %d, %d, %d)\n", id, button, up));

    if (id >= joy->njoysticks) {
        fprintf(stderr, "joystick_button_changed: invalid joystick id %d\n", id);
        return;
    }

    if (id >= joy->stick[id].nbuttons) {
        fprintf(stderr, "joystick_button_changed: invalid button %d for joystick %d\n", button, id);
        return;
    }

    if (up) {
        joy->stick[id].buttons &= ~(1 << button);
    } else {
        joy->stick[id].buttons |= (1 << button);
    }
}

void joystick_add(struct arch_joystick* joy, int buttons, int axes) {
    DBG((stderr, "joystick_add(joy, %d, %d)\n", buttons, axes));

    if (joy->njoysticks >= MAX_JOYSTICKS) {
        fprintf(stderr, "joystick_add: reached max number of joysticks\n");
        return;
    }

    joy->stick[joy->njoysticks].nbuttons = buttons;
    if (joy->stick[joy->njoysticks].nbuttons > MAX_BUTTONS)
        joy->stick[joy->njoysticks].nbuttons = MAX_BUTTONS;

    joy->stick[joy->njoysticks].naxes = axes;
    if (joy->stick[joy->njoysticks].naxes > MAX_AXES)
        joy->stick[joy->njoysticks].naxes = MAX_AXES;

    joy->njoysticks++;
}

void Joystick_Poll(ARMul_State* state, CycleCount nowtime)
{
    EventQ_RescheduleHead(state, nowtime + 12500, Joystick_Poll); /* TODO - Should probably be realtime */
    /* Call host-specific routine */
    Joy_PollHostJoy(state);
}

void Joy_Init(ARMul_State* state)
{
    static arch_joystick joy;
    state->Joy = &joy;

    EventQ_Insert(state, ARMul_Time + 12500, Joystick_Poll);
}


enum {
    RTFM_Left1  = 0x01,
    RTFM_Right1 = 0x02,
    RTFM_Up1    = 0x04,
    RTFM_Down1  = 0x08,
    RTFM_Fire1  = 0x10,

    RTFM_Left2  = 0x08,
    RTFM_Right2 = 0x20,
    RTFM_Up2    = 0x40,
    RTFM_Down2  = 0x80,
    RTFM_Fire2  = 0x10,
};

/*
 * Based on code from Arculator: https://github.com/sarah-walker-pcem/arculator/blob/master/src/joystick.c#L148
 */
ARMword RTFM_Read(ARMul_State* state, ARMword offset) {

    uint8_t temp = 0xff;

    switch (offset & 0xc) {
    case 4:
        temp = 0;
        if (JOY.stick[0].axis[0] > 16383)
            temp |= RTFM_Left1;
        if (JOY.stick[0].axis[0] < -16383)
            temp |= RTFM_Right1;
        if (JOY.stick[0].axis[1] > 16383)
            temp |= RTFM_Up1;
        if (JOY.stick[0].axis[1]< -16383)
            temp |= RTFM_Down1;
        if (JOY.stick[0].buttons & 1)
            temp |= RTFM_Fire1;
        break;
    case 8:
        temp = 0;
        if (JOY.stick[1].axis[0] > 16383)
            temp |= RTFM_Left2;
        if (JOY.stick[1].axis[0] < -16383)
            temp |= RTFM_Right2;
        if (JOY.stick[1].axis[1] > 16383)
            temp |= RTFM_Up2;
        if (JOY.stick[1].axis[1] < -16383)
            temp |= RTFM_Down2;
        if (JOY.stick[1].buttons & 1)
            temp |= RTFM_Fire2;
        break;
    }
    return temp;
}
