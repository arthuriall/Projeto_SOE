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

// Variáveis globais
std::atomic<bool> sistemaAtivo(false);
std::atomic<bool> exitProgram(false);
std::atomic<bool> botaoPressionado(false);

std::mutex mtxCor;
std::string corDetectada = "";

constexpr int PIN_BOTAO = 26;
constexpr int PIN_LASER = 19;

void threadBotao() {
    bool ultimoEstadoBotao = true;
    std::cout << "[BOTAO] Thread iniciada" << std::endl;
    while (!exitProgram) {
        if (verificarPressionado(PIN_BOTAO, ultimoEstadoBotao)) {
            sistemaAtivo = !sistemaAtivo;
            botaoPressionado = true;
            std::cout << "[BOTAO] Botao pressionado, sistemaAtivo = " << sistemaAtivo << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::cout << "[BOTAO] Thread finalizada" << std::endl;
}
void threadCamera() {
    cv::VideoCapture cap;
    int r, g, b;
    std::cout << "[CAMERA] Thread iniciada" << std::endl;

    while (!exitProgram) {
        if (sistemaAtivo) {
            if (!cap.isOpened()) {
                cap.open(0);
                if (!cap.isOpened()) {
                    std::cerr << "[CAMERA] Erro ao abrir camera!" << std::endl;
                    sistemaAtivo = false;
                    continue;
                } else {
                    std::cout << "[CAMERA] Camera aberta com sucesso" << std::endl;
                }
            }

            cv::Mat frame;
            cap >> frame;
            if (frame.empty()) {
                std::cerr << "[CAMERA] Frame vazio!" << std::endl;
                continue;
            }

            int rectSize = 20;
            int x = frame.cols / 2;
            int y = frame.rows / 2;
            cv::Rect roi(x - rectSize / 2, y - rectSize / 2, rectSize, rectSize);

            cv::Mat region = frame(roi);
            cv::Scalar meanColor = cv::mean(region);

            b = static_cast<int>(meanColor[0]);
            g = static_cast<int>(meanColor[1]);
            r = static_cast<int>(meanColor[2]);

            std::string cor = detectColorRGB(cv::Vec3b(b, g, r));

            {
                std::lock_guard<std::mutex> lock(mtxCor);
                if (cor != corDetectada) {
                    corDetectada = cor;
                    botaoPressionado = true;
                    std::cout << "[CAMERA] Cor detectada atualizada: " << cor << std::endl;
                }
            }

            // Desenhar retngulo na regio central e texto com cor detectada
            cv::rectangle(frame, roi, cv::Scalar(255, 0, 0), 2);
            cv::putText(frame, cor, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0,
                        cv::Scalar(255, 255, 255), 2);

            // Redimensionar para 320x240 antes de mostrar
            cv::Mat resizedFrame;
            cv::Size tamanho(320, 240);
            cv::resize(frame, resizedFrame, tamanho);

            cv::imshow("Camera", resizedFrame);
            cv::waitKey(1);  // para atualizar a imagem na janela
        } else {
            if (cap.isOpened()) {
                cap.release();
                cv::destroyWindow("Camera");
                std::cout << "[CAMERA] Camera fechada" << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    if (cap.isOpened()) {
        cap.release();
        cv::destroyWindow("Camera");
    }
    std::cout << "[CAMERA] Thread finalizada" << std::endl;
}


void threadLaser() {
    constexpr int intervaloPiscarMs = 500;
    bool estadoLaser = false;  // varivel para manter o estado do laser

    while (!exitProgram) {
        if (sistemaAtivo) {
            alternarLaser(PIN_LASER, estadoLaser, intervaloPiscarMs);
        } else {
            desligarLaser(PIN_LASER);
            estadoLaser = false;  // garante que o estado est resetado quando inativo
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    desligarLaser(PIN_LASER);
}


void threadLCD() {
    std::cout << "[LCD] Thread iniciada" << std::endl;
    while (!exitProgram) {
        if (botaoPressionado) {
            lcdClear();
            if (sistemaAtivo) {
                lcdSetCursor(0, 0);
                lcdPrint("Sistema ativo");
                lcdSetCursor(1, 0);
                std::lock_guard<std::mutex> lock(mtxCor);
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
            std::cout << "[LCD] Display atualizado" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "[LCD] Thread finalizada" << std::endl;
}

int main() {
    std::cout << "[MAIN] Inicializando wiringPi" << std::endl;
    wiringPiSetupGpio();

    std::cout << "[MAIN] Configurando botao e laser" << std::endl;
    configurarBotao(PIN_BOTAO);
    configurarLaser(PIN_LASER);

    std::cout << "[MAIN] Inicializando LCD" << std::endl;
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

    std::cout << "[MAIN] Entrando no loop principal (ESC para sair)" << std::endl;

    while (true) {
        int key = cv::waitKey(1);
        if (key == 27) {  // ESC
            std::cout << "[MAIN] ESC pressionado, finalizando..." << std::endl;
            exitProgram = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    tBotao.join();
    tCamera.join();
    tLaser.join();
    tLCD.join();

    std::cout << "[MAIN] Programa finalizado com sucesso" << std::endl;
    return 0;
}
