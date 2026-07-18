# Hangerőgörbe (volume curve) működése    
A készülék hangerőszabályzása nem lineáris, hanem egy hangerőgörbét használ. Ez azt jelenti, hogy a 0–21 közötti hangerőlépésekhez külön dB értékek tartoznak, így kis hangerőnél is finomabban állítható a hang.

## Honnan kapja a kezdeti értékeket induláskor?    
Induláskor a rendszer az alábbi sorrendben dolgozik:

1. Megpróbálja betölteni a hangerőgörbét az ESP32 FLASH LittleFS fájlból:
/data/data/volcurve.csv  
2.  Ha ez a fájl hiányzik vagy hibás formátumú, akkor a korábban mentett (EEPROM) értékek maradnak érvényben.
3. Ha az EEPROM-ban lévő görbe is érvénytelen, akkor gyári alapértékeket állít be a config.cpp fájlban található `kDefaultVolumeCurveDb` tömbből.   
```
constexpr int8_t kDefaultVolumeCurveDb[21] = {-32, -28, -24, -20, -18, -16, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, -0};
```

## A volcurve.csv elvárt formátuma
A fájl CSV formátumú, 2 oszloppal:

* step: 1-től 21-ig
* db: -60 és 0 közötti érték    

Példa fejléc:   
step,db

Fontos szabályok:

1. Mind a 21 lépésnek szerepelnie kell.
2. A step tartomány 1..21.
3. A db tartomány -60..0.
4. Teljesen -1 értékekből álló görbe nem fogadható el.  

## Hol található a projektben a kiinduló fájl?
A repository-ben a feltöltendő alapfájl itt található:  
```
data/data/volcurve.csv  
```
Ez tipikusan a LittleFS tartalom részeként kerül az eszközre.

## Mi történik hibás fájl esetén?
Ha a betöltött volcurve.csv hibás:

1.  A rendszer elutasítja.
2.  Visszaáll gyári görbére.
3.  A javított/érvényesített állapotot elmenti.
4.  Így a készülék nem marad használhatatlan hangerőbeállítással.

## Mit érdemes tudni felhasználóként?  
Ha finomabb hangerőkarakterisztikát szeretnél, a volcurve.csv módosítható.
Ügyelj a pontos formátumra és a 21 sorra.
Hibás fájltól nem “romlik el” a készülék, mert van automatikus visszaállítás.   

A beállítás a webes felületen is indítható. 
<br><br>
![volume curve](../images/volcurve.jpg)<br><br>

## A gombbok működése a következő: 
**SAVE CSV** -  a webes felületen beállított görbét menti az ESP32 FLASH LittleFS  data/data/volcurve.csv fájlba. A program következő indításakor ezt tölti be.     
**RESET BASE CURVE** -  a gyári alapértékekre állítja vissza a görbét. Ha azt szeretnéd, hogy a következő indításkor ez legyen érvénybe, akkor a **SAVE CSV** gombbal el kell menteni azt!    
**EXPORT CSV** -  a webes felületen beállított görbét letölthetővé teszi a böngésző számára. A fájl neve: volcurve.csv .    
**IMPORT CSV** -  a böngészőből feltölthető a volcurve.csv fájl. A program ellenőrzi a formátumot és a 21 sor meglétét. Hibás fájl esetén visszaáll gyári alapértékre.
<br><br>
![volume curve](../images/volcurve1.jpg)<br><br>    

### Ha támogatni szeretnéd a munkámat itt meghívhatsz egy kávéra!!!     
<a href="https://buymeacoffee.com/vtom">
    <img src="../images/buymeacoffee.png" width="200">
</a>