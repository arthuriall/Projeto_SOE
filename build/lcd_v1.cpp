#include <iostream>
#include <opencv2/opencv.hpp>
#include <wiringPi.h>
#include <unistd.h>
#include <thread>
#include <atomic>

// === LCD Definitions ===
#define RS 0
#define E  1
#define D4 4
#define D5 5
#define D6 6
#define D7 7

// === Botão ===
#define BOTAO 2  // GPIO27 (WiringPi 2)

// === Controle do loop principal ===
std::atomic<bool> executando(false);

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

std::string detectColorRGB(int r, int g, int b) {
    if (r > 200 && g < 80 && b < 80) return "Vermelho";
    if (g > 200 && r < 80 && b < 80) return "Verde";
    if (b > 200 && r < 80 && g < 80) return "Azul";
    if (r > 200 && g > 200 && b < 80) return "Amarelo";
    if (r > 200 && g > 100 && b > 100) return "Rosa";
    if (r < 50 && g < 50 && b < 50) return "Preto";
    if (r > 200 && g > 200 && b > 200) return "Branco";
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
    while (executando) {
        int r, g, b;
        std::string cor = processFrame(cap, r, g, b);

        lcdClear();
        lcdSetCursor(0, 0);
        lcdPrint("Cor: " + cor);
        lcdSetCursor(1, 0);
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "R:%d G:%d B:%d", r, g, b);
        lcdPrint(buffer);

        usleep(500000); // 0.5s
    }
}

int main() {
    std::cout << "Iniciando sistema...\n";

    if (wiringPiSetup() == -1) {
        std::cerr << "Erro ao iniciar WiringPi!\n";
        return 1;
    }

    pinMode(BOTAO, INPUT);
    pullUpDnControl(BOTAO, PUD_DOWN);  // Usa resistor pull-down

    lcdInit();
    lcdClear();
    lcdPrint("Sistema pronto");

    cv::VideoCapture cap(0);
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    if (!cap.isOpened()) {
        lcdClear();
        lcdPrint("Erro na camera");
        return 1;
    }

    std::thread t;

    while (true) {
        if (digitalRead(BOTAO) == HIGH) {
            if (!executando) {
                lcdClear();
                lcdPrint("Detectando...");
                executando = true;
                t = std::thread(colorThread, std::ref(cap));
            } else {
                executando = false;
                if (t.joinable()) t.join();
                lcdClear();
                lcdPrint("Pausado");
            }
            delay(1000);  // Debounce
        }

        delay(100);
    }

    cap.release();
    return 0;
}
