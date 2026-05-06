#pragma once
#include "../AsyncWebServer/ESPAsyncWebServer.h"
#define APPEND_GROUP(name) strcat(nsBuf, "\"" name "\",")

enum requestType_e : uint8_t  { PLAYLIST=1, STATION=2, STATIONNAME=3, ITEM=4, TITLE=5, VOLUME=6, NRSSI=7, BITRATE=8, MODE=9, EQUALIZER=10, BALANCE=11, PLAYLISTSAVED=12, STARTUP=13, GETINDEX=14, GETACTIVE=15, GETSYSTEM=16, GETSCREEN=17, GETTIMEZONE=18, GETWEATHER=19, GETCONTROLS=20, DSPON=21, SDPOS=22, SDLEN=23, SDSNUFFLE=24, SDINIT=25, GETPLAYERMODE=26, CHANGEMODE=27 };
enum import_e      : uint8_t  { IMDONE=0, IMPL=1, IMWIFI=2 };
const char emptyfs_html[] PROGMEM = R"(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1, minimum-scale=0.25"><meta charset="UTF-8">
<title>VTom Radio - WEB Board Uploader</title><style>html, body { margin: 0; padding: 0; height: 100%; } body{background-color: #000;color: #4b95d6;font-size:20px;display:flex;flex-direction:column;font-family:"Roboto","Helvetica Neue",Arial,sans-serif;font-weight:400;}
hr{margin:20px 0;border:0; border-top: #555 1px solid;} p{text-align:center;margin-bottom:10px;} section{max-width:500px; text-align:center;margin:0 auto 30px auto;padding:20px;flex:1;}
.hidden{display:none;}a { color: var(--accent-color); text-decoration: none; font-weight: bold } a:hover { text-decoration: underline }
#copy { text-align: center; padding: 14px; font-size: 14px; }
#uploadwrap.hidden,#wupload.hidden,#apintro.hidden{display:none;}
#wifi_scan_wrap{margin:25px 0;text-align:center;}
#scan_wifi_btn{border:2px solid #4b95d6;color:#000;background-color:#4b95d6;border-radius:25px;font-size:20px;padding:0 24px;height:46px;box-sizing:border-box;cursor:pointer;font-family:"Roboto","Helvetica Neue",Arial,sans-serif;font-weight:400;}
#scan_wifi_btn:disabled{opacity:.7;cursor:wait;}
#wifi_scan_list{margin-top:8px;max-height:180px;overflow:auto;border:1px solid #555;border-radius:8px;padding:6px;display:none;text-align:left;}
.wifi-item{display:flex;justify-content:space-between;gap:10px;padding:6px 8px;border-radius:6px;cursor:pointer;}
.wifi-item:hover{background: #272727;}
.wifi-rssi{color: #999;font-size:14px;}
input[type=file]{color: #ccc;} input[type=file]::file-selector-button, input[type=submit]{border:2px solid #4b95d6;color: #000;padding:6px 16px;border-radius:25px;background-color: #4b95d6;margin:0 6px;cursor:pointer;}
input[type=submit]{font-size:18px;text-transform:uppercase;padding:8px 26px;margin-top:10px;font-family:"Roboto","Helvetica Neue",Arial,sans-serif;font-weight:400;} span{color: #ccc} .flex{display:flex;justify-content: space-around;margin-top:10px;}
#wupload p{margin:0;} #wupload input[type=submit]{margin-top:0;height:46px;box-sizing:border-box;padding:0 26px;}
input[type=text],input[type=password]{width:170px;background: #272727;color: #4b95d6;padding:6px 12px;font-size:20px;border: #2d2d2d 1px solid;margin:4px 0 0 4px;border-radius:4px;outline:none;}
@media screen and (max-width:480px) {section{zoom:0.7;-moz-transform:scale(0.7);}#scan_wifi_btn{font-size:22px;padding:0 26px;height:50px;}}
</style>
<script type="text/javascript" src="/variables.js"></script>
</head><body>
<section>
<h2>VTom Radio - WEB Board Uploader</h2>
<div id="apintro">
<hr />
<span>Enter your Wi-Fi credentials below.<br />After the device connects to the network, open the IP address shown on the display to upload additional files.</span>
</div>
<div id="uploadwrap">
<hr />
<span>Select <u>ALL</u> files from <i>VTomRadio/data/www/</i><br />and upload them using the form below</span>
<hr />
<form action="/webboard" method="post" enctype="multipart/form-data">
<p><label for="www">www:</label> <input type="file" name="www" id="www" multiple></p>
<hr />
<span>Upload font files (<i>.vlw</i>) to <i>/fonts</i> &mdash; <b>required</b></span>
<p><label for="fonts">fonts:</label><input type="file" name="fonts" id="fonts" accept=".vlw" multiple></p>
<hr />
<span>-= OPTIONAL =-<br />You can also upload image files<br />to <i>/images</i></span>
<p><label for="images">images:</label><input type="file" name="images" id="images" accept=".png,.jpg,.jpeg,.gif,.webp,.bmp,.svg,.ico" multiple></p>
<hr />
<span>-= OPTIONAL =-<br />You can also upload <i>playlist.csv</i><br />and <i>wifi.csv files</i> from your backup</span>
<p><label for="data">wifi:</label><input type="file" name="data" id="data" multiple></p>
<hr />
<p><input type="submit" name="submit" value="Upload Files"></p>
</form>
</div>
<div style="padding:10px 0 0;" id="wupload">
<hr />
<form name="wifiform" method="post" action="/">
<div class="flex"><div><label for="ssid">ssid:</label><input type="text" id="ssid" name="ssid" value="" maxlength="30" autocomplete="off"></div>
<div><label for="pass">pass:</label><input type="password" id="pass" name="pass" value="" maxlength="40" autocomplete="off"></div>
</div>
<div id="wifi_scan_wrap">
<input type="button" id="scan_wifi_btn" value="Scan Wi-Fi" />
<div id="wifi_scan_list"></div>
</div>
<p><input type="submit" name="submit" value="Save & Restart"></p>
</form>
</div>
</section>
<p><a href="/emergency">emergency firmware uploader</a></p>
<div id="copy">powered by <a target="_blank" href="https://github.com/VaraiTamas/yoRadio">VTom Radio</a><span id="version"></span></div>
</body>
<script>
const connectedMode = (typeof isStaConnected !== 'undefined') ? !!isStaConnected : (playMode=='player');
document.wifiform.action = connectedMode ? '/webboard' : '/';
if(connectedMode) {
  document.getElementById("wupload").classList.add("hidden");
  document.getElementById("apintro").classList.add("hidden");
} else {
  document.getElementById("uploadwrap").classList.add("hidden");
}
document.getElementById("version").innerHTML=` | v${fwVersion}`;
const scanBtn=document.getElementById('scan_wifi_btn');
const scanList=document.getElementById('wifi_scan_list');
const ssidInput=document.getElementById('ssid');
if(!connectedMode&&scanBtn&&scanList&&ssidInput){
  const sleep=(ms)=>new Promise(resolve=>setTimeout(resolve,ms));
  const fetchWifiScan=async()=>{
    for(let attempt=0;attempt<8;attempt++){
      const resp=await fetch('/wifiscan',{cache:'no-store'});
      const running=resp.headers.get('X-WiFi-Scan-Running')==='1';
      const data=await resp.json();
      if(Array.isArray(data)&&data.length>0){
        return data;
      }
      if(!running){
        return Array.isArray(data)?data:[];
      }
      await sleep(350);
    }
    return [];
  };
  scanBtn.addEventListener('click', async ()=>{
    scanBtn.disabled=true;
    scanBtn.value='Scanning...';
    scanList.style.display='none';
    scanList.innerHTML='';
    try{
      const data=await fetchWifiScan();
      if(!Array.isArray(data)||data.length===0){
        scanList.innerHTML='<div class="wifi-rssi">No networks found</div>';
      }else{
        data.forEach(item=>{
          const row=document.createElement('div');
          row.className='wifi-item';
          const left=document.createElement('span');
          left.textContent=item.ssid||'';
          const right=document.createElement('span');
          right.className='wifi-rssi';
          right.textContent=`${item.rssi} dBm`;
          row.appendChild(left);
          row.appendChild(right);
          row.addEventListener('click',()=>{
            ssidInput.value=item.ssid||'';
            scanList.style.display='none';
          });
          scanList.appendChild(row);
        });
      }
      scanList.style.display='block';
    }catch(e){
      scanList.innerHTML='<div class="wifi-rssi">Scan failed</div>';
      scanList.style.display='block';
    }finally{
      scanBtn.disabled=false;
      scanBtn.value='Scan Wi-Fi';
    }
  });
}
</script>
</html>
)";
// index.html
const char index_html[] PROGMEM = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <meta name="theme-color" content="#4b95d6">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-status-bar-style" content="default">
  <link rel="icon" type="image/svg+xml" href="/logo.svg">
  <link rel="stylesheet" href="theme.css" type="text/css" />
  <link rel="stylesheet" href="style.css" type="text/css" />
  <script type="text/javascript" src="variables.js"></script>
  <script type="text/javascript" src="script.js"></script>
  <script type="text/javascript" src="dragpl.js"></script>
  </head>
<body>
<div id="content" class="hidden progmem">
</div><!--content-->
<div id="progress"><span id="loader"></span></div>
<div id="heap"></div>
</body>
</html>
)";
const char emergency_form[] PROGMEM = R"(
<form method="POST" action="/update" enctype="multipart/form-data">
  <input type="hidden" name="updatetarget" value="fw" />
  <label for="uploadfile">upload firmware</label>
  <input type="file" id="uploadfile" accept=".bin,.hex" name="update" />
  <input type="submit" value="Update" />
</form>
)";
struct nsRequestParams_t
{
  requestType_e type;
  uint8_t clientId;
};

class NetServer {
  public:
    import_e importRequest;
    bool resumePlay;
    char chunkedPathBuffer[40];
    char nsBuf[BUFLEN], nsBuf2[BUFLEN];
  public:
    NetServer() {};
    bool begin(bool quiet=false);
    void loop();
    void requestOnChange(requestType_e request, uint8_t clientId);
    void setRSSI(int val) { rssi = val; };
    int  getRSSI()        { return rssi; };
    void chunkedHtmlPage(const String& contentType, AsyncWebServerRequest *request, const char * path);
    void onWsMessage(void *arg, uint8_t *data, size_t len, uint8_t clientId);
    bool irRecordEnable;
#if IR_PIN!=255
    void irToWs(const char* protocol, uint64_t irvalue);
    void irValsToWs(); 
#endif
    void resetQueue();
  private:
    requestType_e request;
    QueueHandle_t nsQueue;
    char _wscmd[65], _wsval[65];
    char wsBuf[BUFLEN*3];
    int rssi;
    uint32_t playerBufMax;
    void getPlaylist(uint8_t clientId);
    bool importPlaylist();
    static size_t chunkedHtmlPageCallback(uint8_t* buffer, size_t maxLen, size_t index);
    void processQueue();
    int _readPlaylistLine(File &file, char * line, size_t size);
};

extern NetServer netserver;
extern AsyncWebSocket websocket;

