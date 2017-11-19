
/*
 * Computer Assisted Transceiver
 * 
 * Handle checking the serial port for commands from a computer 
 * 
 * Using Serial.print a lot rather than sprintf(buf) / Serial.print(buf) actually saves a lot of progmem.
 */

#include "bitxultra.h"

#if HAVE_CAT

static const char ERR_INTX    [] PROGMEM = "Not valid while TX";
static const char ERR_INVALID [] PROGMEM = "Invalid parameter";
static const char ERR_RANGE   [] PROGMEM = "Out of range";
static const char ERR_DIS     [] PROGMEM = "TX Disabled on freq";

static const char S_0         [] PROGMEM = "0";

static const char CMD_STATUS  [] PROGMEM = "status";
static const char CMD_FREQ    [] PROGMEM = "freq";
static const char CMD_RIT     [] PROGMEM = "rit";
static const char CMD_MOD     [] PROGMEM = "mod";
static const char CMD_VFO     [] PROGMEM = "vfo";
static const char CMD_CHANNEL [] PROGMEM = "channel";
static const char CMD_STORE   [] PROGMEM = "store";
static const char CMD_SIDETONE[] PROGMEM = "sidetone";
static const char CMD_WPM     [] PROGMEM = "wpm";
static const char CMD_CAL     [] PROGMEM = "cal";
static const char CMD_BFOTRIM [] PROGMEM = "bfotrim";
static const char CMD_TX      [] PROGMEM = "tx";
static const char CMD_METER   [] PROGMEM = "meter";
static const char CMD_ANN     [] PROGMEM = "ann";
static const char CMD_CW      [] PROGMEM = "cw";
static const char CMD_CWB     [] PROGMEM = "cwb";
static const char CMD_HELP    [] PROGMEM = "help";

typedef PGM_P (*remoteHandler)(char *p);

struct cmd {
  PGM_P cmdname;
  remoteHandler handler;
};

#define UNUSED(x) if (x) { }  // optimized away but stops unused var warning

#define SERIAL_IN_SIZE 30
static char serial_in[SERIAL_IN_SIZE+1];
static unsigned char serial_in_count = 0;


static PGM_P h_status(char *p) {
  UNUSED(p)
  byte i;
     Serial.print(F("vfos:"));
     Serial.println(state.vfoCount);
     Serial.print(F("vfo:"));
     Serial.println(state.vfoActive+'A');
#if HAVE_CHANNELS
     Serial.print(F("chCount:"));
     Serial.println(state.channelCount);
     Serial.print(F("chActive:"));
     Serial.println(state.channelActive);
#endif
#if HAVE_CW
     Serial.print(F("sTone:"));
     Serial.println(state.sideTone);
#if HAVE_CW==2
     Serial.print(F("WPM:"));
     Serial.println(state.wpm);
#endif
#endif
     Serial.print(F("VFOMode:"));
     Serial.println((state.useVFO ? FH(S_ON) : FH(S_OFF)));
     for (i=0; i<state.vfoCount; i++) {
         struct vfo *vfo = &vfos[i];
         Serial.print(F("VFO"));
         Serial.print((char)(i+'A'));
         Serial.print(FH(S_COLON));
         Serial.print(vfo->frequency);
         Serial.print(FH(S_COMMA));
         Serial.print(vfo->rit);
         Serial.print(FH(S_COMMA));
         Serial.print(vfo->ritOn);
         Serial.print(FH(S_COMMA));
         Serial.println(FH(mod_name(vfo->mod)));
     }
     return NULL;
}

static PGM_P h_freq(char *p) {
     if (*p) {
        Frequency f = atol(p);
        if (inTx!=INTX_NONE) {
           return ERR_INTX;
        } else if (f>=LOWEST_FREQ && f<=HIGHEST_FREQ) {
           vfos[state.vfoActive].frequency=f;
           state.useVFO=true;
           setFrequency(RIT_ON);
           updateDisplay();
        } else {
           return ERR_INVALID;
        }
     } else {
        Serial.print(F("FREQ:"));
        Serial.println(vfos[state.vfoActive].frequency);
     }
     return NULL;
}

static PGM_P h_rit(char *p) {
     if (*p) {
        long rit = atoi(p);
        if ((rit==0) && strcmp(p,"0"))
           return ERR_INVALID;
        if (rit>RIT_MAX || rit<RIT_MIN)
           return ERR_RANGE;
        if (inTx!=INTX_NONE)
           return ERR_INTX;
           
        vfos[state.vfoActive].rit   = rit;
        vfos[state.vfoActive].ritOn = (rit==0 ? false : true);
        setFrequency(RIT_ON);
        updateDisplay();
     } else {
        Serial.print(F("RIT:"));
        Serial.print(vfos[state.vfoActive].rit);
        Serial.print(F(","));
        Serial.println(vfos[state.vfoActive].ritOn ? FH(S_ON) : FH(S_OFF));
     }
     return NULL;
}

static PGM_P h_mod(char *p) {
     if (*p) {
        if (inTx!=INTX_NONE) {
           return ERR_INTX;
        } else if (!strcmp_P(p, S_LSB)) {
           vfos[state.vfoActive].mod=MOD_LSB;
        } else if (!strcmp_P(p, S_USB)) {
           vfos[state.vfoActive].mod=MOD_USB;
        } else if (!strcmp_P(p, S_AUTO)) {
           vfos[state.vfoActive].mod=MOD_USB;
        } else {
           return ERR_INVALID;
        }

        setFrequency(RIT_ON); 
        updateDisplay();
     } else {
        Serial.print(F("MOD:"));
        Serial.println(strcpy_P(c,mod_name(vfos[state.vfoActive].mod)));
     }
     return NULL;
}

static PGM_P h_vfo(char *p) {
     if (*p) {
        char v=toupper(*p);
        if (v<'A' || v>=(state.vfoCount+'A'))
           return ERR_RANGE;
        v-='A';
        state.vfoActive=v;
        setFrequency(RIT_ON);
        updateDisplay();
     } else {
        Serial.print(F("VFO:"));
        Serial.println(state.vfoActive+'A');
     }
     return NULL;
}

#if !CAT_MINIMAL
#if HAVE_CHANNELS
static PGM_P h_channel(char *p) {
     if (*p) {
        int ch=atoi(p);
        if ((ch==0) && strcmp_P(p,S_0))
           return ERR_INVALID;
        if (ch<0 || ch>=state.channelCount)
           return ERR_RANGE;
        if (inTx!=INTX_NONE)
           return ERR_INTX;
        state.channelActive = ch;
        state.useVFO=false;
        get_channel((unsigned char)ch);
        setFrequency(RIT_ON);
        updateDisplay();
     } else {
        Serial.print(F("CHANNEL:"));
        Serial.println(state.channelActive);
     }
     return NULL;
}

static PGM_P h_store(char *p) {
     if (*p) {
        int ch=atoi(p);
        if ((ch==0) && strcmp_P(p,S_0))
           return ERR_INVALID;
        if (ch<0 || ch>=state.channelCount)
           return ERR_RANGE;
        put_channel(ch);
     } else {
        return ERR_INVALID;
     }
     return NULL;
}
#endif // HAVE_CHANNELS

#if HAVE_CW
static PGM_P h_sidetone(char *p) {
     if (*p) {
        int st=atoi(p);
        if (st<SIDETONE_MIN || st>SIDETONE_MAX)
           return ERR_RANGE;
        state.sideTone=st;
     } else {
        Serial.print(F("SIDETONE:"));
        Serial.println(state.sideTone);
     }
     return NULL;
}

#if HAVE_CW == 2
static PGM_P h_wpm(char *p) {
     if (*p) {
        int wpm=atoi(p);
        if (wpm<WPM_MIN || wpm>WPM_MAX) {
           return ERR_RANGE;
        } else {
           state.wpm=wpm;
        }
     } else {
        Serial.print(F("WPM:"));
        Serial.println(state.wpm);
     }
     return NULL;
}
#endif

#if HAVE_CW_SENDER
static PGM_P h_cwsender(char *p) {
     if (*p) {
        send_cw_string(p);
     } else {
        Serial.print(F("CW: send what?"));
     }
     return NULL;
}

#if HAVE_CW_BEACON
static PGM_P h_cwbeacon(char *p) {
     if (*p) {
        if (*p=='|') {
           // copy the beacon string back out into the send buffer. interprets prosigns.
           send_cw_string(EH(CWBEACON_EEPROM_START), CWBEACON_MAXLEN);
        } else {
           // store the beacon string to EEPROM
           put_beacon_text(p);
        }

     } else {
        Serial.print(F("CWB:"));
        // dump the beacon text to Serial
        print_beacon_text();
     }
     return NULL;
}
#endif
#endif
#endif // HAVE_CW

static PGM_P h_cal(char *p) {
  UNUSED(p)
  int32_t cal;
  if (*p) {
     cal = atoi(p);
     if (cal<-300000 || cal>300000)
        return ERR_RANGE;
     put_calibration(cal);
     setFrequency(RIT_ON);
  } else {
     get_calibration(cal);
     Serial.print(F("CAL:"));
     Serial.println(cal);
  }
  return NULL;
}

#if HAVE_BFO
static PGM_P h_bfotrim(char *p) {
  UNUSED(p)
  if (*p) {
     int trim = atoi(p);
     if (trim<BFOTRIM_MIN || trim>BFOTRIM_MAX)
        return ERR_RANGE;
     state.bfo_trim = trim;
     setBFO(bfo_freq);
     setFrequency(RIT_AUTO);
  } else {
     Serial.print(F("BFOTRIM:"));
     Serial.println(state.bfo_trim);
  }
  return NULL;
}
#endif // HAVE_BFO

#endif // !CAT_MINIMAL

static PGM_P h_tx(char *p) {
     if (!strcmp_P(p,PSTR("on"))) {
        if (TXon(INTX_CAT)) {
           return ERR_DIS;
           // next loop() will change back to whatever PTT is doing
        }
     } else {
        TXoff();
     }
     return NULL;
}

#if HAVE_SMETER
static PGM_P h_meter(char *p) {
  UNUSED(p)
     if (inTx!=INTX_NONE) {
        sprintf_P(c,PSTR("SWR:%1.1f"),last_swr/100);
        Serial.println(c);
     } else {
        #define StoNum(s) (s<=9 ? s : (s-9) * 10)
        Serial.print(F("S-METER:"));
        Serial.print(StoNum(avg_s_level));
        Serial.print(F(" p "));
        Serial.println(StoNum(peak_s_level));
     }
     return NULL;
}
#endif

#if HAVE_ANALYSER
static PGM_P h_ann(char *p) {
  UNUSED(p)
  if (*p) mode=MODE_NORMAL; // any parameter exits analyser mode
  else startAnalyser();
  return NULL;
}
#endif

#if !CAT_MINIMAL
static PGM_P h_help(char *p);
#endif


static const struct cmd commandlist[] PROGMEM = {
  { CMD_STATUS,   &h_status },
  { CMD_FREQ,     &h_freq },
  { CMD_RIT,      &h_rit },
  { CMD_MOD,      &h_mod },
  { CMD_VFO,      &h_vfo },

#if !CAT_MINIMAL
#if HAVE_CHANNELS
  { CMD_CHANNEL,  &h_channel },
  { CMD_STORE,    &h_store },
#endif
#if HAVE_CW
  { CMD_SIDETONE, &h_sidetone },
#if HAVE_CW == 2
  { CMD_WPM,      &h_wpm },
#endif
#if HAVE_CW_SENDER
  { CMD_CW,       &h_cwsender },
#endif
#if HAVE_CW_BEACON
  { CMD_CWB,      &h_cwbeacon },
#endif
#endif // HAVE_CW
  { CMD_CAL,      &h_cal },
#if HAVE_BFO
  { CMD_BFOTRIM,  &h_bfotrim },
#endif
#endif // !CAT_MINIMAL

#if HAVE_PTT
  { CMD_TX,       &h_tx },
#endif
#if HAVE_SMETER
  { CMD_METER,    &h_meter },
#endif
#if HAVE_ANALYSER
  { CMD_ANN,      &h_ann },
#endif
#if !CAT_MINIMAL
  { CMD_HELP,     &h_help },
#endif
};


#if !CAT_MINIMAL
static PGM_P h_help(char *p) {
  UNUSED(p)
  byte i;
  Serial.print(F("Valid Commands:"));
  for (i=0; i<(sizeof(commandlist)/sizeof(commandlist[0])); i++) {
      Serial.print(FH(pgm_read_word(&(commandlist[i].cmdname))));
      Serial.print(FH(S_COMMA));
  }
  Serial.println();
  return NULL;
}
#endif // !CAT_MINIMAL

/**
 * Called each time we get a newline on the serial port. 
 * cmd will be at least one char, NULL terminated, but may not be a valid command.
 */

void process_command(char *cmd)
{
  char *p=cmd;
  PGM_P err=NULL;
  byte i;
  
  while (*p && (*p!=' ')) p++; // find the first space
  if (*p) *p++='\0'; // turn the first space into a NULL unless there wasn't one.

  i=0;
  while (i<(sizeof(commandlist)/sizeof(commandlist[0]))) {
        if (!strcmp_P(cmd, pgm_read_word(&(commandlist[i].cmdname)))) {
           remoteHandler handler = (remoteHandler)pgm_read_word(&(commandlist[i].handler));
           err=handler(p);
           if (err!=NULL) {
              Serial.print(F("ERR: "));
              Serial.println(FH(err));
           } else {
              Serial.println(F("OK"));
           }
           return;
        }
        i++;
  }
  Serial.println(F("ERR:Unknown Command"));
}

/**
 *  SerialEvent is called between each run of loop().
 *  We should avoid any slow actions in here (like delays)
 *  We should also minimize delays in loop() so this gets called regularly.
 */

void serialEvent()
{
  while (Serial.available()) {
     char ch = Serial.read();
     switch (ch) {
         case '\r':
         case '\n': if (serial_in_count>0) {
                       serial_in[serial_in_count]='\0';
#if 0
                       Serial.print(F(">"));
                       Serial.println(serial_in);
#endif
                       process_command(serial_in);
                       serial_in_count=0;
                    }
                    break;
         default:   if (serial_in_count < (SERIAL_IN_SIZE-1)) serial_in[serial_in_count++]=ch;
     }
  }
}

#endif // HAVE_CAT

