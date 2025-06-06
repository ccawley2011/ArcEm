/*
  ArcemConfig

  (c) 2006 P Howkins

  Part of Arcem released under the GNU GPL, see file COPYING
  for details.

  ArcemConfig - Stores the running configuration of the Arcem
  emulator.
 
  The ArcemConfig structure can be filled in by means other
  than the Command Line parser as needed on a platform by
  platform basis.

  Using a configuration value in the rest of the emulator code
  ------------------------------------------------------------
   Use the define 'CONFIG' to access the structure for the current
  state. Its properties are defined in the struct ArcemConfig_s
  below. Use enumeration values or #defines for all properties that
  support that.

  Adding a config value to the emulator
  -------------------------------------
   1) Add a value of the correct type to ArcemConfig.
      If possible use an enumeration or #defines for
      its values.
   2) Make sure the value is initialised to its default
      value in ArcemConfig_SetupDefaults()
   3) Add a way of setting it to alternate values in
      ArcemConfig_ParseCommandLine()
   4) If ways of setting the config other than Command Line
      are implimented add code for parsing and setting values
      there too.
*/
#ifndef ARCEMCONFIG_HEADER
#define ARCEMCONFIG_HEADER

#include "arch/hdc63463.h"
#include "../armdefs.h"
#include "../c99.h"

typedef enum ArcemConfig_MemSize_e {
  MemSize_256K,
  MemSize_512K,
  MemSize_1M,
  MemSize_2M,
  MemSize_4M,
  MemSize_8M,
  MemSize_12M,
  MemSize_16M
} ArcemConfig_MemSize;

typedef enum ArcemConfig_Processor_e {
  Processor_ARM2,                 /* ARM 2 */
  Processor_ARM250,               /* ARM 2AS */
  Processor_ARM3                  /* ARM 2AS */
} ArcemConfig_Processor;

typedef enum ArcemConfig_DisplayDriver_e {
  DisplayDriver_Palettised,
  DisplayDriver_Standard /* i.e. 16/32bpp true colour */
} ArcemConfig_DisplayDriver;

typedef struct ArcemConfig_Label_s {
    const char *name;
    unsigned int value;
} ArcemConfig_Label;

/* THIS IS THE MAIN CONFIGURATION STRUCTURE */

struct ArcemConfig_s {
  ArcemConfig_MemSize   eMemSize;
  ArcemConfig_Processor eProcessor; 

  char *sRomImageName;

  char *sCMOSFileName;

#if defined(EXTNROM_SUPPORT)
  char *sEXTNDirectory;
#endif /* EXTNROM_SUPPORT */

#if defined(HOSTFS_SUPPORT)
  char *sHostFSDirectory;
#endif /* HOSTFS_SUPPORT */

  char *aFloppyPaths[4];
  char *aST506Paths[4];

  /* Shapes of the MFM ST506 drives as set in the config file */
  struct HDCshape aST506DiskShapes[4];

  /* Platform-specific bits */
#if defined(SYSTEM_riscos_single) || defined(SYSTEM_win)
  ArcemConfig_DisplayDriver eDisplayDriver;
  bool bAspectRatioCorrection; /* Apply H/V scaling for aspect ratio correction */
  bool bUpscale; /* Allow upscaling to fill screen */
#endif
#if defined(SYSTEM_riscos_single)
  bool bRedBlueSwap; /* Red/blue swap 16bpp output */
  bool bNoLowColour; /* Disable 1/2/4bpp modes */
  int iMinResX,iMinResY;
  int iLCDResX,iLCDResY;
  int iTweakMenuKey1,iTweakMenuKey2;
#endif

};

#define CONFIG (*(state->Config))

/** 
 * ArcemConfig_SetupDefaults
 *
 * Called on program startup, sets up the defaults for
 * all the configuration values for the emulator
 *
 * @param pConfig The structure to fill in
 */
extern void ArcemConfig_SetupDefaults(ArcemConfig *pConfig);

/**
 * ArcemConfig_ParseConfigFile
 *
 * Parse and fill in the runtime options into the supplied configuration
 * structure from the config file.
 *
 * @param pConfig The structure to fill in
 */
extern void ArcemConfig_ParseConfigFile(ArcemConfig* pConfig);

/**
 * ArcemConfig_ParseCommandLine
 *
 * Given the commandline arguments that the program was started with,
 * parse and fill in the runtime options into the supplied configuration
 * structure. Will exit the program if command line arguments are
 * incorrectly formatted (or the --help or --version argument is used).
 *
 * @param pConfig The structure to fill in
 * @param argc Number of entries in argv
 * @param argv Array of char*'s represented space seperated commandline arguments
 */
extern void ArcemConfig_ParseCommandLine(ArcemConfig* pConfig, int argc, char* argv[]);

#endif
