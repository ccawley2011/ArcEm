/* (c) David Alan Gilbert 1995-1999 - see Readme file for copying info
   Nintendo 64 version by Cameron Cawley, 2025 */
/* Display and keyboard interface for the Arc emulator */

#include <libdragon.h>

#include "../armdefs.h"
#include "armarc.h"
#include "archio.h"
#include "dagstandalone.h"
#include "eventq.h"
#include "arch/dbugsys.h"
#include "arch/displaydev.h"
#include "arch/keyboard.h"
#include "ControlPane.h"

/* An upper limit on how big to support monitor size, used for
   allocating a scanline buffer and bounds checking. It's much
   more than a VIDC1 can handle, and should be pushing the RPC/A7000
   VIDC too, if we ever get around to supporting that. */
#define MaxVideoWidth 2048
#define MaxVideoHeight 1536

static surface_t screen, cursor;
static uint16_t palette[256] __attribute__((aligned(16)));
static uint16_t cursor_palette[4] __attribute__((aligned(16)));
static color_t border_col;

/* Palettised display code */
#define PDD_Name(x) pdd_##x

typedef struct {
  ARMword *data;
  int offset;
} PDD_Row;

static void PDD_Name(Host_ChangeMode)(ARMul_State *state,int width,int height,int depth,int hz);

static void PDD_Name(Host_SetPaletteEntry)(ARMul_State *state,int i,uint_fast16_t phys)
{
  color_t col;

  /* Convert to 8-bit component values */
  col.r = (phys & 0xf)*0x11;
  col.g = ((phys>>4) & 0xf)*0x11;
  col.b = ((phys>>8) & 0xf)*0x11;
  col.a = 0xff;

  UncachedUShortAddr(palette)[i] = color_to_packed16(col);
}

static void PDD_Name(Host_SetCursorPaletteEntry)(ARMul_State *state,int i,uint_fast16_t phys)
{
  color_t col;

  /* Convert to 8-bit component values */
  col.r = (phys & 0xf)*0x11;
  col.g = ((phys>>4) & 0xf)*0x11;
  col.b = ((phys>>8) & 0xf)*0x11;
  col.a = 0xff;

  UncachedUShortAddr(cursor_palette)[i+1] = color_to_packed16(col);
}

static void PDD_Name(Host_SetBorderColour)(ARMul_State *state,uint_fast16_t phys)
{
  /* Convert to 8-bit component values */
  border_col.r = (phys & 0xf)*0x11;
  border_col.g = ((phys>>4) & 0xf)*0x11;
  border_col.b = ((phys>>8) & 0xf)*0x11;
  border_col.a = 0xff;
}

static inline PDD_Row PDD_Name(Host_BeginRow)(ARMul_State *state,int row,int offset,int *alignment)
{
  PDD_Row drow;
  uintptr_t base = ((uintptr_t)screen.buffer + screen.stride*row) + offset;
  drow.offset = ((base<<3) & 0x18); /* Just in case bytes per line isn't aligned */
  drow.data = (ARMword *) (base & ~0x3);
  *alignment = drow.offset;
  return drow;
}

static inline void PDD_Name(Host_EndRow)(ARMul_State *state,PDD_Row *row) { /* nothing */ }

static inline ARMword *PDD_Name(Host_BeginUpdate)(ARMul_State *state,PDD_Row *row,unsigned int count,int *outoffset)
{
  *outoffset = row->offset;
  return row->data;
}

static inline void PDD_Name(Host_EndUpdate)(ARMul_State *state,PDD_Row *row) { /* nothing */ }

static inline void PDD_Name(Host_AdvanceRow)(ARMul_State *state,PDD_Row *row,unsigned int count) {
  row->offset += count;
  row->data += count>>5;
  row->offset &= 0x1f;
}

static void PDD_Name(Host_PollDisplay)(ARMul_State *state);

static void PDD_Name(Host_DrawBorderRect)(ARMul_State *state,int x,int y,int width,int height)
{
  /* handled in Host_PollDisplay */
}

#include "../arch/paldisplaydev.c"

void PDD_Name(Host_ChangeMode)(ARMul_State *state,int width,int height,int depth,int hz)
{
  if (width > MaxVideoWidth || height > MaxVideoHeight) {
      ControlPane_Error(EXIT_FAILURE,"Resize_Window: new size (%d, %d) exceeds maximum (%d, %d)\n",
          width, height, MaxVideoWidth, MaxVideoHeight);
  }

  HD.Width = width;
  HD.Height = height;
  HD.XScale = 1;
  HD.YScale = 1;

  if (screen.buffer)
    surface_free(&screen);
  if (width > 0 && height > 0)
    screen = surface_alloc(FMT_CI8, width, height);

  if (cursor.buffer)
    surface_free(&cursor);
  if (width > 0 && height > 0)
    cursor = surface_alloc(FMT_CI4, 32, height);

  /* Calculate expansion params */
  if(depth == 3)
  {
    /* No expansion */
    HD.ExpandTable = NULL;
  }
  else
  {
    /* Expansion! */
    static ARMword expandtable[256];
    HD.ExpandFactor = (3-depth);
    HD.ExpandTable = expandtable;
    GenExpandTable(HD.ExpandTable,1<<depth,HD.ExpandFactor,1);
  }
}

/* Refresh the mouse's image */
static int PDD_Name(RefreshMouse)(ARMul_State *state) {
  int x,y,offset,memptr,pix0,pix1;
  int Height = ((int)VIDC.Vert_CursorEnd - (int)VIDC.Vert_CursorStart);
  uint8_t *curbmp = (uint8_t *)cursor.buffer;
  uint_fast16_t curstride = cursor.stride;

  offset=0;
  memptr=MEMC.Cinit*16;
  for(y=0;y<Height;y++) {
    if (offset<512*1024) {
      ARMword tmp[2];

      tmp[0]=MEMC.PhysRam[memptr/4];
      tmp[1]=MEMC.PhysRam[memptr/4+1];

      for(x=0;x<32;x+=2) {
        pix0 = ((tmp[(x+0)/16]>>(((x+0) & 15)*2)) & 3);
        pix1 = ((tmp[(x+1)/16]>>(((x+1) & 15)*2)) & 3);
        curbmp[x/2] = (pix0 << 4) | pix1;
      } /* x */
    } else break;
    memptr+=8;
    offset+=8;
    curbmp = (uint8_t *) ((uintptr_t)curbmp + curstride);
  }; /* y */

  return Height;
} /* RefreshMouse */

static void PDD_Name(Host_PollDisplay)(ARMul_State *state) {
  int HorizPos, VertPos, Height;
  rdpq_blitparms_t params = {};
  surface_t *display;

  Height = PDD_Name(RefreshMouse)(state);
  DisplayDev_GetCursorPos(state,&HorizPos,&VertPos);

  display = display_get();
  rdpq_attach(display, NULL);
  rdpq_clear(border_col);

  rdpq_set_mode_copy(false);
  rdpq_mode_tlut(TLUT_RGBA16);
  rdpq_tex_upload_tlut(UncachedUShortAddr(palette), 0, 256);
  rdpq_tex_blit(&screen, 0, 0, NULL);

  if (Height > 0) {
    params.s0 = 0;
    params.t0 = 0;
    params.width = 32;
    params.height = Height;
    rdpq_set_mode_copy(true);
    rdpq_mode_tlut(TLUT_RGBA16);
    rdpq_tex_upload_tlut(UncachedUShortAddr(cursor_palette), 0, 4);
    rdpq_tex_blit(&cursor, HorizPos, VertPos, &params);
  }

  rdpq_detach_wait();
  display_show(display);
}

/*-----------------------------------------------------------------------------*/
bool DisplayDev_Init(ARMul_State *state)
{
  return DisplayDev_Set(state,&PDD_DisplayDev);
}

/*-----------------------------------------------------------------------------*/
int Kbd_PollHostKbd(ARMul_State *state)
{
  joypad_buttons_t pressed, released;
  joypad_inputs_t inputs;
  int xdiff, ydiff;

  joypad_poll();
  inputs = joypad_get_inputs(JOYPAD_PORT_1);
  pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);
  released = joypad_get_buttons_released(JOYPAD_PORT_1);

  if (pressed.z)  keyboard_key_changed(&KBD, ARCH_KEY_button_1, 0);
  if (pressed.b)  keyboard_key_changed(&KBD, ARCH_KEY_button_2, 0);
  if (pressed.a)  keyboard_key_changed(&KBD, ARCH_KEY_button_3, 0);
  if (released.z) keyboard_key_changed(&KBD, ARCH_KEY_button_1, 1);
  if (released.b) keyboard_key_changed(&KBD, ARCH_KEY_button_2, 1);
  if (released.a) keyboard_key_changed(&KBD, ARCH_KEY_button_3, 1);

  xdiff = (inputs.stick_x);
  ydiff = (inputs.stick_y);

#ifdef DEBUG_MOUSEMOVEMENT
  dbug_kbd("MouseMoved: xdiff = %d  ydiff = %d\n",
           xdiff, ydiff);
#endif

  if (xdiff > 63)
    xdiff=63;
  if (xdiff < -63)
    xdiff=-63;

  if (ydiff>63)
    ydiff=63;
  if (ydiff<-63)
    ydiff=-63;

  KBD.MouseXCount = xdiff & 127;
  KBD.MouseYCount = ydiff & 127;

  return 0;
}

/*-----------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
  static const resolution_t RESOLUTION_640x256 = {640, 256, INTERLACE_OFF};
  display_init(RESOLUTION_640x256, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_DISABLED);
  dfs_init(DFS_DEFAULT_LOCATION);
  rtc_init_async();
  joypad_init();
  rdpq_init();

  return dagstandalone(argc, argv);
}
