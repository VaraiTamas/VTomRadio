// clang-format off
/* https://trip5.github.io/ehRadio_myoptions/generator.html
   https://github.com/VaraiTamas/yoRadio.git
   Használat előtt olvasd el!!! - Read the before use !!!
   https://github.com/VaraiTamas/yoRadio/blob/main/README.md !!!

  - A sor elején a // jel inaktívvá teszi a parancsot ezért a fordító nem veszi figyelembe! 
    Ezzel tudod beállítani a te hardveredmek megfelelő összeállítást.
  - The // sign at the beginning of the line makes the command inactive, so the compiler ignores it! 
    This allows you to set the appropriate configuration for your hardware.
*/

#pragma once

#ifndef ARDUINO_ESP32S3_DEV
    #define ARDUINO_ESP32S3_DEV
#endif

// #define HEAP_DBG

/* Itt tudod beállítani a program nyelvét
   You can set the program language here.
   Supported languages: HU NL PL RU EN GR SK DE UA ES. */
#define LANGUAGE HU

/* -- Névnapok megjelenítése -- Display name days --
Supported languages: HU, PL, NL, GR, DE (UA Local/namedays/namedays_UA.h is not filled in.) */
#define NAMEDAYS_FILE HU

#define USE_BUILTIN_LED false /* The RGB LED does not turn on.. */

/* Arduino OTA Support */
 #define USE_OTA true                    /* Enable OTA updates from Arduino IDE */
// #define OTA_PASS "myotapassword12345"   /* OTA password for secure updates */

/* HTTP Authentication */
// #define HTTP_USER ""               /* HTTP basic authentication username */
// #define HTTP_PASS ""               /* HTTP basic authentication password */

/*----- LCD DISPLAY -----*/
#define DSP_MODEL DSP_ILI9488
//#define DSP_MODEL DSP_ST7796

/*----- DISPLAY PIN SETS -----*/
#define TFT_DC         9
#define TFT_CS         10
#define TFT_RST        -1
#define BRIGHTNESS_PIN 14
/*
   GPIO 11 - MOSI
   GPIO 12 - CLK
   GPIO 13 - MISO  // Ne csatlakoztasd a kijelzőhöz!!! - Do not connect to the LCD display!!!
*/

/*----- Touch ISP -----*/
#define TS_MODEL TS_MODEL_XPT2046
#define TS_CS    3

/*----- Touch I2C -----*/
// #define TS_MODEL TS_MODEL_FT6X36
// #define TS_MODEL TS_MODEL_AXS15231B
// #define TS_SCL     7
// #define TS_SDA     8
// #define TS_INT    17 
// #define TS_RST     1

/*----- NEXTION DISPLAY serial port -----*/
// #define NEXTION_RX			15
// #define NEXTION_TX			16

/*----- PCM5102A  DAC -----*/
#define I2S_DOUT 4
#define I2S_BCLK 5
#define I2S_LRC  6
// #define I2S_MCLK 0  /* CS4344 DAC: MCLK pin (PCM5102A-nál nem szükséges / not needed for PCM5102A) */
//Szabad és MCLK-ra alkalmas jelöltek: GPIO 0, 7, 8, 15, 16, 17, 18, 45, 46

/*----- ENCODER 1 ------*/
#define ENC_BTNR 47 // S2 41
#define ENC_BTNL 42 // S1 40 
#define ENC_BTNB 21 // KEY 39
 #define ENC_INTERNALPULLUP	true

/*----- ENCODER 2 -----*/
#define ENC2_BTNR 41 // S2 47
#define ENC2_BTNL 40 // S1 48-RGB LED!
#define ENC2_BTNB 39 // KEY 21
#define ENC2_INTERNALPULLUP	true

/*----- CLOCK MODUL RTC DS3132 -----*/
 #define RTC_SCL			     7
 #define RTC_SDA			     8
 #define RTC_MODULE DS3231

/*----- REMOTE CONTROL INFRARED RECEIVER -----*/
/*----- Alvásból ébresztéshez a GPIO 2 -őt kell használni, mert a GPIO 38 nem RTC pin. A PCB-n át kell kötni! -----*/
/*----- To wake from sleep, you must use GPIO 2, because GPIO 38 is not an RTC pin. It must be connected via the PCB! -----*/
#define IR_PIN 2  //38

/*----- SD CARD -----*/
// #define SDC_CS     18
// #define SDSPISPEED 4000000 /* 4MHz - Slower speed to prevent display flicker on shared SPI bus */

/*----- Ezzel a beállítással nincs görgetés az időjárás sávon. -----*/
/*----- With this setting there is no scrolling on the weather bar. -----*/
#define WEATHER_FMT_SHORT

/*----- Ezzel a beállítással a teljes időjárás jelentés megjelenik. -----*/
/*----- With this setting, the full weather report is displayed. -----*/
// #define EXT_WEATHER  true

/*----- Ezzel a beállítással a szél sebessége km/h lesz. -----*/
/*----- With this setting, the wind speed will be km/h. -----*/
// #define WIND_SPEED_IN_KMH

/*----- Az itt beállított pin vezérelheti egy audio erősítő tápellátását. Zenelejátszás közben a pin HIGH (magas) állapotban van ami meghúzza az
erősítő tápellátását kapcsoló relét. Amikor nincs zenelejátszás (STOP vagy a hangerő 0), a pin LOW (alacsony) állapotban van.
Ez a változás akkor történik, amikor a képernyővédő "while not playing" üzemmódban bekapcsol.
This pin controls the amplifier's power supply. When music is playing, the pin is set to HIGH to control the relay.
When music is not playing (stopped or volume is 0), the pin is set to LOW. This change occurs when the screensaver is running. -----*/
// #define PWR_AMP 2

/*----- Ha ez definiálva van a rádió indításakor, mindig az első csatorna lesz beállítva. -----*/
/*----- If this is defined at radio startup, the first channel will always be set. -----*/
//#define ALWAYS_START_FROM_FIRST

/*----- Sleep functions -----*/
/*----- A WAKE_PIN helyett mostantól két pin állítható be az ébresztéshez: WAKE_PIN1 és WAKE_PIN2 -----*/
/*----- Így távirányítóval és egy másik gombbal is felébreszthető az eszköz. -----*/
/*----- Instead of WAKE_PIN, you can now set two pins for wake-up: WAKE_PIN1 and WAKE_PIN2 -----*/
/*----- This way, you can wake up the device with a remote control and another button. -----*/
 //#define BTN_MODE ENC_BTNB
  #define WAKE_PIN1 IR_PIN
// #define WAKE_PIN2 ENC2_BTNB

/*----- by Zsolt Simon -----*/
/*----- Tested on Synology NAS ----- */
// #define USE_DLNA
// #define dlnaHost "192.168.1.200"
// #define dlnaIDX  21

//#define POWER_LED 38      // Button LED pin (will be turned on when player is on)
