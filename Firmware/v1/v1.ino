#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>

#define RXD2 32   
#define TXD2 27   

const char* ssid = "輸入SSID名稱";
const char* password = "輸入Wifi密碼";
String G_SCRIPT_URL = "Google Apps Script 部署網址";

#define LED_PIN 26  
#define SS_PIN 21
#define RST_PIN 33   
#define valvePin 13 
#define VALVE_ACTIVE HIGH

#define TEMP_SENSOR_PIN 34 

#define PHYSICAL_POWER_DURATION 10000
#define LOGICAL_OPEN_DURATION   30000 
const unsigned long HEARTBEAT_INTERVAL = 60000;


WiFiClientSecure secured_client;
MFRC522 rfid(SS_PIN, RST_PIN);

struct RFIDTag { uint8_t uid[4]; String name; };
struct RFIDTag tags[2] = { {{220, 2, 232, 169}, "李政諺"}, {{137, 41, 41, 99}, "邱詠善"} };
byte totalTags = sizeof(tags) / sizeof(RFIDTag);

bool isValvePowered = false;    
bool isLogicallyOpen = false;   
unsigned long valveOpenStartTime = 0; 
unsigned long rfidCooldownTime = 0;   
unsigned long lastSendTime = 0;       
String lastMegaMsg = "0";

float readTemperature() {
  int rawValue = analogRead(TEMP_SENSOR_PIN);
  
  float voltage = (rawValue / 4095.0) * 3.3;
  float tempC = voltage * 100.0;
  
  return tempC;
}

String urlEncode(String str) {
    String encodedString = "";
    char c; char code0; char code1; char code2;
    for (int i = 0; i < str.length(); i++) {
      c = str.charAt(i);
      if (c == ' ') encodedString += '+';
      else if (isalnum(c)) encodedString += c;
      else {
        code1 = (c & 0xf) + '0'; if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
        c = (c >> 4) & 0xf; code0 = c + '0'; if (c > 9) code0 = c - 10 + 'A';
        code2 = '\0'; encodedString += '%'; encodedString += code0; encodedString += code1;
      }
    }
    return encodedString;
}

void setup() {
  Serial.begin(115200);
  
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(valvePin, OUTPUT);
  digitalWrite(valvePin, !VALVE_ACTIVE);
  
  pinMode(TEMP_SENSOR_PIN, INPUT);
  
  SPI.begin();
  rfid.PCD_Init();

  secured_client.setInsecure();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.print("正在連線到WiFi");
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi 已連線!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    lastSendTime = millis(); // 初始化心跳計時器
  } else {
    Serial.println("\n❌ WiFi 連線逾時，請檢查密碼或熱點");
  }
  
  Serial.println("系統啟動完成...");
}

void loop() {
  handleGoogleSheets();
  handleRFID();
  
  if (WiFi.status() == WL_CONNECTED && (millis() - lastSendTime > HEARTBEAT_INTERVAL)) {
    Serial.println("觸發心跳機制...");
    String vState = isLogicallyOpen ? "1" : "0";
    sendDataToGoogleSheet(lastMegaMsg, vState);
  }
}

void sendDataToGoogleSheet(String megaData, String valveState) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    float currentTemp = readTemperature();
    
    String url = G_SCRIPT_URL + "?data=" + urlEncode(megaData) 
                              + "&valve=" + urlEncode(valveState)
                              + "&temp=" + String(currentTemp, 1); 
    
    Serial.print("上傳中 (Temp: ");
    Serial.print(currentTemp);
    Serial.println(" C): " + url);

    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.setTimeout(15000); 

    if (http.begin(secured_client, url)) {
      int httpCode = http.GET();
      if (httpCode > 0) {
        if(httpCode == 200 || httpCode == 302) {
              Serial.println("✅ 上傳成功!");
              lastSendTime = millis(); 
        } else {
              Serial.printf("❌ 伺服器錯誤 (Code: %d)\n", httpCode);
        }
      }
      else Serial.printf("❌ 連線失敗: %s\n", http.errorToString(httpCode).c_str());
      http.end();
    }
  } else {
    Serial.println("⚠️ WiFi 未連線，嘗試重新連線...");
    WiFi.reconnect();
  }
}

void handleGoogleSheets() {
  if (Serial2.available()) {
    String msg = Serial2.readStringUntil('\n');
    msg.trim(); 
    
    Serial.print("收到 Mega: ");
    Serial.println(msg);
    
    lastMegaMsg = msg;        

    String vState = isLogicallyOpen ? "1" : "0";
    
    sendDataToGoogleSheet(msg, vState);
    
    digitalWrite(LED_PIN, HIGH); delay(100); digitalWrite(LED_PIN, LOW);
    Serial2.println("Ok");
  }
}

void handleRFID() {
  unsigned long currentMillis = millis();

  if (isValvePowered && (currentMillis - valveOpenStartTime > PHYSICAL_POWER_DURATION)) {
    isValvePowered = false; 
    digitalWrite(valvePin, !VALVE_ACTIVE); 
    Serial.println("🔒 電磁閥保護斷電");
  }

  if (isLogicallyOpen && (currentMillis - valveOpenStartTime > LOGICAL_OPEN_DURATION)) {
    isLogicallyOpen = false; 
    Serial.println("⏰ 邏輯開門結束，更新狀態");
    sendDataToGoogleSheet(lastMegaMsg, "0");
  }

  if (currentMillis - rfidCooldownTime < 1500) return;
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;
  
  rfidCooldownTime = currentMillis; 
  
  byte *id = rfid.uid.uidByte;
  byte idSize = rfid.uid.size;
  bool foundTag = false;

  for (byte i = 0; i < totalTags; i++) {
    if (memcmp(tags[i].uid, id, idSize) == 0) {
      Serial.print("授權使用者："); Serial.println(tags[i].name);
      foundTag = true;
      
      digitalWrite(valvePin, VALVE_ACTIVE);
      isValvePowered = true; 
      isLogicallyOpen = true; 
      valveOpenStartTime = currentMillis; 
      
      sendDataToGoogleSheet(lastMegaMsg, "1");
      break;
    }
  }

  if (!foundTag) Serial.println("無此人");
  
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
