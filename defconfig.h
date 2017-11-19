
/*
 * Default config values for features.
 * Pinouts and filter configs have no defaults.
 */

#ifndef DEFCONFIG_H
#define DEFCONFIG_H

// Config Defaults

#ifndef HAVE_PTT
#define HAVE_PTT          0
#endif

#ifndef HAVE_CW
#define HAVE_CW           0
#endif

#ifndef HAVE_CW_SENDER
#define HAVE_CW_SENDER    0
#endif

#ifndef HAVE_CW_BEACON
#define HAVE_CW_BEACON    0
#endif

#ifndef HAVE_FSQ_BEACON
#define HAVE_FSQ_BEACON   0
#endif

#ifndef HAVE_BFO
#define HAVE_BFO          0
#endif

#ifndef HAVE_SHUTTLETUNE
#define HAVE_SHUTTLETUNE  0
#endif

#ifndef HAVE_SAVESTATE
#define HAVE_SAVESTATE    0
#endif

#ifndef HAVE_MENU
#define HAVE_MENU         0
#endif

#ifndef HAVE_CHANNELS
#define HAVE_CHANNELS     0
#endif

#ifndef HAVE_FILTERS
#define HAVE_FILTERS      0
#endif

#ifndef HAVE_PULSE
#define HAVE_PULSE        0
#endif

#ifndef HAVE_SWR
#define HAVE_SWR          0
#endif

#ifndef HAVE_SMETER
#define HAVE_SMETER       0
#endif

#ifndef HAVE_CAT
#define HAVE_CAT          0
#endif

#ifndef CAT_MINIMAL
#define CAT_MINIMAL       0
#endif

#ifndef HAVE_ANALYSER
#define HAVE_ANALYSER     0
#endif

#ifndef TUNE_BANDS_ONLY
#define TUNE_BANDS_ONLY   2
#endif

// Turn some stuff back off if requirements aren't met
#if !HAVE_PTT
#undef HAVE_ANALYSER
#undef HAVE_CW
#define HAVE_ANALYSER 0
#define HAVE_CW 0
#endif

#if HAVE_CW<2
#undef HAVE_CW_SENDER
#define HAVE_CW_SENDER 0
#endif

#if !HAVE_CW_SENDER
#undef HAVE_CW_BEACON
#define HAVE_CW_BEACON 0
#endif

#if !HAVE_CW
#undef HAVE_FSQ_BEACON
#define HAVE_FSQ_BEACON 0
#endif

#if !HAVE_SWR
#undef HAVE_ANALYSER
#define HAVE_ANALYSER 0
#endif

#if !HAVE_SAVESTATE
#undef HAVE_CHANNELS
#define HAVE_CHANNELS 0
#endif

#if !HAVE_MENU
#undef HAVE_CHANNELS
#undef HAVE_ANALYSER
#define HAVE_ANALYSER 0
#define HAVE_CHANNELS 0
#endif

#endif

