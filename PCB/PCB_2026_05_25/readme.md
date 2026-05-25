### PCB változások 2026.05.25 (terv még nincs tesztelve)

- Az IR LED átkerült a GPIO 2-re, mert a távirányítóval való ébresztéshez az RTC szükséges.
- A PCM5102A DAC VIN +5V bemenetét egy A03401A MOSFET kapcsolja a GPIO 38 állapotának megfelelően.Így a DAC sem kap áramot az ESP alatatásakor.
- Lett egy 3.3V-os kimenet a bekapcsoló gomb LED világításának meghajtásához mely egy BC817 tranzisztorral van kapcsolva a GPIO 38-hoz. Így a bekapcsológomb világítása is kikapcsol az ESP alvó állapotában.
- A rotary használta az ESP32 alaplap RGB LED GPIO 48 -at. Ez GPIO 42 -re lett változtatva.
- Az I2C touch kivezetés hozzá lett adva.
    - INT GPIO 17
    - SDA GPIO 8
    - RST GPIO 1
    - SCL GPIO 7

- A furatok és a kűlső méretek nem változtak.   
<br><br>


![PCB front](2D_pcb_top_98x100mm.jpg)<br><br>
![PCB back](2D_pcb_bottom_98x100mm.jpg)<br><br>

### Ha támogatni szeretnéd a munkámat itt meghívhatsz egy kávéra!!!     
https://buymeacoffee.com/vtom