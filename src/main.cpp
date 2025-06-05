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

int main() {
    wiringPiSetupGpio();
    pinMode(PIN_BOTAO, INPUT);
    pullUpDnControl(PIN_BOTAO, PUD_UP);  // Ativa pull-up interno
    pinMode(PIN_LASER, OUTPUT);
    digitalWrite(PIN_LASER, LOW);

    lcdInit();
    lcdClear();
    usleep(3000);  // Delay para garantir limpeza
    lcdSetCursor(0, 0);
    lcdPrint("Bem vindo!");
    lcdSetCursor(1, 0);
    lcdPrint("Pressione botao");

    bool sistemaAtivo = false;
    bool laserLigado = false;
    bool ultimoEstadoBotao = true;

    cv::VideoCapture cap;

    int r = 0, g = 0, b = 0;
    std::string ultimaCor = "";

    while (true) {
        bool estadoBotao = digitalRead(PIN_BOTAO);

        // Detecta borda de descida (boto pressionado)
        if (ultimoEstadoBotao == HIGH && estadoBotao == LOW) {
            sistemaAtivo = !sistemaAtivo;  // Alterna estado do sistema

            if (sistemaAtivo) {
                cap.open(0);  // Abre a cmera
                if (!cap.isOpened()) {
                    std::cerr << "Erro ao abrir a cmera!" << std::endl;
                    lcdClear();
                    lcdPrint("Erro camera!");
                    sistemaAtivo = false;
                } else {
                    lcdClear();
                    lcdPrint("Sistema ativo");
                    digitalWrite(PIN_LASER, HIGH);
                }
            } else {
                cap.release();
                digitalWrite(PIN_LASER, LOW);
                lcdClear();
                lcdPrint("Sistema inativo");
            }
        }

        ultimoEstadoBotao = estadoBotao;

        if (sistemaAtivo) {
            std::string cor = processFrame(cap, r, g, b);
            if (!cor.empty() && cor != ultimaCor) {
                lcdClear();
                lcdSetCursor(0, 0);
                lcdPrint("Cor detectada:");
                lcdSetCursor(1, 0);
                lcdPrint(cor);
                ultimaCor = cor;
            }
        }

        if (cv::waitKey(1) == 27) break;  // Sai se ESC for pressionado
        std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Pequeno delay para no usar CPU 100%
    }

    if (cap.isOpened()) {
        cap.release();
    }
    digitalWrite(PIN_LASER, LOW);

    return 0;
}
