/* (c) David Alan Gilbert 1995 - see Readme file for copying info */
#ifndef CONTROLPANE_HEADER
#define CONTROLPANE_HEADER

#ifdef SYSTEM_X 
#include "X11/Xlib.h"
#include "X11/Xutil.h"
#endif

bool ControlPane_Init(ARMul_State *state);

#ifdef SYSTEM_X 
void ControlPane_Event(ARMul_State *state, XEvent *e);
#endif

/* Report an error and exit when fatal */
GNU_PRINTF(2, 3) void ControlPane_Error(bool fatal,MSVC_PRINTF const char *fmt,...);

#endif
