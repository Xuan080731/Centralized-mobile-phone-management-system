#include "ChineseText.h"
#include "ChineseFont.h"

ChineseText::ChineseText(Adafruit_ILI9341* _tft) {
    tft = _tft;
    fgColor = ILI9341_WHITE;
    bgColor = ILI9341_BLACK;
    spacing = 2;
}

void ChineseText::setStyle(uint16_t fg, uint16_t bg, uint8_t gap) {
    fgColor = fg;
    bgColor = bg;
    spacing = gap;
}

void ChineseText::drawText(const uint16_t* codes, int len, int x, int y) {
    for (int i = 0; i < len; ++i) {
        drawWord(codes[i], x + i * (32 + spacing), y);
    }
}

void ChineseText::drawWord(uint16_t code, int x, int y) {
    const uint8_t* bmp = findBitmap(code);
    if (bmp) {
        tft->fillRect(x, y, 32, 32, bgColor);
        tft->drawBitmap(x, y, bmp, 32, 32, fgColor);
    }
}

void ChineseText::drawWordNoBG(uint16_t unicode, int x, int y, uint16_t color, int w, int h) {
  const uint8_t* bmp = findBitmap(unicode);
  if (bmp) {
    tft->drawBitmap(x, y, bmp, w, h, color);
  }
}

// 尋找字型 (從 wordTable 對照表中搜尋)
const uint8_t* ChineseText::findBitmap(uint16_t code) {
    // wordTable 定義在 ChineseFont.h 中
    for (int i = 0; i < wordCount; ++i) {
        if (wordTable[i].unicode == code)
            return wordTable[i].bitmap;
    }
    return nullptr; 
}

void ChineseText::drawUTF8Text(const char* utf8str, int x, int y, uint16_t fg, int ww, int dx) {
    int i = 0;
    int xpos = x;
    while (utf8str[i]) {
        if ((utf8str[i] & 0xF0) == 0xE0) {
            uint16_t code = ((utf8str[i] & 0x0F) << 12) |
                            ((utf8str[i + 1] & 0x3F) << 6) |
                            (utf8str[i + 2] & 0x3F);
            
            drawWordNoBG(code, xpos, y, fg, ww, 32);
            xpos += ww + dx; 
            i += 3; 
        } 
        else if ((utf8str[i] & 0x80) == 0) {
            tft->setCursor(xpos, y+8);
            tft->setTextSize(2);
            tft->setTextColor(fg);
            tft->print(utf8str[i]);
            xpos += 12; 
            i++;
        }
        else {
            i++;
        }
    }
}
