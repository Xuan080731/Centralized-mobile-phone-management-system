/*
 * ESP32 Master Code - V60.0 (Schedule Logic Fixed)
 * * CRITICAL FIX:
 * - Added 'checkSchedule()' back into the main loop() (It was missing in V54).
 * - Now Auto-Unlock will actually trigger.
 * * FEATURES:
 * - Remote Unlock (CMD_UNLOCK check).
 * - Schedule Sync from Google Sheets.
 * - Glass UI Compatibility.
 */

#include "ChineseFont.h" 
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "HudScreen.h" 
#include "time.h"
#include <DHT.h> 

// ★★★ 定時解鎖設定 ★★★
// 定時解鎖時，電磁鎖吸住 15 秒
#define SCHEDULE_UNLOCK_DURATION 15000 

struct ScheduleTime { int hour; int minute; };
ScheduleTime unlockSchedule[30]; 
int scheduleCount = 0;           
int lastUnlockMinute = -1;       

#define DOOR_SENSOR_PIN 14  
#define AUTO_LOCK_HOLD_TIME 2000 
#define WARMUP_TIME 25000 

#define BUZZER_PIN 26          
#define BUZZER_TYPE 0  
#if BUZZER_TYPE == 0
  #define BUZZER_ON  LOW
  #define BUZZER_OFF HIGH
#else
  #define BUZZER_ON  HIGH
  #define BUZZER_OFF LOW
#endif

String MY_CITY = "高雄市"; 
#define CWA_API_KEY "CWA-7E9EEA37-C85C-4017-BECF-066F3077E35E"      
String CWA_URL = "https://opendata.cwa.gov.tw/api/v1/rest/datastore/E-A0015-001?Authorization=";

#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST    4
#define SS_PIN     5    
#define RST_PIN   33   
#define RXD2      32   
#define TXD2      27   
#define valvePin  13 

#define MQ2_PIN   35  
#define DHT_PIN   25  
#define DHT_TYPE  DHT11

const int pwmFreq = 5000;
const int pwmResolution = 8;

#define PHYSICAL_POWER_DURATION 10000 
#define HIGH_POWER_DURATION     500   
#define LOGICAL_OPEN_DURATION   30000 
const unsigned long HEARTBEAT_INTERVAL = 60000;

const int level_GreenStart = 10;   
const int level_Yellow     = 50;   
const int level_Orange     = 90;   
const int fire_Threshold   = 110;  

const int SIGNAL_GAIN = 2; 

#define ILI9341_ORANGE 0xFD20 

const char* ssid = "TPLINK-C50";
const char* password = "home6977";
String G_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbyginNk74L6clpON6vAueQzlnEon2UmKcO2SSZS44Df2D5Z45Qpf92EH1gcBHqny9MJXw/exec";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 28800; 
const int   daylightOffset_sec = 0;

WiFiClientSecure secured_client;
MFRC522 rfid(SS_PIN, RST_PIN);
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
HudScreen* hud;
DHT dht(DHT_PIN, DHT_TYPE);

uint16_t exactBgColor;

struct RFIDTag { uint8_t uid[4]; String name; };
struct RFIDTag tags[3] = { 
  {{220, 2, 232, 169}, "Li Zheng-Yan"}, 
  {{137, 41, 41, 99}, "Meow"}, 
  {{203, 63, 170, 13}, "School-Info"}
};
byte totalTags = sizeof(tags) / sizeof(RFIDTag);

bool isValvePowered = false;     
bool isLogicallyOpen = false;    
unsigned long valveOpenStartTime = 0;
unsigned long rfidCooldownTime = 0;    
float currentTemp = 0.0;
float currentHum = 0.0; 

int currentSmokeDiff = -1; 
int smokeBaseline = 0;    
float smoothedSmokeVal = 0;

bool isWarmedUp = false;
bool isEmergencyMode = false;
unsigned long emergencyStartTime = 0;
unsigned long lastQuakeCheckTime = 0;

bool lastDoorClosedState = false; 
bool isAutoLocking = false;   
bool isScheduleUnlock = false; // 標記：是否為定時解鎖

volatile bool wifiReady = false;        
String sharedMegaMsg = "0";              
String sharedValveState = "0";           
volatile bool dataChanged = false;       
unsigned long lastUploadTime = 0;        
String lastAuthUser = "";

TaskHandle_t TaskUploadHandle;

void getLocalTimeStr(char* timeBuffer, char* dateBuffer) {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    strcpy(timeBuffer, "--:--");
    strcpy(dateBuffer, "----/--/--");
    return;
  }
  strftime(timeBuffer, 10, "%H:%M", &timeinfo);
  strftime(dateBuffer, 12, "%Y/%m/%d", &timeinfo); 
}

String urlEncode(String str) {
    String encodedString = "";
    char c; char code0; char code1;
    for (int i = 0; i < str.length(); i++) {
      c = str.charAt(i);
      if (c == ' ') encodedString += '+';
      else if (isalnum(c)) encodedString += c;
      else {
        code1 = (c & 0xf) + '0';
        if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
        c = (c >> 4) & 0xf; code0 = c + '0';
        if (c > 9) code0 = c - 10 + 'A';
        encodedString += '%'; encodedString += code0; encodedString += code1;
      }
    }
    return encodedString;
}

// 解析從 Google 傳來的排程字串 (例如 "08:10,12:00")
void updateScheduleFromString(String payload) {
    if (payload.length() < 3) return;
    if (payload.indexOf("Error") >= 0 || payload.indexOf("Busy") >= 0) return;

    // 如果沒有包含冒號，代表沒有有效時間，跳過
    if (payload.indexOf(':') == -1) return;

    int index = 0;
    int arrayPos = 0;
    
    // 清空舊排程 (重要)
    scheduleCount = 0; 

    while (index < payload.length() && arrayPos < 30) {
        int commaIndex = payload.indexOf(',', index);
        if (commaIndex == -1) commaIndex = payload.length();
        
        String timeStr = payload.substring(index, commaIndex); 
        int colonIndex = timeStr.indexOf(':');
        
        if (colonIndex > 0) {
            int h = timeStr.substring(0, colonIndex).toInt();
            int m = timeStr.substring(colonIndex + 1).toInt();
            unlockSchedule[arrayPos].hour = h;
            unlockSchedule[arrayPos].minute = m;
            arrayPos++;
        }
        index = commaIndex + 1;
    }
    scheduleCount = arrayPos;
    Serial.print("Schedule Sync OK. Count: "); Serial.println(scheduleCount);
}

// 統一解鎖函式
void triggerUnlock(String line1, String line2, bool isSchedule) {
    Serial.println("Unlock: " + line1 + " " + line2);
    hud->updateLastUser(line1, line2);
    
    ledcWrite(valvePin, 255); 
    isValvePowered = true; 
    isLogicallyOpen = true; 
    isAutoLocking = false; 
    isScheduleUnlock = isSchedule; // 設定標記
    
    valveOpenStartTime = millis(); 
    sharedValveState = "1"; 
    lastAuthUser = line1 + "|" + line2; 
    dataChanged = true;
}

bool performUpload(String megaData, String valveState, String user) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String doorStatus = (digitalRead(DOOR_SENSOR_PIN) == HIGH) ? "0" : "1";
    Serial.print("Uploading... "); 

    String url = G_SCRIPT_URL + "?data=" + urlEncode(megaData) + 
                 "&valve=" + urlEncode(valveState) + 
                 "&temp=" + String(currentTemp, 1) +
                 "&hum=" + String(currentHum, 0) + 
                 "&smoke=" + String(currentSmokeDiff) + 
                 "&user=" + urlEncode(user) +
                 "&door=" + doorStatus; 

    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.setTimeout(8000); 
    
    if (http.begin(secured_client, url)) {
      int httpCode = http.GET();
      if (httpCode > 0 && (httpCode == 200 || httpCode == 302)) {
          String response = http.getString();
          
          // 1. 檢查遠端開門指令
          if (response.indexOf("CMD_UNLOCK") >= 0) {
              Serial.println("[CMD] Remote Unlock!");
              triggerUnlock("Remote", "Web User", false);
          }
          
          // 2. 更新排程 (如果不含 CMD_UNLOCK，通常就是純排程字串)
          updateScheduleFromString(response);
          
          lastUploadTime = millis();
          Serial.println("Success");
          http.end();
          return true;
      }
      http.end();
    }
  } else {
    WiFi.reconnect();
  }
  return false;
}

void TaskUploadCode(void * pvParameters) {
  for(;;) { 
    if (wifiReady) {
      if (millis() - lastUploadTime > 2000) {
          bool needUpload = false;
          if (dataChanged) { needUpload = true; dataChanged = false; }
          if (millis() - lastUploadTime > HEARTBEAT_INTERVAL) { needUpload = true; }
          
          if (needUpload) { performUpload(sharedMegaMsg, sharedValveState, lastAuthUser); }
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); 
  }
}

void lockTheDoor() {
    isValvePowered = false; 
    isLogicallyOpen = false;
    isAutoLocking = false; 
    isScheduleUnlock = false;
    ledcWrite(valvePin, 0); 
    sharedValveState = "0"; 
    lastAuthUser = ""; 
    dataChanged = true;
    Serial.println("System Locked.");
}

void triggerEmergency(String reason) {
  if (isEmergencyMode) return; 
  if (!isWarmedUp) return; 

  Serial.println("EMERGENCY: " + reason);
  isEmergencyMode = true;
  emergencyStartTime = millis();

  ledcWrite(valvePin, 255);
  isValvePowered = true;
  valveOpenStartTime = millis(); 

  digitalWrite(BUZZER_PIN, BUZZER_ON);
  
  sharedValveState = "1";
  lastAuthUser = "System|EMERGENCY-" + reason;
  dataChanged = true;
}

void checkEarthquake() {
  if (String(CWA_API_KEY) == "") return;
  if (!isWarmedUp) return;
  
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = CWA_URL + String(CWA_API_KEY) + "&limit=1&format=JSON";
    
    if (http.begin(url)) {
      int httpCode = http.GET();
      if (httpCode == 200) {
        String payload = http.getString();
        if (payload.indexOf(MY_CITY) > 0) {
           if (payload.indexOf("4級") > 0 || payload.indexOf("5弱") > 0 || payload.indexOf("5強") > 0 || payload.indexOf("6弱") > 0) {
               Serial.println("Earthquake Alert in " + MY_CITY);
               triggerEmergency("Quake(" + MY_CITY + ")");
           }
        }
      }
      http.end();
    }
  }
}

// ★★★ 檢查排程函式 ★★★
void checkSchedule() {
  if (scheduleCount == 0) return; 

  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){ return; }

  // 避免同一分鐘重複觸發
  if (timeinfo.tm_min == lastUnlockMinute) return;
  
  // 排除週末 (0=週日, 6=週六)
  if (timeinfo.tm_wday == 0 || timeinfo.tm_wday == 6) return;

  for(int i=0; i<scheduleCount; i++) {
    if (timeinfo.tm_hour == unlockSchedule[i].hour && 
        timeinfo.tm_min == unlockSchedule[i].minute) {
        
        // 觸發定時解鎖
        String timeStr = String(timeinfo.tm_hour) + ":" + ((timeinfo.tm_min < 10) ? "0" : "") + String(timeinfo.tm_min);
        triggerUnlock("AUTOUNLOCK", timeStr, true);
        
        lastUnlockMinute = timeinfo.tm_min; 
        return;
    }
  }
  // 更新分鐘標記，確保每分鐘只檢查一次
  lastUnlockMinute = timeinfo.tm_min; 
}

void handleMegaSerial() {
  if (Serial2.available()) {
    String bestData = "";
    while (Serial2.available()) {
      String temp = Serial2.readStringUntil('\n');
      temp.trim();
      if (temp.length() > 0) {
        bestData = temp;
      }
    }
    if (bestData.length() > 0) {
      hud->updateCircles(bestData); 
      if (bestData != sharedMegaMsg) {
         sharedMegaMsg = bestData; 
         dataChanged = true; 
         Serial.println("Mega Updated: " + sharedMegaMsg);
      }
      Serial2.println("Ok");
    }
  }
}

void handleRFID() {
  unsigned long currentMillis = millis();
  
  if (isEmergencyMode) {
     if ((currentMillis / 200) % 2 == 0) digitalWrite(BUZZER_PIN, BUZZER_ON); 
     else digitalWrite(BUZZER_PIN, BUZZER_OFF);

     if (currentMillis - emergencyStartTime > 20000) {
        Serial.println("Emergency End");
        isEmergencyMode = false;
        digitalWrite(BUZZER_PIN, BUZZER_OFF); 
     }
  }

  int doorVal = digitalRead(DOOR_SENSOR_PIN); 
  bool currentDoorClosed = (doorVal == HIGH); 

  if (currentDoorClosed && !lastDoorClosedState) {
      Serial.println("Door Closed -> Auto-Latch (2s)");
      ledcWrite(valvePin, 255); 
      isValvePowered = true;
      valveOpenStartTime = currentMillis; 
      isAutoLocking = true;
      dataChanged = true;
  }
  lastDoorClosedState = currentDoorClosed;

  if (isValvePowered) {
    if (isEmergencyMode) {
       if (currentMillis - valveOpenStartTime > HIGH_POWER_DURATION) {
          ledcWrite(valvePin, 135); 
       }
       valveOpenStartTime = currentMillis; 
    } else {
       if (currentMillis - valveOpenStartTime > HIGH_POWER_DURATION) {
          ledcWrite(valvePin, 135); 
       }
       
       unsigned long durationLimit = PHYSICAL_POWER_DURATION; 
       if (isAutoLocking) {
           durationLimit = AUTO_LOCK_HOLD_TIME; 
       } else if (isScheduleUnlock) {
           durationLimit = SCHEDULE_UNLOCK_DURATION; // 15s
       }

       if (currentMillis - valveOpenStartTime > durationLimit) {
          lockTheDoor();
       }
    }
  }

  if (isLogicallyOpen && (currentMillis - valveOpenStartTime > LOGICAL_OPEN_DURATION)) {
     if (!isEmergencyMode) {
       lockTheDoor();
     }
  }

  if (currentMillis - rfidCooldownTime < 1000) return; 
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;
  rfidCooldownTime = currentMillis;

  String uidStr = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uidStr += String(rfid.uid.uidByte[i]);
    if (i < rfid.uid.size - 1) uidStr += "-";
  }
  
  bool foundTag = false;
  for (byte i = 0; i < totalTags; i++) {
    if (memcmp(tags[i].uid, rfid.uid.uidByte, rfid.uid.size) == 0) {
      foundTag = true;
      triggerUnlock(tags[i].name, uidStr, false); 
      break;
    }
  }
  if (!foundTag) hud->updateLastUser("Access Denied", uidStr); 
  rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
}

int readMQ2Median() {
  const int NUM_READS = 13; 
  int values[NUM_READS];
  for(int i=0; i<NUM_READS; i++) {
    values[i] = analogRead(MQ2_PIN);
    delay(2);
  }
  for(int i=0; i<NUM_READS-1; i++) {
    for(int j=0; j<NUM_READS-i-1; j++) {
      if(values[j] > values[j+1]) {
        int temp = values[j];
        values[j] = values[j+1];
        values[j+1] = temp;
      }
    }
  }
  return values[NUM_READS/2];
}

void waitForMegaSync() {
  Serial.println("Syncing with Mega...");
  unsigned long start = millis();
  while(millis() - start < 2000) {
    if (Serial2.available()) {
      String temp = Serial2.readStringUntil('\n');
      temp.trim();
      if (temp.length() > 0) {
        sharedMegaMsg = temp;
        if (sharedMegaMsg != "0") {
          dataChanged = true;
        }
        Serial.println("Synced: " + sharedMegaMsg);
        break;
      }
    }
    delay(50);
  }
}

void setup() {
  delay(1000); 

  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, BUZZER_ON); delay(100);
  digitalWrite(BUZZER_PIN, BUZZER_OFF); delay(100);
  digitalWrite(BUZZER_PIN, BUZZER_ON); delay(100);
  digitalWrite(BUZZER_PIN, BUZZER_OFF); 

  tft.begin();
  tft.setRotation(1);
  exactBgColor = tft.color565(0, 70, 110); 
  tft.fillScreen(exactBgColor);
  hud = new HudScreen(&tft);
  hud->showBootConnecting(); 

  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial2.setTimeout(300); 
  
  waitForMegaSync();

  if (!ledcAttach(valvePin, pwmFreq, pwmResolution)) {
    Serial.println("PWM Attach Failed!"); 
  }
  ledcWrite(valvePin, 0); 
  
  pinMode(MQ2_PIN, INPUT); 
  pinMode(DOOR_SENSOR_PIN, INPUT_PULLUP); 

  dht.begin();
  SPI.begin();
  rfid.PCD_Init();
  
  secured_client.setInsecure();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) { delay(500); retry++; Serial.print("."); }
  
  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    wifiReady = true; 
    hud->showBootSuccess(); 
    delay(1000);
    hud->showMain(); 
    hud->updateCircles(sharedMegaMsg);
    dataChanged = true; 
  } else {
    hud->showBootFailed();
    delay(2000);
    hud->showMain();
  }
  xTaskCreatePinnedToCore(TaskUploadCode, "UploadTask", 10000, NULL, 0, &TaskUploadHandle, 0);                
}

void loop() {
  handleMegaSerial();
  handleRFID();
  checkSchedule(); // ★★★ 最關鍵的修復：把這行加回來了！ ★★★
  
  if (millis() < WARMUP_TIME) {
      isWarmedUp = false;
      int remaining = (WARMUP_TIME - millis()) / 1000;
      
      static unsigned long lastWarmupUpdate = 0;
      if (millis() - lastWarmupUpdate > 1000) {
           float t = dht.readTemperature();
           float h = dht.readHumidity();
           if (!isnan(t)) { if (t < 0) t = -t; currentTemp = t; }
           if (!isnan(h) && h >= 0 && h <= 100) { currentHum = h; }
           
           char timeBuf[10]; char dateBuf[12];
           getLocalTimeStr(timeBuf, dateBuf);
           hud->updateTimeTemp(currentTemp, currentHum, timeBuf, dateBuf);
           
           int ax = 200; int ay = 165; 
           tft.fillRect(ax, ay, 120, 20, exactBgColor);
           tft.setTextSize(2);
           tft.setCursor(ax, ay);
           tft.setTextColor(ILI9341_CYAN); 
           tft.print("Wait: " + String(remaining) + "s");
           
           lastWarmupUpdate = millis();
      }
      return; 
  }

  if (!isWarmedUp) {
      isWarmedUp = true;
      long sum = 0;
      for(int i=0; i<20; i++) {
         sum += readMQ2Median();
         delay(10);
      }
      smokeBaseline = sum / 20;
      smoothedSmokeVal = smokeBaseline; 
      currentSmokeDiff = 0; 
      dataChanged = true;   
      Serial.println("Warmup Done. Baseline: " + String(smokeBaseline));
  }

  int rawAvg = readMQ2Median();
  
  smoothedSmokeVal = (smoothedSmokeVal * 0.6) + (rawAvg * 0.4);
  int diff = (int)smoothedSmokeVal - smokeBaseline;
  if (diff < 0) diff = 0; 
  diff = diff * SIGNAL_GAIN; 
  
  if (abs(diff - currentSmokeDiff) > 1 || diff > 20) {
      currentSmokeDiff = diff; 
      dataChanged = true; 
  }

  if (currentSmokeDiff >= fire_Threshold) {
    triggerEmergency("Fire");
  }

  if (millis() - lastQuakeCheckTime > 60000) {
    checkEarthquake();
    lastQuakeCheckTime = millis();
  }

  static unsigned long lastUpdate = 0;
  static String lastAirStr = "";
  
  if (millis() - lastUpdate > 1000) {
      float t = dht.readTemperature();
      float h = dht.readHumidity();

      if (!isnan(t)) { if (t < 0) t = -t; currentTemp = t; }
      if (!isnan(h) && h >= 0 && h <= 100) { currentHum = h; }
      
      char timeBuf[10]; char dateBuf[12];
      getLocalTimeStr(timeBuf, dateBuf);
      
      hud->updateTimeTemp(currentTemp, currentHum, timeBuf, dateBuf);
      
      String airStatus = "Air:Good!"; 
      uint16_t airColor = ILI9341_GREEN;
      
      if (currentSmokeDiff >= fire_Threshold) { 
        airStatus = "Emergency!"; airColor = ILI9341_RED;
      } else if (currentSmokeDiff >= level_Orange) { 
        airStatus = "Air:Bad"; airColor = ILI9341_ORANGE;
      } else if (currentSmokeDiff >= level_Yellow) { 
        airStatus = "Air:Mid"; airColor = ILI9341_YELLOW;
      } else if (currentSmokeDiff >= level_GreenStart) { 
        airStatus = "Air:Good!"; airColor = ILI9341_GREEN;
      }
      
      if (airStatus != lastAirStr) {
          int ax = 200; int ay = 165; 
          tft.fillRect(ax, ay, 120, 20, exactBgColor);
          tft.setTextSize(2);
          tft.setCursor(ax, ay);
          tft.setTextColor(airColor);
          tft.print(airStatus);
          lastAirStr = airStatus;
      }
      lastUpdate = millis();
  }
}