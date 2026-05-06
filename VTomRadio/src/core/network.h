#ifndef network_h
#define network_h
#include <WiFi.h>
#include <time.h>

enum n_Status_e { CONNECTED, SOFT_AP, FAILED, SDREADY };

class MyNetwork {
  public:
    n_Status_e status;
    struct tm timeinfo;
    bool lostPlaying = false, beginReconnect = false;
  public:
    MyNetwork() {};
    void begin();
    void requestTimeSync();
    void requestWeatherSync();
    void setWifiParams();
    bool wifiBegin(bool silent=false);
  private:
    void raiseSoftAP();
    static void WiFiLostConnection(WiFiEvent_t event, WiFiEventInfo_t info);
    static void WiFiReconnected(WiFiEvent_t event, WiFiEventInfo_t info);
};

extern MyNetwork network;

void network_get_timeinfo_snapshot(struct tm* out);
void network_set_timeinfo(const struct tm& in);

extern __attribute__((weak)) void network_on_connect();

#endif
