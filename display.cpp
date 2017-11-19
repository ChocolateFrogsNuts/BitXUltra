
 /** 
 * The Raduino board is the size of a standard 16x2 LCD panel.
 * 
 * 16 pin LCD connector. This connector is meant specifically for the standard 16x2
 * LCD display in 4 bit mode. The 4 bit mode requires 4 data lines and two control lines to work:
 * Lines used are : RESET, ENABLE, D4, D5, D6, D7 
 * We include the library and declare the configuration of the LCD panel too
 */
#include <LiquidCrystal.h>
LiquidCrystal lcd(8,9,10,11,12,13);

#include "bitxultra.h"

static char printBuff[17];

// remember what the 8 special chars are set up for.
enum special_states { SP_NONE, SP_VBG, SP_HBG } specialconfig=SP_NONE;

// Lock the content of line2 for a time...
unsigned long line2hold_start=0;
unsigned  int line2hold_delay=0;

void initDisplay()
{
  lcd.begin(16, 2);
  printBuff[0] = 0;
  //specialconfig=SP_NONE;
}

// Configure the custom chars on the display for a VERTICAL bar graph
// For value 0 use ' ', other values = char 1-7, max (solid block)=char 255
void setupLCD_BarGraph() {
  byte i;
  if (specialconfig==SP_VBG) return;
  specialconfig=SP_VBG;
#if 0
  // uses 48 bytes more progmem but no dynamic
  byte img[8];
  img[0]=B00000;
  for (i=1; i<8; i++) {
      img[i]=B11111;
  }
  for (i=1; i<8; i++) {
      lcd.createChar(8-i,img);
      //delay(25);
      img[i]=B00000;
  }
#else
  // uses 56 bytes more dynamic but saves 48 bytes progmem
  static byte img[7][8] = {
    {0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F},
    {0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F},
    {0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F},
    {0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F},
  };
  for (i=0; i<7; i++)
      lcd.createChar(7-i, img[i]);
#endif
}

// HORIZONTAL bar graph
// For value 0 use ' ', values 1-5 = char 1 - 5
// For a long line... 5,5,5,5...,x
void setupLCD_BarGraph2() {
  byte i;
  if (specialconfig==SP_HBG) return;
  specialconfig=SP_HBG;
#if 1
  // less progmem, no dynamic
  byte img[8];
  byte j,v;
  v=0x1E;
  for (i=0; i<4; i++) {
    for (j=0; j<8; j++) img[j]=v;
    lcd.createChar(4-i,img);
    v = (v<<1);// & 0x1F;
  }
#else
  // 4 bytes more progmem, uses 32 bytes dynamic
  static byte img[4][8] = {
    {0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E},
    {0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C},
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18},
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},
  };
  for (i=0; i<4; i++)
      lcd.createChar(4-i, img[i]);
#endif
}

// fill buffer with a bar that takes maxchars chars
// The coloured portion will be based on val's position from vmin to vmax.
char * BarGraph2(int val, int vmin, int vmax, byte maxchars) {
  static char buf[20];
  if (maxchars>(sizeof(buf)-1)) maxchars=sizeof(buf-1);
  int v = (maxchars * 5) * (val - vmin) / (vmax - vmin);
  if (v>maxchars*5) v=maxchars*5;
  else if (v<0) v=0;
  byte pos=v/5;
  memset(buf, 0xFF, pos);
  buf[pos++] = (v % 5) == 0 ? ' ' :  v % 5;
  while (pos<maxchars) buf[pos++]=' ';
  buf[pos] = '\0';
  return buf;
}


/**
 * Display Routines
 * These display routines print a line of characters to the upper and lower lines of the 16x2 display
 * Now available in FLASH string versions!
 */

// NOTE: there is an onboard LED we can use as a pulse... but it is also a data bit for the LCD!
//       the workaround is to set it's state after every write to the LCD.

void printLine1(const char *s){
  if (strncmp(printBuff, s, sizeof(printBuff))){
    strlcpy(printBuff, s, sizeof(printBuff));
    lcd.setCursor(0, 0);
    lcd.print(printBuff);
    #if HAVE_PULSE
       digitalWrite(LED_BUILTIN,pulseState);
       //Serial.print("*L1:");
       //Serial.println(printBuff);
    #endif
  }
}

void printLine1(const __FlashStringHelper *ifsh){
  PGM_P p = reinterpret_cast<PGM_P>(ifsh);
  if (strncmp_P(printBuff, p, sizeof(printBuff))){
    strlcpy_P(printBuff, p, sizeof(printBuff));
    lcd.setCursor(0, 0);
    lcd.print(printBuff);
    #if HAVE_PULSE
       digitalWrite(LED_BUILTIN,pulseState);
       //Serial.print("*L1:");
       //Serial.println(printBuff);
    #endif
  }
}


void printLine2(const char *c){
  if (interval(&line2hold_start, line2hold_delay)) {
     lcd.setCursor(0, 1);
     lcd.print(c);
     line2hold_delay=0;
     #if HAVE_PULSE
       digitalWrite(LED_BUILTIN,pulseState);
       //Serial.print("*L2:");
       //Serial.println(c);
     #endif
  }
}

void printLine2(const __FlashStringHelper *ifsh){
  if (interval(&line2hold_start, line2hold_delay)) {
     lcd.setCursor(0, 1);
     lcd.print(ifsh);
     line2hold_delay=0;
     #if HAVE_PULSE
       digitalWrite(LED_BUILTIN,pulseState);
       //Serial.print("*L2:");
       //Serial.println(c);
     #endif
  }
}

void holdLine2(unsigned long ms) {
  line2hold_start=millis();
  line2hold_delay=ms;
}

void checkLine2Hold() {
  if (line2hold_delay && interval(&line2hold_start, line2hold_delay+200)) {
     line2hold_delay=0;
     printLine2(FH(BLANKLINE));
  }
}

void miniMeter(const char c) {
  lcd.setCursor(1, 0);
  //lcd.print(c);
  lcd.write(c);
}

/**
 * Building upon the previous  two functions, 
 * update Display paints the first line as per current state of the radio
 * 
 * At present, we are not using the second line. YOu could add a CW decoder or SWR/Signal strength
 * indicator
 */

void updateDisplay(){

    sprintf_P(b, PSTR("%08ld"), vfos[state.vfoActive].frequency);
    sprintf_P(c, PSTR("%c:%.2s.%.4s"), 'A'+state.vfoActive, b, b+2);

    strcat(c, (inTx==INTX_NONE && vfos[state.vfoActive].mod == MOD_AUTO) ? "*" : " ");
    switch (vfos[state.vfoActive].mod) {
      case MOD_LSB: strcat_P(c, S_LSB); break;
      case MOD_USB: strcat_P(c, S_USB); break;
      case MOD_AUTO:strcat_P(c, (vfos[state.vfoActive].frequency>=10000000l) ? S_USB : S_LSB);
    }

    if (inTx==INTX_DIS)
      strcat_P(c, PSTR("DIS"));
    else if (inTx==INTX_CAT)
      strcat_P(c, PSTR("REM"));
    else if (inTx==INTX_ANA)
      strcat_P(c, PSTR("ANA"));
    else if (inTx==INTX_CW || cwTimeout>0)
      strcat_P(c, PSTR(" CW"));
    else if (inTx==INTX_PTT)
      strcat_P(c, PSTR(" TX"));
    else if (vfos[state.vfoActive].ritOn)
      strcat_P(c, PSTR(" +R"));
    else
      strcat_P(c, PSTR("   "));
    
    printLine1(c);

    if ((mode==MODE_NORMAL) && !state.useVFO) {
       sprintf_P(c, PSTR("   Channel %2d   "), state.channelActive);
       printLine2(c);
    }
}

