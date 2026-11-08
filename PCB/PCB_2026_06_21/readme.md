# PCB verzió 2026.06.21 
## verzió: 2026.06.15 -től
- Az SMD ellenállások mérete 1206 egységesen.
- Az előző verzióhoz képest a DS3231 óramodul csatlakozójának 3,3V-os táp kimenetén 5 Volt táp van amit a POWER_LED és a DAC áramkör táplál. Így alvó üzemmódban ez az eszköz is lekapcsol. Lejjebb olvashatsz róla. A 3,3V elírás! A DS3231 erre fel van készítve.   
## Verzió: 2026.06.21 -től
- A PCB feliratok a 2026.06.15 verzióhoz képest javítva lettek, a feliratok a valóságot tükrözik.
## -- Ismeretető --

- **PCB mérete:** 98 × 100 mm
- Kapcsolási rajz PDF formátumban letölthető: [pcb_wiring_diagram_2026_06_21.pdf](../../PCB/PCB_2026_06_21/pcb_wiring_diagram_2026_06_21.pdf)  
- **R1-R2** az I2C busz SCL és SCA felhúzóellenállásai. 4.7kΩ (Capacitive touch és RTC modul DS3231 használja.)   
- **R3** az I2C TOUCH -> INT GPIO 17 felhúzóellenállása. 4.7kΩ
- **R4** az I2C TOUCH -> RST GPIO 1 felhúzóellenállása. 4.7kΩ   
- "LED OUT" csatlakozó 3.3V-os kimenet a bekapcsoló gomb LED világításának meghajtásához, mely egy BC817 tranzisztorral van kapcsolva a GPIO 38 állapotának megfelelően, a LED GND lábát kapcsolja. Használatával a bekapcsológomb világítása is kikapcsol az ESP alvó állapotában.    
Figyelem!!! Fontos beállítani a myoptions.h fájlban, ellenkező esetben a DAC és a POWER LED nem kapcsol be!!!
```cpp
#define POWER_LED 38
```
- **R5** A POWER LED -et kapcsoló BC817 tranzisztor bázis előtét ellenállása 1kΩ    
- **R6** A POWER LED előtét ellenállása a BC817 taranzisztor által kapcsolt GND ágban 1kΩ   
- **R13** Az ESP32 GPIO 0 -hoz csatlakozó felhúzóellenállás 10kΩ. A GPIO 0 -t a bootloader a boot mód kiválasztására használja. A modulon levő saját felhúzóellenállás nem mindig elegendő, ezért a PCB-n is van egy 10kΩ-os felhúzóellenállás. Elhagyása esetén tapasztalható, hogy a modul csak a második indításra kapcsol be.   

## Az alábbi ellenállásokat nem kell beforrasztani, csak fejlesztéshez vannak kialakítva!!!
- **R14** Az SPI TOUCH -> CS GPIO 3 felhúzóellenállása. 4.7kΩ  
- **R15** Az SPI SD_CARD -> CS GPIO 18 felhúzóellenállása. 4.7kΩ 

## A PCM5102A DAC VIN +5V bemenetét kapcsoló A03401A MOSFET áramkör ellenállásai és működése:
- A PCM5102A DAC VIN +5V bemenetét egy A03401A MOSFET kapcsolja a GPIO 38 állapotának megfelelően. Így a DAC sem kap áramot az ESP alatatásakor.
- **R16** (330 Ω)   
Az optocsatoló kimenete és a MOSFET gate között van. Korlátozza a gate töltő- és kisütőáramát,
csökkenti a kapcsolási tranziens tüskéket és a rezgéseket (ringing),
védi az optocsatoló tranzisztorát a túl nagy impulzusáramoktól.
Nélküle a gate kapacitása rövid ideig nagy áramot vehetne fel.
- **R17** (47 kΩ)   
A gate-et húzza fel a +5 V-ra. Biztosítja, hogy az optocsatoló kikapcsolt állapotában a gate a source feszültségére kerüljön,
így a P-MOSFET biztosan kikapcsolt állapotban maradjon, megakadályozza a gate lebegését.    
- **R18** (680 Ω)   
Korlátozza az optocsatoló LED áramát.
- MOSFET kapcsolás működése:    
Optocsatoló LED kikapcsolva
R17 felhúzza a gate-et +5 V-ra.
Gate ≈ Source.  
MOSFET kikapcsol.   
Optocsatoló LED bekapcsolva.    
Az optocsatoló tranzisztora lehúzza a gate-et a GND felé.
A gate feszültsége kb. 0 V lesz.
A source +5 V-on van.   
A P-csatornás MOSFET bekapcsol. 

Figyelem!!! Fontos beállítani a myoptions.h fájlban, ellenkező esetben a DAC és a POWER LED nem kapcsol be!!!
```cpp
#define POWER_LED 38
```

## Forgó jeleadó - rotary encoder EC11 modul bekötése:
<img src="../../images/rotary_encoder_modul.png" alt="rotary_encoder_modul.png" height="400"><br><br>   

- **R7, R8, R10, R11** a rotary enkóder felhúzóellenállásai 10kΩ. Nem kell beforrasztani, ha a rotary modulon már van beépítve! 
- **R9, R12** a rotary enkóder gomb felhúzóellenállásai 10kΩ. Nem kell beforrasztani, ha a rotary modulon már van beépítve! 

Ha nincs beépítve a modulon **R7 - R12**, tapasztalatom szerint elegendő bekapcsolni a belső felhúzóellenállásokat a myoptions.h fájlban.
```cpp
/*----- Encoder 1 esetén -----*/
#define ENC_INTERNALPULLUP	true

/*----- Encoder 2 esetén -----*/
#define ENC2_INTERNALPULLUP	true
```

Az encoderek használata esetén a `myoptions.h` fájlban definiálni kell őket:    
```cpp
/*----- ENCODER 1 ------*/
#define ENC_BTNL 42 // S1
#define ENC_BTNR 47 // S2
#define ENC_BTNB 21 // KEY
// #define ENC_INTERNALPULLUP	true
// #define ENC_HALFQUARD true

/*----- ENCODER 2 -----*/
#define ENC2_BTNL 40 // S1
#define ENC2_BTNR 41 // S2
#define ENC2_BTNB 39 // KEY
// #define ENC2_INTERNALPULLUP	true
// #define ENC2_HALFQUARD       true
```
- Ha nem EC11 rotary enkódereket használsz, hanem a nagyobb (EC12) rotary jeladókat ,akkor a `myoptions.h` fájlban engedélyezni kell a fél lépéses működést is:
```cpp
 /*----- Encoder 1 esetén -----*/
#define ENC_HALFQUARD	true

/*----- Encoder 2 esetén -----*/
#define ENC2_HALFQUARD	true
```

## Érintő képernyő 
### SPI buszt használó XPT2046 chip esetén (resistive) 
- Csatlakozó foglalat -> SPI TOUCH
- A T.CK, T.CS, T-MISO, T.MOSI érintkezők az LCD modulon az érintőképernyő SPI vezetékei. Használat esetén mindegyiket be kell kötni. Amennyiben nem akarsz használni érintő kijelzőt és nem kötöd be ezeket, úgy a myoptions.h fájlban kommenteld ki a sor elejére helyezett // jellel az ide vonatkozó definíciókat. Ellenkező esetben mindig a hangerő képernyő jelenhet meg.  

```cpp
/*----- Touch ISP -----*/
 #define TS_MODEL TS_MODEL_XPT2046
 #define TS_CS    3
```

### I2C buszt használó FT6X36 chip esetén (capacitive) 
- Csatlakozó foglalat -> I2C TOUCH
- A myoptions.h fájlban kell engedélyezni ezt a funkciót. 

```cpp
/*----- Touch I2C -----*/
 #define TS_MODEL TS_MODEL_FT6X36
// #define TS_MODEL TS_MODEL_AXS15231B
 #define TS_SCL     7
 #define TS_SDA     8
 #define TS_INT    17 
 #define TS_RST     1
``` 
- További beállítások a WEB UI felületen érhetőek el a program futása közben.
    - flip touch
    - X toutch mirroring
    - Y touch mirroring     

## SD kártya
- Csatlakozó foglalat -> SPI SD CARD
- Használata esetén a myoptions.h fájlban be kell kapcsolni a definíciókat.
```cpp
/* DS CARD */
#define SDC_CS 18
```
## RTC óramodul
- Az I2C csatlakozó alkalmas közvetlenül óramodul RTC DS3231 fogadására. Használata esetén a myoptions.h fájlban be kell kapcsolni a definíciókat. Az óramodul elhagyása esetén a kijelző órája induláskor 00:00:00 időt mutat, amig a netről nem tudja lekérdezni a pontos időt.
```cpp
/* CLOCK MODUL RTC DS3231 */
#define RTC_SCL			     7
#define RTC_SDA			     8
#define RTC_MODULE DS3231
```
## Power-select jumperek       
A PWS_LCD power select jumperek csak tesztelési céllal kerültek fel. Amennyiben a modul tartalmaz saját 3.3V -os stabilizátor IC -t, úgy lehet választani az 5V -os táplálást.

- 5V_ESP32 zárása → az ESP a saját 3.3 V stabilizátorát használja

- 3.3V_ESP32 zárása → az ESP-t az alaplapi stabilizátor táplálja        
Ajánlás: csak az 5V_ESP32 ágat zárd! 

- Kijelzőknél a 3.3V használata javasolt. 4"-os ILI9488 kijelzőnél az 5V -os záróagat érdemes lehet összekötni 2 darab 1N4007 dióda sorbakötésével, így a kijelző 3,8 Voltot kap és erősebb képe lesz tapasztalatom szerint. Az 5 Voltos táplálás instabil működést okozhat.

## Távirányító bekötése:    

- Az IR LED a GPIO 2 -öt használja, mert a távirányítóval való ébresztéshez RTC képes GPIO -ra van szükség.
### Infravörös vevő power szűrő   
<img src="../../images/IR_power_filter.png" alt="IR_power_filter" height="300"><br><br>  
Ha vezetékkel van kivezetve az IR led a tápvezeték összeszed zajokat a környező vezetékek mágneses tere miatt, ezért stabilizálni kell a +5 Voltot a GND hez képest. Ez bevált gyakorlat a háztartási gépeknél is mint például TV vagy klíma.   
A zajt tanításnál lehet észrevenni, mert a távirányító megnyomása nélkül jelennek meg hexadecimális számok, vagy nem tudod ugyanazt a számot generálni egy gomb nyomkodásával.  
Az infravörös led tápjára párhuzamosan kell kötni egy 10μF elektrolit és egy 100nF kerámia kondenzátort, közel az infravevőhöz. 

## Az ESP32-S3-DevKitC 1 N16R8 fejlesztőmodul lábkiosztása.      
<img src="../../images//esp32-S3-DevKitC-1-original-pinout-low.jpg" alt="esp32-S3-DevKitC-1-original-pinout-low" height="400"><br><br>   

- Az SPI lábak ellenőrzéséhez itt találod a board-variánsokat:  
  https://github.com/espressif/arduino-esp32/tree/master/variants

- Fordítási konfigurációk:  
  https://github.com/sivar2311/ESP32-S3-PlatformIO-Flash-and-PSRAM-configurations
   

## PCB képek:

- A rögzítő furatok és a kűlső méretek nem változtak.   
<br><br>


![PCB front](pcb_top.png)<br><br>
![PCB back](pcb_bottom.png)<br><br>

## Alkatrészek:
| Alkatrész neve    | Érték | Típus    |
|-------------------|-------|----------|     
| R1 - R4           | 4,7k  | SMD 1206 |    
| R5 - R6           | 1k    | SMD 1206 |
| R7 - R13          | 10k   | SMD 1206 |
| R14 - R15         | 4,7k  | SMD 1206 |
| R16               | 330   | SMD 1206 |
| R17               | 47k   | SMD 1206 |
| R18               | 680   | SMD 1206 |
| C1                | 220uF/10V | 2,54 |
| C2                | Kondenzátor 100nF 100V 10% Polipropilén RM-5 | 2,54 |
| C3                | 10uF/10V | 2,54 |
| C4                | Kondenzátor 100nF 100V 10% Polipropilén RM-5 | 2,54 |
| LD1117AS33TR POS.V-REG. | 3.3V 1.2A Low-Drop.(1.15V) | SOT-223       | 
| AO3401A | P-MOSFET SMD 30V (3.8A) | SOT-23 |
| BC817-40.215 NPN SMD 45V/50V 0.5A 0.25W 100MHz | | SOT-23 |
| LTV817STA1 | Optocsatoló 5kV tranzisztor kimenet 35V 50mA 80KHz 4p. SMD 
| Csatlakozók| JST-XH | 2,54 |  

## A PCB -hez tartozó teljes "myoptions.h" tartalom.
```cpp
// clang-format off
/* https://trip5.github.io/ehRadio_myoptions/generator.html
   https://github.com/VaraiTamas/VTomRadio.git
   Használat előtt olvasd el!!! - Read the before use !!!
   https://github.com/VaraiTamas/VTomRadio/blob/main/README.md !!!

  - A sor elején a // jel inaktívvá teszi a parancsot ezért a fordító nem veszi figyelembe! 
    Ezzel tudod beállítani a te hardveredmek megfelelő összeállítást.
  - The // sign at the beginning of the line makes the command inactive, so the compiler ignores it! 
    This allows you to set the appropriate configuration for your hardware.
*/

#pragma once

#ifndef ARDUINO_ESP32S3_DEV
    #define ARDUINO_ESP32S3_DEV
#endif

/* Itt tudod beállítani a program nyelvét
   You can set the program language here.
   Supported languages: HU NL PL RU EN GR SK DE UA ES CZ. */
#define LANGUAGE HU

/* -- Névnapok megjelenítése -- Display name days --
Supported languages: HU, PL, NL, GR, DE, CZ, SK (UA Local/namedays/namedays_UA.h is not filled in.) */
#define NAMEDAYS_FILE HU

#define USE_BUILTIN_LED false /* The RGB LED does not turn on.. */

/* Arduino OTA Support */
 #define USE_OTA true                    /* Enable OTA updates from Arduino IDE */
// #define OTA_PASS "myotapassword12345"   /* OTA password for secure updates */

/* HTTP Authentication */
// #define HTTP_USER ""               /* HTTP basic authentication username */
// #define HTTP_PASS ""               /* HTTP basic authentication password */

/*----- LCD DISPLAY -----*/
//#define DSP_MODEL DSP_ILI9341    // 320x240
//#define DSP_MODEL DSP_ILI9488    // 480x320
//#define DSP_MODEL DSP_ST7789     // 320x240
#define DSP_MODEL DSP_ST7796       // 480x320

/*----- LovyanGFX SPI bus speed -----*/
/* Instabil kijelző, rossz vezetékelésnél használd csak. A használható értékek 24, 26, 28, 30, 32, 36 MHz */
/* Unstable display, use only if wiring is bad. Usable values ​​are 24, 26, 28, 30, 32, 36 MHz */
// #  define LGFX_LCD_FREQ_WRITE 24000000             // original: 40000000
// #  define LGFX_LCD_FREQ_READ  10000000             // original: 16000000
// #  define LGFX_TOUCH_SPI_FREQ   800000             // original: 2500000

/*----- SCREEN ROTATION -----*/
//#define DEFAULT_SCREEN_ROTATION 3  /* 0-3 */

/*----- DISPLAY PIN SETS -----*/
#define TFT_DC         9
#define TFT_CS         10
#define TFT_RST        -1
#define BRIGHTNESS_PIN 14
#define BRIGHTNESS_PWM_FREQ 5000
/*
   GPIO 11 - MOSI
   GPIO 12 - CLK
   GPIO 13 - MISO  // Ne csatlakoztasd a kijelzőhöz!!! - Do not connect to the LCD display!!!
*/

/*----- Touch SPI -----*/
//#define TS_MODEL TS_MODEL_XPT2046
//#define TS_CS    3

/*----- Touch I2C -----*/
#define TS_MODEL TS_MODEL_FT6X36
// #define TS_MODEL TS_MODEL_AXS15231B

 #define TS_SCL     7
 #define TS_SDA     8
 #define TS_INT    17 
 #define TS_RST     1

/*----- PCM5102A  DAC -----*/
#define I2S_DOUT 4
#define I2S_BCLK 5
#define I2S_LRC  6
// #define I2S_MCLK 0  /* CS4344 DAC: MCLK pin (PCM5102A-nál nem szükséges / not needed for PCM5102A) */
//Szabad és MCLK-ra alkalmas jelöltek: GPIO 0, 7, 8, 15, 16, 17, 18, 45, 46

/*----- ENCODER 1 ------*/
#define ENC_BTNL 42 // S1
#define ENC_BTNR 47 // S2
#define ENC_BTNB 21 // KEY
//#define ENC_INTERNALPULLUP	true
//#define ENC_HALFQUARD true

/*----- ENCODER 2 -----*/
#define ENC2_BTNL 40 // S1
#define ENC2_BTNR 41 // S2
#define ENC2_BTNB 39 // KEY
//#define ENC2_INTERNALPULLUP	true
//#define ENC2_HALFQUARD       true

/*----- CLOCK MODUL RTC DS3132 -----*/
 #define RTC_SCL			     7
 #define RTC_SDA			     8
 #define RTC_MODULE DS3231

/*----- REMOTE CONTROL INFRARED RECEIVER -----*/
/*----- Alvásból ébresztéshez a GPIO 2 -őt kell használni -----*/
/*----- To wake from sleep, you must use GPIO 2 -----*/
#define IR_PIN 2
#define IR_NEC_ONLY  // Build only NEC decoder sources from IRremoteESP8266 (faster/smaller build)

/*----- Sleep functions -----*/
/*----- A WAKE_PIN helyett mostantól két pin állítható be az ébresztéshez: WAKE_PIN1 és WAKE_PIN2 -----*/
/*----- Így távirányítóval és egy másik gombbal is felébreszthető az eszköz. -----*/
/*----- Instead of WAKE_PIN, you can now set two pins for wake-up: WAKE_PIN1 and WAKE_PIN2 -----*/
/*----- This way, you can wake up the device with a remote control and another button. -----*/
//#define BTN_MODE ENC_BTNB
#define WAKE_PIN1 IR_PIN
#define WAKE_PIN2 ENC_BTNB

/*----- SD CARD -----*/
 #define SDC_CS     18
 #define SD_SPIPINS 12, 13, 11, SDC_CS  // SCK, MISO, MOSI, CS
 #define SDSPISPEED 10000000

/*----- Ezzel a beállítással nincs görgetés az időjárás sávon. -----*/
/*----- With this setting there is no scrolling on the weather bar. -----*/
#define WEATHER_FMT_SHORT

/*----- Ezzel a beállítással a teljes időjárás jelentés megjelenik. -----*/
/*----- With this setting, the full weather report is displayed. -----*/
 #define EXT_WEATHER  true

/*----- Ezzel a beállítással a szél sebessége km/h lesz. -----*/
/*----- With this setting, the wind speed will be km/h. -----*/
 #define WIND_SPEED_IN_KMH

/*----- Az itt beállított pin vezérelheti egy audio erősítő tápellátását. Zenelejátszás közben a pin HIGH (magas) állapotban van ami meghúzza az
erősítő tápellátását kapcsoló relét. Amikor nincs zenelejátszás (STOP vagy a hangerő 0), a pin LOW (alacsony) állapotban van.
Ez a változás akkor történik, amikor a képernyővédő "while not playing" üzemmódban bekapcsol.
This pin controls the amplifier's power supply. When music is playing, the pin is set to HIGH to control the relay.
When music is not playing (stopped or volume is 0), the pin is set to LOW. This change occurs when the screensaver is running. -----*/
// #define PWR_AMP 38

/*----- Ha ez definiálva van a rádió indításakor, mindig az első csatorna lesz beállítva. -----*/
/*----- If this is defined at radio startup, the first channel will always be set. -----*/
//#define ALWAYS_START_FROM_FIRST

/*----- by Zsolt Simon -----*/
/*----- Tested on Synology NAS ----- */
 #define USE_DLNA
 #define dlnaHost "192.168.1.200"
 #define dlnaIDX  21

#define POWER_LED 38      // Button LED, DAC, RTC power on/off pin 

```
### Ha támogatni szeretnéd a munkámat itt meghívhatsz egy kávéra!!!     
<a href="https://buymeacoffee.com/vtom">
    <img src="../../images/buymeacoffee.png" width="200">
</a>

### Ez a PCB verzió legyártható a [jlcpcb.com](https://jlcpcb.com/?from=AMOSWLDYVIS) oldalon a [yoradio_gerber.zip](../../PCB/PCB_2026_06_21/PCB_2026.06.21_gerber.zip)   fájl feltöltésével.     
