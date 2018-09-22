/*
 * Main sketch, global variable declarations
 */
/*
 * @title WLED project sketch
 * @version 0.8.0-a
 * @author Christian Schwinne
 */

//ESP8266-01 got too little storage space to work with all features of WLED. To use it, you must use ESP8266 Arduino Core v2.3.0 and the setting 512K(64K SPIFFS).
//Uncomment the following line to disable some features (currently Mobile UI, welcome page and single digit + cronixie overlays) to compile for ESP8266-01
//#define WLED_FLASH_512K_MODE
//CURRENTLY NOT WORKING


//library inclusions
#include <Arduino.h>
#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#include <ESPmDNS.h>
#include "src/dependencies/webserver/WebServer.h"
#include <HTTPClient.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#endif

#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <WiFiUDP.h>
#include <DNSServer.h>
#include "src/dependencies/webserver/ESP8266HTTPUpdateServer.h"
#include "src/dependencies/time/Time.h"
#include "src/dependencies/time/TimeLib.h"
#include "src/dependencies/timezone/Timezone.h"
#include "htmls00.h"
#include "htmls01.h"
#include "htmls02.h"
#include "WS2812FX.h"
#include "src/dependencies/blynk/BlynkSimpleEsp.h"
#include "src/dependencies/e131/E131.h"


//version code in format yymmddb (b = daily build)
#define VERSION 1809222
char versionString[] = "0.8.0-a";


//AP and OTA default passwords (for maximum change them!)
char apPass[65] = "wled1234";
char otaPass[33] = "wledota";


//spiffs FS only useful for debug (only ESP8266)
//#define USEFS


//to toggle usb serial debug (un)comment following line(s)
//#define DEBUG


//Hardware CONFIG (only changeble HERE, not at runtime)
//LED strip pin changeable in NpbWrapper.h. Only change for ESP32
byte buttonPin = 0;                           //needs pull-up
byte auxPin = 15;                             //debug feature, use e.g. for external relay with API call AX=
byte auxDefaultState   = 0;                   //0: input 1: high 2: low
byte auxTriggeredState = 0;                   //0: input 1: high 2: low
char ntpServerName[] = "0.wled.pool.ntp.org"; //NTP server to use


//WiFi CONFIG (all these can be changed via web UI, no need to set them here)
char clientSSID[33] = "Your_Network";
char clientPass[65] = "";
char cmDNS[33] = "led";                       //mDNS address (x.local), only for Apple and Windows, if Bonjour installed
char apSSID[65] = "";                         //AP off by default (unless setup)
byte apChannel = 1;                           //2.4GHz WiFi AP channel (1-13)
byte apHide = 0;                              //hidden AP SSID
byte apWaitTimeSecs = 32;                     //time to wait for connection before opening AP
bool recoveryAPDisabled = false;              //never open AP (not recommended)
IPAddress staticIP(0, 0, 0, 0);               //static IP of ESP
IPAddress staticGateway(0, 0, 0, 0);          //gateway (router) IP
IPAddress staticSubnet(255, 255, 255, 0);     //most common subnet in home networks
IPAddress staticDNS(8, 8, 8, 8);              //only for NTP, google DNS server


//LED CONFIG
uint16_t ledCount = 10;                       //lowered to prevent accidental overcurrent                      
bool useRGBW = false;                         //SK6812 strips can contain an extra White channel
bool autoRGBtoRGBW = false;                   //if RGBW enabled, calculate White channel from RGB
bool turnOnAtBoot  = true;                    //turn on LEDs at power-up
byte bootPreset = 0;                          //save preset to load after power-up

byte colS[]{255, 159, 0};                     //default RGB color
byte colSecS[]{0, 0, 0};                      //default RGB secondary color
byte whiteS = 0;                              //default White channel
byte whiteSecS = 0;                           //default secondary White channel
byte briS = 127;                              //default brightness
byte effectDefault = 0;                   
byte effectSpeedDefault = 75;
byte effectIntensityDefault = 128;            //intensity is supported on some effects as an additional parameter (e.g. for blink you can change the duty cycle)
byte effectPaletteDefault = 0;                //palette is supported on the FastLED effects, otherwise it has no effect

bool useGammaCorrectionBri = false;           //gamma correct brightness (not recommended)
bool useGammaCorrectionRGB = true;            //gamma correct colors (strongly recommended)

byte nightlightTargetBri = 0;                 //brightness after nightlight is over
byte nightlightDelayMins = 60;
bool nightlightFade = true;                   //if enabled, light will gradually dim towards the target bri. Otherwise, it will instantly set after delay over
bool fadeTransition = true;                   //enable crossfading color transition
bool enableSecTransition = true;              //also enable transition for secondary color
uint16_t transitionDelay = 1200;              //default crossfade duration in ms

bool reverseMode  = false;                    //flip entire LED strip (reverses all effect directions)
bool initLedsLast = false;                    //turn on LEDs only after WiFi connected/AP open
bool skipFirstLed = false;                    //ignore first LED in strip (useful if you need the LED as signal repeater)
byte briMultiplier =  100;                    //% of brightness to set (to limit power, if you set it to 50 and set bri to 255, actual brightness will be 127)


//User Interface CONFIG
char serverDescription[33] = "WLED Light";    //Name of module
byte currentTheme = 0;                        //UI theme index for settings and classic UI
byte uiConfiguration = 0;                     //0: automatic (depends on user-agent) 1: classic UI 2: mobile UI
bool useHSB = true;                           //classic UI: use HSB sliders instead of RGB by default
char cssFont[33] = "Verdana";                 //font to use in classic UI

bool useHSBDefault = useHSB;


//Sync CONFIG
bool buttonEnabled = true;

uint16_t udpPort    = 21324;                  //WLED notifier default port
uint16_t udpRgbPort = 19446;                  //Hyperion port

bool receiveNotificationBrightness = true;    //apply brightness from incoming notifications
bool receiveNotificationColor      = true;    //apply color
bool receiveNotificationEffects    = true;    //apply effects setup
bool notifyDirect =  true;                    //send notification if change via UI or HTTP API
bool notifyButton =  true;                     
bool notifyAlexa  = false;                    //send notification if updated via Alexa
bool notifyMacro  = false;                    //send notification for macro
bool notifyHue    =  true;                    //send notification if Hue light changes
bool notifyTwice  = false;                    //notifications use UDP: enable if devices don't sync reliably

bool alexaEnabled = true;                     //enable device discovery by Amazon Echo
char alexaInvocationName[33] = "Light";       //speech control name of device. Choose something voice-to-text can understand

char blynkApiKey[36] = "";                    //Auth token for Blynk server. If empty, no connection will be made

uint16_t realtimeTimeoutMs = 2500;            //ms timeout of realtime mode before returning to normal mode
int  arlsOffset = 0;                          //realtime LED offset
bool receiveDirect    =  true;                //receive UDP realtime
bool enableRealtimeUI = false;                //web UI accessible during realtime mode (works on ESP32, lags out ESP8266)
bool arlsDisableGammaCorrection = true;       //activate if gamma correction is handled by the source
bool arlsForceMaxBri = false;                 //enable to force max brightness if source has very dark colors that would be black

bool e131Enabled = true;                      //settings for E1.31 (sACN) protocol
uint16_t e131Universe = 1;
bool e131Multicast = false;

bool huePollingEnabled = false;               //poll hue bridge for light state
uint16_t huePollIntervalMs = 2500;            //low values (< 1sec) may cause lag but offer quicker response
char hueApiKey[65] = "api";                   //key token will be obtained from bridge
byte huePollLightId = 1;                      //ID of hue lamp to sync to. Find the ID in the hue app ("about" section)
IPAddress hueIP = (0,0,0,0);                  //IP address of the bridge
bool hueApplyOnOff = true;
bool hueApplyBri   = true;
bool hueApplyColor = true;


//Time CONFIG
bool ntpEnabled = false;                      //get internet time. Only required if you use clock overlays or time-activated macros
bool useAMPM = false;                         //12h/24h clock format
byte currentTimezone = 0;                     //Timezone ID. Refer to timezones array in wled10_ntp.ino
int  utcOffsetSecs   = 0;                     //Seconds to offset from UTC before timzone calculation

byte overlayDefault = 0;                      //0: no overlay 1: analog clock 2: single-digit clocl 3: cronixie
byte overlayMin = 0, overlayMax = ledCount-1; //boundaries of overlay mode

byte analogClock12pixel = 0;                  //The pixel in your strip where "midnight" would be
bool analogClockSecondsTrail = false;         //Display seconds as trail of LEDs instead of a single pixel
bool analogClock5MinuteMarks = false;         //Light pixels at every 5-minute position

char cronixieDisplay[] = "HHMMSS";            //Cronixie Display mask. See wled13_cronixie.ino
bool cronixieBacklight = true;                //Allow digits to be back-illuminated

bool countdownMode = false;                   //Clock will count down towards date
byte countdownYear = 19, countdownMonth = 1;  //Countdown target date, year is last two digits
byte countdownDay  =  1, countdownHour  = 0;
byte countdownMin  =  0, countdownSec   = 0;


byte macroBoot = 0;                           //macro loaded after startup
byte macroNl = 0;                             //after nightlight delay over
byte macroCountdown = 0;                      
byte macroAlexaOn = 0, macroAlexaOff = 0;
byte macroButton = 0, macroLongPress = 0;


//Security CONFIG
bool otaLock = false;                         //prevents OTA firmware updates without password. ALWAYS enable if system exposed to any public networks
bool wifiLock = false;                        //prevents access to WiFi settings when OTA lock is enabled
bool aOtaEnabled = true;                      //ArduinoOTA allows easy updates directly from the IDE. Careful, it does not auto-disable when OTA lock is on


uint16_t userVar0 = 0, userVar1 = 0;



//internal global variable declarations
//color
byte col[]{255, 159, 0};                      //target RGB color
byte colOld[]{0, 0, 0};                       //color before transition
byte colT[]{0, 0, 0};                         //current color
byte colIT[]{0, 0, 0};                        //color that was last sent to LEDs
byte colSec[]{0, 0, 0};
byte colSecT[]{0, 0, 0};
byte colSecOld[]{0, 0, 0};
byte colSecIT[]{0, 0, 0};
byte white = whiteS, whiteOld, whiteT, whiteIT;
byte whiteSec = whiteSecS, whiteSecOld, whiteSecT, whiteSecIT;

byte lastRandomIndex = 0;                     //used to save last random color so the new one is not the same

//transitions
bool transitionActive = false;
uint16_t transitionDelayDefault = transitionDelay;
uint16_t transitionDelayTemp = transitionDelay;
unsigned long transitionStartTime;
float tperLast = 0;                           //crossfade transition progress, 0.0f - 1.0f

//nightlight
bool nightlightActive = false;
bool nightlightActiveOld = false;
uint32_t nightlightDelayMs = 10;
unsigned long nightlightStartTime;
byte briNlT = 0;                              //current nightlight brightness

//brightness
byte bri = briS;
byte briOld = 0;
byte briT = 0;
byte briIT = 0;
byte briLast = 127;                           //brightness before turned off. Used for toggle function

//button
bool buttonPressedBefore = false;
unsigned long buttonPressedTime = 0;

//notifications
bool notifyDirectDefault = notifyDirect;
bool receiveNotifications = true;
unsigned long notificationSentTime = 0;
byte notificationSentCallMode = 0;
bool notificationTwoRequired = false;

//effects
byte effectCurrent = effectDefault;
byte effectSpeed = effectSpeedDefault;
byte effectIntensity = effectIntensityDefault;
byte effectPalette = effectPaletteDefault;

//network
bool onlyAP = false;                          //only Access Point active, no connection to home network
bool udpConnected = false, udpRgbConnected = false;

//ui style
char cssCol[9][5]={"","","","","",""};
String cssColorString="";
bool showWelcomePage = false;

//hue
char hueError[25] = "Inactive";
uint16_t hueFailCount = 0;
float hueXLast=0, hueYLast=0;
uint16_t hueHueLast=0, hueCtLast=0;
byte hueSatLast=0, hueBriLast=0;
unsigned long hueLastRequestSent = 0;
unsigned long huePollIntervalMsTemp = huePollIntervalMs;
bool hueAttempt = false;

//overlays
byte overlayCurrent = overlayDefault;
byte overlaySpeed = 200;
unsigned long overlayRefreshMs = 200;
unsigned long overlayRefreshedTime;
int overlayArr[6];
uint16_t overlayDur[6];
uint16_t overlayPauseDur[6];
int nixieClockI = -1;
bool nixiePause = false;

//cronixie
byte dP[]{0,0,0,0,0,0};
bool cronixieInit = false;

//countdown
unsigned long countdownTime = 1514764800L;
bool countdownOverTriggered = true;

//timer
byte lastTimerMinute = 0;
byte timerHours[]   = {0,0,0,0,0,0,0,0};
byte timerMinutes[] = {0,0,0,0,0,0,0,0};
byte timerMacro[]   = {0,0,0,0,0,0,0,0};
byte timerWeekday[] = {255,255,255,255,255,255,255,255}; //weekdays to activate on
//bit pattern of arr elem: 0b11111111: sat,fri,thu,wed,tue,mon,sun,validity

//blynk
bool blynkEnabled = false;

//preset cycling
bool presetCyclingEnabled = false;
byte presetCycleMin = 1, presetCycleMax = 5;
uint16_t presetCycleTime = 1250;
unsigned long presetCycledTime = 0; byte presetCycCurr = presetCycleMin;
bool presetApplyBri = true, presetApplyCol = true, presetApplyFx = true;
bool saveCurrPresetCycConf = false;

//realtime
bool realtimeActive = false;
IPAddress realtimeIP = (0,0,0,0);
unsigned long realtimeTimeout = 0;

//auxiliary debug pin
byte auxTime = 0;
unsigned long auxStartTime = 0;
bool auxActive = false, auxActiveBefore = false;

//alexa udp
WiFiUDP UDP;
IPAddress ipMulti(239, 255, 255, 250);
unsigned int portMulti = 1900;
String escapedMac;

//dns server
DNSServer dnsServer;
bool dnsActive = false;

//network time
bool ntpConnected = false;
time_t local = 0;
unsigned long ntpLastSyncTime = 999000000L;
unsigned long ntpPacketSentTime = 999000000L;
IPAddress ntpServerIP;
unsigned int ntpLocalPort = 2390;
#define NTP_PACKET_SIZE 48

//string temp buffer
#define OMAX 1750
char obuf[OMAX];
uint16_t olen = 0;

//server library objects
#ifdef ARDUINO_ARCH_ESP32
WebServer server(80);
#else
ESP8266WebServer server(80);
#endif
HTTPClient hueClient;
ESP8266HTTPUpdateServer httpUpdater;

//udp interface objects
WiFiUDP notifierUdp, rgbUdp;
WiFiUDP ntpUdp;
E131 e131;

//led fx library object
WS2812FX strip = WS2812FX();

//debug macros
#ifdef DEBUG
 #define DEBUG_PRINT(x)  Serial.print (x)
 #define DEBUG_PRINTLN(x) Serial.println (x)
 #define DEBUG_PRINTF(x) Serial.printf (x)
 unsigned long debugTime = 0;
 int lastWifiState = 3;
 unsigned long wifiStateChangedTime = 0;
#else
 #define DEBUG_PRINT(x)
 #define DEBUG_PRINTLN(x)
 #define DEBUG_PRINTF(x)
#endif

//filesystem
#ifdef USEFS
#include <FS.h>;
File fsUploadFile;
#endif

//gamma 2.4 lookup table used for color correction
const byte gamma8[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

String txd = "Please disable OTA Lock in security settings!";

//function prototypes
void serveMessage(int,String,String,int=255);


//turns all LEDs off and restarts ESP
void reset()
{
  briT = 0;
  setAllLeds();
  DEBUG_PRINTLN("MODULE RESET");
  ESP.restart();
}


//append new c string to temp buffer efficiently
bool oappend(char* txt)
{
  uint16_t len = strlen(txt);
  if (olen + len >= OMAX) return false; //buffer full
  strcpy(obuf + olen, txt);
  olen += len;
  return true;
}


//append new number to temp buffer efficiently
bool oappendi(int i)
{
  char s[11]; 
  sprintf(s,"%ld", i);
  return oappend(s);
}


//boot starts here
void setup() {
    wledInit();
}


//main program loop
void loop() {
    server.handleClient();
    handleSerial();
    handleNotifications();
    handleTransitions();
    userLoop();
    
    yield();
    handleButton();
    handleNetworkTime();
    if (aOtaEnabled) ArduinoOTA.handle();
    handleAlexa();
    handleOverlays();
    
    if (!realtimeActive) //block stuff if WARLS/Adalight is enabled
    {
      if (dnsActive) dnsServer.processNextRequest();
      handleHue();
      handleNightlight();
      handleBlynk();
      if (briT) strip.service(); //do not update strip if off, prevents flicker on ESP32
    }
    
    //DEBUG serial logging
    #ifdef DEBUG
    if (millis() - debugTime > 5000)
    {
      DEBUG_PRINTLN("---MODULE DEBUG INFO---");
      DEBUG_PRINT("Runtime: "); DEBUG_PRINTLN(millis());
      DEBUG_PRINT("Unix time: "); DEBUG_PRINTLN(now());
      DEBUG_PRINT("Free heap: "); DEBUG_PRINTLN(ESP.getFreeHeap());
      DEBUG_PRINT("Wifi state: "); DEBUG_PRINTLN(WiFi.status());
      if (WiFi.status() != lastWifiState)
      {
        wifiStateChangedTime = millis();
      }
      lastWifiState = WiFi.status();
      DEBUG_PRINT("State time: "); DEBUG_PRINTLN(wifiStateChangedTime);
      DEBUG_PRINT("NTP last sync: "); DEBUG_PRINTLN(ntpLastSyncTime);
      DEBUG_PRINT("Client IP: "); DEBUG_PRINTLN(WiFi.localIP());
      debugTime = millis(); 
    }
    #endif
}


