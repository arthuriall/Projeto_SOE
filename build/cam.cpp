#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>

#include <opencv2/opencv.hpp>

#include <wiringPi.h>
#include <unistd.h>

// LCD Pins (WiringPi numbering)
#define LCD_RS  0
#define LCD_E   1
#define LCD_D4  4
#define LCD_D5  5
#define LCD_D6  6
#define LCD_D7  7

// --- LCD Control Functions ---
void pulseEnable() {
    digitalWrite(LCD_E, HIGH);
    usleep(500);
    digitalWrite(LCD_E, LOW);
    usleep(500);
}

void sendNibble(int data) {
    digitalWrite(LCD_D4, (data >> 0) & 1);
    digitalWrite(LCD_D5, (data >> 1) & 1);
    digitalWrite(LCD_D6, (data >> 2) & 1);
    digitalWrite(LCD_D7, (data >> 3) & 1);
    pulseEnable();
}

void sendByte(int data, int mode) {
    digitalWrite(LCD_RS, mode);
    sendNibble(data >> 4);
    sendNibble(data & 0x0F);
    usleep(100);
}

void lcdInit() {
    pinMode(LCD_RS, OUTPUT);
    pinMode(LCD_E, OUTPUT);
    pinMode(LCD_D4, OUTPUT);
    pinMode(LCD_D5, OUTPUT);
    pinMode(LCD_D6, OUTPUT);
    pinMode(LCD_D7, OUTPUT);

    usleep(50000);
    sendNibble(0x03); usleep(4500);
    sendNibble(0x03); usleep(4500);
    sendNibble(0x03); usleep(150);
    sendNibble(0x02);

    sendByte(0x28, 0);  // 4-bit, 2 linhas, fonte 5x8
    sendByte(0x0C, 0);  // Display ligado, cursor e blink desligados
    sendByte(0x06, 0);  // Incrementa cursor, sem shift display
    sendByte(0x01, 0);  // Limpa display
    usleep(2000);
}

void lcdClear() {
    sendByte(0x01, 0);
    usleep(2000);
}

void lcdSetCursor(int row, int col) {
    int row_offsets[] = {0x00, 0x40};
    if (row < 0) row = 0;
    if (row > 1) row = 1;
    sendByte(0x80 | (row_offsets[row] + col), 0);
}

void lcdPrint(const std::string& text) {
    for (char c : text) {
        sendByte(c, 1);
    }
}
// --- End LCD functions ---


// Variável atômica para parar thread
std::atomic<bool> stop_processing(false);

// Função para abrir câmera
bool openCamera(cv::VideoCapture& cap, int device) {
    if (!cap.open(device)) {
        std::cerr << "Erro ao abrir câmera no dispositivo " << device << std::endl;
        return false;
    }
    std::cout << "Câmera aberta no dispositivo " << device << std::endl;
    return true;
}

// Função para fechar câmera
void closeCamera(cv::VideoCapture& cap) {
    if (cap.isOpened()) {
        cap.release();
        std::cout << "Câmera liberada." << std::endl;
    }
}

// Função para detectar cor primária RGB (recebe int direto)
std::string detectColorRGB(int r, int g, int b) {
    int threshold = 120;
    int margin = 60;

    if (r > threshold && r > g + margin && r > b + margin) return "Vermelho";
    if (g > threshold && g > r + margin && g > b + margin) return "Verde";
    if (b > threshold && b > r + margin && b > g + margin) return "Azul";
    return "Indefinido";
}

// Thread de processamento da câmera
void cameraProcessingThread(cv::VideoCapture& cap) {
    std::string lastColor = "";
    cv::Mat frame;

    while (!stop_processing) {
        if (!cap.isOpened()) {
            std::cerr << "Câmera fechada inesperadamente." << std::endl;
            break;
        }

        if (!cap.read(frame) || frame.empty()) {
            std::cerr << "Erro ao capturar frame." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        int rectSize = 20;
        int cx = frame.cols / 2;
        int cy = frame.rows / 2;

        if (frame.cols < rectSize || frame.rows < rectSize) {
            std::cerr << "Frame muito pequeno para ROI." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
        }

        cv::Rect roi(cx - rectSize/2, cy - rectSize/2, rectSize, rectSize);
        cv::Mat region = frame(roi);
        cv::Scalar meanColor = cv::mean(region);

        int b = static_cast<int>(meanColor[0]);
        int g = static_cast<int>(meanColor[1]);
        int r = static_cast<int>(meanColor[2]);

        std::string currentColor = detectColorRGB(r, g, b);

        if (currentColor != lastColor) {
            std::cout << "Cor detectada: " << currentColor 
                      << " (R=" << r << " G=" << g << " B=" << b << ")\n";

            // Atualiza LCD (sobrescreve com espaços para evitar flicker)
            lcdSetCursor(0, 0);
            lcdPrint(currentColor + std::string(16 - currentColor.size(), ' '));

            lcdSetCursor(1, 0);
            std::string rgbText = "R" + std::to_string(r) + " G" + std::to_string(g) + " B" + std::to_string(b);
            if (rgbText.size() > 16) rgbText = rgbText.substr(0, 16);
            else rgbText += std::string(16 - rgbText.size(), ' ');
            lcdPrint(rgbText);

            lastColor = currentColor;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Thread de processamento finalizada." << std::endl;
}

int main() {
    if (wiringPiSetup() == -1) {
        std::cerr << "Erro ao inicializar WiringPi." << std::endl;
        return 1;
    }
    std::cout << "WiringPi inicializado." << std::endl;

    lcdInit();
    lcdPrint("Detector RGB");
    lcdSetCursor(1, 0);
    lcdPrint("Iniciando...");
    usleep(2000000);

    cv::VideoCapture cap;
    if (!openCamera(cap, 0)) {
        lcdClear();
        lcdPrint("Erro Camera");
        return -1;
    }

    lcdClear();
    lcdPrint("Camera OK");
    lcdSetCursor(1, 0);
    lcdPrint("Processando...");

    stop_processing = false;
    std::thread camThread(cameraProcessingThread, std::ref(cap));

    std::cout << "Pressione ENTER para parar..." << std::endl;
    std::cin.get();

    stop_processing = true;
    if (camThread.joinable()) camThread.join();

    closeCamera(cap);

    lcdClear();
    lcdPrint("Desligado.");
    std::cout << "Programa finalizado." << std::endl;
    return 0;
}
