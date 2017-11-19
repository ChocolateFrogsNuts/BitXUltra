
 /*
  * BitXUltra MENU system
  */

#include "bitxultra.h"

#if HAVE_MENU

typedef void (*menuHandler)();

struct menuItem {
  PGM_P mname;
  menuHandler handler;
};

unsigned char menuIdx=0;
struct adjustment adjustment_data;

static const char M_RIT  [] PROGMEM = "RIT";
static const char M_MOD  [] PROGMEM = "Sideband";
static const char M_CHAN [] PROGMEM = "Channel/VFO";
static const char M_SCHAN[] PROGMEM = "Store Channel";
static const char M_STONE[] PROGMEM = "S-Tone";
static const char M_WPM  [] PROGMEM = "WPM";
static const char M_KEYER[] PROGMEM = "Keyer Mode";
#define M_BEAC S_CWBEACON
static const char M_SYNC [] PROGMEM = "Sync VFOs";
static const char M_CAL  [] PROGMEM = "CALIBRATE";
static const char M_BFO  [] PROGMEM = "BFO-Trim";
static const char M_ANN  [] PROGMEM = "Analyser";
static const char M_SAVE [] PROGMEM = "Save Defaults";

static const char S_STORETO[] PROGMEM = "Store to";
static const char S_CAL    [] PROGMEM = "CAL";


#define defineAdjustment(_name,_desc,_min,_max,_step,_show,_change,_set) \
    static const struct adjustment _name PROGMEM = { \
          desc:_desc, value:0, min:_min, max:_max, step:_step, \
          cb_show:_show, cb_change:_change, cb_set:_set \
    }


void displayAdjustment() {
  struct adjustment *adj = &adjustment_data;
  if (adj->cb_show) adj->cb_show();
  else {
     sprintf_P(c, ((adj->value > 0) && (adj->min < 0)) ? PSTR("%S: +%ld") : PSTR("%S: %ld"), adj->desc, adj->value);
     printLine2(strpad(c, 16));
  }
}

// copies adjustment parameters from a record in progmem.
void startAdjustment(const struct adjustment &adj, unsigned long val)
{
  memcpy_P(&adjustment_data, &adj, sizeof(adjustment_data));
  adjustment_data.value = val;
  displayAdjustment();
}

char doAdjustment() {
  static unsigned long last=0;
  if (!interval(&last, 400)) return ADJ_NIL;

  struct adjustment *adj = &adjustment_data; // using a ptr makes no diff to code size.
  if (btnDown()) {
     mode=MODE_NORMAL;
     strcpy_P(c, adj->desc);
     strcat_P(c, PSTR(" Set"));
     printLine2(strpad(c,16));
     if (adj->cb_set) adj->cb_set();
     holdLine2(1000); // gives the user time to read it without having to delay here
     waitBtnUp();     // make sure the button has been released.
     return ADJ_SET;
  } else {
      int knob = analogRead(ANALOG_TUNING);
      long val = adj->value;
      if (knob < 400 && val > adj->min) {
         if (adj->step>0)      val -= adj->step;
         else if (adj->step<0) val -= pow10((400 - knob) / 120) * (-adj->step);
         else                  val -= pow10((400 - knob) / 120);
      } else if (knob > 600 && val < adj->max) {
         if (adj->step>0)      val += adj->step;
         else if (adj->step<0) val += pow10((knob - 600) / 120) * (-adj->step);
         else                  val += pow10((knob - 600) / 120);
      }
      if (val < adj->min) val=adj->min;
      else if (val > adj->max) val=adj->max;
      
      if (val != adj->value) {
         adj->value = val;
         displayAdjustment();
         if (adj->cb_change) adj->cb_change();
         return ADJ_CHANGE;
      }
  }
return ADJ_NIL;
}


/*
 * Menu callback naming conventions:
 * A_*  structure with adjustment name, limits, value if used
 * h_*  menu item activated
 * d_*  display the item's current value
 * c_*  Value was changed
 * s_*  Value should be set/stored.
 * Both c_ and s_ can be serviced by one handler.
 * If the d_ handler is NULL, the default is used. It will do for most settings,
 * One of c_ or s_ should be set or the changed value will never be stored.
 */

static void s_rit() {
             vfos[state.vfoActive].rit = adjustment_data.value;
             setFrequency(RIT_AUTO);
             vfos[state.vfoActive].ritOn=(adjustment_data.value!=0);
}
static void h_rit() {
             defineAdjustment(A_RIT, M_RIT, RIT_MIN, RIT_MAX, RIT_STEP, NULL, &s_rit, &s_rit);
             vfos[state.vfoActive].ritOn=true;
             startAdjustment(A_RIT, vfos[state.vfoActive].rit);
             mode=MODE_ADJUSTMENT;
}



static void d_sideband() {
             enum modulation m = (enum modulation)adjustment_data.value;
             strcpy_P(c,PSTR("Sideband: "));
             strcat_P(c, mod_name(m));
             printLine2(strpad(c, 16));
}
static void s_sideband() {
#if 1
             vfos[state.vfoActive].mod = (enum modulation)adjustment_data.value;
#else
             switch (adjustment_data.value) {
                  case 0:  vfos[state.vfoActive].mod=MOD_LSB;  break;
                  case 1:  vfos[state.vfoActive].mod=MOD_USB;  break;
                  case 2:  vfos[state.vfoActive].mod=MOD_AUTO; break;
                  default: vfos[state.vfoActive].mod=MOD_AUTO;
             }
#endif
             setFrequency(RIT_ON);
             holdLine2(1000);
             updateDisplay();
}
static void h_sideband() {
             defineAdjustment(A_SIDEBAND, M_MOD, 0, 2, 1, &d_sideband, NULL, &s_sideband);
             byte m;
#if 1
             m=(byte)vfos[state.vfoActive].mod;
#else
             switch (vfos[state.vfoActive].mod) {
                  case MOD_LSB:  m=0;  break;
                  case MOD_USB:  m=1;  break;
                  case MOD_AUTO: m=2;  break;
                  default: vfos[state.vfoActive].mod=MOD_AUTO;
             }
#endif
             startAdjustment(A_SIDEBAND, m);
             mode=MODE_ADJUSTMENT;
}


#if HAVE_CHANNELS
static void h_channel() {
             if (state.useVFO) {
                state.useVFO=false;
                get_channel(state.channelActive);
                setFrequency(RIT_ON);
             } else {
                state.useVFO=true;
                printLine2(F("    VFO Mode    "));
                holdLine2(1000);
             }
             updateDisplay();
             mode=MODE_NORMAL;
}


static void c_store() {
             state.channelActive = adjustment_data.value;
}
static void s_store() {
             put_channel(state.channelActive);
}
static void h_store() {
             defineAdjustment(A_STORE, S_STORETO, 0, state.channelCount-1, 1, NULL, &c_store, &s_store);
             startAdjustment(A_STORE, state.channelActive);
             mode=MODE_ADJUSTMENT;
}
#endif

#if HAVE_CW

static void s_sidetone() {
             state.sideTone = adjustment_data.value;
}
static void h_sidetone() {
             defineAdjustment(A_STONE, M_STONE, SIDETONE_MIN, SIDETONE_MAX, SIDETONE_STEP, NULL, &s_sidetone, &s_sidetone);
             startAdjustment(A_STONE, state.sideTone);
             mode=MODE_ADJUSTMENT;
}

#if HAVE_CW == 2
static void s_wpm() {
             state.wpm = adjustment_data.value;
}
static void h_wpm() {
             defineAdjustment(A_WPM, M_WPM, WPM_MIN, WPM_MAX, WPM_STEP, NULL, &s_wpm, &s_wpm);
             startAdjustment(A_WPM, state.wpm);
             mode=MODE_ADJUSTMENT;
}

static void s_keyermode() {
             state.cw_ultimatic    = (adjustment_data.value & 0x02) ? true : false;
             state.cw_swap_paddles = (adjustment_data.value & 0x01) ? true : false;
}
static void d_keyermode() {
             strcpy_P(c, adjustment_data.value & 0x02 ? PSTR("Ultimatic ") : PSTR("Iambic    "));
             strcat_P(c, adjustment_data.value & 0x01 ? PSTR("Rev") : PSTR("   "));
             printLine2(strpad(c,16));             
}

static void h_keyermode() {
             defineAdjustment(A_KEYER, M_KEYER, 0, 3, 1, &d_keyermode, &s_keyermode, &s_keyermode);
             byte v = 0;
             if (state.cw_ultimatic)    v |= 0x02;
             if (state.cw_swap_paddles) v |= 0x01;
             startAdjustment(A_KEYER, v);
             mode=MODE_ADJUSTMENT;
}
#if HAVE_CW_BEACON
static void s_cwbeacon() {
             state.cw_beacon_interval = adjustment_data.value;
             printLine2(FH(S_CWBEACON));
             mode=MODE_RUNBEACON;
}
static void h_cwbeacon() {
             defineAdjustment(A_BEAC, M_BEAC, CWBEACON_INTERVAL_MIN, CWBEACON_INTERVAL_MAX, CWBEACON_INTERVAL_STEP, 
                          NULL, NULL, &s_cwbeacon);
             startAdjustment(A_BEAC, state.cw_beacon_interval);
             mode=MODE_ADJUSTMENT;
}
#endif
#endif // HAVE_CW==2
#endif // HAVE_CW

static void h_sync() {
             sync_vfos();
             printLine2(F("VFOs Sync'd"));
             holdLine2(1000);
             mode=MODE_NORMAL;
}


#if NEW_CAL
static void c_calibration() {
             // while the calibration is in progress, keep tweaking the frequency
             si5351.set_correction(adjustment_data.value,SI5351_PLL_INPUT_XO);
             si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
             si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLB);
             setFrequency(RIT_OFF);
}
static void s_calibration() {
             put_calibration(adjustment_data.value);
             setFrequency(RIT_ON);
             printLine2(F("Calibrated    "));
             holdLine2(2000);
}
#endif

static void h_calibrate() {
#if NEW_CAL
             defineAdjustment(A_CAL, S_CAL, -300000, 300000, -100, NULL, &c_calibration, &s_calibration); // auto, 100*usual rate
             int32_t cal;
             get_calibration(cal);
             startAdjustment(A_CAL, cal);
             mode = MODE_ADJUSTMENT;
#else
             si5351.set_correction(0,SI5351_PLL_INPUT_XO);
             printLine2(F("Calibration... "));
             delay(3000);
             if (btnDown()) {
                mode = MODE_CALIBRATE;    
             }
#endif
}

#if HAVE_BFO
static void s_bfotrim() {
             state.bfo_trim = adjustment_data.value;
             setBFO(bfo_freq);
             setFrequency(RIT_AUTO);
}
static void h_bfotrim() {
             defineAdjustment(A_BFO, M_BFO, BFOTRIM_MIN, BFOTRIM_MAX, BFOTRIM_STEP, NULL, &s_bfotrim, &s_bfotrim);
             startAdjustment(A_BFO, state.bfo_trim);
             mode=MODE_ADJUSTMENT;
}
#endif

#if HAVE_ANALYSER
static void h_analyser() {
             startAnalyser();
}
#endif

#if HAVE_SAVESTATE
static void h_save() {
             put_state();
             put_vfos();
             printLine2(F(" Defaults Saved "));
             holdLine2(1000);
             mode=MODE_NORMAL;
}
#endif

static const struct menuItem menuItems[] PROGMEM = {
  { M_RIT,   &h_rit },
  { M_MOD,   &h_sideband },
#if HAVE_CHANNELS
  { M_CHAN,  &h_channel },
  { M_SCHAN, &h_store },
#endif
#if HAVE_CW
  { M_STONE, &h_sidetone },
  #if HAVE_CW == 2
  { M_WPM,   &h_wpm },
  { M_KEYER, &h_keyermode },
  #if HAVE_CW_BEACON
  { M_BEAC,  &h_cwbeacon },
  #endif
  #endif
#endif
  { M_SYNC,  &h_sync },
  { M_CAL,   &h_calibrate },
#if HAVE_BFO
  { M_BFO,   &h_bfotrim },
#endif
#if HAVE_ANALYSER
  { M_ANN,   &h_analyser },
#endif
#if HAVE_SAVESTATE
  { M_SAVE,  &h_save },
#endif
};

#define MENU_LEN (sizeof(menuItems)/sizeof(menuItems[0]))

void showMenuItem() {
  unsigned char len = strlcpy_P(c, (char *)pgm_read_word(&(menuItems[menuIdx].mname)), 17);
  while (len<16) c[len++]=' ';
  c[len++]='\0';
  printLine2(c);  
}

void setMenuItem(byte idx) {
  if (idx>=MENU_LEN) idx=0;
  menuIdx=idx;
}

void checkMenu(){
  static unsigned long last=0;
  static unsigned int  stepdelay=0;
  if (!interval(&last, stepdelay)) return;

  stepdelay=10;
  if (btnDown()) {
     
     // do menu item
     menuHandler handler= (menuHandler)pgm_read_word(&(menuItems[menuIdx].handler));
     handler();
     
     // make sure the button is released
#if NEW_CAL
     waitBtnUp();
#else
     if (mode != MODE_CALIBRATE) {
        waitBtnUp();
     }
#endif

     stepdelay=200;
     if ((mode==MODE_NORMAL) 
          #if HAVE_CHANNELS
          && (handler != &h_channel)
          #endif
        ) {
        holdLine2(300); // gives the user time to read it without having to delay here
     }
  } else {
    // check if the tuning knob is turned, change item/value
    int knob = analogRead(ANALOG_TUNING)-10;
    if (knob < 400) {
       setMenuItem(menuIdx==0 ? MENU_LEN-1 : menuIdx-1);
       showMenuItem();
       stepdelay=500;
    } else if (knob > 600) {
       setMenuItem(menuIdx+1);
       showMenuItem();
       stepdelay=500;
    }
  }
}
#endif // HAVE_MENU

