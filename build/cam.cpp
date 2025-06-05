#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

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
    // std::cout << "DEBUG_LCD: pulseEnable start" << std::endl;
    digitalWrite(LCD_E, HIGH);
    usleep(500);
    digitalWrite(LCD_E, LOW);
    usleep(500);
    // std::cout << "DEBUG_LCD: pulseEnable end" << std::endl;
}

void sendNibble(int data) {
    // std::cout << "DEBUG_LCD: sendNibble start data=" << data << std::endl;
    digitalWrite(LCD_D4, (data >> 0) & 1);
    digitalWrite(LCD_D5, (data >> 1) & 1);
    digitalWrite(LCD_D6, (data >> 2) & 1);
    digitalWrite(LCD_D7, (data >> 3) & 1);
    pulseEnable();
    // std::cout << "DEBUG_LCD: sendNibble end" << std::endl;
}

void sendByte(int data, int mode) {
    // std::cout << "DEBUG_LCD: sendByte start data=" << data << " mode=" << mode << std::endl;
    digitalWrite(LCD_RS, mode);
    sendNibble(data >> 4);
    sendNibble(data & 0x0F);
    usleep(100);
    // std::cout << "DEBUG_LCD: sendByte end" << std::endl;
}

void lcdInit() {
    std::cout << "DEBUG_LCD: lcdInit start" << std::endl;
    pinMode(LCD_RS, OUTPUT);
    pinMode(LCD_E, OUTPUT);
    pinMode(LCD_D4, OUTPUT);
    pinMode(LCD_D5, OUTPUT);
    pinMode(LCD_D6, OUTPUT);
    pinMode(LCD_D7, OUTPUT);
    std::cout << "DEBUG_LCD: pinModes set" << std::endl;

    usleep(50000);
    std::cout << "DEBUG_LCD: post power-up sleep" << std::endl;
    sendNibble(0x03); usleep(4500);
    sendNibble(0x03); usleep(4500);
    sendNibble(0x03); usleep(150);
    sendNibble(0x02);   // Set to 4-bit mode
    std::cout << "DEBUG_LCD: 4-bit mode sequence sent" << std::endl;

    sendByte(0x28, 0);  // Function Set: 4-bit, 2 lines, 5x8 dots
    std::cout << "DEBUG_LCD: Function Set sent" << std::endl;
    sendByte(0x0C, 0);  // Display Control: Display ON, Cursor OFF, Blink OFF
    std::cout << "DEBUG_LCD: Display Control sent" << std::endl;
    sendByte(0x06, 0);  // Entry Mode Set: Increment cursor, no display shift
    std::cout << "DEBUG_LCD: Entry Mode Set sent" << std::endl;
    sendByte(0x01, 0);  // Clear display
    std::cout << "DEBUG_LCD: Clear display sent" << std::endl;
    usleep(2000);
    std::cout << "DEBUG_LCD: lcdInit end" << std::endl;
}

void lcdClear() {
    std::cout << "DEBUG_LCD: lcdClear start" << std::endl;
    sendByte(0x01, 0);
    usleep(2000);
    std::cout << "DEBUG_LCD: lcdClear end" << std::endl;
}

void lcdSetCursor(int row, int col) {
    // std::cout << "DEBUG_LCD: lcdSetCursor start row=" << row << " col=" << col << std::endl;
    int row_offsets[] = {0x00, 0x40};
    if (row < 0) row = 0;
    if (row > 1) row = 1;
    sendByte(0x80 | (row_offsets[row] + col), 0);
    // std::cout << "DEBUG_LCD: lcdSetCursor end" << std::endl;
}

void lcdPrint(const std::string& text) {
    // std::cout << "DEBUG_LCD: lcdPrint start text='" << text << "'" << std::endl;
    for (char c : text) {
        sendByte(c, 1);
    }
    // std::cout << "DEBUG_LCD: lcdPrint end" << std::endl;
}
// --- End LCD functions ---


std::atomic<bool> stop_processing(false);

bool openCamera(cv::VideoCapture& cap, int device) {
    std::cout << "DEBUG_CAM: openCamera start device=" << device << std::endl;
    if (!cap.open(device)) {
        std::cerr << "Erro ao abrir câmera no dispositivo " << device << std::endl;
        std::cout << "DEBUG_CAM: openCamera FAILED" << std::endl;
        return false;
    }
    std::cout << "DEBUG_CAM: cap.open succeeded" << std::endl;
    // Adicionar verificação isOpened aqui também
    if (!cap.isOpened()) {
        std::cerr << "DEBUG_CAM: openCamera - cap.open() succeeded BUT cap.isOpened() is false!" << std::endl;
        return false; // Considerar falha se não estiver realmente aberto
    }
    std::cout << "Câmera aberta no dispositivo " << device << std::endl;
    std::cout << "DEBUG_CAM: openCamera end - SUCCESS" << std::endl;
    return true;
}

void closeCamera(cv::VideoCapture& cap) {
    std::cout << "DEBUG_CAM: closeCamera start" << std::endl;
    if (cap.isOpened()) {
        cap.release();
        std::cout << "Câmera liberada." << std::endl;
    } else {
        std::cout << "DEBUG_CAM: closeCamera - camera was not open." << std::endl;
    }
    std::cout << "DEBUG_CAM: closeCamera end" << std::endl;
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
    std::cout << "DEBUG_THREAD: cameraProcessingThread started" << std::endl;
    std::string lastColor = "";
    cv::Mat frame;
    int frame_count = 0;

    while (!stop_processing) {
        // std::cout << "DEBUG_THREAD: Loop iteration " << frame_count << std::endl;
        if (!cap.isOpened()) {
            std::cerr << "DEBUG_THREAD: Camera not open in loop! Exiting thread." << std::endl;
            break;
        }

        // std::cout << "DEBUG_THREAD: Before cap.read()" << std::endl;
        bool read_success = cap.read(frame);
        // std::cout << "DEBUG_THREAD: After cap.read(), success=" << read_success << std::endl;

        if (!read_success || frame.empty()) {
            // std::cerr << "DEBUG_THREAD: Failed to capture frame or frame empty." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        // std::cout << "DEBUG_THREAD: Frame captured, size: " << frame.cols << "x" << frame.rows << std::endl;

        int rectSize = 20;
        int cx = frame.cols / 2;
        int cy = frame.rows / 2;

        if (frame.cols < rectSize || frame.rows < rectSize) {
            // std::cerr << "DEBUG_THREAD: Frame too small for ROI." << std::endl;
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
            
            // std::cout << "DEBUG_THREAD: Before LCD update" << std::endl;
            lcdSetCursor(0, 0);
            lcdPrint(currentColor + std::string(16 - currentColor.length(), ' '));

            lcdSetCursor(1, 0);
            std::string rgbText = "R" + std::to_string(r_val) + "G" + std::to_string(g_val) + "B" + std::to_string(b_val);
            if (rgbText.length() > 16) rgbText = rgbText.substr(0, 16);
            else rgbText += std::string(16 - rgbText.length(), ' ');
            lcdPrint(rgbText);
            // std::cout << "DEBUG_THREAD: After LCD update" << std::endl;

            lastColor = currentColor;
        }
        
        // std::cout << "DEBUG_THREAD: Before sleep" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        // std::cout << "DEBUG_THREAD: After sleep" << std::endl;
        frame_count++;
    }
    std::cout << "DEBUG_THREAD: cameraProcessingThread finished loop. Total frames processed (attempted): " << frame_count << std::endl;
}

int main() {
    std::cout << "DEBUG_MAIN: Program start" << std::endl;

    std::cout << "DEBUG_MAIN: Initializing WiringPi..." << std::endl;
    if (wiringPiSetup() == -1) {
        std::cerr << "Erro CRITICO ao inicializar WiringPi." << std::endl;
        return 1;
    }
    std::cout << "DEBUG_MAIN: WiringPi inicializado com sucesso." << std::endl;
    lcdPrint("WiringPi OK"); // Teste rápido no LCD
    usleep(1000000);


    std::cout << "DEBUG_MAIN: Initializing LCD..." << std::endl;
    lcdInit(); // Esta função já tem logs internos
    std::cout << "DEBUG_MAIN: LCD inicializado (chamada a lcdInit retornou)." << std::endl;
    lcdClear();
    lcdPrint("Detector RGB");
    lcdSetCursor(1, 0);
    lcdPrint("Iniciando Cam...");
    usleep(1000000);

    cv::VideoCapture cap;
    std::cout << "DEBUG_MAIN: Calling openCamera..." << std::endl;
    if (!openCamera(cap, 0)) { // Câmera 0
        std::cerr << "DEBUG_MAIN: openCamera FAILED in main." << std::endl;
        lcdClear();
        lcdPrint("Erro Camera");
        return -1;
    }
    std::cout << "DEBUG_MAIN: openCamera call returned true." << std::endl;

    if (!cap.isOpened()) {
        std::cerr << "DEBUG_MAIN: CRITICAL - cap.isOpened() is false AFTER successful openCamera call!" << std::endl;
        lcdClear();
        lcdPrint("Falha Cam Open");
        return -1;
    }
    std::cout << "DEBUG_MAIN: cap.isOpened() is true." << std::endl;

    lcdClear();
    lcdPrint("Camera OK");
    lcdSetCursor(1, 0);
    lcdPrint("Inic. Thread...");
    usleep(1000000);

    std::cout << "DEBUG_MAIN: Creating camera processing thread..." << std::endl;
    stop_processing = false;
    std::thread camThread(cameraProcessingThread, std::ref(cap));
    std::cout << "DEBUG_MAIN: Camera processing thread created." << std::endl;

    std::cout << "Pressione ENTER para parar..." << std::endl;
    // std::cout << "DEBUG_MAIN: Before std::cin.get()" << std::endl; // Este será o último se travar antes de esperar input
    std::cin.get(); // Aguarda Enter
    std::cout << "DEBUG_MAIN: ENTER pressed, stopping processing..." << std::endl;

    stop_processing = true;
    std::cout << "DEBUG_MAIN: Waiting for thread to join..." << std::endl;
    if (camThread.joinable()) {
        camThread.join();
    }
    std::cout << "DEBUG_MAIN: Thread joined." << std::endl;

    closeCamera(cap);

    lcdClear();
    lcdPrint("Desligado.");
    std::cout << "DEBUG_MAIN: Program finalized." << std::endl;
    return 0;
}