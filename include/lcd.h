#ifndef LCD_H
#define LCD_H

#include <string>

void lcdInit();
void lcdClear();
void lcdSetCursor(int row, int col);
void lcdPrint(const std::string& text);

#endif