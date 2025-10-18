/* Redraw and other services for the control pane */
/* (c) David Alan Gilbert 1995 - see Readme file for copying info */


#include "../armdefs.h"
#include "archio.h"
#include "armarc.h"
#include "ControlPane.h"
#include "arch/dbugsys.h"

#include <stdarg.h>
#include <stdio.h>
#include <libdragon.h>

bool ControlPane_Init(ARMul_State *state)
{
  return true;
}

void ControlPane_Error(bool fatal,const char *fmt,...)
{
  va_list args;

  console_init();

  va_start(args,fmt);
  log_msgv(LOG_ERROR,fmt,args);
  va_end(args);

  /* Quit */
  while(1) {}
}

void log_msgv(int type, const char *format, va_list ap)
{
  vfprintf(stdout, format, ap);
}
