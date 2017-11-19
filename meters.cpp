
/*
 * BitXUltra SWR Meter, S-Meter and Analyser
 */

#include "bitxultra.h"

#if HAVE_SWR || HAVE_SMETER

// these three are also accessed by remote.cpp
unsigned int  last_swr=0;     // last swr reading * 100
unsigned char avg_s_level=0;
unsigned char peak_s_level=0;

static unsigned long last_update=0;
static unsigned long last_recalc=0;

#define HIST_COUNT 50
static unsigned int  rp_hist[HIST_COUNT];
static unsigned int  fp_hist[HIST_COUNT];
static unsigned int   s_hist[HIST_COUNT];
static unsigned char hist_pos=0;
#if HAVE_SMETER
static unsigned int  s_hist_avg=0, s_hist_peak=0;
#endif


inline void _to_slevel_up(int &s, unsigned long &p, unsigned int m, unsigned int d, byte stp) {
     while ((p<=S9_LEVEL) && (s>0)) {
        p  = (p*m)/d;
        s -= stp;
     }  
}

inline void _to_slevel_dn(int &s, unsigned long &p, unsigned int m, unsigned int d, byte stp) {
     while ((p>S9_LEVEL) && (s<170)) {
        p  = (p*m)/d;
        s += stp;
     }  
}

int to_slevel(int v) {
  // convert analogRead value to S-units. 1-9=slevel, then 10=10/9, 11=20/9, 12=30/9, 13=40/9....
  // this produces the same result (for our purposes), but is 1300 bytes smaller than 20.0 * log10((double)v/S9_LEVEL))
  // Returns S-Level * 10  (eg 50 = S5) with resolution 0.1 +/-0.1 s-units. (better than most digital displays)
  unsigned long p=v;
  int s;

  if (p<=S9_LEVEL) {
     s = 90;
     _to_slevel_up(s, p, 2,1, 10); // reduce s by 10 for every 6dB below S9

     if (p<=S9_LEVEL) {
        _to_slevel_up(s, p, 10715,10000, 1); // reduce s by 1 for every 0.6dB below S9
     } else {
        _to_slevel_dn(s, p, 9332,10000, 1); // increase s by 1 for every 0.6dB above S9
     }
  } else {
     s=90;
     _to_slevel_dn(s, p, 3162,10000, 10); // increase s by 10 for every 10dB above S9

     if (p<=S9_LEVEL) {
        _to_slevel_up(s, p, 11202,10000, 1); // reduce s by 1 for every 1dB below S9
     } else {
        _to_slevel_dn(s, p, 8913,10000, 1);  // increase s by 1 for every 1dB above S9
     }
  }

  return s;
}

#if 0
int to_slevel_log(int v) {
  if (v==0) return 0;
  int db =  20.0 * log10((double)v/S9_LEVEL);
  if (v>S9_LEVEL) return 90+db;
  return 90+(db*10/6);  
}

void s_level_test() {
  static int test_done=0;
  if (test_done) return;
  test_done=1;
  int i;

  for (i=0; i<10; i++) {
      sprintf(c,"i=%d,  S1: %d,  S2: %d", i, to_slevel(i), to_slevel_log(i));
      Serial.println(c);
  }
  for (i=10; i<100; i+=10) {
      sprintf(c,"i=%d,  S1: %d,  S2: %d", i, to_slevel(i), to_slevel_log(i));
      Serial.println(c);
  }

  for (i=100; i<=1400; i+=100) {
      sprintf(c,"i=%d,  S1: %d,  S2: %d", i, to_slevel(i), to_slevel_log(i));
      Serial.println(c);
  }
}
#endif

struct power_stats {
  unsigned int avg_fp, peak_fp;
  unsigned int avg_rp, peak_rp;
  unsigned int avg_s,  peak_s;
};

static void read_meters() {
      s_hist[hist_pos] = analogRead(S_POWER) * S_POWER_CAL / 100;
     rp_hist[hist_pos] = analogRead(R_POWER) * R_POWER_CAL / 100;
     fp_hist[hist_pos] = analogRead(F_POWER) * F_POWER_CAL / 100;
     if (++hist_pos>=HIST_COUNT) hist_pos=0;
}

static void calc_power_stats(struct power_stats &p) {
  byte i;
#if HIST_COUNT>64
  // more than 64 samples means total may not fit in an unsigned int (64*1024), so use unsigned long.
  unsigned long int tot_fp, tot_rp, tot_s;
#else
  unsigned int tot_fp, tot_rp, tot_s;
#endif

  p.peak_rp=rp_hist[0];
  p.peak_fp=fp_hist[0];
  p.peak_s = s_hist[0];
  tot_rp =rp_hist[0];
  tot_fp =fp_hist[0];
  tot_s  = s_hist[0];

  for (i=1; i<HIST_COUNT; i++) {
      if (rp_hist[i]>p.peak_rp) p.peak_rp=rp_hist[i];
      tot_rp += rp_hist[i];
      if (fp_hist[i]>p.peak_fp) p.peak_fp=fp_hist[i];
      tot_fp += fp_hist[i];
      if (s_hist[i]>p.peak_s) p.peak_s=s_hist[i];
      tot_s += s_hist[i];
  }
  p.avg_rp = tot_rp / HIST_COUNT;
  p.avg_fp = tot_fp / HIST_COUNT;
  p.avg_s  = tot_s  / HIST_COUNT;
}

static unsigned int calc_swr(unsigned int fp, unsigned int rp) {
     if (fp==rp) return 999;
     if (fp==0 || rp==0)  return 100;
     register long swr;
     
     // these all come up with much the same results.
     // FIXME: hardware seems to report relatively high value when rp is small. This messes up our readings :(
     swr = (100 * (fp + rp))/(fp - rp);  // SWR Meter
     //swr = (100 * (100+((100*rp/fp)))) / (100-((100*rp)/fp));

     // this one might give better spread
     //swr = 100 * (log10(fp/100) + log10(rp/100)) / (log10(fp/100) - log10(rp/100));
     return abs(swr);     
}


/*
 * Display either an S-Meter (RX) or SWR meter (TX) in line2.
 */
void doMeters() {
  read_meters();
  
  if (!interval(&last_recalc,50)) return;

  struct power_stats pwr;
  calc_power_stats(pwr);
  
#if !HAVE_PTT
  // If we don't have a PTT connection we can guess based on forward power :)
  if (p.avg_fp > 1 && p.avg_fp>p.avg_rp) {
#else
  if (inTx!=INTX_NONE) {
#endif
     #if HAVE_SMETER
        s_hist_avg=9999; // force first update returning to RX
     #endif
     #if HAVE_SWR
     unsigned int swr, fp, rp;
     //fp=pwr.peak_fp; rp=pwr.peak_rp;
     fp=pwr.avg_fp;  rp=pwr.avg_rp;
     
     swr = calc_swr(fp, rp);
     
     if ((swr != last_swr) && interval(&last_update,200)) {
        //sprintf_P(c, PSTR("%5.1d %3.1f %5.1f "), to_power(fpa)/1000, (swr >= 990) ? 9.9 : swr/100, to_power(rpa)/1000);
        sprintf_P(c, PSTR("%5u %5u S%03d"), fp, rp, swr>999 ? 999 : swr);

        //unsigned int p=fp*fp;
        //sprintf_P(c, PSTR("P:%8u S:%03d"), p,swr);
        printLine2(c);
        last_swr=999;//swr;
     }
     #endif
  } else {        // S-METER
     last_swr=9999;

#if HAVE_SMETER
     unsigned char s_level = to_slevel(pwr.avg_s);
     unsigned char s_peak  = to_slevel(pwr.peak_s);

#if HAVE_SMETER == SMETER_HIRES
     // Graphic - uses 300 bytes more progmem than text version (LCD setup code)
     // more updates, smoothest display
     if (((pwr.avg_s != s_hist_avg) || (pwr.peak_s != s_hist_peak)) && interval(&last_update, 200)) {
        unsigned char peak=16 * s_peak / 170;
        char *bar=BarGraph2(s_level, 0, 170, 16);
        if (bar[8]==' ') bar[8]=':';
        if (peak>0 && bar[peak-1]==' ') bar[peak-1] = '|';

        setupLCD_BarGraph2();
        printLine2(bar);
        s_hist_avg=pwr.avg_s;
        s_hist_peak=pwr.peak_s;
        avg_s_level=s_level;
        peak_s_level=s_peak;
     }
#else
     avg_s_level  = s_level;
     peak_s_level = s_peak;
     s_level /= 10;
     s_peak  /= 10;
     if (((pwr.avg_s != s_hist_avg) || (pwr.peak_s != s_hist_peak)) && interval(&last_update, 200)) {
     #if HAVE_SMETER == SMETER_TEXT
         // text graph - least progmem used
         byte i;
         strcpy_P(c, PSTR("........:......."));
         for (i=0; i<s_level; i++) c[i]='|';
         if (s_peak>0 && s_peak>s_level) c[s_peak-1]='*';
     #elif HAVE_SMETER == SMETER_BARS
         // Graphic using analyser glyphs (looks like the familiar signal bars)
         byte i;
         setupLCD_BarGraph();
         for (i=0; i<s_level; i++) c[i] = (i>=7 ? 0xFF : i+1);
         while (i<16) c[i++]=' ';
         c[i++]='\0';
         if (s_level<9) c[8]=':';
         if (s_peak>0 && s_peak>s_level) c[s_peak-1]='|';
     #elif HAVE_SMETER == SMETER_MINI
         // Like _BARS but only displays largest bar (ie always 1 char wide)
         // displayed in second char of line 1.
         setupLCD_BarGraph();
         char m=s_level/2;
         if (m>=7) m=0xFF;
         else if (m==0) m=' ';
         miniMeter(m);
     #elif HAVE_SMETER == SMETER_NUMERIC
         // Numeric - uses more progmem than text graph because of sprintf.
         #define StoNum(s) (s<=9 ? s : (s-9) * 10)
         sprintf_P(c, PSTR("S%2d  P%2d          "), StoNum(s_level), StoNum(s_peak));
     #elif HAVE_SMETER == SMETER_NONE

     #elif HAVE_SMETER == SMETER_DEBUG
         // debugging - print rpa values
         //sprintf_P(c, PSTR("%4d %7ld %2d "),avg_rpa, to_power(rpa)/100, s_level);
         sprintf_P(c, PSTR("F%3d P%3d S%02d/%02d"), pwr.avg_s, pwr.peak_s, s_level, s_peak);
     #else
         #error HAVE_SMETER is not set to a valid value in config.h
     #endif
     #if HAVE_SMETER != SMETER_NONE && HAVE_SMETER != SMETER_MINI
         printLine2(c);
     #endif
     s_hist_avg=pwr.avg_s;
     s_hist_peak=pwr.peak_s;
     }
#endif // HAVE_SMETER == SMETER_HIRES
#endif // HAVE_SMETER
  }
}


#if HAVE_ANALYSER

static const struct band * analyser_band;
static Frequency prev_freq;
static Frequency analyser_step;
static          char results[17];
static unsigned char resultspos;

// Similar to CWon/CWoff but far less BFO shift, no VFO adjustment and no sidetone.
static void toneOn() {
  #if HAVE_BFO
      setBFO(bfo_freq-50); // generate tone just inside the edge of the crystal filter passband
      si5351.drive_strength(BFO_OUTPUT, SI5351_DRIVE_2MA); // 2,4,6 or 8ma
      si5351.output_enable(BFO_OUTPUT, 1);
  #endif
  digitalWrite(CW_KEY, HIGH);
}

static void toneOff() {
  #if HAVE_BFO
      setBFO(bfo_freq);
  #endif
  digitalWrite(CW_KEY, LOW);
}

void startAnalyser() {
  resultspos=0;
  
  if ((analyser_band = findBand(vfos[state.vfoActive].frequency))) {
     if (analyser_band->tx) {
        setupLCD_BarGraph();
        analyser_step = (analyser_band->hi - analyser_band->lo)/16;
        prev_freq=vfos[state.vfoActive].frequency;
        vfos[state.vfoActive].frequency = analyser_band->lo + (analyser_step/2);
        
        setFrequency(RIT_OFF);
        // output low powered carrier (some leaks in through the edge of the xtal filter)
        if (TXon(INTX_ANA)) { // should always be true, but....
           mode=MODE_ANALYSER;
           toneOn();
           //Serial.println(F("AN:start"));
           printLine2(F("  Analysing...  "));
        } else {
           printLine2(F("  Can't TX      "));
        }
     } else {
        analyser_band=NULL;
        printLine2(F("  Not TX Band   "));
     }
  } else {
    printLine2(F("  No Band       "));
  }
}


void doAnalyser() {
  /*
   * This is called at each run of loop() if in analyser mode.
   */
  if (analyser_band) {
     byte i;
     struct power_stats pwr;
     delay(10); // let the last frequency change stabilize
     
     for (i=0; i<HIST_COUNT; i++) {
         read_meters();
     }
     calc_power_stats(pwr);
     
     unsigned int swr = calc_swr(pwr.avg_fp, pwr.avg_rp);
     
     Serial.print(F("AN:"));
     Serial.print(vfos[state.vfoActive].frequency);
     Serial.print(F(", "));
     Serial.println(swr);

     if      (swr < 102 ) results[resultspos]=' ';
     else if (swr < 110 ) results[resultspos]=0x01;
     else if (swr < 120 ) results[resultspos]=0x02;
     else if (swr < 150 ) results[resultspos]=0x03;
     else if (swr < 200 ) results[resultspos]=0x04;
     else if (swr < 300 ) results[resultspos]=0x05;
     else if (swr < 500 ) results[resultspos]=0x06;
     else if (swr <1000 ) results[resultspos]=0x07;
     else                 results[resultspos]=0xFF;
     resultspos++;
     
     vfos[state.vfoActive].frequency += analyser_step;
     if (resultspos>=sizeof(results) || vfos[state.vfoActive].frequency >= analyser_band->hi) {
        // back to RX on previous frequency
        toneOff();
        TXoff();
        results[resultspos++]='\0';
        analyser_band=NULL;
        vfos[state.vfoActive].frequency = prev_freq;
        setFrequency(RIT_ON);

        // charset test
        //strcpy_P(results,PSTR("\xFF \x01\x02\x03\x04\x05\x06\x07\xFF      "));

        // show the user what we got
        updateDisplay();
        printLine2(results);
        Serial.println(F("AN:done"));
     } else {
        setFrequency(RIT_OFF);
        updateDisplay();
     }
  } else if (btnDown()) {
     // hold the display until the user presses the button.
     waitBtnUp();
     mode=MODE_NORMAL;
     printLine2(FH(BLANKLINE));
     updateDisplay();
     Serial.println(F("AN:exit"));
  }
}
#endif // HAVE_ANALYSER

#endif // HAVE_SWR || HAVE_SMETER

