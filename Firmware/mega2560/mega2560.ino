#include <Arduino.h>

// 定義 1~20 號對應的實體腳位 (雙號對應1-10，單號對應11-20)
const uint8_t pPort[] = {
  // 1~10 號 (雙號腳)
  28, 30, 32, 34, 36, 38, 40, 42, 44, 46,
  // 11~20 號 (單號腳)
  29, 31, 33, 35, 37, 39, 41, 43, 45, 47
};

const uint8_t N = sizeof(pPort) / sizeof(pPort[0]);

// 用來記錄上一次傳送的狀態，避免重複傳送
String lastSentState = "";

void setup() {
  // 設定所有腳位為輸入模式
  for(int i=0; i<N; i++) {
    pinMode(pPort[i], INPUT); 
  }
  
  Serial.begin(115200); // 電腦監控用
  Serial1.begin(9600);  // 與 ESP32 通訊用 (TX1=18, RX1=19)
  Serial.println("Mega 2560 就緒 (含放開歸零功能)");
}

void loop() {
  String v = "";
  
  // 快速掃描 20 個腳位
  for (uint8_t i = 0; i < N; ++i) {
    // 假設按下按鈕是 LOW (接地)
    if (digitalRead(pPort[i]) == LOW) { 
      v += String(i + 1) + ","; 
    }
  }

  if (v.length() > 0) {
    v.remove(v.length() - 1);
  }

  // 只有當「狀態改變」時才傳送
  if (v != lastSentState) {
    
    // 更新記錄
    lastSentState = v;

    // 如果 v 有長度，代表有按鈕被按下
    if (v.length() > 0) {
      Serial1.println(v);        // 傳送按鈕編號 (例如 "1,5")
      Serial.println("狀態改變，已傳送: " + v);
    } 
    // 如果 v 是空的，代表全部放開了
    else {
      // 當全部放開時，傳送 "0" 給 ESP32
      // 這樣 Google Apps Script 收到 0，就會把所有欄位顯示為 No
      Serial1.println("0"); 
      Serial.println("按鈕已全部放開 (傳送歸零訊號 0)");
    }
    
    // 稍微閃一下燈表示有動作
    digitalWrite(13, HIGH);
    delay(10);
    digitalWrite(13, LOW);
  }

  delay(50); 
}
