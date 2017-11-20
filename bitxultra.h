
#ifndef BITXULTRA_H
#define BITXULTRA_H

// Required in defconfig.h which is loaded by config.h
#define SMETER_NONE    1
#define SMETER_TEXT    2
#define SMETER_BARS    3
#define SMETER_MINI    4
#define SMETER_NUMERIC 5
#define SMETER_HIRES   6
#define SMETER_DEBUG   9

// used as a reference into fsq_params (x-1) if only one FSQ mode is enabled
// or FSQ_ANY will build for ALL of these selectable at run time.
// Note that there isn't room for all modes with this firmware in the chip.
#define WITH_FSQ_JT9   1
#define WITH_FSQ_JT65  2
#define WITH_FSQ_JT4   3
#define WITH_FSQ_WSPR  4
#define WITH_FSQ_2     5
#define WITH_FSQ_3     6
#define WITH_FSQ_4_5   7
#define WITH_FSQ_6     8
#define WITH_FSQ_ANY  99


#include "config.h"
#include <avr/pgmspace.h>

/** 
 *  The main chip which generates upto three oscillators of various frequencies in the
 *  Raduino is the Si5351a. To learn more about Si5351a you can download the datasheet 
 *  from www.silabs.com although, strictly speaking it is not a requirment to understand this code. 
 *  Instead, you can look up the Si5351 library written by Jason Mildrum, NT7S. You can download and 
 *  install it from https://github.com/etherkit/Si5351Arduino to complile this file.
 *  The Wire.h library is used to talk to the Si5351 and we also declare an instance of 
 *  Si5351 object to control the clocks.
 */
#include <Wire.h>

#include "si5351.h"

typedef unsigned long Frequency;
typedef unsigned long FilterId;

#pragma pack(1)

struct band {
  Frequency     lo, hi;
  bool          tx;
  FilterId      filter;
};


/**
 *  We can have many VFOs.. use a structure to keep track of the global config and each VFO's settings.
 */
// if this grows beyond 50 bytes we need to adjust *_START and *_EEPROM_SIZE
// Current size is 17 bytes.
struct state {
  unsigned char magic;
  unsigned char vfoActive;
  unsigned char vfoCount;
  unsigned char channelActive;
  unsigned char channelCount;
  unsigned int  sideTone;
           int  bfo_trim;
           bool useVFO;
           byte wpm;
           bool cw_swap_paddles:1;
           bool cw_ultimatic:1;
  unsigned int  cw_beacon_interval;
  unsigned int  fsq_beacon_interval;
  unsigned char fsq_mode;
};


enum modulation { MOD_LSB, MOD_USB, MOD_AUTO };


// Max struct size is VFO_EEPROM_SIZE bytes. Current Size: 10 bytes
struct vfo {
  unsigned char magic;
  Frequency frequency;
  long rit;
  enum modulation mod:2;
  bool ritOn:1;
  //bool split:1;
};

#pragma pack()

// EEPROM size is 1K in the nano.
#define EEPROM_SIZE 1024


// Do some compile time checks to ensure the EEPROM allocations fit.
#if CWBEACON_EEPROM_START + CWBEACON_MAXLEN > STATE_EEPROM_START
#error CW Beacon string length is too long.
#endif
#if STATE_EEPROM_START + STATE_EEPROM_SIZE > FSQ_EEPROM_START
#error State will overwrite FSQ data storage
#endif
#if FSQ_EEPROM_START + FSQ_EEPROM_SIZE > VFO_EEPROM_START
#error FSQ Data will overwrite VFO storage
#endif
#if (VFO_EEPROM_START + (VFO_COUNT * VFO_EEPROM_SIZE)) > CHANNEL_EEPROM_START
#error VFOs will overwrite Channel storage
#endif
#if (CHANNEL_EEPROM_START + (CHANNEL_COUNT * CHANNEL_EEPROM_SIZE)) > EEPROM_SIZE
#error Channel storage is too big for EEPROM
#endif


#define STEP_AUTO 0 // turn more = bigger step


/**
 * The raduino operates in multiple modes:
 * MODE_NORMAL : works the radio  normally
 * MODE_CALIBRATION : used to calibrate Raduino.
 * MODE_MENU : menu mode. tuning and FBUTTON select menu items
 * MODE_ADJUSTMENT : Generic adjustment mode (adjustment_data determines what is adjusted)
 * MODE_RUNBEACON : CW Beacon Mode
 * MODE_ANALYSER : Antenna Analyser
 */
enum modes { MODE_NORMAL, MODE_MENU, MODE_ADJUSTMENT, MODE_CWBEACON, MODE_FSQBEACON, MODE_ANALYSER,
             #if !NEW_CAL
             MODE_CALIBRATE
             #endif
           };

// What started the current TX (or NONE if RXing)
// PTT - a hardware PTT line
// CW  - the CW key input
// DIS - not actually TXing because TX disabled on this freq.
// CAT - Serial port request.
// ANA - analyser
enum txcause { INTX_NONE, INTX_PTT, INTX_CW, INTX_DIS, INTX_CAT, INTX_ANA, INTX_FSQ };

// RIT states for setFrequency()
// _OFF - rit is ignored. Tune the VFO to the dial freq. (ie TX mode)
// _ON  - rit is applied if it's on for the current VFO. (ie RX mode)
// _CW  - rit is applied as required for CW (move the carrier by the sidetone freq if needed).
// _AUTO- rit is applied if it's on for the current VFO and we're not TXing.
// For CW, carrier will go out at tuned+-sidetone. ie tune the other station to appear at your sidetone freq
// to send back at the same freq, or up or down a bit if you want to be on a different freq.
// You can also set the vfo rit so tuning as above will TX off the other frequency by the rit amount.
enum ritstate { RIT_OFF, RIT_ON, RIT_CW, RIT_AUTO };


// where the F() macro is for string literals, FH() is for PROGMEM strings defined earlier
// and other pointers to strings in PROGMEM.
#define FH(progmem_addr) ((__FlashStringHelper*)(progmem_addr))

// Like FlashStringHelper but for EEPROM strings
class __EEPROMStringHelper;
#define EH(eeprom_addr) ((__EEPROMStringHelper*)(eeprom_addr))


// cw.cpp
extern unsigned long cwTimeout;

#if HAVE_CW
#if HAVE_CW_SENDER
extern void send_cw_flush();
extern bool send_cw_code(byte);
extern bool send_cw_char(char);
extern void send_cw_string(char *);
extern void send_cw_string(const __FlashStringHelper *);
extern void send_cw_string(const __EEPROMStringHelper *, byte maxlen);
#endif
extern void checkCW();
#endif

#if HAVE_MENU

typedef void (*adjustmentHandler)();

struct adjustment {
  PGM_P desc;
  long value, min, max, step;
  adjustmentHandler cb_show, cb_change, cb_set;
};

#define ADJ_NIL    0
#define ADJ_CHANGE 1
#define ADJ_SET    2

// menu.cpp
extern unsigned char menuIdx;
extern char doAdjustment();
extern void showMenuItem();
extern void checkMenu();
#endif // HAVE_MENU

#if HAVE_SWR || HAVE_SMETER
// meters.cpp
extern unsigned char peak_s_level;
extern unsigned char avg_s_level;
extern unsigned int  last_swr;

extern void doMeters();
#if HAVE_ANALYSER
extern void startAnalyser();
extern void doAnalyser();
#endif
#endif // HAVE_SWR || HAVE_SMETER

// filters.cpp
extern const struct band * findBand(Frequency f);
extern Frequency findNextBandFreq(Frequency f);
#if HAVE_FILTERS
extern void setFilters(const struct band *band);
#endif

// fsq.cpp
struct fsq_data { // must not exceed 50 bytes
  char call[10];
  char loc[10];
  byte magic;
  byte dbm;
  char message[14];
};
extern PGM_P get_fsq_name(byte);
extern char find_fsq_mode(char *);
extern void set_fsq_data(struct fsq_data *data);
extern void get_fsq_data(struct fsq_data *data);
extern void start_fsq_tx();
extern void do_fsq_tx();

extern Si5351 si5351;

// BitXUltra.ino
extern struct state state;
extern struct vfo vfos[];
extern char b[], c[];
extern enum txcause inTx;
extern enum modes mode;
extern Frequency bfo_freq;
#if HAVE_PULSE
extern byte pulseState;
#endif

extern void _setFrequency(Frequency f, long fine);
extern void setFrequency(enum ritstate);
#if HAVE_BFO
extern void setBFO(Frequency f, long fine);
extern void setBFO(Frequency f);
#endif
extern bool TXon(enum txcause cause);
extern void TXoff();


// utils.cpp
extern const PROGMEM char S_USB[];
extern const PROGMEM char S_LSB[];
extern const PROGMEM char S_AUTO[];
extern const PROGMEM char S_ON[];
extern const PROGMEM char S_OFF[];
extern const PROGMEM char S_COMMA[];
extern const PROGMEM char S_COLON[];
extern const PROGMEM char S_CWBEACON[];
extern const PROGMEM char S_FSQBEACON[];
extern const PROGMEM char BLANKLINE[];

extern PGM_P mod_name(enum modulation);
extern char *strpad(char *, byte);
extern void init_vfo(unsigned char);
extern void init_state();
extern void sync_vfos();
extern void get_channel(unsigned char);
extern void put_channel(unsigned char);
extern void get_calibration(int32_t &);
extern void put_calibration(int32_t);
extern void put_beacon_text(const char *);
extern void print_beacon_text();
extern byte getEEPROMByte(unsigned int addr);
extern unsigned long pow10(unsigned int x);
extern bool interval(unsigned long *last, unsigned int limit);
extern void bleep(unsigned int freq, unsigned int duration);
extern void bleeps(unsigned int freq, unsigned int duration, byte count, unsigned int gap);
extern void bleep_check();
extern bool btnDown();
extern int waitBtnUp();

#if HAVE_SAVESTATE
extern void put_vfos();
extern void get_vfos();
extern void put_state();
extern void get_state();
#endif

#if HAVE_PULSE
extern void setupStackCanary();
extern uint16_t getMinFreeSpace();
#endif

// display.cpp
extern void initDisplay();
extern void setupLCD_BarGraph();
extern void setupLCD_BarGraph2();
extern char *BarGraph2(int val, int vmin, int vmax, byte maxchars);
extern void printLine1(const char *c);
extern void printLine2(const char *c);
extern void printLine1(const __FlashStringHelper *ifsh);
extern void printLine2(const __FlashStringHelper *ifsh);
extern void holdLine2(unsigned long ms);
extern void checkLine2Hold();
extern void miniMeter(const char c);
extern void updateDisplay();

#endif // BITXULTRA_H

