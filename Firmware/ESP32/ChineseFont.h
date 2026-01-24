#ifndef CHINESE_FONT_H
#define CHINESE_FONT_H

#include <stdint.h>
#include <pgmspace.h>
#include "tWord.h"

struct ChineseGlyph {
  uint16_t unicode;
  const uint8_t* bitmap;
};

const ChineseGlyph wordTable[] = {
  // 原有字
  { 0x958B, CH_U958B_32x32 }, // 開
  { 0x6A5F, CH_U6A5F_32x32 }, // 機
  { 0x9023, CH_U9023_32x32 }, // 連
  { 0x7DDA, CH_U7DDA_32x32 }, // 線
  { 0x4E2D, CH_U4E2D_32x32 }, // 中
  { 0x6210, CH_U6210_32x32 }, // 成
  { 0x529F, CH_U529F_32x32 }, // 功
  { 0x5931, CH_U5931_32x32 }, // 失
  { 0x6557, CH_U6557_32x32 }, // 敗
  { 0x8CC7, CH_U8CC7_32x32 }, // 資
  { 0x6599, CH_U6599_32x32 }, // 料
  { 0x5EAB, CH_U5EAB_32x32 }, // 庫

  // 新增字 (人名與授權)
  { 0x674E, CH_U674E_32x32 }, // 李
  { 0x653F, CH_U653F_32x32 }, // 政
  { 0x8AFA, CH_U8Afa_32x32 }, // 諺
  { 0x55B5, CH_U55B5_32x32 }, // 喵
  { 0x6D77, CH_U6D77_32x32 }, // 海
  { 0x9752, CH_U9752_32x32 }, // 青
  { 0x5DE5, CH_U5DE5_32x32 }, // 工
  { 0x5546, CH_U5546_32x32 }, // 商
  { 0x8A0A, CH_U8A0A_32x32 }, // 訊
  { 0x79D1, CH_U79D1_32x32 }, // 科
  { 0x6388, CH_U6388_32x32 }, // 授
  { 0x6B0A, CH_U6B0A_32x32 }  // 權
};

const int wordCount = sizeof(wordTable) / sizeof(wordTable[0]);

#endif
