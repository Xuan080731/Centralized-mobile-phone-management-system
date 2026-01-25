#pragma once
#include <Adafruit_ILI9341.h>
#include <stdint.h>

class ChineseText {
public:
  ChineseText(Adafruit_ILI9341* display);
  
  void setStyle(uint16_t fg, uint16_t bg, uint8_t gap);
  void drawText(const uint16_t* codes, int len, int x, int y);
  void drawWord(uint16_t unicode, int x, int y);
  void drawWordNoBG(uint16_t unicode, int x, int y, uint16_t color, int w = 32, int h = 32);
  void drawUTF8Text(const char* text, int x, int y, uint16_t color, int w = 32, int dx = 0);

  const uint8_t* findBitmap(uint16_t unicode);

private:
  Adafruit_ILI9341* tft;
  uint16_t fgColor, bgColor;
  uint8_t spacing;
  
};
