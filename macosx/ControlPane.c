/* Redraw and other services for the control pane */
/* (c) David Alan Gilbert 1995 - see Readme file for copying info */


#include "armdefs.h"
#include "archio.h"
#include "armarc.h"
#include "DispKbd.h"
#include "ControlPane.h"

#include <stdarg.h>
#include <stdio.h>

void ControlPane_Init(ARMul_State *state)
{

}

void ControlPane_MessageBox(const char *fmt,...)
{
  va_list args;

  /* Log it */
  va_start(args,fmt);
  vfprintf(stderr,fmt,args);
  va_end(args);
}
