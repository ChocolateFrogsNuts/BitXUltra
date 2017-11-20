/*
 * FSQ (JT*, WSPR) for the BITX
 * 
 * Based on the code at https://github.com/bobh/Bitx40FSQdemo/
 * 
 * Requires Jason Milldrum's JTEncode library.
 */

#include "bitxultra.h"

#if HAVE_FSQ_BEACON

#include <JTEncode.h>

static const char S_JT9  [] PROGMEM = "JT9";
static const char S_JT65 [] PROGMEM = "JT65";
static const char S_JT4  [] PROGMEM = "JT4";
static const char S_WSPR [] PROGMEM = "WSPR";
static const char S_FSQ2 [] PROGMEM = "FSQ2";
static const char S_FSQ3 [] PROGMEM = "FSQ3";
static const char S_FSQ45[] PROGMEM = "FSQ4.5";
static const char S_FSQ6 [] PROGMEM = "FSQ6";

struct fsq_params {
   PGM_P name;
   int tone_spacing;
   int tone_delay;
   Frequency freq_ofs;   
   byte symbol_count;
};

// Order is important - indexed by FSQ_* constants
const struct fsq_params fsq_params[] PROGMEM = {
   // Name,  tone spacing, delay, frequency, symbol count
   {S_JT9,   174, 576,  +3050,  JT9_SYMBOL_COUNT},
   {S_JT65,  269, 371,  +1350, JT65_SYMBOL_COUNT},
   {S_JT4,   437, 229,  +1350,  JT4_SYMBOL_COUNT},
   {S_WSPR,  146, 683,  +1350, WSPR_SYMBOL_COUNT},
   {S_FSQ2,  879, 500,  +1350, 0},
   {S_FSQ3,  879, 333,  +1350, 0},
   {S_FSQ45, 879, 222,  +1350, 0},
   {S_FSQ6,  879, 167,  +1350, 0},
};


struct fsq_params fsq_mode_params={S_JT4,   437, 229, 14078500UL,  JT4_SYMBOL_COUNT};
byte fsq_buffer_pos=0; // current pos in buffer
byte fsq_buffer_len=0; // total symbols in the buffer
byte fsq_buffer[255];  // the buffer of symbols to TX.
JTEncode jtencode;

char find_fsq_mode(char *m) {
  byte i=0;
  while (i<(sizeof(fsq_params)/sizeof(fsq_params[0]))) {
    if (!strcmp_P(m, pgm_read_word(&(fsq_params[i].name)))) {
       return i;
    }
    i++;
  }
  return -1;
}


PGM_P get_fsq_name(byte i) {
  if (i<(sizeof(fsq_params)/sizeof(fsq_params[0]))) {
     return pgm_read_word(&(fsq_params[i].name));
  }
  return PSTR("---");
}

void encode_data(byte *tx_buffer, struct fsq_data *data) {
#if HAVE_FSQ_BEACON==WITH_FSQ_ANY
  if       (fsq_mode_params.name==S_JT9)  jtencode.jt9_encode (data->message, tx_buffer);
  else if  (fsq_mode_params.name==S_JT65) jtencode.jt65_encode(data->message, tx_buffer);
  else if  (fsq_mode_params.name==S_JT4)  jtencode.jt4_encode (data->message, tx_buffer);
  else if  (fsq_mode_params.name==S_WSPR) jtencode.wspr_encode(data->call, data->loc, data->dbm, tx_buffer);
  else {
          //jtencode.fsq_dir_encode(data->call, "to_call", ' ', data->message, tx_buffer);
          jtencode.fsq_encode(data->call, data->loc, tx_buffer);
       }
       
#elif HAVE_FSQ_BEACON==WITH_FSQ_JT9
  jtencode.jt9_encode (data->message, tx_buffer);
  
#elif HAVE_FSQ_BEACON==WITH_FSQ_JT65
  jtencode.jt65_encode(data->message, tx_buffer);
  
#elif HAVE_FSQ_BEACON==WITH_FSQ_JT4
  jtencode.jt4_encode (data->message, tx_buffer);
  
#elif HAVE_FSQ_BEACON==WITH_FSQ_WSPR
  jtencode.wspr_encode(data->call, data->loc, data->dbm, tx_buffer);
  
#else // FSQ_2, _3, _4_5, _6
  jtencode.fsq_encode(data->call, data->loc, tx_buffer);
#endif
}


void start_fsq_tx() {
  fsq_buffer_len=0;
  fsq_buffer_pos=0;
  memset(fsq_buffer, 0, sizeof(fsq_buffer));

  if (state.fsq_mode>=(sizeof(fsq_params)/sizeof(fsq_params[0]))) return;

  struct fsq_data data;
  #if HAVE_FSQ_BEACON==WITH_FSQ_ANY
    memcpy_P(&fsq_mode_params, &fsq_params[state.fsq_mode], sizeof(fsq_mode_params));
  #else
    memcpy_P(&fsq_mode_params, &fsq_params[HAVE_FSQ_BEACON-1], sizeof(fsq_mode_params));
  #endif
  
  get_fsq_data(&data);
  encode_data(fsq_buffer, &data);
  
  if (fsq_mode_params.symbol_count==0) {
     byte i=0;
     while((i<sizeof(fsq_buffer)) && (fsq_buffer[i++]!=0xFF));
     fsq_buffer_len=i-1;
  } else {
     fsq_buffer_len=fsq_mode_params.symbol_count;
  }

  // begin TX
  setFrequency(RIT_OFF);
  updateDisplay();
  if (!TXon(INTX_FSQ)) {
     fsq_buffer_len=0; // abort if we can't TX
  }
}

unsigned long fsq_last_change=0;

/*
 * Frequency Selection:
 * Output base freq is at dial frequency +/- fsq_mode_params.freq (depending on USB/LSB).
 * Then we add/subtract the individual symbol freq.
 * 
 * So to run WSPR on 40m tune 7.0386 (the nominal freq). The output will be at +1350hz (except JT9).
 * If you want to tx a little higher/lower, tune a bit.  The channel feature might be useful for setting up 
 * the nominal frequency and USB mode for your favourite beacon operations.
 */


void do_fsq_tx() {
  
  if (fsq_buffer_pos<fsq_buffer_len) {
     if (!interval(&fsq_last_change, fsq_mode_params.tone_delay)) return;
     
     #if HAVE_BFO
        //si5351.set_freq(((bfo_freq + state.bfo_trim) * SI5351_FREQ_MULT) - (fsq_buffer[fsq_buffer_pos] * fsq_mode_params.tone_spacing), BFO_OUTPUT);
        setBFO(bfo_freq, -(fsq_buffer[fsq_buffer_pos] * fsq_mode_params.tone_spacing));
     #else
        Frequency f = vfos[sttate.vfoActive].frequency;
        Frequency delta = (fsq_mode_params.freq * SI5351_FREQ_MULT) + (fsq_buffer[fsq_buffer_pos] * fsq_mode_params.tone_spacing);
        case (vfos[state.vfoActive].mod) {
           MOD_AUTO: if (f>=10000000) break;
           MOD_LSB: delta = -delta; break;
           MOD_USB: break;
        }
        // our CW method - use the BFO as the carrier and unbalance the BFO mixer.
        _setFrequency(f, delta);
        
        // "other" CW method - set VFO directly to output freq and unbalance final mixer
        //si5351.set_freq((f * SI5351_FREQ_MULT) + delta, SI5351_CLK2);
     #endif
     fsq_buffer_pos++;
  } else if (fsq_buffer_pos>0) {
     if (!interval(&fsq_last_change, fsq_mode_params.tone_delay)) return;
     // return to RX mode
     #if HAVE_BFO
       setBFO(bfo_freq);
     #endif
     setFrequency(RIT_AUTO);
     TXoff();
     fsq_buffer_pos=0;
     fsq_buffer_len=0;
  }
}

#endif // HAVE_FSQ_BEACON

