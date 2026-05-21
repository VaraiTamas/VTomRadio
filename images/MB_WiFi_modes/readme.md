## WiFi icon visual modification on 480x320 screen

This modification changes the position and size of the WiFi icons on the screen.

Steps:

1. Modify the WiFi icon position and size in the firmware source src\displays\conf\conf_480x320.h.
   
   Line 63:
   const WifiWidgetConfig wifiConf PROGMEM = {390, 288, 80, 25, "/images/wifi_1_80x25.png", "/images/wifi_2_80x25.png", "/images/wifi_3_80x25.png", "/images/wifi_4_80x25.png"}; // (MB)

2. Compile and update the firmware.

3. Unpack modified icons from mod1 or mod2.

4. Upload icons to the data/images directory (Settings > Update > Board > Images).

5. Restart the radio.



The icons are designed for a black background and work best when the theme background is set to black.