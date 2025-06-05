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
    usleep(3000);  // Delay maior para garantir limpeza
    lcdSetCursor(0, 0);
    lcdPrint("Bem vindo!");
    lcdSetCursor(1, 0);
    lcdPrint("Pressione botao");

    cv::VideoCapture cap;

    bool sistemaAtivo = false;
    bool laserLigado = false;
    bool ultimoEstadoBotao = true;

    std::string ultimaCor = "";
    int r=0, g=0, b=0;

    while (true) {
        bool estadoBotao = digitalRead(PIN_BOTAO);

        // Detecta borda de descida (botao pressionado)
        if (ultimoEstadoBotao == HIGH && estadoBotao == LOW) {
            sistemaAtivo = !sistemaAtivo;

            if (sistemaAtivo) {
                lcdClear();
                usleep(3000);
                lcdSetCursor(0, 0);
                lcdPrint("Sistema iniciado");
                if (!openCamera(cap)) {
                    std::cerr << "Erro ao abrir camera\n";
                    lcdClear();
                    usleep(3000);
                    lcdSetCursor(0, 0);
                    lcdPrint("Erro camera");
                    sistemaAtivo = false;
                }
            } else {
                closeCamera(cap);
                digitalWrite(PIN_LASER, LOW);
                lcdClear();
                usleep(3000);
                lcdSetCursor(0, 0);
                lcdPrint("Sistema parado");
                lcdSetCursor(1, 0);
                lcdPrint("Pressione botao");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(300)); // Debounce e evitar mltiplas leituras
        }
        ultimoEstadoBotao = estadoBotao;

        if (sistemaAtivo) {
            // Pisca laser sem atrapalhar camera
            static auto lastBlink = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastBlink).count() > 200) {
                piscarLaser(laserLigado, 0);  // salterna o estado
                lastBlink = now;
            }

            std::string cor = processFrame(cap, r, g, b);

            if (!cor.empty() && cor != ultimaCor) {
                lcdClear();
                usleep(3000);  // Aguarde um pouco aps limpar
                lcdSetCursor(0, 0);
                lcdPrint("Cor: ");
                lcdPrint(cor);

                lcdSetCursor(1, 0);
                char rgbText[20];
                snprintf(rgbText, sizeof(rgbText), "R:%d G:%d B:%d", r, g, b);
                lcdPrint(rgbText);

                ultimaCor = cor;
            }
        } else {
            digitalWrite(PIN_LASER, LOW);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (cv::waitKey(1) == 27) break; // ESC para sair
    }

    closeCamera(cap);
    digitalWrite(PIN_LASER, LOW);
    return 0;
}
