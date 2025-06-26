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

// === LCD Constants ===
#define DADO    1
#define COMANDO 0

// === Controle de Execução ===
std::atomic<bool> running(true);
std::atomic<bool> leituraAtiva(false);

// === LCD Functions (baseadas no segundo código) ===
void Config_Pins() {
    pinMode(E, OUTPUT);
    pinMode(RS, OUTPUT);
    pinMode(D4, OUTPUT);
    pinMode(D5, OUTPUT);
    pinMode(D6, OUTPUT);
    pinMode(D7, OUTPUT);
}

char Send_Nibble(char nibble, char nibble_type) {
    if ((nibble_type != DADO) && (nibble_type != COMANDO)) return -1;
    digitalWrite(E, HIGH);
    digitalWrite(RS, nibble_type);
    digitalWrite(D4, nibble & 1);
    digitalWrite(D5, (nibble >> 1) & 1);
    digitalWrite(D6, (nibble >> 2) & 1);
    digitalWrite(D7, (nibble >> 3) & 1);
    digitalWrite(E, LOW);
    usleep(500);
    return 0;
}

char Send_Byte(char byte, char byte_type) {
    if (Send_Nibble(byte >> 4, byte_type) == -1) return -1;
    Send_Nibble(byte & 0xF, byte_type);
    return 0;
}

void Clear_LCD() {
    Send_Byte(0x01, COMANDO);
    usleep(20000);
    Send_Byte(0x02, COMANDO);
    usleep(20000);
}

void Config_LCD() {
    usleep(10000);
    Config_Pins();
    Send_Nibble(0x3, COMANDO);
    Send_Nibble(0x3, COMANDO);
    Send_Nibble(0x3, COMANDO);
    Send_Nibble(0x2, COMANDO);
    Send_Byte(0x28, COMANDO); // 4-bit, 2 lines, 5x8 font
    Send_Byte(0x0C, COMANDO); // display ON, cursor OFF
    Send_Byte(0x06, COMANDO); // entry mode set
    Clear_LCD();
}

void Set_Cursor(int row, int col) {
    int rowAddr[] = {0x00, 0x40, 0x14, 0x54}; // Para LCD 20x4
    Send_Byte(0x80 + rowAddr[row] + col, COMANDO);
}

void Send_String(const std::string &str) {
    for (char c : str) {
        Send_Byte(c, DADO);
    }
}

// === Debounce ===
bool esperaClique(int pin, int debounceDelay = 50) {
    while (digitalRead(pin) == HIGH) usleep(1000);
    usleep(debounceDelay * 1000);
    if (digitalRead(pin) == LOW) {
        while (digitalRead(pin) == LOW) usleep(1000);
        usleep(debounceDelay * 1000);
        return true;
    }
    return false;
}

// === Detecta Cor ===
std::string detectColorHSV(int hue, int saturation, int value) {
    if (value < 50) return "Preto";
    if (saturation < 50) return "Cinza";
    if (hue < 10 || hue > 160) return "Vermelho";
    if (hue >= 35 && hue <= 85) return "Verde";
    if (hue >= 90 && hue <= 130) return "Azul";
    return "Indefinido";
}

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

void colorThread(cv::VideoCapture& cap) {
    while (running) {
        if (leituraAtiva) {
            int hue, sat, val;
            std::string cor = processFrameHSV(cap, hue, sat, val);

            Clear_LCD();
            Set_Cursor(0, 0); Send_String("Cor: " + cor);
            Set_Cursor(1, 0);
            char buffer[20];
            snprintf(buffer, sizeof(buffer), "H:%d S:%d V:%d", hue, sat, val);
            Send_String(buffer);

            digitalWrite(LED, HIGH);
            usleep(500000);
            digitalWrite(LED, LOW);
            usleep(500000);
        } else {
            Clear_LCD();
            Set_Cursor(0, 0); Send_String("Leitura pausada");
            Set_Cursor(1, 0); Send_String("Pressione botao");
            digitalWrite(LED, LOW);
            usleep(300000);
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

    Config_LCD();
    Set_Cursor(0, 0); Send_String("Aguardando botao");
    Set_Cursor(1, 0); Send_String("para iniciar");

    while (!esperaClique(BOTAO)) usleep(100000);
    leituraAtiva = true;

    Clear_LCD();
    Send_String("Camera inicializa");
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        Clear_LCD(); Send_String("Erro camera");
        std::cerr << "Erro ao abrir camera.\n";
        return 1;
    }

    Clear_LCD();
    Send_String("Camera OK");
    Set_Cursor(1, 0); Send_String("botao=pause/resume");

    std::thread t(colorThread, std::ref(cap));

    while (running) {
        if (esperaClique(BOTAO)) {
            leituraAtiva = !leituraAtiva;
            Clear_LCD();
            Set_Cursor(0, 0);
            if (leituraAtiva)
                Send_String("Leitura ATIVA");
            else
                Send_String("Leitura PAUSADA");
            Set_Cursor(1, 0); Send_String("Pressione botao");
            usleep(1000000);
        }
        usleep(100000);
    }

    running = false;
    t.join();

    Clear_LCD();
    Send_String("Encerrando...");
    cap.release();
    std::cout << "Programa finalizado.\n";
    return 0;
}
