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

#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4
#define SS_PIN    5    
#define RST_PIN   33  
#define RXD2 32   
#define TXD2 27   
#define LED_PIN 26  
#define valvePin 13 
#define TEMP_SENSOR_PIN 34 
#define VALVE_ACTIVE HIGH
#define PHYSICAL_POWER_DURATION 10000 
#define LOGICAL_OPEN_DURATION   30000 
const unsigned long HEARTBEAT_INTERVAL = 60000;

const char* ssid = "Xuan's Xiaomi 14 Ultra";
const char* password = "aa970731";
String G_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbyginNk74L6clpON6vAueQzlnEon2UmKcO2SSZS44Df2D5Z45Qpf92EH1gcBHqny9MJXw/exec";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 28800; 
const int   daylightOffset_sec = 0;

WiFiClientSecure secured_client;
MFRC522 rfid(SS_PIN, RST_PIN);
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
HudScreen* hud;

// ★★★ 修改：使用英文名字 (解決亂碼) ★★★
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

volatile bool wifiReady = false;        
String sharedMegaMsg = "0";             
String sharedValveState = "0";          
volatile bool dataChanged = false;      
unsigned long lastUploadTime = 0;       

TaskHandle_t TaskUploadHandle;

float readTemperature() {
  int totalRaw = 0;
  for(int i=0; i<10; i++) {
    totalRaw += analogRead(TEMP_SENSOR_PIN);
    delay(2); 
  }
  float rawValue = totalRaw / 10.0;
  float voltage = (rawValue / 4095.0) * 3.3;
  float temp = voltage * 100.0;
  return temp + 10.0; 
}

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

bool performUpload(String megaData, String valveState) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    float temp = readTemperature(); 
    String url = G_SCRIPT_URL + "?data=" + urlEncode(megaData) + "&valve=" + urlEncode(valveState) + "&temp=" + String(temp, 1);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.setTimeout(5000); 
    if (http.begin(secured_client, url)) {
      int httpCode = http.GET();
      http.end();
      if (httpCode > 0 && (httpCode == 200 || httpCode == 302)) {
          lastUploadTime = millis();
          Serial.println("[Core 0] 上傳成功!");
          return true;
      }
    }
  } else {
    WiFi.reconnect();
  }
  return false;
}

void TaskUploadCode(void * pvParameters) {
  Serial.print("上傳任務運行在核心: ");
  Serial.println(xPortGetCoreID());
  for(;;) { 
    if (wifiReady) {
      bool needUpload = false;
      String currentMsg;
      String currentValve;
      currentMsg = sharedMegaMsg;
      currentValve = sharedValveState;
      if (dataChanged) { needUpload = true; dataChanged = false; }
      if (millis() - lastUploadTime > HEARTBEAT_INTERVAL) { needUpload = true; }
      if (needUpload) { performUpload(currentMsg, currentValve); }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); 
  }
}

void handleMegaSerial() {
  if (Serial2.available()) {
    String latestMsg = "";
    while (Serial2.available()) {
      latestMsg = Serial2.readStringUntil('\n');
      latestMsg.trim();
    }
    if (latestMsg.length() > 0) {
      hud->updateCircles(latestMsg); 
      Serial.println("[Core 1] LCD更新: " + latestMsg);
      if (latestMsg != sharedMegaMsg) {
         sharedMegaMsg = latestMsg;
         dataChanged = true; 
      }
      digitalWrite(LED_PIN, HIGH); delay(5); digitalWrite(LED_PIN, LOW);
      Serial2.println("Ok");
    }
  }
}

void handleRFID() {
  unsigned long currentMillis = millis();
  if (isValvePowered && (currentMillis - valveOpenStartTime > PHYSICAL_POWER_DURATION)) {
    isValvePowered = false; digitalWrite(valvePin, !VALVE_ACTIVE);
  }
  if (isLogicallyOpen && (currentMillis - valveOpenStartTime > LOGICAL_OPEN_DURATION)) {
    isLogicallyOpen = false; 
    sharedValveState = "0"; 
    dataChanged = true;     
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
  Serial.print(">> 偵測 UID: "); Serial.println(uidStr);

  byte *id = rfid.uid.uidByte; byte idSize = rfid.uid.size; bool foundTag = false;
  for (byte i = 0; i < totalTags; i++) {
    if (memcmp(tags[i].uid, id, idSize) == 0) {
      foundTag = true;
      Serial.print("   驗證成功！歡迎："); Serial.println(tags[i].name);
      
      // 更新介面 (現在吃 String 就不會亂碼了)
      hud->updateLastUser(tags[i].name, uidStr);

      digitalWrite(valvePin, VALVE_ACTIVE);
      isValvePowered = true; isLogicallyOpen = true; valveOpenStartTime = currentMillis; 
      sharedValveState = "1"; dataChanged = true;
      break;
    }
  }
  if (!foundTag) {
      Serial.println("   驗證失敗");
      hud->updateLastUser("Access Denied", uidStr); 
  }
  rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== 系統啟動 ===");

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);
  hud = new HudScreen(&tft);
  hud->showBootConnecting(); 

  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial2.setTimeout(50); 
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(valvePin, OUTPUT);
  digitalWrite(valvePin, !VALVE_ACTIVE);
  pinMode(TEMP_SENSOR_PIN, INPUT);
  
  SPI.begin();
  rfid.PCD_Init();
  
  secured_client.setInsecure();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    delay(500); retry++; Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    wifiReady = true; 
    hud->showBootSuccess(); 
    delay(1000);
    hud->showMain(); 
    hud->updateCircles(sharedMegaMsg);
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
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000) {
      currentTemp = readTemperature();
      char timeStr[10];
      char dateStr[12];
      getLocalTimeStr(timeStr, dateStr);
      hud->updateTimeTemp(currentTemp, timeStr, dateStr); 
      lastUpdate = millis();
  }
}
