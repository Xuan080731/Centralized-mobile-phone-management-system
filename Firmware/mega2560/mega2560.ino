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

void setup() {
  for(int i=0; i<N; i++) {
    pinMode(pPort[i], INPUT); 
  }
  
  Serial.begin(115200);
  Serial1.begin(9600);
  Serial.println("Mega 2560 就緒 (含放開歸零功能)");
}

void loop() {
  String v = "";
  
  for (uint8_t i = 0; i < N; ++i) {
    if (digitalRead(pPort[i]) == LOW) { 
      v += String(i + 1) + ","; 
    }
  }

  if (v.length() > 0) {
    v.remove(v.length() - 1);
  }

  if (v != lastSentState) {
    
    lastSentState = v;

    if (v.length() > 0) {
      Serial1.println(v);        
      Serial.println("狀態改變，已傳送: " + v);
    } 
    else {
      Serial1.println("0"); 
      Serial.println("按鈕已全部放開 (傳送歸零訊號 0)");
    }
    
    digitalWrite(13, HIGH);
    delay(10);
    digitalWrite(13, LOW);
  }

  delay(50); 
}
