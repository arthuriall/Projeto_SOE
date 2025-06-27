#include <iostream>
#include <opencv2/opencv.hpp>
#include <wiringPi.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <chrono> // Para usar std::chrono::steady_clock para debounce

// === Configurações ===
#define I2C_ADDR 0x27   // Endereço I2C do LCD (confirme com i2cdetect)
#define LED      3      // GPIO22 (WiringPi 3)
#define BOTAO    7      // GPIO4 (WiringPi 7) - pino físico 7

// Controle de execução e recursos
std::atomic<bool> running(true);
std::atomic<bool> leituraAtiva(false);
int lcd_fd = -1; // File descriptor for the I2C LCD

// LCD I2C constantes
#define LCD_BACKLIGHT 0x08
#define ENABLE 0b00000100
#define LCD_CHR 1
#define LCD_CMD 0

// === Funções de Controle do LCD ===
void lcd_toggle_enable(uint8_t bits) {
    if (lcd_fd == -1) return;
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
    if (lcd_fd == -1) return;
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
    if (lcd_fd < 0) {
        std::cerr << "ERRO: Nao foi possivel abrir /dev/i2c-1. Verifique se I2C esta habilitado.\n";
        exit(1);
    }
    if (ioctl(lcd_fd, I2C_SLAVE, I2C_ADDR) < 0) {
        std::cerr << "ERRO: Nao foi possivel configurar endereco I2C (" << std::hex << I2C_ADDR << std::dec << "). Verifique o endereco e a conexao.\n";
        close(lcd_fd);
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

// === Funções de Leitura do Botão Aprimoradas ===
static std::chrono::steady_clock::time_point last_click_time = std::chrono::steady_clock::now();
const long DEBOUNCE_MS = 150; // Aumentado o tempo de debounce para 150ms

bool esperaClique(int pin) {
    if (digitalRead(pin) == LOW) { // Botão pressionado
        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_click_time).count();

        if (elapsed_ms > DEBOUNCE_MS) {
            // Espera o botão ser liberado para confirmar o clique completo
            while (digitalRead(pin) == LOW) {
                usleep(1000);
            }
            last_click_time = now; // Atualiza o tempo do último clique
            return true;
        }
    }
    return false;
}


// === Funções de Detecção de Cores ===
// Calibração dos limites HSV (Matiz, Saturação, Valor)
// Hue: 0-179 (OpenCV), Saturation: 0-255, Value: 0-255
std::string detectColorHSV(int hue, int saturation, int value) {
    // --- Valores HSV sempre impressos no terminal para DEBUG/Calibração ---
    std::cout << "H:" << hue << "\tS:" << saturation << "\tV:" << value << std::endl;

    // Prioridade para tons acromáticos (Preto, Branco, Cinza)
    // Estes dependem fortemente de Saturação e Valor, e menos de Matiz
    if (value < 30) return "PRETO"; // Muito escuro, independentemente da saturação
    if (saturation < 20 && value > 220) return "BRANCO"; // Muito desbotado e muito claro
    if (saturation < 30 && value >= 30 && value <= 220) return "CINZA"; // Desbotado e brilho intermediário

    // Para as cores cromáticas, exigimos um mínimo de saturação e valor
    // para garantir que não sejam confundidas com cinzas ou tons muito escuros/claros
    // Ajuste estes limites MINIMOS para sua condição de luz mais FRACA e MAIS FORTE
    const int CHROMATIC_MIN_SATURATION = 60; // Ajuste conforme a pureza das suas cores
    const int CHROMATIC_MIN_VALUE = 60;      // Ajuste conforme o brilho mínimo das suas cores

    if (saturation < CHROMATIC_MIN_SATURATION || value < CHROMATIC_MIN_VALUE) {
        return "INDEFINIDO"; // Não é uma cor vibrante ou clara o suficiente
    }

    // Cores Cromáticas (principalmente por Matiz)
    // Vermelho (Duas faixas - início e fim do espectro)
    if ((hue >= 0 && hue <= 8) || (hue >= 170 && hue <= 179)) return "VERMELHO";

    // Amarelo
    if (hue >= 18 && hue <= 30) return "AMARELO";

    // Verde
    if (hue >= 45 && hue <= 75) return "VERDE";

    // Ciano
    if (hue >= 85 && hue <= 95) return "CIANO";

    // Azul
    if (hue >= 105 && hue <= 130) return "AZUL";

    // Roxo/Magenta
    if (hue >= 140 && hue <= 160) return "ROXO";

    return "INDEFINIDO";
}

std::string processFrameHSV(cv::VideoCapture& cap, int& hue, int& saturation, int& value) {
    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) {
        std::cerr << "ERRO: Frame vazio da camera!\n";
        return "";
    }

    cv::GaussianBlur(frame, frame, cv::Size(5, 5), 0);

    int rectSize = 10;
    int x = frame.cols / 2;
    int y = frame.rows / 2;
    cv::Rect roi(x - rectSize / 2, y - rectSize / 2, rectSize, rectSize);

    if (roi.x < 0) roi.x = 0;
    if (roi.y < 0) roi.y = 0;
    if (roi.x + roi.width > frame.cols) roi.width = frame.cols - roi.x;
    if (roi.y + roi.height > frame.rows) roi.height = frame.rows - roi.y;

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
            int h, s, v; // Estes valores são passados para processFrameHSV e depois usados para imprimir no terminal
            std::string cor = processFrameHSV(cap, h, s, v);

            lcdClear();
            lcdSetCursor(0, 0);
            lcdPrint("Cor Detectada:");
            lcdSetCursor(1, 0);
            lcdPrint(cor); // Apenas a cor detectada no LCD

            digitalWrite(LED, HIGH);
            usleep(500000);
            digitalWrite(LED, LOW);
            usleep(500000);
        } else {
            lcdClear();
            lcdSetCursor(0, 0);
            lcdPrint("Leitura Pausada");
            lcdSetCursor(1, 0);
            lcdPrint("Aperte o Botao");
            digitalWrite(LED, LOW);
            usleep(300000);
        }
    }
    digitalWrite(LED, LOW);
}

// === Função Principal ===
int main() {
    std::cout << "Iniciando sistema de deteccao de cores...\n";

    if (wiringPiSetup() == -1) {
        std::cerr << "ERRO FATAL: Nao foi possivel iniciar WiringPi!\n";
        return 1;
    }

    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);

    pinMode(BOTAO, INPUT);
    pullUpDnControl(BOTAO, PUD_UP);

    lcdInit();

    // --- Mensagem de boas-vindas expandida ---
    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("  Bem-vindo(a)! ");
    lcdSetCursor(1, 0);
    lcdPrint("  Sistema ON  ");
    usleep(2000000);

    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint(" Detector de ");
    lcdSetCursor(1, 0);
    lcdPrint(" Cores do IoT ");
    usleep(2000000);

    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("  Calibre as  ");
    lcdSetCursor(1, 0);
    lcdPrint("  cores, OK?  ");
    usleep(2000000);
    // --- Fim da mensagem de boas-vindas expandida ---

    // Mensagem para iniciar com o botao
    lcdClear();
    lcdSetCursor(0,0);
    lcdPrint("Pressione o");
    lcdSetCursor(1,0);
    lcdPrint("Botao para Iniciar");
    while (!esperaClique(BOTAO)) {
        // Nada aqui, esperaClique gerencia o debounce e espera
    }
    leituraAtiva = true;

    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("Camera...");
    lcdSetCursor(1, 0);
    lcdPrint("Inicializando");

    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        lcdClear();
        lcdPrint("ERRO: Camera!");
        std::cerr << "ERRO FATAL: Nao foi possivel abrir a camera. Verifique a conexao e drivers.\n";
        running = false;
        usleep(5000000);
        return 1;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("Camera OK!");
    lcdSetCursor(1, 0);
    lcdPrint("Botao=PAUSA/INICIA");
    usleep(2000000);

    std::thread t(colorThread, std::ref(cap));

    while (running) {
        if (esperaClique(BOTAO)) {
            leituraAtiva = !leituraAtiva;

            lcdClear();
            lcdSetCursor(0, 0);
            if (leituraAtiva) {
                lcdPrint("Leitura ATIVA");
            } else {
                lcdPrint("Leitura PAUSADA");
            }
            lcdSetCursor(1, 0);
            lcdPrint("Aperte o Botao");
            usleep(1500000);
        }
        // Não precisamos de usleep aqui, pois esperaClique já tem atraso
    }

    t.join();

    lcdClear();
    lcdPrint("Encerrando...");
    usleep(1000000);
    cap.release();
    if (lcd_fd != -1) {
        close(lcd_fd);
    }
    std::cout << "Programa finalizado com sucesso.\n";
    return 0;
}
