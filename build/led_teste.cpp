#include <wiringPi.h>
#include <iostream>
#include <unistd.h>

#define LED 3  // WiringPi 3 = GPIO22 = pino físico 15

int main() {
    if (wiringPiSetup() == -1) {
        std::cerr << "Erro ao iniciar WiringPi\n";
        return 1;
    }

    pinMode(LED, OUTPUT);

    std::cout << "Teste LED GPIO22 (WiringPi 3). Piscar 5 vezes.\n";

    for (int i = 0; i < 5; i++) {
        digitalWrite(LED, HIGH);  // LED ligado
        std::cout << "LED ON\n";
        usleep(500000);           // 500 ms
        digitalWrite(LED, LOW);   // LED desligado
        std::cout << "LED OFF\n";
        usleep(500000);           // 500 ms
    }

    std::cout << "Teste concluído.\n";

    return 0;
}
