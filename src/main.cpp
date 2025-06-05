#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>

#include <wiringPi.h>
#include <opencv2/opencv.hpp>

#include "botao.h"
#include "camera.h"
#include "laser.h"
#include "lcd.h"

// Variveis globais
std::atomic<bool> sistemaAtivo(false);
std::atomic<bool> exitProgram(false);
std::atomic<bool> botaoPressionado(false);

std::mutex mtxCor;
std::string corDetectada = "";

constexpr int PIN_BOTAO = 26;
constexpr int PIN_LASER = 19;

void threadBotao() {
    bool ultimoEstadoBotao = true;
    while (!exitProgram) {
        if (verificarPressionado(PIN_BOTAO, ultimoEstadoBotao)) {
            sistemaAtivo = !sistemaAtivo;
            botaoPressionado = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void threadCamera() {
    cv::VideoCapture cap;
    int r, g, b;
    while (!exitProgram) {
        if (sistemaAtivo) {
            if (!cap.isOpened()) {
                cap.open(0);
                if (!cap.isOpened()) {
                    std::cerr << "Erro ao abrir camera!" << std::endl;
                    sistemaAtivo = false;
                    continue;
                }
            }
            std::string cor = processFrame(cap, r, g, b);
            if (!cor.empty()) {
                std::lock_guard<std::mutex> lock(mtxCor);
                if (cor != corDetectada) {
                    corDetectada = cor;
                    botaoPressionado = true;
                }
            }
        } else {
            if (cap.isOpened()) {
                cap.release();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    if (cap.isOpened()) cap.release();
}

void threadLaser() {
    constexpr int intervaloPiscarMs = 500;
    bool estadoLaser = false;  // Varivel para controlar o estado do laser

    while (!exitProgram) {
        if (sistemaAtivo) {
            alternarLaser(PIN_LASER, estadoLaser, intervaloPiscarMs);
        } else {
            desligarLaser(PIN_LASER);
            estadoLaser = false;  // Resetar estado quando inativo
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    desligarLaser(PIN_LASER);
}

void threadLCD() {
    while (!exitProgram) {
        if (botaoPressionado) {
            lcdClear();
            if (sistemaAtivo) {
                lcdSetCursor(0, 0);
                lcdPrint("Sistema ativo");

                std::lock_guard<std::mutex> lock(mtxCor);
                lcdSetCursor(1, 0);
                if (!corDetectada.empty()) {
                    lcdPrint("Cor:");
                    lcdSetCursor(1, 4);
                    lcdPrint(corDetectada);
                } else {
                    lcdPrint("Aguardando cor");
                }
            } else {
                lcdSetCursor(0, 0);
                lcdPrint("Sistema inativo");
                lcdSetCursor(1, 0);
                lcdPrint("Pressione botao");
            }
            botaoPressionado = false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    wiringPiSetupGpio();
    configurarBotao(PIN_BOTAO);
    configurarLaser(PIN_LASER);
    lcdInit();
    lcdClear();

    lcdSetCursor(0, 0);
    lcdPrint("Bem vindo!");
    lcdSetCursor(1, 0);
    lcdPrint("Pressione botao");

    std::thread tBotao(threadBotao);
    std::thread tCamera(threadCamera);
    std::thread tLaser(threadLaser);
    std::thread tLCD(threadLCD);

    while (true) {
        int key = cv::waitKey(1);
        if (key == 27) {  // ESC para sair
            exitProgram = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    tBotao.join();
    tCamera.join();
    tLaser.join();
    tLCD.join();

    return 0;
}
