/*
 * Misc functions and some flash strings that are used in more than one cpp file.
 */

#include "bitxultra.h"

 /**
 * The EEPROM library is used to store settings like the frequency memory, caliberation data, 
 * callsign etc .
 */
#include <EEPROM.h>

const PROGMEM char S_USB[]="USB";
const PROGMEM char S_LSB[]="LSB";
const PROGMEM char S_AUTO[]="AUTO";

const PROGMEM char S_ON[]="ON";
const PROGMEM char S_OFF[]="OFF";

const char S_COMMA   [] PROGMEM = ",";
const char S_COLON   [] PROGMEM = ":";
const char S_CWBEACON[] PROGMEM = "CW Beacon";
const char S_        [] PROGMEM = "";
const char BLANKLINE [] PROGMEM = "                ";

/* 
 *  get the string name for a modulation 
 */
PGM_P mod_name(enum modulation mod) {
  switch (mod) {
    case MOD_LSB: return S_LSB;  break;
    case MOD_USB: return S_USB;  break;
    case MOD_AUTO:return S_AUTO; break;
  }
  return S_;
}

/*
 * Pad a string out to len.
 */
char *strpad(char *s, byte len) {
  byte i;
  i=strlen(s);
  while (i<len) s[i++]=' ';
  s[i]='\0';
  return s;
}

/*
 * Initialize a VFO to defaults
 */
void init_vfo(unsigned char i){
  vfos[i].magic     = VFO_MAGIC;
  vfos[i].frequency = (i == 0) ? 7100000l : 14200000l;
  vfos[i].rit       = 0;
  vfos[i].ritOn     = false;
  vfos[i].mod       = MOD_AUTO;
}

/*
 * Initialize the config (state, VFOs) to reasonable values
 */
void init_state() {
  char i;
  state.magic        = STATE_MAGIC;
  state.sideTone     = DEFAULT_SIDETONE;
  state.vfoActive    = 0;
  state.vfoCount     = VFO_COUNT;
  state.channelActive= 0;
  state.channelCount = CHANNEL_COUNT;
  state.bfo_trim     = 0;
  state.useVFO       = true;
  state.wpm          = DEFAULT_WPM;
  state.cw_swap_paddles=false;
  state.cw_ultimatic   =false;
  state.cw_beacon_interval=CWBEACON_INTERVAL_MIN;
  for (i=0; i<state.vfoCount; i++) {
      init_vfo(i);
  }
}

/*
 * Set all the VFOs to the current VFO's settings
 */
void sync_vfos() {
    unsigned char i;
    for (i=0; i<state.vfoCount; i++) {
      if (i==state.vfoActive) continue;
      vfos[i].frequency = vfos[state.vfoActive].frequency;
      vfos[i].rit       = vfos[state.vfoActive].rit;
      vfos[i].ritOn     = vfos[state.vfoActive].ritOn;
      vfos[i].mod       = vfos[state.vfoActive].mod;
    }
}



/*
 * Store/Retrieve the VFOs to/from EEPROM
 */
#if HAVE_SAVESTATE
void put_vfos()
{
  unsigned char i;
  for (i=0; i<state.vfoCount; i++) {
    EEPROM.put(VFO_EEPROM_START + (VFO_EEPROM_SIZE * i), vfos[i]);
  }
}

void get_vfos()
{
  unsigned char i;
  // read vfos up to our max...
  if (state.vfoCount > VFO_COUNT) state.vfoCount=VFO_COUNT;
  for (i=0; i<state.vfoCount; i++) {
    EEPROM.get(VFO_EEPROM_START + (VFO_EEPROM_SIZE * i), vfos[i]);
    if (vfos[i].magic != VFO_MAGIC) {
       init_vfo(i);
    }
  }
  // init any vfos we didn't read from EEPROM
  while (i<VFO_COUNT) {
    init_vfo(i++);
  }
}

void put_state()
{
  EEPROM.put(STATE_EEPROM_START, state);
}

void get_state()
{
  EEPROM.get(STATE_EEPROM_START, state);
}

#endif // HAVE_SAVESTATE

#if HAVE_CHANNELS
void put_channel(unsigned char channel) {
  EEPROM.put(CHANNEL_EEPROM_START + (CHANNEL_EEPROM_SIZE * channel), vfos[state.vfoActive]);
}

void get_channel(unsigned char c) {
  if (state.channelCount < c) {
    // set some defaults or just leave it as is?
    init_vfo(state.vfoActive);
  } else {
    EEPROM.get(CHANNEL_EEPROM_START + (CHANNEL_EEPROM_SIZE * c), vfos[state.vfoActive]);
    if (vfos[state.vfoActive].magic != VFO_MAGIC) {
       init_vfo(state.vfoActive);
    }
  }
}
#endif

void get_calibration(int32_t &cal) {
  EEPROM.get(0,cal);
}

void put_calibration(int32_t cal) {
  EEPROM.put(0,cal);
}

#if HAVE_CW_BEACON
// Store txt as the beacon text in EEPROM
void put_beacon_text(const char *txt) {
  byte i=0;
  while (*txt && i<CWBEACON_MAXLEN) {
        EEPROM.write(CWBEACON_EEPROM_START+i,*txt);
        i++; txt++;
  }
  if (i<CWBEACON_MAXLEN)
     EEPROM.write(CWBEACON_EEPROM_START+i,'\0'); // null terminate.
}

// Print the beacon text to Serial
void print_beacon_text() {
  byte i=0;
  char ch;
  while (i<CWBEACON_MAXLEN && (ch=EEPROM.read(CWBEACON_EEPROM_START+i))) {
      Serial.print(ch);
      i++;
  }
  Serial.println();
}

// helper func to save including EEPROM.h in cw.cpp
byte getEEPROMByte(unsigned int addr) {
  return EEPROM.read(addr);
}
#endif

/*
 *  An easy way to make sure a function only executes at a particular rate without using delay.
 *  
 *  Start the function as follows to only run it every 200ms.
 *  static unsigned long last=0;
 *  if (!interval(&last, 200)) return;
 */
bool interval(unsigned long *last, unsigned int limit) {
  register unsigned long now=millis();
  if (!*last || ((now - *last) >= limit)) {
     *last=now;
     return true;
  }
  return false;
}

#define BLEEP_QLEN 10
static unsigned long bleep_last=0;
static byte bleep_head=0, bleep_tail=0;
static byte bleep_freq[BLEEP_QLEN], bleep_len[BLEEP_QLEN];

void bleep(unsigned int freq, unsigned int duration) {
  bleep_freq[bleep_head]=freq/10;
  bleep_len [bleep_head]=duration/10;
  if (bleep_head==bleep_tail) {
     tone(CW_TONE, freq);
     bleep_last=millis();
  }
  bleep_head++;
  if (bleep_head>=BLEEP_QLEN) bleep_head=0;
}

void bleeps(unsigned int freq, unsigned int duration, byte count, unsigned int gap) {
  while (count>0) {
      bleep(freq,duration);
      bleep(0,gap);
      count--;
  }
}

void bleep_check() {
  if (bleep_head!=bleep_tail) {
     if (interval(&bleep_last, bleep_len[bleep_tail]*10)) {
        bleep_tail++;
        if (bleep_tail>=BLEEP_QLEN) bleep_tail=0;
        if ((bleep_head!=bleep_tail) && bleep_freq[bleep_tail]) {
           tone(CW_TONE, bleep_freq[bleep_tail]*10);
        } else {
           noTone(CW_TONE);
        }
     }
  }
}

/**
 * A trivial function to wrap around the function button
 */

// non-debounced
inline bool _btnDown() {
  return digitalRead(FBUTTON) != HIGH;
}

// debounced
bool btnDown(){
  if (_btnDown()) {
     delay(40);
     return _btnDown();
  }
  return false;
}

int waitBtnUp() {
  unsigned int timeout = 10000;
  unsigned long duration=0, start=millis();  
  do {
    while (_btnDown() && (duration=(millis()-start)) < timeout) {
        delay(10);
    }
    delay(10);
  } while (_btnDown() && (duration < timeout));
  
  return duration;
}

/*
 * We use our own pow10 function because we only need integer math.
 * This should be faster with the values of x involved (3-6), 
 * and doesn't add 10% to the size of the compiled code!
 */
#if defined(HAVE_SHUTTLETUNE) || defined(HAVE_MENU)
unsigned long pow10(unsigned int x) {
  unsigned long val=1;
  while (--x > 0) {
    val*=10;
  }
  return val;
}
#endif


#if HAVE_PULSE

#define STACK_CANARY_VAL 0xFD

extern int __heap_start, *__brkval; 
extern char *__bss_end;

int freeRam () 
{
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void setupStackCanary() {
  uint8_t *p = (uint8_t *)&__bss_end;  
  while(p <= SP)
    *p++ = STACK_CANARY_VAL;
}

uint16_t getMaxStackUsed(void) {
  const uint8_t *p = (uint8_t *)(__brkval == 0 ? (int) &__heap_start : (int) __brkval);

  while(*p == STACK_CANARY_VAL && p < (uint8_t *)&p) {
     p++;
  }
  return RAMEND - (uint16_t)p;
}

uint16_t getMinFreeSpace() {
  const uint8_t *p = (uint8_t *)(__brkval == 0 ? (int) &__heap_start : (int) __brkval);
  while(*p == STACK_CANARY_VAL && p < (uint8_t *)&p) {
     p++;
  }
  return p - (uint8_t *)(__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
#endif // HAVE_PULSE


