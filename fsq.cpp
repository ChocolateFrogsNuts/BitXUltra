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

struct fsq_params {
   PGM_P name;
   int tone_spacing;
   int tone_delay;
   Frequency freq;   
   byte symbol_count;
};

static const char S_JT9  [] PROGMEM = "JT9";
static const char S_JT65 [] PROGMEM = "JT65";
static const char S_JT4  [] PROGMEM = "JT4";
static const char S_WSPR [] PROGMEM = "WSPR";
static const char S_FSQ2 [] PROGMEM = "FSQ2";
static const char S_FSQ3 [] PROGMEM = "FSQ3";
static const char S_FSQ45[] PROGMEM = "FSQ4.5";
static const char S_FSQ6 [] PROGMEM = "FSQ6";


enum fsq_modes { FSQ_JT9, FSQ_JT65, FSQ_JT4, FSQ_WSPR, FSQ_2, FSQ_3, FSQ_4_5, FSQ_6 };
const struct fsq_params fsq_params[] PROGMEM = {
   // Name,  tone spacing, delay, frequency, symbol count
   {S_JT9,   174, 576, 14078700UL,  JT9_SYMBOL_COUNT},
   {S_JT65,  269, 371, 14078300UL, JT65_SYMBOL_COUNT},
   {S_JT4,   437, 229, 14078500UL,  JT4_SYMBOL_COUNT},
   {S_WSPR,  146, 683, 14097200UL, WSPR_SYMBOL_COUNT},
   {S_FSQ2,  879, 500,  7103865UL, 0},
   {S_FSQ3,  879, 333,  7103865UL, 0},
   {S_FSQ45, 879, 222,  7103865UL, 0},
   {S_FSQ6,  879, 167,  7103865UL, 0},
};


struct fsq_params fsq_mode_params;
enum fsq_modes fsq_mode = FSQ_2;
byte fsq_buffer_pos=0; // current pos in buffer
byte fsq_buffer_len=0; // total symbols in the buffer
byte fsq_buffer[255];  // the buffer of symbols to TX.
JTEncode jtencode;

void encode_data(byte *tx_buffer, struct fsq_data *data) {
  switch (fsq_mode) {
    case FSQ_JT9 : jtencode.jt9_encode (data->message, tx_buffer); break;
    case FSQ_JT65: jtencode.jt65_encode(data->message, tx_buffer); break;
    case FSQ_JT4 : jtencode.jt4_encode (data->message, tx_buffer); break;
    case FSQ_WSPR: jtencode.wspr_encode(data->call, data->loc, data->dbm, tx_buffer); break;
    case FSQ_2:
    case FSQ_3:
    case FSQ_4_5:
    case FSQ_6:    jtencode.fsq_dir_encode(data->call, "to_call", ' ', data->message, tx_buffer);
                   //jtencode.fsq_encode(data->call, data->loc, tx_buffer);
                   break;
  }
}


void start_fsq_tx() {
  fsq_buffer_len=0;
  fsq_buffer_pos=0;
  memset(fsq_buffer, 0, sizeof(fsq_buffer));

  struct fsq_data data;
  get_fsq_data(&data);
  encode_data(fsq_buffer, &data);
  
  memcpy_P(&fsq_mode_params, &fsq_params[(byte)fsq_mode], sizeof(mode));
  
  if (fsq_mode_params.symbol_count==0) {
     byte i=0;
     while(i<sizeof(fsq_buffer) && fsq_buffer[i++]!=0xFF);
     fsq_buffer_len=i-1;
  } else {
     fsq_buffer_len=fsq_mode_params.symbol_count;
  }

  // begin TX, making sure the VFO is configured for USB and the correct freq.
  vfos[state.vfoActive].frequency=fsq_mode_params.freq;
  vfos[state.vfoActive].mod=MOD_USB;
  setFrequency(RIT_OFF);
  updateDisplay();
  if (!TXon(INTX_FSQ)) {
     fsq_buffer_len=0; // abort if we can't TX
  }

}

unsigned long fsq_last_change=0;

void do_fsq_tx() {
  
  if (fsq_buffer_pos<fsq_buffer_len) {
     if (!interval(&fsq_last_change, fsq_mode_params.tone_delay)) return;
     
     #if HAVE_BFO
        //si5351.set_freq(((bfo_freq + state.bfo_trim) * SI5351_FREQ_MULT) - (fsq_buffer[fsq_buffer_pos] * fsq_mode_params.tone_spacing), BFO_OUTPUT);
        setBFO(bfo_freq, -(fsq_buffer[fsq_buffer_pos] * fsq_mode_params.tone_spacing));
     #else
        // our CW method - use the BFO as the carrier and unbalance the BFO mixer.
        _setFrequency(fsq_mode_params.freq, fsq_buffer[fsq_buffer_pos] * fsq_mode_params.tone_spacing);
        
        // "other" CW method - set VFO directly to output freq and unbalance final mixer
        //si5351.set_freq((fsq_mode_params.freq * SI5351_FREQ_MULT) + (fsq_buffer[fsq_buffer_pos] * fsq_mode_params.tone_spacing), SI5351_CLK2);
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

