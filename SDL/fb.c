/*
  SDL/fb.c

  (c) 2023 Cameron Cawley

  Standard display device implementation for framebuffer SDL displays

*/

#include "platform.h"

/* TODO: Allow selecting the display device at runtime? */
#if !SDL_VERSION_ATLEAST(2, 0, 0) || defined(SDL_PLATFORM_DOS)

#include <stdlib.h>

#include "../armdefs.h"
#include "../arch/ArcemConfig.h"
#include "../arch/armarc.h"
#include "../arch/archio.h"
#include "../eventq.h"
#include "../arch/dbugsys.h"
#include "../arch/displaydev.h"
#include "../arch/ControlPane.h"
#include <stdlib.h>

/* An upper limit on how big to support monitor size, used for
   allocating a scanline buffer and bounds checking. It's much
   more than a VIDC1 can handle, and should be pushing the RPC/A7000
   VIDC too, if we ever get around to supporting that. */
#define MinVideoWidth 512
#define MaxVideoWidth 2048
#define MaxVideoHeight 1536

static SDL_Surface *screen = NULL;
static SDL_Surface *sdd_surface = NULL;
static SDL_Surface *mouse_surface = NULL;
#if SDL_VERSION_ATLEAST(2, 0, 0)
static SDL_Palette *sdd_palette = NULL;
static SDL_Palette *mouse_palette = NULL;
#endif
static SDL_Rect mouse_rect;
static int palette_offset = 0;

static uint32_t GetColour(ARMul_State *state,unsigned int col);
static SDL_Color GetColourStruct(ARMul_State *state,unsigned int col);
static void SetupScreen(ARMul_State *state,int *width,int *height,int bpp);
static void PollDisplay(ARMul_State *state,int XScale,int YScale);

/* ------------------------------------------------------------------ */

/* Standard display device, 16bpp */

#define SDD_HostColour uint16_t
#define SDD_Name(x) sdd16_##x
#define SDD_RowsAtOnce 1
#define SDD_Row SDD_HostColour *
#define SDD_DisplayDev SDD16_DisplayDev

static SDD_HostColour SDD_Name(Host_GetColour)(ARMul_State *state,unsigned int col) { return GetColour(state, col); }

static void SDD_Name(Host_ChangeMode)(ARMul_State *state,int width,int height,int hz);

static inline SDD_Row SDD_Name(Host_BeginRow)(ARMul_State *state,int row,int offset)
{
  UNUSED_VAR(state);
  return ((SDD_Row)(void *) ((uint8_t *)sdd_surface->pixels + sdd_surface->pitch*row))+offset;
}

static inline void SDD_Name(Host_EndRow)(ARMul_State *state,SDD_Row *row)
{
  /* nothing */
  UNUSED_VAR(state);
  UNUSED_VAR(row);
}

static inline void SDD_Name(Host_BeginUpdate)(ARMul_State *state,SDD_Row *row,unsigned int count)
{
  /* nothing */
  UNUSED_VAR(state);
  UNUSED_VAR(row);
  UNUSED_VAR(count);
}

static inline void SDD_Name(Host_EndUpdate)(ARMul_State *state,SDD_Row *row)
{
  /* nothing */
  UNUSED_VAR(state);
  UNUSED_VAR(row);
}

static inline void SDD_Name(Host_SkipPixels)(ARMul_State *state,SDD_Row *row,unsigned int count)
{
  UNUSED_VAR(state);
  (*row) += count;
}

static inline void SDD_Name(Host_WritePixel)(ARMul_State *state,SDD_Row *row,SDD_HostColour pix)
{
  UNUSED_VAR(state);
  *(*row)++ = pix;
}

static inline void SDD_Name(Host_WritePixels)(ARMul_State *state,SDD_Row *row,SDD_HostColour pix,unsigned int count)
{
  UNUSED_VAR(state);
  while(count--) *(*row)++ = pix;
}

static void SDD_Name(Host_PollDisplay)(ARMul_State *state);

#include "../arch/stddisplaydev.c"

static void SDD_Name(Host_ChangeMode)(ARMul_State *state,int width,int height,int hz)
{
  UNUSED_VAR(hz);

  if (width > MaxVideoWidth || height > MaxVideoHeight) {
      ControlPane_Error(true,"Resize_Window: new size (%d, %d) exceeds maximum (%d, %d)",
          width, height, MaxVideoWidth, MaxVideoHeight);
  }

  HD.XScale = 1;
  HD.YScale = 1;
  /* Try and detect rectangular pixel modes */
  if(CONFIG.bAspectRatioCorrection && (width >= height*2) && (height*2 <= MaxVideoHeight))
  {
    HD.YScale = 2;
    height *= 2;
  }
  else if(CONFIG.bAspectRatioCorrection && (height >= width) && (width*2 <= MaxVideoWidth))
  {
    HD.XScale = 2;
    width *= 2;
  }
  /* Try and detect small screen resolutions */
  else if(CONFIG.bUpscale && (width < MinVideoWidth) && (width * 2 <= MaxVideoWidth) && (height * 2 <= MaxVideoHeight))
  {
    HD.XScale = 2;
    HD.YScale = 2;
    width *= 2;
    height *= 2;
  }
  HD.Width = width;
  HD.Height = height;

  SetupScreen(state,&HD.Width,&HD.Height,sizeof(SDD_HostColour)*8);
}

static void SDD_Name(Host_PollDisplay)(ARMul_State *state) {
  int HorizPos, VertPos;
  DisplayDev_GetCursorPos(state,&HorizPos,&VertPos);
  mouse_rect.x = HorizPos*HD.XScale+HD.XOffset;
  mouse_rect.y = VertPos*HD.YScale+HD.YOffset;

  PollDisplay(state,HD.XScale,HD.YScale);
}

#undef SDD_HostColour
#undef SDD_Name
#undef SDD_RowsAtOnce
#undef SDD_Row
#undef SDD_DisplayDev

/* Standard display device, 32bpp */

#define SDD_HostColour uint32_t
#define SDD_Name(x) sdd32_##x
#define SDD_RowsAtOnce 1
#define SDD_Row SDD_HostColour *
#define SDD_DisplayDev SDD32_DisplayDev

static SDD_HostColour SDD_Name(Host_GetColour)(ARMul_State *state,unsigned int col) { return GetColour(state, col); }

static void SDD_Name(Host_ChangeMode)(ARMul_State *state,int width,int height,int hz);

static inline SDD_Row SDD_Name(Host_BeginRow)(ARMul_State *state,int row,int offset)
{
  UNUSED_VAR(state);
  return ((SDD_Row)(void *) ((uint8_t *)sdd_surface->pixels + sdd_surface->pitch*row))+offset;
}

static inline void SDD_Name(Host_EndRow)(ARMul_State *state,SDD_Row *row)
{
  /* nothing */
  UNUSED_VAR(state);
  UNUSED_VAR(row);
}

static inline void SDD_Name(Host_BeginUpdate)(ARMul_State *state,SDD_Row *row,unsigned int count)
{
  /* nothing */
  UNUSED_VAR(state);
  UNUSED_VAR(row);
  UNUSED_VAR(count);
}

static inline void SDD_Name(Host_EndUpdate)(ARMul_State *state,SDD_Row *row)
{
  /* nothing */
  UNUSED_VAR(state);
  UNUSED_VAR(row);
}

static inline void SDD_Name(Host_SkipPixels)(ARMul_State *state,SDD_Row *row,unsigned int count)
{
  UNUSED_VAR(state);
  (*row) += count;
}

static inline void SDD_Name(Host_WritePixel)(ARMul_State *state,SDD_Row *row,SDD_HostColour pix)
{
  UNUSED_VAR(state);
  *(*row)++ = pix;
}

static inline void SDD_Name(Host_WritePixels)(ARMul_State *state,SDD_Row *row,SDD_HostColour pix,unsigned int count)
{
  UNUSED_VAR(state);
  while(count--) *(*row)++ = pix;
}

static void SDD_Name(Host_PollDisplay)(ARMul_State *state);

#include "../arch/stddisplaydev.c"

static void SDD_Name(Host_ChangeMode)(ARMul_State *state,int width,int height,int hz)
{
  UNUSED_VAR(hz);

  if (width > MaxVideoWidth || height > MaxVideoHeight) {
      ControlPane_Error(true,"Resize_Window: new size (%d, %d) exceeds maximum (%d, %d)",
          width, height, MaxVideoWidth, MaxVideoHeight);
  }

  HD.XScale = 1;
  HD.YScale = 1;
  /* Try and detect rectangular pixel modes */
  if(CONFIG.bAspectRatioCorrection && (width >= height*2) && (height*2 <= MaxVideoHeight))
  {
    HD.YScale = 2;
    height *= 2;
  }
  else if(CONFIG.bAspectRatioCorrection && (height >= width) && (width*2 <= MaxVideoWidth))
  {
    HD.XScale = 2;
    width *= 2;
  }
  /* Try and detect small screen resolutions */
  else if(CONFIG.bUpscale && (width < MinVideoWidth) && (width * 2 <= MaxVideoWidth) && (height * 2 <= MaxVideoHeight))
  {
    HD.XScale = 2;
    HD.YScale = 2;
    width *= 2;
    height *= 2;
  }
  HD.Width = width;
  HD.Height = height;

  SetupScreen(state,&HD.Width,&HD.Height,sizeof(SDD_HostColour)*8);
}

static void SDD_Name(Host_PollDisplay)(ARMul_State *state) {
  int HorizPos, VertPos;
  DisplayDev_GetCursorPos(state,&HorizPos,&VertPos);
  mouse_rect.x = HorizPos*HD.XScale+HD.XOffset;
  mouse_rect.y = VertPos*HD.YScale+HD.YOffset;

  PollDisplay(state,HD.XScale,HD.YScale);
}

#undef SDD_HostColour
#undef SDD_Name
#undef SDD_RowsAtOnce
#undef SDD_Row
#undef SDD_DisplayDev

/* ------------------------------------------------------------------ */

/* Palettised display code */
#define PDD_Name(x) pdd_##x

typedef struct {
  ARMword *data;
  int offset;
} PDD_Row;

static void PDD_Name(Host_ChangeMode)(ARMul_State *state,int width,int height,int depth,int hz);

static void PDD_Name(Host_SetPaletteEntry)(ARMul_State *state,int i,uint_fast16_t phys)
{
  SDL_Color col;

  UNUSED_VAR(state);

  col = GetColourStruct(state, phys);

#if SDL_VERSION_ATLEAST(2, 0, 0)
  SDL_SetPaletteColors(sdd_palette, &col, i, 1);
#else
  SDL_SetColors(screen, &col, i, 1);
  SDL_SetColors(sdd_surface, &col, i, 1);
#endif
}

static void PDD_Name(Host_SetCursorPaletteEntry)(ARMul_State *state,int i,uint_fast16_t phys)
{
  SDL_Color col;

  if (palette_offset > 0 && palette_offset < 256)
      PDD_Name(Host_SetPaletteEntry)(state, palette_offset + i + 1, phys);

  col = GetColourStruct(state, phys);

#if SDL_VERSION_ATLEAST(2, 0, 0)
  SDL_SetPaletteColors(mouse_palette, &col, i + 1, 1);
#else
  SDL_SetColors(mouse_surface, &col, i + 1, 1);
#endif
}

static void PDD_Name(Host_SetBorderColour)(ARMul_State *state,uint_fast16_t phys)
{
  /* TODO */
  UNUSED_VAR(state);
  UNUSED_VAR(phys);
}

static inline PDD_Row PDD_Name(Host_BeginRow)(ARMul_State *state,int row,int offset,int *alignment)
{
  PDD_Row drow;
  uintptr_t base = ((uintptr_t)sdd_surface->pixels + sdd_surface->pitch*row) + offset;
  UNUSED_VAR(state);
  drow.offset = ((base<<3) & 0x18); /* Just in case bytes per line isn't aligned */
  drow.data = (ARMword *) (base & ~0x3);
  *alignment = drow.offset;
  return drow;
}

static inline void PDD_Name(Host_EndRow)(ARMul_State *state,PDD_Row *row)
{
  /* nothing */
  UNUSED_VAR(state);
  UNUSED_VAR(row);
}

static inline ARMword *PDD_Name(Host_BeginUpdate)(ARMul_State *state,PDD_Row *row,unsigned int count,int *outoffset)
{
  UNUSED_VAR(state);
  UNUSED_VAR(count);
  *outoffset = row->offset;
  return row->data;
}

static inline void PDD_Name(Host_EndUpdate)(ARMul_State *state,PDD_Row *row)
{
  /* nothing */
  UNUSED_VAR(state);
  UNUSED_VAR(row);
}

static inline void PDD_Name(Host_AdvanceRow)(ARMul_State *state,PDD_Row *row,unsigned int count)
{
  UNUSED_VAR(state);
  row->offset += count;
  row->data += count>>5;
  row->offset &= 0x1f;
}

static void PDD_Name(Host_PollDisplay)(ARMul_State *state);

static void PDD_Name(Host_DrawBorderRect)(ARMul_State *state,int x,int y,int width,int height)
{
  /* TODO */
  UNUSED_VAR(state);
  UNUSED_VAR(x);
  UNUSED_VAR(y);
  UNUSED_VAR(width);
  UNUSED_VAR(height);
}

#include "../arch/paldisplaydev.c"

void PDD_Name(Host_ChangeMode)(ARMul_State *state,int width,int height,int depth,int hz)
{
  UNUSED_VAR(hz);

  if (width > MaxVideoWidth || height > MaxVideoHeight) {
      ControlPane_Error(true,"Resize_Window: new size (%d, %d) exceeds maximum (%d, %d)",
          width, height, MaxVideoWidth, MaxVideoHeight);
  }

  HD.XScale = 1;
  HD.YScale = 1;
  /* Try and detect rectangular pixel modes */
  if(CONFIG.bAspectRatioCorrection && (width >= height*2) && (height*2 <= MaxVideoHeight))
  {
    HD.YScale = 2;
    height *= 2;
  }
  else if(CONFIG.bAspectRatioCorrection && (height >= width) && (width*2 <= MaxVideoWidth))
  {
    HD.XScale = 2;
    width *= 2;
  }
  /* Try and detect small screen resolutions */
  else if(CONFIG.bUpscale && (width < MinVideoWidth) && (width * 2 <= MaxVideoWidth) && (height * 2 <= MaxVideoHeight))
  {
    HD.XScale = 2;
    HD.YScale = 2;
    width *= 2;
    height *= 2;
  }
  HD.Width = width;
  HD.Height = height;

  SetupScreen(state,&HD.Width,&HD.Height,1<<depth);

  /* Calculate expansion params */
  if((depth == 3) && (HD.XScale == 1))
  {
    /* No expansion */
    HD.ExpandTable = NULL;
  }
  else
  {
    /* Expansion! */
    static ARMword expandtable[256];
    unsigned int mul = 1;
    int i;
    HD.ExpandFactor = 0;
    while((1<<HD.ExpandFactor) < HD.XScale)
      HD.ExpandFactor++;
    HD.ExpandFactor += (3-depth);
    HD.ExpandTable = expandtable;
    for(i=0;i<HD.XScale;i++)
    {
      mul |= 1<<(i*8);
    }
    GenExpandTable(HD.ExpandTable,1<<depth,HD.ExpandFactor,mul);
  }
}

static void PDD_Name(Host_PollDisplay)(ARMul_State *state) {
  int HorizPos, VertPos;
  DisplayDev_GetCursorPos(state,&HorizPos,&VertPos);
  mouse_rect.x = HorizPos*HD.XScale+HD.XOffset;
  mouse_rect.y = VertPos*HD.YScale+HD.YOffset;

  PollDisplay(state,HD.XScale,HD.YScale);
}

/* ------------------------------------------------------------------ */

static uint32_t GetColour(ARMul_State *state,unsigned int col)
{
  uint8_t r, g, b;

  UNUSED_VAR(state);

  /* Convert to 8-bit component values */
  r = (col & 0xf)*0x11;
  g = ((col>>4) & 0xf)*0x11;
  b = ((col>>8) & 0xf)*0x11;

#if SDL_VERSION_ATLEAST(3, 0, 0)
  return SDL_MapRGB(SDL_GetPixelFormatDetails(screen->format), sdd_palette, r, g, b);
#else
  return SDL_MapRGB(screen->format, r, g, b);
#endif
}

static SDL_Color GetColourStruct(ARMul_State *state,unsigned int phys)
{
  SDL_Color col;

  UNUSED_VAR(state);

  /* Convert to 8-bit component values */
  col.r = ((phys>>0) & 0xf)*0x11;
  col.g = ((phys>>4) & 0xf)*0x11;
  col.b = ((phys>>8) & 0xf)*0x11;
#if SDL_VERSION_ATLEAST(2, 0, 0)
  col.a = SDL_ALPHA_OPAQUE;
#endif

  return col;
}

#undef HD

/* Refresh the mouse's image                                                    */
static bool RefreshMouse(ARMul_State *state,int XScale,int YScale) {
  int x,y,offset, repeat;
  int memptr;
  int Height = ((int)VIDC.Vert_CursorEnd - (int)VIDC.Vert_CursorStart)*YScale;
  uint8_t *dst;

  /* TODO: Implement horizontal scaling */
  UNUSED_VAR(XScale);

  if (Height <= 0) return false;

  mouse_rect.w = 32;
  mouse_rect.h = Height;

  if (palette_offset == 0) {
    /* Cursor palette */
    SDL_Color cursorPal[3];

    for(x=0; x<3; x++) {
      cursorPal[x] = GetColourStruct(state, VIDC.CursorPalette[x]);
    }

#if SDL_VERSION_ATLEAST(2, 0, 0)
    SDL_SetPaletteColors(mouse_palette, cursorPal, 1, 3);
#else
    SDL_SetColors(mouse_surface, cursorPal, 1, 3);
#endif
  }

  SDL_LockSurface(mouse_surface);

  offset=0;
  memptr=MEMC.Cinit*16;
  repeat=0;
  dst=mouse_surface->pixels;
  for(y=0;y<Height;y++) {
    if (offset<512*1024) {
      ARMword tmp[2];

      tmp[0]=MEMC.PhysRam[memptr/4];
      tmp[1]=MEMC.PhysRam[memptr/4+1];

      for(x=0;x<32;x++) {
        dst[x] = ((tmp[x/16]>>((x & 15)*2)) & 3);
      }; /* x */
      dst += mouse_surface->pitch;
    } else return true;
    if(++repeat == YScale) {
      memptr += 8;
      offset += 8;
      repeat  = 0;
    }
  }; /* y */

  SDL_UnlockSurface(mouse_surface);
  return true;
} /* RefreshMouse */

static void SetupScreen(ARMul_State *state,int *width,int *height,int bpp)
{
  UNUSED_VAR(state);

#if SDL_VERSION_ATLEAST(2, 0, 0)
  if (!sdd_palette)
      sdd_palette = SDL_CreatePalette(256);
  if (!mouse_palette)
      mouse_palette = SDL_CreatePalette(4);

  SDL_SetWindowSize(window, *width, *height);
  {
    SDL_DisplayMode closest;
    SDL_GetClosestFullscreenDisplayMode(SDL_GetDisplayForWindow(window), *width, *height, 0.0f, false, &closest);
    SDL_SetWindowFullscreenMode(window, &closest);
  }
  screen = SDL_GetWindowSurface(window);
  SDL_SetSurfacePalette(screen, sdd_palette);
#else
  screen = SDL_SetVideoMode(*width, *height, screen->format->BitsPerPixel,
                            SDL_SWSURFACE | SDL_HWPALETTE);
#endif

  if (sdd_surface)
      SDL_DestroySurface(sdd_surface);
  if (mouse_surface)
      SDL_DestroySurface(mouse_surface);

#if SDL_VERSION_ATLEAST(3, 0, 0)
  sdd_surface = SDL_CreateSurface(screen->w, screen->h, screen->format);
  mouse_surface = SDL_CreateSurface(32, screen->h, SDL_PIXELFORMAT_INDEX8);
#else
  sdd_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, screen->w, screen->h,
                                     screen->format->BitsPerPixel,
                                     screen->format->Rmask,
                                     screen->format->Gmask,
                                     screen->format->Bmask,
                                     screen->format->Amask);
  mouse_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, 32, screen->h, 8, 0, 0, 0, 0);
#endif
#if SDL_VERSION_ATLEAST(2, 0, 0)
  SDL_SetSurfacePalette(sdd_surface, sdd_palette);
  SDL_SetSurfacePalette(mouse_surface, mouse_palette);
#endif
  SDL_SetSurfaceColorKey(mouse_surface, true, 0);

  /* Screen is expected to be cleared */
  SDL_FillSurfaceRect(sdd_surface, NULL, GetColour(state, 0));

  palette_offset = (bpp > 8) ? 0 : (1 << bpp);

  *width = screen->w;
  *height = screen->h;
}

static void PollDisplay(ARMul_State *state,int XScale,int YScale)
{
  bool has_mouse = RefreshMouse(state,XScale,YScale);

  SDL_BlitSurface(sdd_surface, NULL, screen, NULL);
  if (has_mouse)
    SDL_BlitSurface(mouse_surface, NULL, screen, &mouse_rect);

#if SDL_VERSION_ATLEAST(2, 0, 0)
  SDL_UpdateWindowSurface(window);
#else
  SDL_Flip(screen);
#endif
}

/*-----------------------------------------------------------------------------*/
bool DisplayDev_Init(ARMul_State *state)
{
  int bpp;

  /* Setup display and cursor bitmaps */
#if SDL_VERSION_ATLEAST(3, 0, 0)
  window = SDL_CreateWindow("ArcEm", 640, 512, 0);
  if (!window) {
      ControlPane_Error(false,"Failed to create initial window: %s", SDL_GetError());
      return false;
  }

#ifdef SDL_PLATFORM_DOS
  {
    SDL_DisplayMode closest;
    SDL_GetClosestFullscreenDisplayMode(SDL_GetDisplayForWindow(window), 640, 480, 0.0f, false, &closest);
    SDL_SetWindowFullscreenMode(window, &closest);
  }
#endif

  screen = SDL_GetWindowSurface(window);
  if (!screen) {
      ControlPane_Error(false,"Failed to get the window surface: %s", SDL_GetError());
      return false;
  }
  bpp = SDL_BYTESPERPIXEL(screen->format);
#elif SDL_VERSION_ATLEAST(2, 0, 0)
  window = SDL_CreateWindow("ArcEm", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            640, 512, 0);
  if (!window) {
      ControlPane_Error(false,"Failed to create initial window: %s", SDL_GetError());
      return false;
  }

  screen = SDL_GetWindowSurface(window);
  if (!screen) {
      ControlPane_Error(false,"Failed to get the window surface: %s", SDL_GetError());
      return false;
  }
  bpp = screen->format->BytesPerPixel;
#else
  screen = SDL_SetVideoMode(640, 512, 0, SDL_SWSURFACE);
  if (!screen) {
      ControlPane_Error(false,"Failed to create initial window: %s", SDL_GetError());
      return false;
  }
  SDL_WM_SetCaption("ArcEm", "ArcEm");
  bpp = screen->format->BytesPerPixel;
#endif

  if (bpp == 4) {
      return DisplayDev_Set(state,&SDD32_DisplayDev);
  } else if (bpp == 2) {
      return DisplayDev_Set(state,&SDD16_DisplayDev);
  } else if (bpp == 1) {
      return DisplayDev_Set(state,&PDD_DisplayDev);
  } else {
      ControlPane_Error(false,"Unsupported bytes per pixel: %d", bpp);
      return false;
  }
}

#endif
