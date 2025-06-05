#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector> // Para cv::Rect, cv::Scalar

#include <opencv2/opencv.hpp> // Inclui tudo do OpenCV necessário

#include <wiringPi.h>
#include <unistd.h>

// LCD Pins (WiringPi numbering)
#define LCD_RS  0
#define LCD_E   1
#define LCD_D4  4
#define LCD_D5  5
#define LCD_D6  6
#define LCD_D7  7

// --- LCD Control Functions (seu código do LCD aqui, como antes) ---
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
    if (row > 1) row = 1; // Assuming 2-line display
    sendByte(0x80 | (row_offsets[row] + col), 0);
}

void lcdPrint(const std::string& text) {
    for (char c : text) {
        sendByte(c, 1);
    }
}
// --- End LCD functions ---


std::atomic<bool> stop_processing(false);

bool openCamera(cv::VideoCapture& cap, int device) {
    if (!cap.open(device)) {
        std::cerr << "Erro ao abrir câmera no dispositivo " << device << std::endl;
        return false;
    }
    std::cout << "Câmera aberta no dispositivo " << device << std::endl;
    return true;
}

void closeCamera(cv::VideoCapture& cap) {
    if (cap.isOpened()) {
        cap.release();
        std::cout << "Câmera liberada." << std::endl;
    }
    // cv::destroyAllWindows(); // JANELA OPENCV DESABILITADA
}

std::string detectColorRGB(int r, int g, int b) {
    int threshold = 120;
    int margin = 60;
    if (r > threshold && r > g + margin && r > b + margin) return "Vermelho";
    if (g > threshold && g > r + margin && g > b + margin) return "Verde";
    if (b > threshold && b > r + margin && b > g + margin) return "Azul";
    return "Indefinido";
}

void cameraProcessingThread(cv::VideoCapture& cap) {
    std::string lastColor = "";
    cv::Mat frame;
    // const std::string window_name = "Camera Feed"; // JANELA OPENCV DESABILITADA

    std::cout << "Thread de processamento iniciada (COM LCD, SEM janela OpenCV)." << std::endl;

    while (!stop_processing) {
        if (!cap.isOpened()) {
            std::cerr << "Câmera fechada inesperadamente na thread." << std::endl;
            // Adicione uma pequena pausa antes de tentar sair ou dar break para evitar busy loop no erro
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            break; 
        }

        if (!cap.read(frame) || frame.empty()) {
            // std::cerr << "Erro ao capturar frame na thread." << std::endl; // Pode poluir
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        int rectSize = 20;
        int cx = frame.cols / 2;
        int cy = frame.rows / 2;

        if (frame.cols < rectSize || frame.rows < rectSize) {
            // std::cerr << "Frame muito pequeno para ROI na thread." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
        }

        cv::Rect roi(cx - rectSize/2, cy - rectSize/2, rectSize, rectSize);
        cv::Mat region = frame(roi);
        cv::Scalar meanColor = cv::mean(region);

        int b_val = static_cast<int>(meanColor[0]);
        int g_val = static_cast<int>(meanColor[1]);
        int r_val = static_cast<int>(meanColor[2]);

        std::string currentColor = detectColorRGB(r_val, g_val, b_val);

        if (currentColor != lastColor) {
            std::cout << "[Thread] Cor detectada: " << currentColor 
                      << " (R=" << r_val << " G=" << g_val << " B=" << b_val << ")\n";

            lcdSetCursor(0, 0);
            lcdPrint(currentColor + std::string(16 - currentColor.length(), ' '));

            lcdSetCursor(1, 0);
            std::string rgbText = "R" + std::to_string(r_val) + "G" + std::to_string(g_val) + "B" + std::to_string(b_val); // Sem espaços para caber mais
            if (rgbText.length() > 16) rgbText = rgbText.substr(0, 16);
            else rgbText += std::string(16 - rgbText.length(), ' ');
            lcdPrint(rgbText);

            lastColor = currentColor;
        }

        // JANELA OPENCV (imshow, waitKey) DESABILITADA
        // cv::rectangle(frame, roi, cv::Scalar(0, 255, 0), 1);
        // cv::imshow(window_name, frame);
        // char key = (char)cv::waitKey(1);
        // if (key == 'q' || key == 'Q' || key == 27 /* ESC */) {
        //     stop_processing = true;
        // }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(150)); // Delay para a thread
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
    usleep(1000000); // 1 segundo

    cv::VideoCapture cap;
    // Adicionando logs para abertura da câmera
    std::cout << "MAIN: Tentando abrir a camera..." << std::endl;
    if (!openCamera(cap, 0)) {
        std::cerr << "MAIN: Falha CRÍTICA ao abrir camera!" << std::endl;
        lcdClear();
        lcdPrint("Erro Camera");
        return -1;
    }
    std::cout << "MAIN: Camera parece ter sido aberta com sucesso pela funcao openCamera." << std::endl;
    // Verificação adicional se a câmera está realmente aberta após a chamada
    if (!cap.isOpened()) {
        std::cerr << "MAIN: ERRO GRAVE - cap.isOpened() retornou false APOS openCamera retornar true!" << std::endl;
        lcdClear();
        lcdPrint("Falha Cam Init");
        return -1;
    }
    std::cout << "MAIN: cap.isOpened() confirma que a camera esta aberta." << std::endl;


    lcdClear();
    lcdPrint("Camera OK");
    lcdSetCursor(1, 0);
    lcdPrint("Processando...");

    // JANELA OPENCV DESABILITADA
    // const std::string window_name = "Camera Feed";
    // cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);

    stop_processing = false;
    std::thread camThread(cameraProcessingThread, std::ref(cap));

    std::cout << "Pressione ENTER para parar..." << std::endl;
    std::cin.get();

    stop_processing = true;
    if (camThread.joinable()) camThread.join();

    closeCamera(cap);

    // JANELA OPENCV DESABILITADA
    // cv::destroyAllWindows();

    lcdClear();
    lcdPrint("Desligado.");
    std::cout << "Programa finalizado." << std::endl;
    return 0;
}