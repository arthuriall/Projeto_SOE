#include <iostream>
#include <opencv2/opencv.hpp>
#include <wiringPi.h>
#include <unistd.h>
#include <thread>
#include <atomic>

// === LCD Pins (WiringPi) ===
#define RS 0
#define E  1
#define D4 4
#define D5 5
#define D6 6
#define D7 7

// === LED ===
#define LED 3  // GPIO22 (WiringPi 3)

// === LCD ===
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

// === Cor RGB → Nome ===
std::string detectColorRGB(int r, int g, int b) {
    if (r > g && r < b ) return "Vermelho";
    if (g > r && g < b ) return "Verde";
    if (b > r && b < g ) return "Azul";
    return "Indefinido";
}

// === Processa Frame e Detecta Cor ===
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

// === Thread: Detecta Cor e Pisca LED ===
std::atomic<bool> running(true);

void colorThread(cv::VideoCapture& cap) {
    while (running) {
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
    }
    digitalWrite(LED, LOW); // Garante LED desligado
}

// === Main ===
int main() {
    std::cout << "Iniciando sistema...\n";

    if (wiringPiSetup() == -1) {
        std::cerr << "Erro ao iniciar WiringPi!\n";
        return 1;
    }

    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);

    lcdInit();
    lcdClear();
    lcdPrint("Iniciando camera");

    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        lcdClear();
        lcdPrint("Erro camera");
        std::cerr << "Erro ao abrir camera.\n";
        return 1;
    }

    lcdClear();
    lcdPrint("Camera OK");

    std::thread t(colorThread, std::ref(cap));

    std::cout << "Pressione ENTER para sair.\n";
    std::cin.get(); // Espera ENTER

    running = false;
    t.join();

    lcdClear();
    lcdPrint("Encerrando...");
    cap.release();
    std::cout << "Programa finalizado.\n";
    return 0;
}
