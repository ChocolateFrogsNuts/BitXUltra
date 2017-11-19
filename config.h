
#ifndef CONFIG_H
#define CONFIG_H

/*
 * Options and definitions for BITXUltra firmware
 */

#if 1  // 1=use config, 0=Default Minimal config only

// Select which features are compiled in. Comment out a define to disable a feature.
// *** size is approx 59% of program and 39% of dynamic memory space with minimal features (DDS tuner only)
// *** and 86% of program and 47% of dynamic memory with everything
//
// _PTT: The hardware is in place to control the TX relays. Required for RIT to make sense.
// _BFO: The BFO is driven by a SI5351 clock output. Uses 5% of program memory.
// _HAVE_SWR: an SWR bridge is installed. Displays SWR during TX.
// _SHUTTLETUNE: use shuttle style tuning (vs original style)
// _SAVESTATE: allow storing the current VFO states as default in EEPROM
// _MENU: enable setting extra features via a menu. Uses 8% of program memory.
// _CHANNELS: a channel mode. Requires HAVE_MENU.
// _FILTERS: extra filters are available. Set the control method below.
// _PULSE: flash the onboard LED when the main loop is running. Also enables minimum free memory reports to Serial.
#define HAVE_PTT          1
#define HAVE_SWR          1
#define HAVE_SHUTTLETUNE  1
#define HAVE_SAVESTATE    1
#define HAVE_MENU         1
#define HAVE_CHANNELS     1
#define HAVE_FILTERS      1
//#define HAVE_PULSE        1

// 0 = Standard Fixed BFO (default). VFO is adjusted for CW TX.
// 1 = DDS BFO on clk set by BFO_OUTPUT. BFO is moved into the crystal filter passband for CW TX. Enables BFO-Trim menu.
#define HAVE_BFO          1

// 0 = use the original calibration routine.
// 1 = use new calibration routine - consistent with the way all other menu settings work.
// actual calibration value is the same.
#define NEW_CAL 1

// 0=Tune across all frequencies,
// 1=Only tune to defined bands, go to next band at end.
// 2=Only tune to defined bands, wrap around within current band. Use rapid tune (hold FBUTTON) to change bands.
// Default: 2
#define TUNE_BANDS_ONLY 0

// _CW: The hardware is in place to TX CW. Requires HAVE_PTT. Not RIT enabled... yet. Uses 5% of program memory.
// set to 0 or comment out to disable CW,
// set to 1 for straight key only and smaller code
// set to 2 for paddle/bug and straight key compatible (connect straight key direct, paddle via resistors) +600 bytes
#define HAVE_CW           2

// a work in progress... automated CW sender requires approx 840 bytes with prosign handlers
// maybe use CAT to set the string in EEPROM, then menu to set timing/activate.
#define HAVE_CW_SENDER    1

// Ability and menu settings to be a CW beacon. Use CAT command cwb to set the text in EEPROM.
// (once it's set you can turn off CAT to make room for some other feature - beacon will still work)
// Menu option "CW Beacon" to set the interval and activate the beacon mode.  +500 bytes.
#define HAVE_CW_BEACON    1

/* 
 * Define HAVE_SMETER if you have wired up to monitor the audio levels ust before the volume knob. 
 * It enables your choice of S-Meter on RX.
 * Uses 1% for any display other than none, plus display type.
 * 
 * Type      Size(bytes)
 * _NONE     +0    Hardware is available, but don't display the s-meter.
 * _TEXT     +78   Text version using | : * and .
 * _NUMERIC  +106  Display the numbers
 * _MINI     +180  Single vertical bar in top line
 * _BARS     +266  Familiar Bar type Signal Meter
 * _HIRES    +376  High resolution solid bar.
*/
#define HAVE_SMETER  SMETER_HIRES

// HAVE_CAT, HAVE_ANALYSER are larger features. 
// They won't fit unles you only have one or two bands, or use the cut-down CAT_MINIMAL version.
// I _have_ managed to optimize the si5351 library to gain about 1600 bytes and make everything fit!
// I also made it possible to gain another 2200 bytes by disabling support for clocks 6 and 7 that 
// are not in the variant on the Raduino, but we don't need that space yet.

// Rig control (CAT). Adds about 10% to program and 3% to dynamic memory for full version
#define HAVE_CAT 1
// drop some less useful commands from Rig Control to save up to 600 bytes (2%).
//#define CAT_MINIMAL 1

// Antenna analyser : runs a sweep of the current band TX range and displays the result.
// Adds about 4% to program and 2% to dynamic memory.
#define HAVE_ANALYSER 1


#endif // Config/Minimal

// Don't move this line!
#include "defconfig.h"

/**
 * We need to carefully pick assignment of pin for various purposes.
 * There are two sets of completely programmable pins on the Raduino.
 * First, on the top of the board, in line with the LCD connector is an 8-pin connector
 * that is largely meant for analog inputs and front-panel control. It has a regulated 5v output,
 * ground and six pins. Each of these six pins can be individually programmed 
 * either as an analog input, a digital input or a digital output. 
 * The pins are assigned as follows: 
 *        A0,   A1,  A2,  A3,   GND,   +5v,  A6,  A7 
 *      BLACK BROWN RED ORANGE YELLW  GREEN  BLUEVIOLET
 *      (while holding the board up so that back of the board faces you)
 *      
 * Though, this can be assigned anyway, for this application of the Arduino, we will make the following
 * assignment
 * A2 is connected to a push button that can momentarily ground this line. This will be used to switch between different modes, etc.
 * A6 is to implement a keyer, it is reserved and not yet implemented
 * A7 is connected to a center pin of good quality 100K or 10K linear potentiometer with the two other ends connected to
 * ground and +5v lines available on the connector. This implments the tuning mechanism
 */
// moved Calibrate to startup (or menu if enabled) only, so we can use the FBUTTON and free up an input
// Also reduces risk of accidental calibration.
// moved the FBUTTON to a digital input to free up an analog for the meters.

// SWR Bridge & calibration : 100== * 1.00
#define F_POWER (A2)
#define R_POWER (A0)
#define F_POWER_CAL 100
#define R_POWER_CAL 100

// S-METER
#define S_POWER (A3)
#define S_POWER_CAL 100

// calibrate the S-Meter for S9
#define S9_LEVEL 400

#define ANALOG_KEYER (A6)

#define CAL_BUTTON (2)
#define FBUTTON (2)

#define PTT   (A1)
#define ANALOG_TUNING (A7)

/** 
 *  The second set of 16 pins on the bottom connector are have the three clock outputs and the digital lines to control the rig.
 *  This assignment is as follows :
 *    Pin   1   2    3    4    5    6    7    8    9    10   11   12   13   14   15   16
 *         +5V +5V CLK0  GND  GND  CLK1 GND  GND  CLK2  GND  D2   D3   D4   D5   D6   D7  
 *  These too are flexible with what you may do with them, for the Raduino, we use them to :
 *  - TX_RX line : Switches between Transmit and Receive after sensing the PTT or the morse keyer
 *  - CW_KEY line : turns on the carrier for CW
 *  These are not used at the moment.
 */
#define TX_RX (7)
#define CW_TONE (6)
#define CW_KEY (5)

#if HAVE_FILTERS
// Extra filter settings. No need for any of these if you only have the standard 40m filters on the board.
// How to set the filters for each band is defined in txbands around line 45 of filters.cpp

//#define FILTER_PIN0 (4)
//#define FILTER_PIN1 (3)
//#define FILTER_PIN2 ()

/* Define RELAYS_ON / RELAYS_OFF if you used latching relays to switch filters. 
 * RELAYS_ON: Only used in I2C mode, Cause FILTER_PIN0 to go to the RELAYS_ON state to make power available to the relays.
 */
//#define FILTER_RELAYS_ON  LOW

/* RELAYS_OFF: If FILTER_PIN0 is defined, sets FILTER_PIN0 to the RELAYS_OFF state to cut power to the relays.
 *             Also sets all filter outputs to the RELAYS_OFF state after switching to conserve power.
 */
//#define FILTER_RELAYS_OFF HIGH

// Set to 1 for filter selection using an i2C bus connected chip. You only want to do that if you 
// have lots of filters.  FILTER_PIN0 is used to control power to the relays if defined.
// Set to 0 for normal IO pin selected filters.
#define FILTER_I2C 1

// How are the filter relays connected for I2C control?
#if FILTER_I2C
#define FILTER_CONTROL setFilters_PCF857X
#define PCF857X_COUNT 1
#define PCF857X_SIZES {   16, }   // Array: Number of outputs on each chip.
#define PCF857X_ADDRS { 0x20, }   // Array: I2C Address of each chip
#else
#define FILTER_CONTROL setFilters_IO
#endif

// Sanity check the filter setup
#if !FILTER_I2C
#ifndef FILTER_PIN0
#error Must have either FILTER_PIN0 or FILTER_I2C defined if HAVE_FILTERS is defined.
#endif
#endif

#endif // HAVE_FILTERS


// Only used if HAVE_BFO is enabled
#define BFO_OUTPUT SI5351_CLK1

// beacon string is from here to STATE_EEPROM_START.
// 40 bytes is more than enough for "CQ CQ CQ de CALLSIGN" (another 19 chars to spare)
#define CWBEACON_EEPROM_START 10
#define CWBEACON_MAXLEN       40

// min/max beacon interval (seconds)
#define CWBEACON_INTERVAL_MIN  (30)
#define CWBEACON_INTERVAL_MAX (300)
#define CWBEACON_INTERVAL_STEP (10)

#define STATE_EEPROM_START 50
#define STATE_EEPROM_SIZE  50
#define STATE_MAGIC 0x5A
#define VFO_MAGIC   0xA5

// Room for up to 6 VFOs. If you want more, change CHANNEL_EEPROM_START but that reduces the 
// maximum number of channels that can be used.
#define VFO_COUNT          3
#define VFO_EEPROM_START 100
#define VFO_EEPROM_SIZE   20

// Room for up to 40 channels in the EEPROM, If you want a few more reduce the START address 
// (which also reduces the maximum number of VFOs that can be used).
#define CHANNEL_COUNT         10
#define CHANNEL_EEPROM_START 220
#define CHANNEL_EEPROM_SIZE   20

#define LOWEST_FREQ  (3500000l)
#define HIGHEST_FREQ (7300000l)
#define RIT_MIN (-150)
#define RIT_MAX (150)
#define RIT_STEP (10)

#define BFOTRIM_MIN (-1000)
#define BFOTRIM_MAX (1000)
#define BFOTRIM_STEP (-10)

#define SIDETONE_MIN (400)
#define SIDETONE_MAX (900)
#define SIDETONE_STEP (STEP_AUTO)
#define DEFAULT_SIDETONE (800)

#define WPM_MIN (10)
#define WPM_MAX (30)
#define WPM_STEP (1)
#define DEFAULT_WPM (20)

/**
 *  The raduino has a number of timing parameters, all specified in milliseconds 
 *  CW_TIMEOUT : how many milliseconds between consecutive keyup and keydowns before switching back to receive?
 *  The next set of three parameters determine what is a tap, a double tap and a hold time for the funciton button
 *  TAP_DOWN_MILLIS : upper limit of how long a tap can be to be considered as a button_tap
 *  TAP_UP_MILLIS : upper limit of how long a gap can be between two taps of a button_double_tap
 *  TAP_HOLD_MILIS : many milliseconds of the buttonb being down before considering it to be a button_hold
 */
 
#define TAP_UP_MILLIS (500)
#define TAP_DOWN_MILLIS (600)
#define TAP_HOLD_MILLIS (2000)
#define CW_TIMEOUT (600l) // in milliseconds, this is the parameter that determines how long the tx will hold between cw key downs

#endif

