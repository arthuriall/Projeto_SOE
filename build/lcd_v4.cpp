#include <iostream>
#include <opencv2/opencv.hpp>
#include <wiringPi.h>
#include <unistd.h>
#include <thread>
#include <atomic>

// === GPIO Definitions ===
#define RS    0
#define E     1
#define D4    4
#define D5    5
#define D6    6
#define D7    7
#define LED   3    // GPIO22 (WiringPi 3)
#define BOTAO 8    // GPIO8  (WiringPi 8)

// === Controle de Execução ===
std::atomic<bool> running(true);
std::atomic<bool> leituraAtiva(false);

// === LCD Functions ===
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
    sendNibble(data >> 4);
    sendNibble(data & 0x0F);
    usleep(100);
}

void lcdInit() {
    pinMode(RS, OUTPUT);
    pinMode(E, OUTPUT);
    pinMode(D4, OUTPUT);
    pinMode(D5, OUTPUT);
    pinMode(D6, OUTPUT);
    pinMode(D7, OUTPUT);

    usleep(50000);
    sendNibble(0x03); usleep(4500);
    sendNibble(0x03); usleep(4500);
    sendNibble(0x03); usleep(150);
    sendNibble(0x02);

    sendByte(0x28, 0); // 4-bit, 2 lines
    sendByte(0x0C, 0); // display ON
    sendByte(0x06, 0); // entry mode
    sendByte(0x01, 0); // clear display
    usleep(2000);
}

void lcdClear() {
    sendByte(0x01, 0);
    usleep(2000);
}

void lcdSetCursor(int row, int col) {
    int rowAddr[] = {0x00, 0x40};
    sendByte(0x80 + rowAddr[row] + col, 0);
}

void lcdPrint(const std::string& text) {
    for (char c : text) {
        sendByte(c, 1);
    }
}

// === Debounce para botão físico ===
bool esperaClique(int pin, int debounceDelay = 50) {
    while (digitalRead(pin) == HIGH) usleep(1000); // espera pressionar
    usleep(debounceDelay * 1000); // debounce

    if (digitalRead(pin) == LOW) {
        while (digitalRead(pin) == LOW) usleep(1000); // espera soltar
        usleep(debounceDelay * 1000);
        return true;
    }
    return false;
}

// === Detecta cor usando HSV ===
std::string detectColorHSV(int hue, int saturation, int value) {
    if (value < 50) return "Preto";          // Pouca luz
    if (saturation < 50) return "Cinza";     // Pouca saturação, quase cinza

    if (hue < 10 || hue > 160) return "Vermelho";
    if (hue >= 35 && hue <= 85) return "Verde";
    if (hue >= 90 && hue <= 130) return "Azul";

    return "Indefinido";
}

// === Processa frame e detecta cor via HSV ===
std::string processFrameHSV(cv::VideoCapture& cap, int& hue, int& saturation, int& value) {
    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) return "";

    int rectSize = 20;
    int x = frame.cols / 2;
    int y = frame.rows / 2;
    cv::Rect roi(x - rectSize / 2, y - rectSize / 2, rectSize, rectSize);

    cv::Mat region = frame(roi);

    cv::Mat regionHSV;
    cv::cvtColor(region, regionHSV, cv::COLOR_BGR2HSV);

    cv::Scalar meanHSV = cv::mean(regionHSV);

    hue = static_cast<int>(meanHSV[0]);
    saturation = static_cast<int>(meanHSV[1]);
    value = static_cast<int>(meanHSV[2]);

    return detectColorHSV(hue, saturation, value);
}

// === Thread de leitura de cor ===
void colorThread(cv::VideoCapture& cap) {
    while (running) {
        if (leituraAtiva) {
            int hue, sat, val;
            std::string cor = processFrameHSV(cap, hue, sat, val);

            lcdClear();
            lcdSetCursor(0, 0);
            lcdPrint("Cor: " + cor);
            lcdSetCursor(1, 0);
            char buffer[20];
            snprintf(buffer, sizeof(buffer), "H:%d S:%d V:%d", hue, sat, val);
            lcdPrint(buffer);

            digitalWrite(LED, HIGH);
            usleep(500000);
            digitalWrite(LED, LOW);
            usleep(500000);
        } else {
            lcdClear();
            lcdSetCursor(0, 0);
            lcdPrint("Leitura pausada");
            lcdSetCursor(1, 0);
            lcdPrint("Pressione botao");
            digitalWrite(LED, LOW);
            usleep(300000);
        }
    }
    digitalWrite(LED, LOW);
}

// === MAIN ===
int main() {
    std::cout << "Iniciando sistema...\n";

    if (wiringPiSetup() == -1) {
        std::cerr << "Erro ao iniciar WiringPi!\n";
        return 1;
    }

    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);

    pinMode(BOTAO, INPUT);
    pullUpDnControl(BOTAO, PUD_UP); // Pull-up interno ativado

    lcdInit();
    lcdClear();
    lcdPrint("Aguardando botao");
    lcdSetCursor(1, 0);
    lcdPrint("para iniciar");

    // Espera o primeiro clique para começar
    while (!esperaClique(BOTAO)) {
        usleep(100000);
    }
    leituraAtiva = true;

    lcdClear();
    lcdPrint("Camera inicializa");

    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        lcdClear();
        lcdPrint("Erro camera");
        std::cerr << "Erro ao abrir camera.\n";
        return 1;
    }

    lcdClear();
    lcdPrint("Camera OK");
    lcdSetCursor(1, 0);
    lcdPrint("botao=pause/resume");

    std::thread t(colorThread, std::ref(cap));

    // Loop principal aguarda o botão para pausar ou continuar a leitura
    while (running) {
        if (esperaClique(BOTAO)) {
            leituraAtiva = !leituraAtiva;  // alterna estado

            lcdClear();
            lcdSetCursor(0, 0);
            if (leituraAtiva) {
                lcdPrint("Leitura ATIVA");
            } else {
                lcdPrint("Leitura PAUSADA");
            }
            lcdSetCursor(1, 0);
            lcdPrint("Pressione botao");
            usleep(1000000);
        }
        usleep(100000);
    }

    running = false;
    t.join();

    lcdClear();
    lcdPrint("Encerrando...");
    cap.release();
    std::cout << "Programa finalizado.\n";
    return 0;
}
