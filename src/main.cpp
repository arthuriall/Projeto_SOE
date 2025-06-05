
// main.cpp
#include "camera.h"
#include "lcd.h"
#include <wiringPi.h>
#include <opencv2/opencv.hpp>
#include <iostream>

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

    lcdSetCursor(0, 0);
    lcdPrint("Bem vindo!");
    lcdSetCursor(1, 0);
    lcdPrint("Pressione botao");


    bool sistemaAtivo = false;
    bool laserLigado = false;
    bool ultimoEstadoBotao = true;


    cv::VideoCapture cap;
    if (!openCamera(cap)) {
        std::cerr << "Erro ao abrir a camera\n";
        return -1;
    }

    std::string ultimaCor = "";


    while (true) {
        bool estadoBotao = digitalRead(PIN_BOTAO);

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

    }

    closeCamera(cap);
    return 0;
}
