#include <wiringPi.h>
#include <unistd.h> // para usleep
#include <iostream>
#include <string>

// Definindo os pinos com WiringPi
#define RS 0
#define E  1
#define D4 4
#define D5 5
#define D6 6
#define D7 7

void pulseEnable() {
    digitalWrite(E, HIGH);
    usleep(500);
    digitalWrite(E, LOW);
    usleep(500);
}

void sendNibble(int data) {
    digitalWrite(D4, (data >> 0) & 1);
    digitalWrite(D5, (data >> 1) & 1);
    digitalWrite(D6, (data >> 2) & 1);
    digitalWrite(D7, (data >> 3) & 1);
    pulseEnable();
}

void sendByte(int data, int mode) {
    digitalWrite(RS, mode);
    sendNibble(data >> 4);    // parte alta
    sendNibble(data & 0x0F);  // parte baixa
    usleep(100);
}

void lcdInit() {
    pinMode(RS, OUTPUT);
    pinMode(E, OUTPUT);
    pinMode(D4, OUTPUT);
    pinMode(D5, OUTPUT);
    pinMode(D6, OUTPUT);
    pinMode(D7, OUTPUT);

    usleep(50000); // espera inicial

    sendNibble(0x03);
    usleep(4500);
    sendNibble(0x03);
    usleep(4500);
    sendNibble(0x03);
    sendNibble(0x02); // modo 4 bits

    sendByte(0x28, 0); // 4 bits, 2 linhas, fonte 5x8
    sendByte(0x0C, 0); // Display ON, cursor OFF
    sendByte(0x06, 0); // Incrementa e não desloca
    sendByte(0x01, 0); // Limpa tela
    usleep(2000);
}

void lcdClear() {
    sendByte(0x01, 0);
    usleep(2000);
}

void lcdSetCursor(int row, int col) {
    int rowAddr[] = {0x00, 0x40, 0x14, 0x54};
    sendByte(0x80 + rowAddr[row] + col, 0);
}

void lcdPrint(const std::string& text) {
    for (char c : text) {
        sendByte(c, 1);
    }
}

int main() {
    wiringPiSetup();  // Inicializa WiringPi
    lcdInit();

    lcdSetCursor(0, 0);
    lcdPrint("Te amo");

    lcdSetCursor(1, 0);
    lcdPrint("Pikomon s2");

    return 0;
}
