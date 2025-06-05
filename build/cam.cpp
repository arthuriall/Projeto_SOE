#include <iostream>
#include <string>
#include <vector>
#include <thread>         // Required for std::thread
#include <atomic>         // Required for std::atomic
#include <chrono>         // Required for std::chrono::milliseconds

#include <opencv2/opencv.hpp> // OpenCV main header
#include <opencv2/videoio.hpp> // For cv::VideoCapture
#include <opencv2/imgproc.hpp> // For cv::cvtColor, cv::Rect, cv::mean

// Includes for LCD
#include <wiringPi.h>
#include <unistd.h> // For usleep

// LCD Pin Definitions (WiringPi numbering scheme)
#define LCD_RS  0  // Register select
#define LCD_E   1  // Enable
#define LCD_D4  4  // Data pin 4
#define LCD_D5  5  // Data pin 5
#define LCD_D6  6  // Data pin 6
#define LCD_D7  7  // Data pin 7

// --- LCD Control Functions ---
void pulseEnable() {
    digitalWrite(LCD_E, HIGH);
    usleep(500); // microsecond delay
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
    digitalWrite(LCD_RS, mode); // mode=0 for command, mode=1 for data
    sendNibble(data >> 4);      // Send high nibble
    sendNibble(data & 0x0F);    // Send low nibble
    usleep(100);                // Short delay after byte
}

void lcdInit() {
    pinMode(LCD_RS, OUTPUT);
    pinMode(LCD_E, OUTPUT);
    pinMode(LCD_D4, OUTPUT);
    pinMode(LCD_D5, OUTPUT);
    pinMode(LCD_D6, OUTPUT);
    pinMode(LCD_D7, OUTPUT);

    usleep(50000);      // Wait for LCD to power up
    sendNibble(0x03); usleep(4500);
    sendNibble(0x03); usleep(4500);
    sendNibble(0x03); usleep(150);
    sendNibble(0x02);   // Set to 4-bit mode

    sendByte(0x28, 0);  // Function Set: 4-bit, 2 lines, 5x8 dots
    sendByte(0x0C, 0);  // Display Control: Display ON, Cursor OFF, Blink OFF
    sendByte(0x06, 0);  // Entry Mode Set: Increment cursor, no display shift
    sendByte(0x01, 0);  // Clear display
    usleep(2000);       // Clear display command takes longer
    std::cout << "LCD inicializado." << std::endl;
}

void lcdClear() {
    sendByte(0x01, 0);  // Clear display
    usleep(2000);       // This command needs a longer delay
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
// --- End LCD Control Functions ---


// Global atomic flag to signal the processing thread to stop
std::atomic<bool> stop_processing(false);

// Function to open the camera
bool openCamera(cv::VideoCapture& cap, int device) {
    if (!cap.open(device)) {
        std::cerr << "Erro ao abrir a camera no dispositivo " << device << std::endl;
        return false;
    }
    std::cout << "Câmera aberta no dispositivo " << device << std::endl;
    return true;
}

// Function to close the camera
void closeCamera(cv::VideoCapture& cap) {
    if (cap.isOpened()) {
        cap.release();
        std::cout << "Câmera liberada." << std::endl;
    }
}

// MODIFIED Function to detect color based on RGB values
std::string detectColorRGB(const cv::Vec3b& pixel) {
    int b = pixel[0];
    int g = pixel[1];
    int r = pixel[2];

    // Limiares para considerar uma cor primária como dominante
    int primary_threshold = 120; // Valor mínimo para o canal ser considerado "ativo"
    int dominance_margin = 60;   // Quão mais alto o canal dominante deve ser em relação aos outros

    // Verifica Vermelho
    if (r > primary_threshold && r > (g + dominance_margin) && r > (b + dominance_margin)) {
        return "Vermelho";
    }
    // Verifica Verde
    if (g > primary_threshold && g > (r + dominance_margin) && g > (b + dominance_margin)) {
        return "Verde";
    }
    // Verifica Azul
    if (b > primary_threshold && b > (r + dominance_margin) && b > (g + dominance_margin)) {
        return "Azul";
    }
    
    return "Indefinido"; // Se nenhuma cor primária for claramente dominante
}


// Function to be run in a separate thread for processing camera frames
void camera_processing_thread_func(cv::VideoCapture& cap) {
    static std::string ultimaCorDetectada;
    cv::Mat frame;
    int r_val, g_val, b_val;

    std::cout << "Thread de processamento de câmera iniciada." << std::endl;

    while (!stop_processing) {
        if (!cap.isOpened()) {
            if (!stop_processing) {
                std::cerr << "Erro: Câmera não está aberta no thread." << std::endl;
            }
            stop_processing = true;
            break;
        }

        if (!cap.read(frame)) {
            if (!stop_processing) {
                std::cerr << "Erro: Não foi possível ler o frame da câmera." << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (frame.empty()) {
            if (!stop_processing) {
                std::cerr << "Erro: Frame vazio capturado." << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        int rectSize = 20;
        if (frame.cols < rectSize || frame.rows < rectSize) {
            if(!stop_processing) {
                std::cerr << "Erro: Frame muito pequeno para o ROI." << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
        }
        int x = frame.cols / 2;
        int y = frame.rows / 2;
        cv::Rect roi(x - rectSize / 2, y - rectSize / 2, rectSize, rectSize);
        cv::Mat region = frame(roi);
        cv::Scalar meanColorScalar = cv::mean(region);

        b_val = static_cast<int>(meanColorScalar[0]);
        g_val = static_cast<int>(meanColorScalar[1]);
        r_val = static_cast<int>(meanColorScalar[2]);

        std::string corAtual = detectColorRGB(cv::Vec3b(b_val, g_val, r_val));

        if (corAtual != ultimaCorDetectada) { // Atualiza o LCD se a cor (ou estado "Indefinido") mudou
            std::cout << "Cor detectada: " << corAtual 
                      << " (R=" << r_val << ", G=" << g_val << ", B=" << b_val << ")" << std::endl;
            
            // --- Display on LCD ---
            lcdSetCursor(0, 0); 
            lcdPrint("                "); 
            lcdSetCursor(0, 0);
            lcdPrint(corAtual);

            lcdSetCursor(1, 0);
            std::string rgb_text = "R" + std::to_string(r_val) + " G" + std::to_string(g_val) + " B" + std::to_string(b_val);
            if (rgb_text.length() > 16) rgb_text = rgb_text.substr(0, 16);
            else while(rgb_text.length() < 16) rgb_text += " "; 
            lcdPrint(rgb_text); 
            // --- End Display on LCD ---

            ultimaCorDetectada = corAtual;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Thread de processamento de câmera terminando." << std::endl;
}

int main() {
    if (wiringPiSetup() == -1) {
        std::cerr << "Falha ao inicializar WiringPi." << std::endl;
        return 1;
    }
    std::cout << "WiringPi inicializado." << std::endl;

    lcdInit();
    lcdPrint("Detector RGB"); // Nome mais curto
    lcdSetCursor(1,0);
    lcdPrint("Iniciando...");
    usleep(2000000); 

    cv::VideoCapture camera_capture;
    int camera_device_id = 0;

    if (!openCamera(camera_capture, camera_device_id)) {
        std::cerr << "Falha ao inicializar a câmera. Saindo." << std::endl;
        lcdClear();
        lcdPrint("Erro Camera");
        return -1;
    }
    lcdClear();
    lcdPrint("Camera OK");
    lcdSetCursor(1,0);
    lcdPrint("Processando...");


    std::cout << "Thread principal: Iniciando thread de processamento de câmera." << std::endl;
    std::cout << "Pressione ENTER no console para parar o processamento e sair." << std::endl;

    stop_processing = false;
    std::thread processing_thread(camera_processing_thread_func, std::ref(camera_capture));

    std::cin.get(); 

    std::cout << "Thread principal: Sinalizando para o thread de processamento parar..." << std::endl;
    stop_processing = true;

    if (processing_thread.joinable()) {
        processing_thread.join();
    }
    std::cout << "Thread principal: Thread de processamento finalizado." << std::endl;

    closeCamera(camera_capture);

    lcdClear();
    lcdPrint("Desligado.");
    std::cout << "Programa terminado." << std::endl;
    return 0;
}