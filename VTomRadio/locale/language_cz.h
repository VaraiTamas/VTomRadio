#ifndef dsp_full_loc
#define dsp_full_loc
#include <pgmspace.h>
#include "../myoptions.h"
// clang-format off

const char mon[] PROGMEM = "po";
const char tue[] PROGMEM = "út";
const char wed[] PROGMEM = "st";
const char thu[] PROGMEM = "čt";
const char fri[] PROGMEM = "pá";
const char sat[] PROGMEM = "so";
const char sun[] PROGMEM = "ne";

const char monf[] PROGMEM = "pondělí";
const char tuef[] PROGMEM = "úterý";
const char wedf[] PROGMEM = "středa";
const char thuf[] PROGMEM = "čtvrtek";
const char frif[] PROGMEM = "pátek";
const char satf[] PROGMEM = "sobota";
const char sunf[] PROGMEM = "neděle";

const char jan[] PROGMEM = "leden";
const char feb[] PROGMEM = "únor";
const char mar[] PROGMEM = "březen";
const char apr[] PROGMEM = "duben";
const char may[] PROGMEM = "květen";
const char jun[] PROGMEM = "červen";
const char jul[] PROGMEM = "červenec";
const char aug[] PROGMEM = "srpen";
const char sep[] PROGMEM = "září";
const char octt[] PROGMEM = "říjen";
const char nov[] PROGMEM = "listopad";
const char decc[] PROGMEM = "prosinec";

const char wn_N[]      PROGMEM = "S";
const char wn_NNE[]    PROGMEM = "SSV";
const char wn_NE[]     PROGMEM = "SV";
const char wn_ENE[]    PROGMEM = "VSV";
const char wn_E[]      PROGMEM = "V";
const char wn_ESE[]    PROGMEM = "VJV";
const char wn_SE[]     PROGMEM = "JV";
const char wn_SSE[]    PROGMEM = "JJV";
const char wn_S[]      PROGMEM = "J";
const char wn_SSW[]    PROGMEM = "JJZ";
const char wn_SW[]     PROGMEM = "JZ";
const char wn_WSW[]    PROGMEM = "ZJZ";
const char wn_W[]      PROGMEM = "Z";
const char wn_WNW[]    PROGMEM = "ZSZ";
const char wn_NW[]     PROGMEM = "SZ";
const char wn_NNW[]    PROGMEM = "SSZ";

const char* const dow[]     PROGMEM = { sun, mon, tue, wed, thu, fri, sat };
const char* const dowf[]    PROGMEM = { sunf, monf, tuef, wedf, thuf, frif, satf };
const char* const mnths[]   PROGMEM = { jan, feb, mar, apr, may, jun, jul, aug, sep, octt, nov, decc };
const char* const wind[]    PROGMEM = { wn_N, wn_NNE, wn_NE, wn_ENE, wn_E, wn_ESE, wn_SE, wn_SSE, wn_S, wn_SSW, wn_SW, wn_WSW, wn_W, wn_WNW, wn_NW, wn_NNW, wn_N };

const char    const_PlReady[]    PROGMEM = "[připraveno]";
const char  const_PlStopped[]    PROGMEM = "[zastaveno]";
const char  const_PlConnect[]    PROGMEM = "--> připojování";
const char  const_DlgVolume[]    PROGMEM = "hlasitost";
const char    const_DlgLost[]    PROGMEM = "* ztráta signálu *";
const char  const_DlgUpdate[]    PROGMEM = "* obnova *";
const char const_getWeather[]    PROGMEM = "";
const char  const_waitForSD[]    PROGMEM = "INDEX SD";

const char        apNameTxt[]    PROGMEM = "Název AP";
const char        apPassTxt[]    PROGMEM = "HESLO";
const char       bootstrFmt[]    PROGMEM = "Připojování %s";
const char        apSettFmt[]    PROGMEM = "Stránka s nastavením: HTTP://%s/";
// clang-format on
#ifdef WEATHER_FMT_SHORT
const char weatherFmt[] PROGMEM = "%.1f°C  \007  %d hPa  \007  %d%% RH";
#else
  #if EXT_WEATHER
    #ifdef WIND_SPEED_IN_KMH
      #define WIND_UNIT "km/h"
    #else
      #define WIND_UNIT "m/s"
    #endif
const char weatherFmt[] PROGMEM = "%s, Teplota: %.1f°C \007 Pocitová teplota: %.1f°C \007 Tlak: %d hPa \007 Vlhkost: %d%% \007 Vítr: %.1f " WIND_UNIT " [%s]";
  #else
const char weatherFmt[] PROGMEM = "%s, %.1f°C \007 Tlak: %d hPa \007 Vlhkost: %d%%";
  #endif
#endif
// clang-format off
const char weatherUnits[] PROGMEM = "metric"; /* standard, metric, imperial */
const char weatherLang[]  PROGMEM = "cz";      /* https://openweathermap.org/current#multi */

// ---- Presets screen ----
const char prstAssigned[]     PROGMEM = "Přiřazené";
const char prstDeleted[]      PROGMEM = "Preset smazán";
const char prstNoUrl[]        PROGMEM = "Bez URL";
const char prstEmptyPreset[]  PROGMEM = "Prázdný preset";
const char prstPlay[]         PROGMEM = "Přehrát";
const char prstSave[]         PROGMEM = "Uložit";
const char prstDel[]          PROGMEM = "Smazat";
const char prstSpace[]        PROGMEM = "Mezera";
const char prstCancel[]       PROGMEM = "Zrušit";
const char prstOk[]           PROGMEM = "OK";

#endif
