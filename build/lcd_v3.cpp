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
#define BOTAO 8   // Pino físico 8 do Raspberry Pi (GPIO14) no WiringPi é 15

// === Controle de Execução ===
std::atomic<bool> running(true);
std::atomic<bool> lendo(true);

// === Protótipos ===
void pulseEnable();
void sendNibble(int data);
void sendByte(int data, int mode);
void lcdInit();
void lcdClear();
void lcdSetCursor(int row, int col);
void lcdPrint(const std::string& text);
bool esperaClique(int pin, int debounceDelay = 50);
std::string detectColorRGB(int r, int g, int b);
std::string processFrame(cv::VideoCapture& cap, int& r, int& g, int& b);
void colorThread(cv::VideoCapture& cap);

// === Implementação ===
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

// Debounce do botão
bool esperaClique(int pin, int debounceDelay) {
    while (digitalRead(pin) == HIGH) usleep(1000); // espera botão pressionado

    usleep(debounceDelay * 1000); // debounce

    if (digitalRead(pin) == LOW) {
        while (digitalRead(pin) == LOW) usleep(1000); // espera soltar
        usleep(debounceDelay * 1000);
        return true;
    }

    return false;
}

std::string detectColorRGB(int r, int g, int b) {
    if (r > g && r > b) return "Vermelho";
    if (g > r && g > b) return "Verde";
    if (b > r && b > g) return "Azul";
    return "Indefinido";
}

std::string processFrame(cv::VideoCapture& cap, int& r, int& g, int& b) {
    static std::string ultimaCor;

    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) return "";

    int rectSize = 20;
    int x = frame.cols / 2;
    int y = frame.rows / 2;
    cv::Rect roi(x - rectSize / 2, y - rectSize / 2, rectSize, rectSize);

    cv::Mat region = frame(roi);
    cv::Scalar meanColor = cv::mean(region);

    b = static_cast<int>(meanColor[0]);
    g = static_cast<int>(meanColor[1]);
    r = static_cast<int>(meanColor[2]);

    std::string cor = detectColorRGB(r, g, b);

    if (cor != ultimaCor) {
        std::cout << "Cor: " << cor << " R=" << r << " G=" << g << " B=" << b << "\n";
        ultimaCor = cor;
    }

    return cor;
}

void colorThread(cv::VideoCapture& cap) {
    while (running) {
        if (lendo) {
            int r, g, b;
            std::string cor = processFrame(cap, r, g, b);

            lcdClear();
            lcdSetCursor(0, 0);
            lcdPrint("Cor: " + cor);
            lcdSetCursor(1, 0);
            char buffer[16];
            snprintf(buffer, sizeof(buffer), "R:%d G:%d B:%d", r, g, b);
            lcdPrint(buffer);

            digitalWrite(LED, HIGH);
            usleep(500000);
            digitalWrite(LED, LOW);
            usleep(500000);
        } else {
            lcdClear();
            lcdSetCursor(0, 0);
            lcdPrint("Pausado...");
            lcdSetCursor(1, 0);
            lcdPrint("Pressione botao");
            digitalWrite(LED, LOW);
            usleep(500000);
        }
    }
    digitalWrite(LED, LOW);
}

int main() {
    std::cout << "Iniciando sistema...\n";

    if (wiringPiSetup() == -1) {
        std::cerr << "Erro ao iniciar WiringPi!\n";
        return 1;
    }

    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);

    pinMode(BOTAO, INPUT);
    pullUpDnControl(BOTAO, PUD_UP);

    lcdInit();
    lcdClear();
    lcdPrint("Aguardando");
    lcdSetCursor(1, 0);
    lcdPrint("botao...");

    while (!esperaClique(BOTAO)) {
        usleep(100000);
    }

    lcdClear();
    lcdPrint("Iniciando cam");

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
    lcdPrint("Pressione botao");

    std::thread t(colorThread, std::ref(cap));

    while (running) {
        if (esperaClique(BOTAO)) {
            lendo = !lendo;
            if (lendo) {
                lcdClear();
                lcdPrint("Leitura ON");
            } else {
                lcdClear();
                lcdPrint("Leitura OFF");
            }
            usleep(500000); // debounce visual
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
