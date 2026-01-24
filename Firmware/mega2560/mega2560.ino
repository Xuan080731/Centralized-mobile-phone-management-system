#include <Arduino.h>

// 定義 1~20 號對應的實體腳位 (雙號對應1-10，單號對應11-20)
const uint8_t pPort[] = {
  // 1~10 號 (雙號腳)
  28, 30, 32, 34, 36, 38, 40, 42, 44, 46,
  // 11~20 號 (單號腳)
  29, 31, 33, 35, 37, 39, 41, 43, 45, 47
};
const uint8_t N = sizeof(pPort) / sizeof(pPort[0]);

String lastSentState = "";
unsigned long lastForceSendTime = 0; // ★ 新增：用來計時的變數

void setup() {
  // 設定所有腳位為輸入模式
  for(int i=0; i<N; i++) {
    pinMode(pPort[i], INPUT);
  }
  
  pinMode(13, OUTPUT); // 內建 LED
  Serial.begin(115200); // 電腦監控用
  Serial1.begin(9600);  // 與 ESP32 通訊用
  Serial.println("Mega 2560 就緒 (含每秒自動更新功能)");
}

void loop() {
  String v = "";
  
  // 快速掃描 20 個腳位
  for (uint8_t i = 0; i < N; ++i) {
    if (digitalRead(pPort[i]) == LOW) { 
      v += String(i + 1) + ",";
    }
  }

  if (v.length() > 0) {
    v.remove(v.length() - 1);
  }

  unsigned long currentMillis = millis();

  // ★★★ 關鍵修改：除了狀態改變，每 1000ms (1秒) 也強制傳送一次 ★★★
  // 這樣 ESP32 剛開機或斷線重連後，馬上就能收到最新狀態
  if (v != lastSentState || (currentMillis - lastForceSendTime > 1000)) {
    
    // 更新記錄
    lastSentState = v;
    lastForceSendTime = currentMillis; // 重置計時器

    if (v.length() > 0) {
      Serial1.println(v);
      Serial.println("傳送: " + v);
    } else {
      Serial1.println("0");
      Serial.println("傳送: 0 (全空)");
    }
    
    // 閃燈提示有傳送資料
    digitalWrite(13, HIGH);
    delay(10);
    digitalWrite(13, LOW);
  }

  delay(50); // 掃描頻率
}