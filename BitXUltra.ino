/**
 * This source file is under General Public License version 3.
 * 
 * Most source code are meant to be understood by the compilers and the computers. 
 * Code that has to be hackable needs to be well understood and properly documented. 
 * Donald Knuth coined the term Literate Programming to indicate code that is written be 
 * easily read and understood.
 * 
 * The Raduino is a small board that includes the Arduino Nano, a 16x2 LCD display and
 * an Si5351a frequency synthesizer. This board is manufactured by Paradigm Ecomm Pvt Ltd
 * 
 * To learn more about Arduino you may visit www.arduino.cc. 
 * 
 * The Arduino works by first executing the code in a function called setup() and then it 
 * repeatedly keeps calling loop() forever. All the initialization code is kept in setup()
 * and code to continuously sense the tuning knob, the function button, transmit/receive,
 * etc is all in the loop() function. If you wish to study the code top down, then scroll
 * to the bottom of this file and read your way up.
 * 
 */

// Configuration and IO definitions now in config.h
// Structures and other non-config definitions in bitxultra.h
#include "bitxultra.h"

Si5351 si5351;

/**
 * The Arduino, unlike C/C++ on a regular computer with gigabytes of RAM, has very little memory.
 * We have to be very careful with variables that are declared inside the functions as they are 
 * created in a memory region called the stack. The stack has just a few bytes of space on the Arduino
 * if you declare large strings inside functions, they can easily exceed the capacity of the stack
 * and mess up your programs. 
 * We circumvent this by declaring a few global buffers as  kitchen counters where we can 
 * slice and dice our strings. These strings are mostly used to control the display. 
 */
char c[30], b[20];

struct state state;
struct vfo vfos[VFO_COUNT];
enum modes mode = MODE_NORMAL;
 

enum txcause inTx = INTX_NONE;

/** Tuning Mechanism of the Raduino
 *  We use a linear pot that has two ends connected to +5 and the ground. the middle wiper
 *  is connected to ANALOG_TUNNING pin. Depending upon the position of the wiper, the
 *  reading can be anywhere from 0 to 1024.
 *  The tuning control works in steps of 50Hz each for every increment between 50 and 950.
 *  Hence the turning the pot fully from one end to the other will cover 50 x 900 = 45 KHz.
 *  At the two ends, that is, the tuning starts slowly stepping up or down in 10 KHz steps 
 *  To stop the scanning the pot is moved back from the edge. 
 *  To rapidly change from one band to another, you press the function button and then
 *  move the tuning pot. Now, instead of 50 Hz, the tuning is in steps of 50 KHz allowing you
 *  rapidly use it like a 'bandset' control.
 *  To implement this, we fix a 'base frequency' to which we add the offset that the pot 
 *  points to. We also store the previous position to know if we need to wake up and change
 *  the frequency.
 */

#define INIT_BFO_FREQ (11998000L)
Frequency baseTune =  7100000L;
Frequency bfo_freq = 11998000L;

#if !HAVE_SHUTTLETUNE
int  old_knob = 0;
#endif


#if HAVE_PULSE
unsigned long pulseLast=0;
byte          pulseState=LOW;
#endif


/**
 * To use calibration sets the accurate readout of the tuned frequency
 * To calibrate, follow these steps:
 * 1. Tune in a signal that is at a known frequency.
 * 2. Now, set the display to show the correct frequency, 
 *    the signal will no longer be tuned up properly
 * 3. Press the CAL_BUTTON line to the ground
 * 4. tune in the signal until it sounds proper.
 * 5. Release CAL_BUTTON
 * In step 4, when we say 'sounds proper' then, for a CW signal/carrier it means zero-beat 
 * and for SSB it is the most natural sounding setting.
 * 
 * Calibration is an offset that is added to the VFO frequency by the Si5351 library.
 * We store it in the EEPROM's first four bytes and read it in setup() when the Radiuno is powered up
 */
#if !NEW_CAL
void calibrate(){
    int32_t cal;

    // The tuning knob gives readings from 0 to 1000
    // Each step is taken as 10 Hz and the mid setting of the knob is taken as zero
    cal = (analogRead(ANALOG_TUNING) - 500) * 500ULL;

    // if the button is released, we save the setting
    // and delay anything else by 5 seconds to debounce the CAL_BUTTON
    // Debounce : it is the rapid on/off signals that happen on a mechanical switch
    // when you change it's state
    if (digitalRead(CAL_BUTTON) == HIGH){
      mode = MODE_NORMAL;
      put_calibration(cal);
      printLine2(F("Calibrated    "));
      delay(5000);
      setFrequency(RIT_ON);
    }
    else {
      // while the calibration is in progress (CAL_BUTTON is held down), keep tweaking the
      // frequency as read out by the knob, display the change in the second line
      si5351.set_correction(cal,SI5351_PLL_INPUT_XO);
      si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
      si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLB);

      setFrequency(RIT_OFF);
      updateDisplay();
      sprintf_P(c, PSTR("CAL: %08ld "), cal);
      printLine2(c);
    }  
}
#endif // !NEW_CAL

/**
 * The setFrequency is a little tricky routine, it works differently for USB and LSB
 * 
 * The BITX BFO is permanently set to lower sideband, (that is, the crystal frequency 
 * is on the higher side slope of the crystal filter).
 * 
 * LSB: The VFO frequency is subtracted from the BFO. Suppose the BFO is set to exactly 12 MHz
 * and the VFO is at 5 MHz. The output will be at 12.000 - 5.000  = 7.000 MHz
 * 
 * USB: The BFO is subtracted from the VFO. Makes the LSB signal of the BITX come out as USB!!
 * Here is how it will work:
 * Consider that you want to transmit on 14.000 MHz and you have the BFO at 12.000 MHz. We set
 * the VFO to 26.000 MHz. Hence, 26.000 - 12.000 = 14.000 MHz. Now, consider you are whistling a tone
 * of 1 KHz. As the BITX BFO is set to produce LSB, the output from the crystal filter will be 11.999 MHz.
 * With the VFO still at 26.000, the 14 Mhz output will now be 26.000 - 11.999 = 14.001, hence, as the
 * frequencies of your voice go down at the IF, the RF frequencies will go up!
 * 
 * Thus, setting the VFO on either side of the BFO will flip between the USB and LSB signals.
 */

// without regard for filters and RIT.
void _setFrequency(Frequency f, long fine) {
  Frequency bfo=bfo_freq+state.bfo_trim;
  switch (vfos[state.vfoActive].mod) {
    case MOD_LSB:  f = bfo - f; break;
    case MOD_USB:  f = bfo + f; break;
    case MOD_AUTO: f = (f<10000000UL) ? bfo - f : bfo + f; break;
  }
  //Serial.print("LO:");
  //Serial.println(f);
  si5351.set_freq((f * SI5351_FREQ_MULT) + fine, SI5351_CLK2);
}
void _setFrequency(Frequency f) {
  _setFrequency(f,0);
}

#if HAVE_BFO
void setBFO(Frequency f, long fine) {
  f+=state.bfo_trim;
  //Serial.print("BFO:");
  //Serial.println(f);
  si5351.set_freq((f * SI5351_FREQ_MULT) + fine, BFO_OUTPUT);
}
void setBFO(Frequency f) {
  setBFO(f, 0);
}
#endif

// apply filters and RIT
void setFrequency(enum ritstate withRit){
  Frequency f = vfos[state.vfoActive].frequency;

  if (withRit==RIT_AUTO) {
     if (inTx==INTX_NONE) withRit=RIT_ON;
     if (inTx==INTX_CW)   withRit=RIT_CW;
  }

  switch (withRit) {                 
    case RIT_ON: if (vfos[state.vfoActive].ritOn) {
                    f += vfos[state.vfoActive].rit;
                 }
                 break;
    #if !HAVE_BFO
    case RIT_CW: // without BFO control we must move the VFO by the sidetone freq
                 switch (vfos[state.vfoActive].mod) {
                   case MOD_LSB:  f = f - state.sideTone; break;
                   case MOD_USB:  f = f + state.sideTone; break;
                   case MOD_AUTO: f = (f<10000000UL) ? f - state.sideTone : f + state.sideTone; break;
                 }
                 break;
    #endif
    default: break;
  }

  #if HAVE_FILTERS
  setFilters(findBand(f));
  #endif

  _setFrequency(f);
}



/**
 * The Check TX toggles the T/R line. The current BITX wiring up doesn't use this
 * but if you would like to make use of RIT, etc, You must wireup an NPN transistor to to the PTT line
 * as follows :
 * emitter to ground, 
 * base to TX_RX line through a 4.7K resistr(as defined at the top of this source file)
 * collecter to the PTT line
 * Now, connect the PTT to the control connector's PTT line (see the definitions on the top)
 * 
 * Yeah, surprise! we have CW supported on the Raduino
 */
#if HAVE_PTT

// Put the rig into TX mode if we are allowed on this frequency.
bool TXon(enum txcause cause) {
  //if (inTx!=INTX_NONE) return false;
  const struct band *b=findBand(vfos[state.vfoActive].frequency);
  if (b && b->tx) {
     inTx = cause;
     updateDisplay();
     if (vfos[state.vfoActive].ritOn || cause==INTX_CW) {
        setFrequency(cause == INTX_CW ? RIT_CW : RIT_OFF);
     }
     digitalWrite(TX_RX, 1);
     //give the relays a few ms to settle the T/R relays
     delay(30);
     return true;
  } else {
     inTx=INTX_DIS;
     updateDisplay();
     return false;
  }
}

void TXoff() {
  inTx = INTX_NONE;
  if (vfos[state.vfoActive].ritOn || inTx==INTX_CW) {
     setFrequency(RIT_ON); // return to listen frequency
  }
  digitalWrite(TX_RX, 0);
  #if HAVE_BFO
  if (inTx==INTX_CW) setBFO(bfo_freq);
  #endif
  updateDisplay();
}

void checkTX(){
  
  // We don't check for ptt when transmitting cw
  // as long as the cwTimeout is non-zero, we will continue to hold the
  // radio in transmit mode
  if (cwTimeout > 0)
    return;
    
  register char ptt=digitalRead(PTT);
  // make TX_RX reflect PTT input
  if (ptt == 0 && inTx == INTX_NONE){
     TXon(INTX_PTT);
  }
	
  if (ptt == 1 && inTx!=INTX_NONE){
     TXoff();
  }
}
#endif // HAVE_PTT


/**
 * A click on the function button toggles the RIT
 * A double click selects the next VFO
 * A long press copies both the VFOs to the same frequency or activates the menu if enabled.
 */
void checkButton(){
  int knob, new_knob;
  unsigned long t1, duration=0;
  
  //rest of this function is interesting only if the button is pressed
  if (!btnDown())
    return;
    
  // note the time of the button going down and where the tuning knob was
  t1 = millis();
  knob = analogRead(ANALOG_TUNING);
  
  // if you move the tuning knob within 3 seconds (3000 milliseconds) of pushing the button down
  // then consider it to be a coarse tuning where you you can move by 100 Khz in each step
  // This is useful only for multiband operation.
  while (btnDown() && (duration=(millis()-t1)) < 3000){
  
      new_knob = analogRead(ANALOG_TUNING);
      //has the tuninng knob moved while the button was down from its initial position?
      if (abs(new_knob - knob) > 10){
        /* track the tuning and return */
        while (btnDown()){
          vfos[state.vfoActive].frequency = baseTune = ((analogRead(ANALOG_TUNING) * 30000l) + 1000000l);
          setFrequency(RIT_ON);
          updateDisplay();
          delay(200);
        }
        waitBtnUp();
        return;
      } /* end of handling the bandset */
      
     delay(100);
  }

  //we reach here only upon the button being released

  // if the button has been down for more than TAP_HOLD_MILLIS, we consider it a long press
  // set all VFOs to the same frequency, update the display and be done
  // or if HAVE_MENU activate the menu mode
  if (duration > TAP_HOLD_MILLIS){
#if HAVE_MENU
    showMenuItem();
    mode = MODE_MENU;
    waitBtnUp();
#else
    sync_vfos();
    printLine2(F("VFOs Reset!"));
#endif
    delay(300);
    //updateDisplay();
    return;    
  }

  //now wait for another click
  delay(100);
  // if there a second button press, toggle the VFOs
  if (btnDown()){

     //Change to next VFO on double tap
     state.vfoActive++;
     if (state.vfoActive >= state.vfoCount) state.vfoActive=0;

     // If we don't have a menu, also store the VFO defaults... if we have that.
#if !HAVE_MENU
#if HAVE_SAVESTATE
     put_state();
     put_vfos();
#endif
#endif
     updateDisplay();
  }
  // No, there was not more taps
  else {
    //on a single tap, toggle the RIT
    vfos[state.vfoActive].ritOn = vfos[state.vfoActive].ritOn ? false : true;
    updateDisplay();
  }    
  waitBtnUp();
}

/**
 * The Tuning mechansim of the Raduino works in a very innovative way. It uses a tuning potentiometer.
 * The tuning potentiometer that a voltage between 0 and 5 volts at ANALOG_TUNING pin of the control connector.
 * This is read as a value between 0 and 1000. Hence, the tuning pot gives you 1000 steps from one end to 
 * the other end of its rotation. Each step is 50 Hz, thus giving approximately 50 Khz of tuning range.
 * When the potentiometer is moved to either end of the range, the frequency starts automatically moving
 * up or down in 10 Khz increments
 */


void doTuning(){
 static unsigned long last=0;
 static unsigned int stepdelay=0;
 if (!interval(&last, stepdelay)) return;
 
 // never let the tuning move during TX
 if (inTx!=INTX_NONE) return;

 int knob = analogRead(ANALOG_TUNING)-10;
 Frequency frequency = vfos[state.vfoActive].frequency;
 
#if HAVE_SHUTTLETUNE

  int delta=0;
  if (knob < 400) {
     delta=400-knob;
     frequency -= pow10((delta/120)+3);
  } else if (knob > 600) {
     delta=knob-600;
     frequency += pow10((delta/120)+3);
  }
  if (delta) {
     stepdelay = 400 - ((int)((delta % 120)/30) * 100);
  } else {
     stepdelay=400;
  }
#else // original tuning  
  
  stepdelay=200;
  // the knob is fully on the low end, move down by 10 Khz and wait for 200 msec
  if (knob < 10 && frequency > LOWEST_FREQ) {
      baseTune = baseTune - 10000l;
      frequency = baseTune;
  } 
  // the knob is full on the high end, move up by 10 Khz and wait for 200 msec
  else if (knob > 1010 && frequency < HIGHEST_FREQ) {
     baseTune = baseTune + 10000l; 
     frequency = baseTune + 50000l;
  }
  // the tuning knob is at neither extremities, tune the signals as usual
  else if (knob != old_knob){
     frequency = baseTune + (50l * knob);
     old_knob = knob;
     stepdelay=0;
  }
#endif

  // if the frequency was changed, update things
  if (frequency != vfos[state.vfoActive].frequency) {
     #if TUNE_BANDS_ONLY
       frequency = findNextBandFreq(frequency);
     #endif
     vfos[state.vfoActive].frequency = frequency;
     setFrequency(RIT_ON);
     updateDisplay();
  }
}


#if HAVE_CHANNELS
void doChannel() {
  static unsigned long last=0;
  if (!interval(&last, 400)) return;
  
  int knob = analogRead(ANALOG_TUNING) - 10;
  unsigned char c=state.channelActive;
  if (knob < 400) {
     if (c>0) c--; else c=state.channelCount-1;
  } else if (knob > 600) {
     c++;
     if (c>=state.channelCount) c=0;
  }
  if (c != state.channelActive) {
     state.channelActive=c;
     get_channel(state.channelActive);
     setFrequency(RIT_ON);
     updateDisplay();
  }
}
#endif



/**
 * setup is called on boot up
 * It sets up the modes for various pins as inputs or outputs
 * initiliaizes the Si5351 and sets various variables to initial state
 * 
 * Just in case the LCD display doesn't work well, the debug log is dumped on the serial monitor
 * Choose Serial Monitor from Arduino IDE's Tools menu to see the Serial.print messages
 */

static const char S_VERSION[] PROGMEM = "BitXUltra 1.0";
static const char S_AUTHOR [] PROGMEM = "  by VK6MN   ";

void setup()
{
  int32_t cal;
  
#if HAVE_PULSE
  setupStackCanary();
#endif
  
  initDisplay();

  printLine1(FH(S_VERSION));
  printLine2(FH(S_AUTHOR));
  
  // Start serial and initialize the Si5351
  Serial.begin(9600);
  analogReference(DEFAULT);
  Serial.print(F("*"));
  Serial.println(FH(S_VERSION));
  Serial.print(F("* by "));
  Serial.println(FH(S_AUTHOR));
  
  //configure the function button to use the external pull-up
  pinMode(FBUTTON, INPUT_PULLUP);

#if HAVE_PTT
  pinMode(PTT, INPUT_PULLUP);
  pinMode(TX_RX, OUTPUT);
  digitalWrite(TX_RX, LOW);
  //delay(500);
#endif

#if CAL_BUTTON != FBUTTON
  pinMode(CAL_BUTTON, INPUT_PULLUP);
#endif

#if HAVE_CW
  pinMode(CW_KEY, OUTPUT);  
  pinMode(CW_TONE, OUTPUT);  
  digitalWrite(CW_KEY, LOW);
  digitalWrite(CW_TONE, LOW);
#endif

#ifdef FILTER_PIN0
  pinMode(FILTER_PIN0, OUTPUT);
#endif
#ifdef FILTER_PIN1
  pinMode(FILTER_PIN1, OUTPUT);
#endif
#ifdef FILTER_PIN2
  pinMode(FILTER_PIN2, OUTPUT);
#endif

#if HAVE_SHUTTLETUNE
  vfos[state.vfoActive].frequency = baseTune;
#endif

  get_calibration(cal);
  si5351.init(SI5351_CRYSTAL_LOAD_8PF,25000000l, cal);
  //Serial.print(F("*CAL:"));
  //Serial.println(cal);
  //si5351.set_correction(cal,SI5351_PLL_INPUT_XO);

#if HAVE_SAVESTATE
  get_state();
  if (state.magic == STATE_MAGIC) {
     Serial.println(F("*Reading Config"));
     get_vfos();
  } else
#endif  
  {
    Serial.println(F("*Defaults Loaded"));
    printLine2(F("Defaults Loaded"));
    init_state();
  }

  Serial.println(F("*Initialize Si5351"));

  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLB);
  //si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA);
  si5351.output_enable(SI5351_CLK0, 0);
  si5351.output_enable(SI5351_CLK1, 0);
  si5351.output_enable(SI5351_CLK2, 1);
//  si5351.set_freq(5000000ULL * SI5351_FREQ_MULT,  SI5351_CLK2);   

#if HAVE_BFO
  setBFO(bfo_freq);
  si5351.output_enable(BFO_OUTPUT, 1);
#endif
  Serial.println(F("*Si5351 ON"));
  mode = MODE_NORMAL;
  delay(10);

#if HAVE_MENU
  // Holding FBUTTON at power-on will reset the config
  if (btnDown()) {
    init_state();
    printLine2(F("  Clear Config  "));
  }
  setFrequency(RIT_ON);
#else
  // if there's no menu, allow calibration at power on by holding Func/CAL
  if (digitalRead(CAL_BUTTON) == LOW) {
    mode = MODE_CALIBRATE;    
    si5351.set_correction(0,SI5351_PLL_INPUT_XO);
    setFrequency(RIT_OFF);
    printLine2(F("Calibration... "));
  } else {
    setFrequency(RIT_ON);
  }
#endif

  // so the user gets to read whetever is on the display
  holdLine2(1000);
  delay(500);
  
  updateDisplay();
  bleeps(700,50,3,50); // freq,duration,repeat,gap
}


#if HAVE_CW_BEACON || HAVE_FSQ_BEACON
unsigned long last_beacon_start=0;
#endif


void loop(){

#if 0
  // some test code to see how often loop() runs.
  // basically checks to see if something is using delay significantly.
  static unsigned long counter=0;
  static unsigned long etime=0;
  counter++;
  if ((millis()-etime) > 1000) {
     sprintf(c,"%ld loops in %ld millis", counter, millis()-etime);
     Serial.println(c);
     etime=millis();
     counter=0;
  }
#endif

#if HAVE_PULSE
  // there is a LED wired to pin 13.. it can be useful to indicate loop is running...
  if (interval(&pulseLast, 1000)) {
     pulseState = (pulseState==LOW) ? HIGH : LOW;
     digitalWrite(LED_BUILTIN, pulseState);

     // Dump the minimum amount of free RAM to Serial whenever it changes.
     // We use the STACK CANARY technique to know the max size the stack has ever been.
     static uint16_t minfree=0xFFFF;
     uint16_t freespace = getMinFreeSpace();
     if (freespace<minfree) {
        Serial.print(F("*Min Free Space: "));
        Serial.println(freespace);
        minfree=freespace;
     }
  }
#endif

#if HAVE_PTT
  if (inTx != INTX_ANA && inTx != INTX_CAT) {
     #if HAVE_CW
       checkCW();
     #endif
     checkTX();
  }
#endif

  checkLine2Hold(); // clears line 2 if something else hasn't done it already.
  bleep_check();
  
  switch (mode) {
    case MODE_NORMAL:
         #if HAVE_SWR || HAVE_SMETER
            doMeters();
         #endif
         #if HAVE_CHANNELS
            if (!state.useVFO)
               doChannel();
            else
         #endif
            doTuning();

         // checkButton is last so the display isn't overwritten after a mode change.
         checkButton();
         break;

#if !NEW_CAL
    case MODE_CALIBRATE:
         calibrate();
         break;
#endif

#if HAVE_MENU
    case MODE_MENU:
         checkMenu();
         break;
    case MODE_ADJUSTMENT: 
         // Generic adjustment mode - handles all settings with callbacks in menu.cpp.
         switch (doAdjustment()) {
            case ADJ_CHANGE:
            case ADJ_SET:
                 break;
         }
         break;

#if HAVE_CW_BEACON
    case MODE_CWBEACON:
         if (btnDown()) {
            mode=MODE_NORMAL;
            last_beacon_start=0;
            send_cw_flush();
            printLine2(F("  Beacon Off    "));
            holdLine2(500);
            waitBtnUp();
         } else if (interval(&last_beacon_start, (unsigned long)state.cw_beacon_interval * 1000UL)) {
            // copy the beacon string into the send buffer
            send_cw_string(EH(CWBEACON_EEPROM_START), CWBEACON_MAXLEN);
            printLine2(S_CWBEACON);
         }
         break;
#endif // HAVE_CW_BEACON

#if HAVE_FSQ_BEACON
    case MODE_FSQBEACON:
         if (btnDown()) {
            mode=MODE_NORMAL;
            last_beacon_start=0;
            printLine2(F("  Beacon Off    "));
            holdLine2(500);
            waitBtnUp();
         } else if (interval(&last_beacon_start, (unsigned long)state.cw_beacon_interval * 1000UL)) {
            printLine2(S_FSQBEACON);
            start_fsq_tx();
         } else {
            do_fsq_tx();
         }
         break;
#endif // HAVE_CW_BEACON

#if HAVE_ANALYSER
    case MODE_ANALYSER:
         doAnalyser();
         break;
#endif // HAVE_ANALYSER

#endif // HAVE_MENU
    default:
         mode=MODE_NORMAL;
  }

  delay(5);
}

