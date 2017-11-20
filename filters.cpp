
#include "bitxultra.h"

// Which filter to use for which frequencies... FILTER_DEFAULT is selected if no band matches.
// We only allow TX in defined bands where tx=true - but there is nothing stopping you putting a band at
// the end for 0-200Mhz which uses no filtering at all (except you ARE a responsible HAM aren't you!)
// RX Tuning is limited to between LOWEST_FREQ and HIGHEST_FREQ defined in config.h.
// Depending on how your filter relays are wired, you have the ability to pair different 
// BPFs with one LPF - so you need less filters in total.
// You _could_ have a no-filter BPF option (a wire link) for all-frequency RX.
// Note - the FIRST matching band is used... so order is important!

// handy macro for use with latched relays. Connect the set coils to outputs 0-15, reset coils to 16-31.
// or alter the macro to suit your layout.
#define LATCHED(x) (((~x) << 16) | x)


static struct band band;

#if !HAVE_FILTERS

// minimum layout (one band, stock filters only)
// this is really just defined so we know where we can TX.
const struct band txbands[] PROGMEM = {
  {lo:  7000000l, hi:  7300000l, tx:true, filter:0}, // 40m
};

const struct band * findBand(Frequency f) {
  memcpy_P(&band, &txbands[0], sizeof(band));
  if (f>=band.lo && f<=band.hi) return &band;
  return NULL;
}

Frequency findNextBandFreq(Frequency f) {
  if (f<band.lo) return band.hi;
  if (f>band.hi) return band.lo;
  return f;
}

#else

#if 0
// Set the above line to #if 1 and put your custom band and filter definitions here
// Each band costs 12 bytes of progmem
#define FILTER_DEFAULT 0x0000
const struct band txbands[] PROGMEM = {
  //{lo:   135700l, hi:   137800l, tx:true, filter:-},
  //{lo:   472000l, hi:   479000l, tx:true, filter:-},
//  {lo:  1800000l, hi:  1875000l, tx:true, filter:-}, // 160m
//  {lo:  3500000l, hi:  3700000l, tx:true, filter:-}, // 80m
//  {lo:  3776000l, hi:  3800000l, tx:true, filter:-}, // 80m DX
  //{lo:  5351500l, hi:  5366500l, tx:false,filter:-}, // 60m (use 40m LPF)
  {lo:  7000000l, hi:  7300000l, tx:true, filter:0}, // 40m
  //{lo: 10100000l, hi: 10150000l, tx:true, filter:-}, // 30m (use 20m LPF)
//  {lo: 14000000l, hi: 14350000l, tx:true, filter:-}, // 20m
  //{lo: 18068000l, hi: 18168000l, tx:true, filter:-}, // 17m (use 10m LPF)
  //{lo: 21000000l, hi: 21450000l, tx:true, filter:-}, // 15m (use 10m LPF)
  //{lo: 24890000l, hi: 24990000l, tx:true, filter:-}, // 12m (use 10m LPF)
  //{lo: 28000000l, hi: 29700000l, tx:true, filter:-}, // 10m
  //{lo: 50000000l, hi: 54000000l, tx:true, filter:-}, //  6m
  //{lo:144000000l, hi:148000000l, tx:true, filter:-}, //  2m
};

#else

// All HF bands (for Australia) using QRPLabs filter boards connected via a I2C IO chip. 
// A,B,C selects the filter board, 1-5 selects a filter on that board. 
// Each filter is a plug-in module so it's easy to swap them around if you want.
#define BPFA(f)     (0x0000 | ((f)<<8))
#define BPFB(f)     (0x8000 | ((f)<<8))
#define BPFC(f)     (0x4000 | ((f)<<8))

#define LPFA(f)     (0x0000 | (f)) 
#define LPFB(f)     (0x0080 | (f))

#define F1     0x01
#define F2     0x02
#define F3     0x04
#define F4     0x08
#define F5     0x10

//SPARES   0x2060 (3 left)

#define FILTER_DEFAULT 0x0000                             // bypass BPFs & LPFs - RX only
const struct band txbands[] PROGMEM = {
//  {lo:   135700l, hi:   137800l, tx:false, filter:FILTER_DEFAULT},
//  {lo:   472000l, hi:   479000l, tx:false, filter:FILTER_DEFAULT},
  {lo:  1800000l, hi:  1875000l, tx:true,  filter:BPFA(F1) | LPFA(F1) }, // 160m
  {lo:  3500000l, hi:  3700000l, tx:true,  filter:BPFA(F2) | LPFA(F2) }, // 80m
  {lo:  3776000l, hi:  3800000l, tx:true,  filter:BPFA(F2) | LPFA(F2) }, // 80m DX
  {lo:  5351500l, hi:  5366500l, tx:false, filter:BPFA(F3) | LPFA(F3) }, // 60m (uses 40m LPF)
  {lo:  7000000l, hi:  7300000l, tx:true,  filter:BPFA(F4) | LPFA(F3) }, // 40m
  {lo: 10100000l, hi: 10150000l, tx:true,  filter:BPFA(F5) | LPFA(F4) }, // 30m (uses 20m LPF)
  {lo: 14000000l, hi: 14350000l, tx:true,  filter:BPFB(F1) | LPFA(F4) }, // 20m
  {lo: 18068000l, hi: 18168000l, tx:true,  filter:BPFB(F2) | LPFA(F5) }, // 17m
  {lo: 21000000l, hi: 21450000l, tx:true,  filter:BPFB(F3) | LPFA(F5) }, // 15m
  {lo: 24890000l, hi: 24990000l, tx:true,  filter:BPFB(F4) | LPFA(F5) }, // 12m
  {lo: 28000000l, hi: 29700000l, tx:true,  filter:BPFB(F5) | LPFA(F5) }, // 10m
//  {lo: 50000000l, hi: 54000000l, tx:false, filter:FILTER_DEFAULT}, //  6m
//  {lo:144000000l, hi:148000000l, tx:false, filter:FILTER_DEFAULT}, //  2m
};
#endif // 0/1

/**
 * Raduino needs to keep track of current state of the transceiver. These are a few variables that do it
 */

FilterId  txFilter;
bool      txFilterInit=false;

/**   
 * Select a filter appropriate for the frequency.
 * 
 * if no filters are enabled, that *should* be the onboard filter.
 * It should be possible to get an extra filter without using any extra outputs
 * by driving a third relay only when both outputs are high (AND gate, or 
 * transistor with collector on FILTER_PIN0, base on FILTER_PIN1.
 * ** with RL1 and RL3 on, RL1 won't matter...
 * ---RL2----RL1--- 40m LOW,LOW (onboard)
 *      \      \___ 20m LOW,HIGH
 *       \
 *        \__RL3--- 80m HIGH,LOW
 *             \___ ??m HIGH,HIGH
 */


/*
 * Another way to do this is with the LPF relay board from QRP Labs and an output expansion.
 * PCF8574 or PCF8575 based I2C I/O expanders are cheap on E-Bay. Pair that with one or more
 * filter relay boards from QRP Labs and you can plug in any of their BPF and/or LPF kits.
 * I used 2 relay boards (one for LPFs and one for BPFs) and a single PCF8575.
 * You can install the filters in ANY order, and ANY combination - then configure txbands above 
 * to set the filter outputs appropriately to enable any number of relays required.
 */
#if FILTER_I2C
static const byte pcf857x_sizes[PCF857X_COUNT] PROGMEM = PCF857X_SIZES;
static const byte pcf857x_addrs[PCF857X_COUNT] PROGMEM = PCF857X_ADDRS;

void setFilters_PCF857X(FilterId filt) { // any combo of PCF857X-like chips at any addresses
  byte i=0;
  char s;
  filt = ~filt; // invert all outputs.
  while ((i<PCF857X_COUNT) && (s=pgm_read_byte(&pcf857x_sizes[i]))) {
    Wire.beginTransmission(pgm_read_byte(&pcf857x_addrs[i]));
    while (s>0) {
      Wire.write(filt & 0xFF);
      filt>>=8;
      s-=8;
    }
    Wire.endTransmission();
    i++;
  }
}

FilterId getFilters_PCF857X() {
  byte i=0;
  char s;
  FilterId filt=0, temp;
  
  while ((i<PCF857X_COUNT) && (s=pgm_read_byte(&pcf857x_sizes[i]))) {
    Wire.requestFrom((uint8_t)pgm_read_byte(&pcf857x_addrs[i]), (uint8_t)((s+7)/8));
    temp=0;
    while (Wire.available()) {
      temp>>=8;
      temp|=(FilterId)Wire.read() << ((sizeof(temp)-1)*8);
    }
    temp >>= (sizeof(temp)*8) - s;
    filt <<= s;
    filt |= temp;
  }

  filt = ~filt;
  return filt;
}
#endif // FILTER_I2C

#ifdef FILTER_PIN0
void setFilters_IO(byte filt) { // Direct control via IO pins. Max 3 outputs only!
  digitalWrite(FILTER_PIN0, filt & 0x01);
  #ifdef FILTER_PIN1
  digitalWrite(FILTER_PIN1, (filt >> 1) & 0x01);  
  #endif
  #ifdef FILTER_PIN2
  digitalWrite(FILTER_PIN2, (filt >> 2) & 0x01);  
  #endif
}
#endif


void setFilters(const struct band *band) {
  unsigned int filt = band ? band->filter : FILTER_DEFAULT;
  if (!txFilterInit || (filt != txFilter)) {
     if (inTx!=INTX_NONE && inTx!=INTX_DIS) {
        digitalWrite(TX_RX, 0); // Turn off RF output while switching.
        delay(20);
     }

     FILTER_CONTROL(filt);
     txFilter=filt;
     txFilterInit=true;
     
     #if FILTER_I2C
       #ifdef FILTER_PIN0
         #ifdef FILTER_RELAYS_ON
           digitalWrite(FILTER_PIN0, FILTER_RELAYS_ON); // power relays
           delay(20);
           #ifdef FILTER_RELAYS_OFF
             digitalWrite(FILTER_PIN0, FILTER_RELAYS_OFF); // cut relay power
           #endif
         #endif
       #endif
       #ifdef FILTER_RELAYS_OFF
         FILTER_CONTROL(FILTER_RELAYS_OFF==HIGH ? 0xFFFFFFFF : 0x00000000);
       #endif
     #else // Direct IO (non-I2C) control only
       #ifdef FILTER_RELAYS_OFF
         delay(30);
         setFilters_IO(FILTER_RELAYS_OFF==HIGH ? 0xFF : 0x00);
       #endif
     #endif

     if (inTx!=INTX_NONE && inTx!=INTX_DIS) {
        delay(20);
        digitalWrite(TX_RX, 1);
     }
  }
}

/*
 * Return the band data for a given frequency if found.
 */

static Frequency last_f=0;
static unsigned char bandidx=0;

//#define DEBUG_FIND_BAND

const struct band * findBand(Frequency f) {
  if (last_f>0) {
     #ifndef DEBUG_FIND_BAND
     if (f >= band.lo && f <= band.hi) return &band;
     if (f==last_f) return NULL; // if we didn't find a band last time, don't bother looking this time.
     #endif
  }

  unsigned char i;
  unsigned char cnt=sizeof(txbands)/sizeof(struct band);
  for (i=0; i<cnt; i++) {
      if ( f >= pgm_read_dword(&(txbands[i].lo)) && f <= pgm_read_dword(&(txbands[i].hi)) ) {
         memcpy_P(&band, &txbands[i], sizeof(band));
         #ifdef DEBUG_FIND_BAND
           sprintf(c, "Band: %2d/%2d     ", i, cnt);
           Serial.println(c);
         #endif
         last_f=f;
         bandidx=i;
         return &band;
      }
  }
  return NULL;
}

Frequency findNextBandFreq(Frequency f) {

#if TUNE_BANDS_ONLY == 0

  return f;
  
#elif TUNE_BANDS_ONLY == 1

  // find the next frequency in a defined band, based on the current band and which end we fell off
  byte idx = bandidx;
  if (f<band.lo) {
     if (idx > 0) {
        idx--;
     } else {
        idx = (sizeof(txbands)/sizeof(struct band)) -1;
     }
     return pgm_read_dword(&(txbands[idx].hi));
     
  } else if (f>band.hi) {
     if (idx < (sizeof(txbands)/sizeof(struct band) -1) ) {
        idx++;
     } else {
        idx=0;
     }
     return pgm_read_dword(&(txbands[idx].lo)); 
  }

#elif TUNE_BANDS_ONLY == 2

  // wrap around at the end of the band. If you want to change bands, use rapid tune.
  if (f<band.lo) f=band.hi;
  else if (f>band.hi) f=band.lo;

#else
#error Invalid value for TUNE_BANDS_ONLY in config.h
#endif

  return f;
}

#endif // HAVE_FILTERS

