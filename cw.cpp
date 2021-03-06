
/* CW is generated by injecting the side-tone into the mic line.
 * Watch http://bitxhacks.blogspot.com for the CW hack
 * nonzero cwTimeout denotes that we are in cw transmit mode.
 * 
 * This function is called repeatedly from the main loop() hence, it branches
 * each time to do a different thing
 * 
 * There are three variables that track the CW mode
 * inTX     : true when the radio is in transmit mode 
 * keyDown  : true when the CW is keyed down, you maybe in transmit mode (inTX true) 
 *            and yet between dots and dashes and hence keyDown could be true or false
 * cwTimeout: Figures out how long to wait between dots and dashes before putting 
 *            the radio back in receive mode
 */

#include "bitxultra.h"

/*
 * Key Connections - HAVE_CW 1  (basic straight key only)
 * Expects a straight key shorting the CW_KEY line to ground. Nothing else.
 * 
 * Key Connections - HAVE_CW 2  (handles anything)
 * Makes use of the analog input to recognise no less than 5 key states - Open, Dah, Dit, Both, Closed (Straight) - by
 * wiring the dit and dah contact via two different resistors to a single analog input, thus producing different voltages.
 * Wire up 3 CW input pins on whatever socket you like, or across multiple sockets if you prefer so 
 * you can plug your key in to the appropriate socket.
 * Connect the dit pin to via a higher value resistor. Dah via a lower value. The values depend on your pullup resistor,
 * for a 10K pullup, a 10K and a 6K should do the job.
 * Optionally add a much higher value resistor (70K) to provide a TX switch via the same input. This will be useful 
 * if you like to use a foot switch or one of the Czech army keys with a built-in TX switch.
 * 
 *          10K
 * +5v ----/\/\/--|
 * CW_KEY input --+---------- Straight
 *                |---/\/\/-- Dit      6K6
 *                |---/\/\/-- Dah     10K
 *                ----/\/\/-- TX      66K
 *                 
 * Straight Key - just wire it up to short the input. Nothing new here.
 * 
 * Paddle (single or double) - connect dit side to Dit, dah side to Dah.
 * 
 * Bug - connect dit side to Dit, other side to Straight.
 * 
 * No mode switching or keyer selection required - just grab something and pound out CW :)
 * There are config options for swapping the paddles and ultimatic keyer mode though.
 * HAVE_CW 2 always operates in semi-break-in, however it supports holding PTT to keep the rig in TX while you key
 * and will go back to RX as soon as the timeout expires and PTT is released - that is there will always be a minimum
 * time of TX after the last dit/dah.
 * 
 */

unsigned long cwTimeout = 0;

#if HAVE_CW

#if HAVE_CW==1
static char keyDown = 0;
#endif

// Do frequency changes and TX start ready for CW TX
void CWstart(){
  #if HAVE_BFO
      setBFO(bfo_freq - state.sideTone);
  #else
      setFrequency(RIT_CW); // shift the VFO instead
  #endif
  TXon(INTX_CW);     // into TX mode, but no carrier yet  
}

// Return frequencies to SSB mode after CW
void CWstop(){
  register char ptt=digitalRead(PTT);
  #if HAVE_BFO
      setBFO(bfo_freq);
      si5351.output_enable(BFO_OUTPUT, 1);
  #else
      setFrequency(ptt ? RIT_OFF : RIT_ON);
  #endif
  if (ptt == 1){ // only return to RX if PTT is not pressed.
     TXoff();
  }
}

// Turn on the carrier
void CWon() {
    #if HAVE_BFO
      // generate the carrier right in the crystal filter passband.
      // might even be able to use the power out control of the 5351 to slope the envelope
      // Adjusting by the sidetone freq means tuning a signal to (or near) your preferred sidetone will
      // always put your reply on (or very near) their frequency.
      // Works the same for both LSB and USB!
      // Drive strength is ramped to help shape the envelope
      si5351.drive_strength(BFO_OUTPUT, SI5351_DRIVE_2MA); // 2,4,6 or 8ma
      si5351.output_enable(BFO_OUTPUT, 1);
      digitalWrite(CW_KEY, HIGH);
      delay(1);
      si5351.drive_strength(BFO_OUTPUT, SI5351_DRIVE_4MA); // 2,4,6 or 8ma
    #else
      digitalWrite(CW_KEY, HIGH);
    #endif
    tone(CW_TONE, state.sideTone);
}

// turn off the carrier
void CWoff() {
    #if HAVE_BFO
      si5351.drive_strength(BFO_OUTPUT, SI5351_DRIVE_2MA); // 2,4,6 or 8ma
      delay(1);
      si5351.output_enable(BFO_OUTPUT, 0);
      digitalWrite(CW_KEY, LOW);
    #else
      digitalWrite(CW_KEY, LOW);
    #endif
    noTone(CW_TONE);
}



#if HAVE_CW_SENDER
// CW patterns are in 1-bit-per slot, 1 byte per code.
// need 6 bits for dit/dah sequence. (..--.. = ?) - we have 7 bits we can use!
// use next lower bit to store "end code" and << code as it's sent until code==0x80.
// eg A = 0B01100000 = dit, dah, end.
//    N = 0B10100000 = dit, dah, end.
//    B = 0B10001000 = dah, dit, dit, dit, end.

// being able to buffer the entire beacon string simplifies things
#define CW_SEND_BUFLEN (CWBEACON_MAXLEN)
byte cw_send_buffer[CW_SEND_BUFLEN];
byte cw_send_head=0, cw_send_tail=0;
byte cw_send_state=0; // 0=code bit next, 1=gap bit next
byte cw_send_current=0; // remains of char we are currently sending.

// some "special" codes
#define CW_SPACE 0xFF
#define CW_END   0x80


// CW lookup tables.
const byte cw_table_num[10] PROGMEM = { // 0-9
    0B11111100, // 0
    0B01111100, // 1
    0B00111100,
    0B00011100,
    0B00001100,
    0B00000100, // 5
    0B10000100,
    0B11000100,
    0B11100100,
    0B11110100, // 9
};

const byte cw_table_alpha[26] PROGMEM = { // A-Z
    0B10100000, // A
    0B10001000,
    0B10101000,
    0B10010000,
    0B01000000, // E
    0B00101000,
    0B11010000,
    0B00001000,
    0B00100000, // I
    0B01111000,
    0B10110000,
    0B00101000,
    0B11100000, // M
    0B10100000,
    0B11110000,
    0B01101000,
    0B11011000, // Q
    0B01010000,
    0B00010000,
    0B11000000,
    0B00110000, // U
    0B00011000,
    0B01110000,
    0B10011000,
    0B01001000, // Y
    0B00111000
};

byte char2cw(char ch) {
  // translate a char into a code.
  byte b=0;
  ch=toupper(ch);
  if (ch>='A' && ch<='Z') b=pgm_read_byte(&cw_table_alpha[ch-'A']);
  else if (ch=='0')       b=pgm_read_byte(&cw_table_num[0]);
  else if (ch>='1' && ch<'9') b=pgm_read_byte(&cw_table_num[ch-'1'+1]);
  else {
    switch (ch) { // anything else
       case '.': b=0B01010110; break;
       case ',': b=0B11001110; break;
       case '?': b=0B00110010; break;
       case '\'':
       case '`': b=0B01111010; break;
       case '!': b=0B10101110; break;
       case '/': b=0B10010100; break;
       case '<':
       case '{':
       case '(': b=0B10110100; break;
       case '>':
       case '}':
       case ')': b=0B10110110; break;
       case '&': b=0B01000100; break;
       case ':': b=0B11100010; break;
       case ';': b=0B10101010; break;
       case '=': b=0B10001100; break;
       case '+': b=0B01010100; break;
       case '-': b=0B10000110; break;
       case '_': b=0B00110110; break;
       case '"': b=0B01001010; break;
       case '$': b=0B00010011; break;
       case '@': b=0B01101010; break;
       case ' ': b=CW_SPACE; break; // word gap
      }
  }
  return b;
}


// get the next code from the queue
static byte cw_send_get() {
  byte code=0;
  if (cw_send_head!=cw_send_tail) {
     code = cw_send_buffer[cw_send_tail];
     cw_send_tail++;
     if (cw_send_tail>=CW_SEND_BUFLEN) cw_send_tail=0;
  }
  return code;
}

// delete everything from the queue and schedule a stop sending.
void send_cw_flush() {
  cw_send_head=cw_send_tail=0;            // empty the queue
  if (cw_send_current) cw_send_current=0; // stop current send.
}

// add a code to the queue
bool send_cw_code(byte code) {
  // add a code to the send buffer if it's a valid code and there's room.
  if (code) {
     if (cw_send_current==0) { // not sending? get started.
        cw_send_current = code;
        return true;
     }

     byte h=cw_send_head+1;
     if (h>=CW_SEND_BUFLEN) h=0;
     if (h!=cw_send_tail) { // if incrementing the head won't catch the tail...
        cw_send_buffer[cw_send_head]=code; // save code to buffer
        cw_send_head=h;
        return true;
     }
  }  
  return false;
}

// this takes 2 chars and merges their codes to make a prosign.
// Totally generic - make up any prosign you like as long as the result fits in 7 elements.
// <HH> is converted to <HS> (will anyone notice the missing dit?)
byte prosign2cw(char c1, char c2) {

  if (c1=='H' && c2=='H') c2='S'; // cheating because we can only send 7-element codes.

  byte code1=char2cw(c1);
  byte code2=char2cw(c2);
  byte cend1=0, cend2=0;

  if ((code1==0) || (code2==0)) return 0;
  // find the length of the codes by finding the lowest set bit.
  while (!(code1 & (0x01<<cend1))) cend1++;
  while (!(code2 & (0x01<<cend2))) cend2++;

  if (cend1 + cend2 > 6 ) { // will it fit?
     byte prosign  = code1 & (0xFF<<(cend1+1));
          prosign |= code2 >> (7-cend1);
     return prosign;
  }
  return 0;
}

// convert a char to a code and add it to the queue. No prosigns.
// To send a prosign, use send_cw_code(prosign2cw(c1,c2)) or send_cw_string("<xx>")
bool send_cw_char(char ch) {
  return send_cw_code(char2cw(ch));
}


// _send_cw_string will convert a string into CW codes and put it in the send buffer. 
// Supports prosigns embedded as <XX>
// The source string is accessed via the provided getCharFunc so we can use this same
// code for any string stored anywhere.
typedef byte (*getCharFunc)(unsigned int addr);

void _send_cw_string(unsigned int ptr, byte maxlen, getCharFunc getChar) {
  char ch, ch2;
  unsigned int pse;
  unsigned int s=ptr;
  unsigned int maxpos=s+maxlen;
  
  while ((s<maxpos) && (ch=getChar(s))) { 
    pse=0;
    if (ch=='<') {
       pse=s+1;
       while ((ch2=getChar(pse)) && (ch2!='>') && (pse-s)<=3) pse++;
       if ((ch2=='>') && ((pse-s)==3)) {
          if (!send_cw_code(prosign2cw(getChar(s+1), getChar(s+2)))) pse=0;
       } else pse=0;
       send_cw_char(ch);
    }
    if (!pse) {
       send_cw_char(getChar(s));
    } else {
       s=pse;
    }
    s++;
  }  
}


// helper funcs for _send_cw_string accessing ram and progmem. The eeprom one lives in utils.cpp.
static byte _getByte_mem(unsigned int addr) {
  return *((char *)addr);
}
static byte _getByte_pgm(unsigned int addr) {
  return (char)pgm_read_byte(addr);
}

void send_cw_string(char *s) {
#if 1
  _send_cw_string((unsigned int)s, CW_SEND_BUFLEN, &_getByte_mem);
#else
  char *pse;
  while (*s) { 
    pse=NULL;
    if (*s=='<') {
      pse=s+1;
      while (*pse && (*pse!='>') && ((pse-s)<=3)) pse++;
      if ((*pse=='>') && ((pse-s)==3)) {
         if (!send_cw_code(prosign2cw(s[1],s[2]))) pse=NULL;
      } else pse=NULL;
    }
    if (!pse) {
      send_cw_char(*s);
    } else {
      s=pse;
    }
    s++;
  }
#endif
}

void send_cw_string(const __FlashStringHelper *fs) {
#if 1
  _send_cw_string((unsigned int)fs, CW_SEND_BUFLEN, &_getByte_pgm);
#else
  char ch, ch2, *pse, *s=(char *)fs;
  while ((ch=pgm_read_byte(s))) { 
    pse=NULL;
    if (ch=='<') {
       pse=s+1;
       while ((ch2=pgm_read_byte(pse)) && (ch2!='>') && (pse-s)<=3) pse++;
       if ((ch2=='>') && ((pse-s)==3)) {
          if (!send_cw_code(prosign2cw(pgm_read_byte(s+1), pgm_read_byte(s+2)))) pse=NULL;
       } else pse=NULL;
       send_cw_char(ch);
    }
    if (!pse) {
       send_cw_char(pgm_read_byte(s));
    } else {
       s=pse;
    }
    s++;
  }
#endif
}

void send_cw_string(const __EEPROMStringHelper *es, byte maxlen) {
#if 1
  _send_cw_string((unsigned int)es, maxlen, &getEEPROMByte);
#else
  char ch, ch2;
  unsigned int pse;
  unsigned int s=(unsigned int)es;
  unsigned int maxpos=s+maxlen;
  
  while ((s<maxpos) && (ch=EEPROM.read(s))) { 
    pse=0;
    if (ch=='<') {
       pse=s+1;
       while ((ch2=EEPROM.read(pse)) && (ch2!='>') && (pse-s)<=3) pse++;
       if ((ch2=='>') && ((pse-s)==3)) {
          if (!send_cw_code(prosign2cw(EEPROM.read(s+1), EEPROM.read(s+2)))) pse=0;
       } else pse=0;
       send_cw_char(ch);
    }
    if (!pse) {
       send_cw_char(EEPROM.read(s));
    } else {
       s=pse;
    }
    s++;
  }
#endif
}

#endif // HAVE_CW_SENDER


void checkCW(){
  // Note: ensure <=10k impedance on signal - a 10k pullup resistor works just fine.
  // the internal pullup is NOT enough.
  int key=analogRead(ANALOG_KEYER);

#if HAVE_CW == 2
  // NEW CW code - 6-state input to handle both straight key and paddle. Costs 300 bytes of progmem
  //               Even handles a TX switch on the same input.
  enum keystate { KS_NONE, KS_KEY, KS_DIT, KS_DAH, KS_TX };
  enum keystate newState=KS_NONE;
                 // = 1 minute / standard_word_len / wpm
  int dotlen = 60000 / 50 / state.wpm;
  int dashlen= dotlen * 3;

  static int last_key=0;

  static enum keystate cwstate=KS_NONE, last_cwstate=KS_NONE;
  static unsigned long last=0;
  static unsigned int  hold=0;
  
  if (!interval(&last,hold)) return;

  if (abs(key-last_key)>80) {
     // large change in value, let it stabilize so we don't get a read as it swings and re-read.
     delay(2);
     key=analogRead(ANALOG_KEYER);
  }
  last_key=key;
  
  #if HAVE_CW_SENDER
  if (key<=930) send_cw_flush(); // any manual keying stops the auto-keyer and wipes buffer.
  #endif
  
  if (key<50) {
     // straight key is pressed
     newState  = KS_KEY;
     hold      = 0;

  } else if (key>=200 && key<=330) {
     // both paddles pressed
     // cw_ultimatic:  false: iambic mode (both=dit,dah,dit,dah....)  true: ultimatic mode (both=dit,dit,dit.... or dah,dah,dah...)
     if ((last_cwstate==KS_DIT) != state.cw_ultimatic) { // setting ultimatic==true inverts the way this works :)
        newState = KS_DAH;
        hold     = dashlen;
     } else {
        newState = KS_DIT;
        hold     = dotlen;
     }
     
  } else if (key>=340 && key<=440) {
     // dit paddle is pressed
     if (state.cw_swap_paddles) {
        newState=KS_DAH;
        hold    =dashlen; // check again in 3 dits (aka dah)
     } else {
        newState=KS_DIT;
        hold    =dotlen; // check again in 1 dit
     }
     
  } else if (key>=450 && key<=600) {
     // dah paddle is pressed
     if (state.cw_swap_paddles) {
        newState=KS_DIT;
        hold    =dotlen; // check again in 1 dit
     } else {
        newState=KS_DAH;
        hold    =dashlen; // check again in 3 dits (aka dah)
     }

  } else if (key>=800 && key<930) {
     // For a separate TX switch for CW work.
     newState = KS_TX;
     
  } else if (key>930) {
     // nothing pressed
     newState=KS_NONE;
     hold    =dotlen; // enforce gap at least 1 dot.

     #if HAVE_CW_SENDER
       // if there is something in the send queue, send it

       switch (cw_send_current) {
         case 0: break; // not sending
         
         case CW_SPACE:
               // wait 4 dits (word gap - char gap)
               hold = 4 * dotlen;
               cw_send_current = cw_send_get(); // grab next code
               break;
          
         case CW_END:
               // end of ths code - insert char gap
               hold = 3 * dotlen;
               cw_send_current = cw_send_get();
               break;
               
         default:
               if (cw_send_state==0) {
                  // next bit of code
                  if (cw_send_current & 0x80) {
                     newState = KS_DAH;
                     hold = dashlen;
                  } else {
                     newState = KS_DIT;
                     hold = dotlen;
                  }
                  cw_send_state=1;
                  cw_send_current<<=1;
               } else {
                  // tx a 1-dit gap
                  newState=KS_NONE;
                  hold = dotlen;
                  cw_send_state=0;
               }
       }
     #endif
  }

  last_cwstate=cwstate;

  if (newState != KS_NONE) { // something is pressed, reset timeout and make sure TX is on
     
     if (inTx!=INTX_CW) {
        CWstart();
     }

    // set the carrier
    if (cwstate!=KS_NONE && newState!=KS_KEY) {
       // we are already doing a dit or dah, so inject a gap
       CWoff();
       miniMeter(' ');
       hold      = dotlen;
       cwstate   = KS_NONE;
       cwTimeout = CW_TIMEOUT + millis();
    } else if (newState==KS_TX) {
       // keep TX on, but not sending anything.
       hold = dotlen;
       cwTimeout = millis();
    } else {
       // not already in a dit or dah, or straight key is pressed
       if (cwstate==KS_NONE) {
          CWon();
       }
       miniMeter(newState==KS_DIT ? '.' : (newState==KS_DAH ? '-' : '_'));
       cwstate = newState;
       cwTimeout = CW_TIMEOUT + millis();
    }
  } else if (cwTimeout > 0) {
    if (cwstate != KS_NONE) {
       CWoff();     
       miniMeter(':');
       hold = dotlen;
       cwstate = KS_NONE;
       cwTimeout = CW_TIMEOUT + millis();
    }
    if (inTx!=INTX_NONE && cwTimeout < millis()) {
       miniMeter(':');
       cwTimeout=0;
       CWstop();
    }
  }

#else

  // OLD CW code - straight key only, no timing.
  if (keyDown == 0 && key < 50){     // Key Down
    cwTimeout = CW_TIMEOUT + millis();

    //switch to transmit mode if we are not already in it
    if (inTx == INTX_NONE){
       CWstart();
    }

    CWon();
    keyDown = 1;

  } else if (keyDown == 1) {
    if (key > 150){    // Key Up
       keyDown = 0;
       CWoff();
       cwTimeout = millis() + CW_TIMEOUT;
    } else {
       //reset the timer as long as the key is down
       cwTimeout = CW_TIMEOUT + millis();
    }
  } else if (cwTimeout > 0 && inTx!=INTX_NONE && cwTimeout < millis()){
    //if we are in cw-mode and have a keyup for a longish time
    //move the radio back to receive
    cwTimeout = 0;
    CWstop();
  }
#endif // HAVE_CW
}
#endif // HAVE_CW > 0

