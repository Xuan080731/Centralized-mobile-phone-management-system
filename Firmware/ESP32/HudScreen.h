#ifndef HUD_SCREEN_H
#define HUD_SCREEN_H

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "ChineseText.h"

class HudScreen {
public:
  HudScreen(Adafruit_ILI9341* display)
    : tft(display), cText(display) { 
    colBG    = tft->color565(0, 70, 110);     
    colRed   = ILI9341_RED; 
    colGreen = ILI9341_GREEN;
    colText  = tft->color565(230, 242, 255); 
    colValue = tft->color565(255, 114, 198); 
    colHum   = tft->color565(0, 255, 255); 
    colTime  = tft->color565(250, 204,  21);
  }

  void showBootConnecting() {
    drawBackground();
    uint16_t text[] = {0x958B, 0x6A5F, 0x9023, 0x7DDA}; 
    drawBigText2x(text, 4, ILI9341_WHITE); 
  }

  void showBootSuccess() {
    drawBackground();
    uint16_t text[] = {0x9023, 0x7DDA, 0x6210, 0x529F}; 
    drawBigText2x(text, 4, ILI9341_GREEN); 
  }

  void showBootFailed() {
    drawBackground();
    uint16_t text[] = {0x9023, 0x7DDA, 0x5931, 0x6557}; 
    drawBigText2x(text, 4, ILI9341_RED); 
  }

  void showMain() {
    drawBackground();
    drawDots();                             
    drawStatus(0.0, 0.0, "--:--", "--/--/--"); 
    updateLastUser("Waiting...", "----"); 
  }

  void updateCircles(String data) {
    for(int i=1; i<=20; i++) updateSingleCircle(i, false); 
    if (data == "0" || data == "") return;

    int startIndex = 0;
    int endIndex = data.indexOf(',');
    while (endIndex != -1) {
      String numStr = data.substring(startIndex, endIndex);
      int num = numStr.toInt();
      if (num >= 1 && num <= 20) updateSingleCircle(num, true); 
      startIndex = endIndex + 1;
      endIndex = data.indexOf(',', startIndex);
    }
    String lastNumStr = data.substring(startIndex);
    int num = lastNumStr.toInt();
    if (num >= 1 && num <= 20) updateSingleCircle(num, true); 
  }

  void updateTimeTemp(float temp, float hum, const char* timeStr, const char* dateStr) {
      drawStatus(temp, hum, timeStr, dateStr);
  }

  void updateLastUser(String name, String uid) {
      tft->fillRect(10, 135, 300, 55, colBG);
      
      tft->setTextSize(2);
      tft->setCursor(20, 140);
      tft->setTextColor(ILI9341_CYAN, colBG);
      tft->print("Auth: ");
      
      tft->setTextColor(ILI9341_WHITE, colBG);
      tft->print(name);

      tft->setCursor(20, 165);
      tft->setTextColor(ILI9341_YELLOW, colBG);
      tft->print("UID : ");
      tft->print(uid);
  }

private:
  Adafruit_ILI9341* tft;
  ChineseText cText;
  uint16_t colBG, colRed, colGreen, colText, colValue, colHum, colTime;

  void drawBackground() {
    tft->fillScreen(colBG);
    uint16_t border = tft->color565(30, 64, 120);
    tft->drawRoundRect(2, 2, 316, 236, 18, border);
  }

  void drawBigText2x(uint16_t* text, int len, uint16_t color) {
    int charWidth = 64; 
    int totalW = len * (charWidth + 4);
    int startX = (320 - totalW) / 2;
    int startY = (240 - 64) / 2;

    for(int i=0; i<len; i++) {
        const uint8_t* bmp = cText.findBitmap(text[i]);
        if(bmp) {
            for(int y=0; y<32; y++) {
                for(int x=0; x<4; x++) { 
                    uint8_t b = pgm_read_byte(&bmp[y*4 + x]);
                    for(int bit=0; bit<8; bit++) {
                        if(b & (0x80 >> bit)) {
                            tft->fillRect(startX + (i*(charWidth+4)) + (x*8+bit)*2, startY + y*2, 2, 2, color);
                        }
                    }
                }
            }
        }
    }
  }

  void drawDots() {
    tft->setTextSize(1);
    tft->setTextColor(colRed, colBG); 
    int radius = 10; int startX = 35; int spacing = 28; 
    int y1 = 45; int y2 = 80; 
    for (int i = 0; i < 10; ++i) drawCircleNumber(startX + i * spacing, y1, radius, i + 1);
    for (int i = 0; i < 10; ++i) drawCircleNumber(startX + i * spacing, y2, radius, i + 11);
  }

  void drawCircleNumber(int cx, int cy, int r, int num) {
    tft->drawCircle(cx, cy, r, colRed); 
    int x = cx - (num < 10 ? 3 : 6);
    int y = cy - 4;
    tft->setTextColor(colRed, colBG);
    tft->setCursor(x, y);
    tft->print(num);
  }

  void updateSingleCircle(int i, bool isOccupied) {
    int radius = 10; int startX = 35; int spacing = 28; 
    int y1 = 45; int y2 = 80;
    int row = (i - 1) / 10; int col = (i - 1) % 10;
    int x = startX + col * spacing; int y = (row == 0) ? y1 : y2;
    
    uint16_t color = isOccupied ? colGreen : colRed;
    
    if (isOccupied) {
        tft->fillCircle(x, y, radius, color); 
        tft->setTextColor(colBG); 
    } else {
        tft->fillCircle(x, y, radius, colBG); 
        tft->drawCircle(x, y, radius, color); 
        tft->setTextColor(color, colBG); 
    }
    
    tft->setTextSize(1);
    int tx = x - (i < 10 ? 3 : 6); int ty = y - 4; 
    tft->setCursor(tx, ty); 
    tft->print(i);
  }

  void drawStatus(float temp, float hum, const char* timeStr, const char* dateStr) {
    int baseY = 205; 
    tft->fillRect(0, baseY - 5, 320, 35, colBG);

    tft->setTextSize(2); 

    tft->setCursor(2, baseY); 
    tft->setTextColor(colText, colBG);
    tft->print(dateStr); 

    tft->setCursor(126, baseY); 
    tft->setTextColor(colTime, colBG);
    tft->print(timeStr); 

    tft->fillCircle(196, baseY + 7, 4, ILI9341_YELLOW); 
    
    tft->setCursor(205, baseY);
    tft->setTextColor(ILI9341_YELLOW, colBG); 
    
    if (isnan(temp)) {
       tft->print("Err"); 
    } else {
       tft->print((int)temp); 
       tft->print((char)247); 
       tft->print("C");       
    }

    tft->fillCircle(275, baseY + 7, 4, ILI9341_CYAN); 
    
    tft->setCursor(284, baseY);
    tft->setTextColor(colHum, colBG); 
    
    if (isnan(hum)) {
       tft->print("--");
    } else {
       tft->print((int)hum); 
       tft->print("%");
    }
  }
};

#endif
