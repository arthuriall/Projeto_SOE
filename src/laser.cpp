#include "laser.h"
#include <wiringPi.h>
#include <chrono>
#include <thread>

void configurarLaser(int pino) {
    pinMode(pino, OUTPUT);
    digitalWrite(pino, LOW);
}

void ligarLaser(int pino) {
    digitalWrite(pino, HIGH);
}

void desligarLaser(int pino) {
    digitalWrite(pino, LOW);
}

void alternarLaser(int pino, bool& estado, int intervaloMs) {
    estado = !estado;
    digitalWrite(pino, estado ? HIGH : LOW);
    std::this_thread::sleep_for(std::chrono::milliseconds(intervaloMs));
}
