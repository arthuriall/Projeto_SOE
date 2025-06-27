#include <iostream>
#include <opencv2/opencv.hpp>
#include <wiringPi.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <fcntl.h>      // Required for open()
#include <linux/i2c-dev.h> // Required for I2C_SLAVE
#include <sys/ioctl.h>  // Required for ioctl()

// === CONFIGURAÇÕES ===
#define I2C_ADDR 0x27   // Endereço I2C do LCD (confirme com i2cdetect)
#define LED      3      // GPIO22 (WiringPi 3)
#define BOTAO    8      // GPIO8  (WiringPi 8)

// === CONTROLE ===
std::atomic<bool> running(true);
std::atomic<bool> leituraAtiva(false);
int lcd_fd; // File descriptor for the I2C LCD

// === LCD I2C ===
#define LCD_BACKLIGHT 0x08
#define ENABLE 0b00000100
#define LCD_CHR 1
#define LCD_CMD 0

void lcd_toggle_enable(uint8_t bits) {
    write(lcd_fd, &bits, 1);
    usleep(500);
    uint8_t enableBits = bits | ENABLE;
    write(lcd_fd, &enableBits, 1);
    usleep(500);
    enableBits = bits & ~ENABLE;
    write(lcd_fd, &enableBits, 1);
    usleep(500);
}

void lcd_byte(uint8_t bits, uint8_t mode) {
    uint8_t high = mode | (bits & 0xF0) | LCD_BACKLIGHT;
    uint8_t low  = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT;

    write(lcd_fd, &high, 1);
    lcd_toggle_enable(high);
    write(lcd_fd, &low, 1);
    lcd_toggle_enable(low);
}

void lcdInit() {
    const char *i2c_file = "/dev/i2c-1";
    lcd_fd = open(i2c_file, O_RDWR);
    if (lcd_fd < 0 || ioctl(lcd_fd, I2C_SLAVE, I2C_ADDR) < 0) {
        std::cerr << "Erro ao conectar com LCD I2C. Verifique o endereço e conexão.\n";
        exit(1);
    }

    lcd_byte(0x33, LCD_CMD);
    lcd_byte(0x32, LCD_CMD);
    lcd_byte(0x06, LCD_CMD);
    lcd_byte(0x0C, LCD_CMD);
    lcd_byte(0x28, LCD_CMD);
    lcd_byte(0x01, LCD_CMD);
    usleep(2000);
}

void lcdClear() {
    lcd_byte(0x01, LCD_CMD);
    usleep(2000);
}

void lcdSetCursor(int row, int col) {
    int rowAddr[] = {0x00, 0x40};
    lcd_byte(0x80 + rowAddr[row] + col, LCD_CMD);
}

void lcdPrint(const std::string& text) {
    for (char c : text) {
        lcd_byte(c, LCD_CHR);
    }
}

// === BOTÃO ===
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

// === COR HSV ===
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

    lcdInit(); // Initialize I2C LCD
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
