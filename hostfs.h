#ifndef HOSTFS_H
#define HOSTFS_H

#include "armdefs.h"
#include "dbugsys.h"

#define ARCEM_SWI_CHUNK    0x56ac0
#define ARCEM_SWI_SHUTDOWN  (ARCEM_SWI_CHUNK + 0)
#define ARCEM_SWI_HOSTFS    (ARCEM_SWI_CHUNK + 1)
#define ARCEM_SWI_DEBUG     (ARCEM_SWI_CHUNK + 2)
#define ARCEM_SWI_NANOSLEEP (ARCEM_SWI_CHUNK + 3)
#define ARCEM_SWI_NETWORK   (ARCEM_SWI_CHUNK + 4)

#define hostfs_error ControlPane_Error

#define HOSTFS_ARCEM /* Build ArcEm version, not RPCEmu */

extern void hostfs(ARMul_State *state);
extern void hostfs_init(void);
extern void hostfs_reset(void);


#endif
