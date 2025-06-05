#include "camera.h"
#include "lcd.h"
#include <wiringPi.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <unistd.h>


constexpr int PIN_BOTAO = 26;  // GPIO do boto
constexpr int PIN_LASER = 19;  // GPIO do laser

void piscarLaser(bool& ligado, int intervaloMs) {
    ligado = !ligado;
    digitalWrite(PIN_LASER, ligado ? HIGH : LOW);
    std::this_thread::sleep_for(std::chrono::milliseconds(intervaloMs));
}

constexpr int PIN_BOTAO = 26;  // GPIO do boto
constexpr int PIN_LASER = 19;  // GPIO do laser

void piscarLaser(bool& ligado, int intervaloMs) {
    ligado = !ligado;
    digitalWrite(PIN_LASER, ligado ? HIGH : LOW);
    std::this_thread::sleep_for(std::chrono::milliseconds(intervaloMs));
}

int main() {
    wiringPiSetupGpio();
    pinMode(PIN_BOTAO, INPUT);
    pullUpDnControl(PIN_BOTAO, PUD_UP);  // Ativa pull-up interno
    pinMode(PIN_LASER, OUTPUT);
    digitalWrite(PIN_LASER, LOW);

<<<<<<< HEAD
=======
    lcdInit();
    lcdClear();
    usleep(3000);  // Delay maior para garantir limpeza
>>>>>>> b28c11b5354bd1b9998e7111aa4de3d80c695df6
    lcdSetCursor(0, 0);
    lcdPrint("Bem vindo!");
    lcdSetCursor(1, 0);
    lcdPrint("Pressione botao");
<<<<<<< HEAD


    bool sistemaAtivo = false;
    bool laserLigado = false;
    bool ultimoEstadoBotao = true;

=======
>>>>>>> b28c11b5354bd1b9998e7111aa4de3d80c695df6

    cv::VideoCapture cap;

    bool sistemaAtivo = false;
    bool laserLigado = false;
    bool ultimoEstadoBotao = true;

    std::string ultimaCor = "";
    int r=0, g=0, b=0;


    while (true) {
        bool estadoBotao = digitalRead(PIN_BOTAO);

<<<<<<< HEAD
        if (!estadoBotao)//Botao pressionado
        {

        std::string cor = processFrame(cap);
        if (!cor.empty() && cor != ultimaCor) {
            lcdClear();
            lcdSetCursor(0, 0);
            lcdPrint("Cor detectada:");
            lcdSetCursor(1, 0);
            lcdPrint(cor);
          
          ultimaCor = cor;
        }

        if (cv::waitKey(1) == 27) break; // ESC
    closeCamera(cap);
    digitalWrite(PIN_LASER, LOW);
    return 0;
}
