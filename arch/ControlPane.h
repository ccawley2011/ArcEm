/* (c) David Alan Gilbert 1995 - see Readme file for copying info */
#ifndef CONTROLPANE_HEADER
#define CONTROLPANE_HEADER

#ifdef SYSTEM_X 
#include "X11/Xlib.h"
#include "X11/Xutil.h"
#endif

void ControlPane_Init(ARMul_State *state);

#ifdef SYSTEM_X 
void ControlPane_Event(ARMul_State *state, XEvent *e);
#endif

/* Report an error */
void ControlPane_MessageBox(const char *fmt,...);

/* Report an error and exit */
#ifdef SYSTEM_riscos_single
void ControlPane_Error(int code,const char *fmt,...);
#else
#define ControlPane_Error(code,...) (ControlPane_MessageBox(__VA_ARGS__), exit(code))
#endif

#endif
