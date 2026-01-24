#include "ChineseText.h"
#include "ChineseFont.h" // 引入字型檔 (這裡面有 wordTable)

// 建構函式
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

// 繪製一串中文字 (Unicode 陣列)
void ChineseText::drawText(const uint16_t* codes, int len, int x, int y) {
    for (int i = 0; i < len; ++i) {
        drawWord(codes[i], x + i * (32 + spacing), y);
    }
}

// 繪製單字 (有背景，會蓋掉底色)
void ChineseText::drawWord(uint16_t code, int x, int y) {
    const uint8_t* bmp = findBitmap(code);
    if (bmp) {
        tft->fillRect(x, y, 32, 32, bgColor);
        tft->drawBitmap(x, y, bmp, 32, 32, fgColor);
    }
}

// 繪製單字 (無背景，透明)
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
    return nullptr; // 找不到字
}

// ★★★ 繪製 UTF-8 字串 (用於顯示名字) ★★★
// 這段程式碼負責將 "李政諺" 這種文字轉成字碼並畫出來
void ChineseText::drawUTF8Text(const char* utf8str, int x, int y, uint16_t fg, int ww, int dx) {
    int i = 0;
    int xpos = x;
    while (utf8str[i]) {
        // 判斷是否為中文字 (UTF-8 3 bytes: 0xE0開頭)
        if ((utf8str[i] & 0xF0) == 0xE0) {
            // 將 3 bytes UTF-8 轉為 16-bit Unicode
            uint16_t code = ((utf8str[i] & 0x0F) << 12) |
                            ((utf8str[i + 1] & 0x3F) << 6) |
                            (utf8str[i + 2] & 0x3F);
            
            // 繪製這個字
            drawWordNoBG(code, xpos, y, fg, ww, 32);
            xpos += ww + dx; // 移動游標
            i += 3;          // 跳過 3 bytes
        } 
        // 判斷是否為一般 ASCII (英文/數字)
        else if ((utf8str[i] & 0x80) == 0) {
            // 這裡暫時不處理英文 (若有需要可用 tft->print)
            // 為了版面整齊，我們只跳過或簡單畫個空格
            tft->setCursor(xpos, y+8);
            tft->setTextSize(2);
            tft->setTextColor(fg);
            tft->print(utf8str[i]);
            xpos += 12; 
            i++;
        }
        else {
            i++; // 其他編碼直接略過
        }
    }
}
