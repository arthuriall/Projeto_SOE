#include <iostream>
#include <opencv2/opencv.hpp>
#include <wiringPi.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

// === Configurações ===
#define I2C_ADDR 0x27   // Endereço I2C do LCD (confirme com i2cdetect)
#define LED      3      // GPIO22 (WiringPi 3)
#define BOTAO    7      // GPIO4 (WiringPi 7) - pino físico 7

// Controle de execução
std::atomic<bool> running(true);
std::atomic<bool> leituraAtiva(false);
int lcd_fd = -1; // Inicializado para indicar que não está aberto

// LCD I2C constantes
#define LCD_BACKLIGHT 0x08
#define ENABLE 0b00000100
#define LCD_CHR 1
#define LCD_CMD 0

// Função para enviar um pulso de enable ao LCD
void lcd_toggle_enable(uint8_t bits) {
    if (lcd_fd == -1) return; // Garante que o LCD esteja aberto
    write(lcd_fd, &bits, 1);
    usleep(500); // Pequeno atraso para o pulso de enable
    uint8_t enableBits = bits | ENABLE;
    write(lcd_fd, &enableBits, 1);
    usleep(500); // Atraso para manter o enable ativo
    enableBits = bits & ~ENABLE;
    write(lcd_fd, &enableBits, 1);
    usleep(500); // Atraso após desativar enable
}

// Função para enviar um byte ao LCD (dividido em nibbles)
void lcd_byte(uint8_t bits, uint8_t mode) {
    if (lcd_fd == -1) return; // Garante que o LCD esteja aberto
    uint8_t high = mode | (bits & 0xF0) | LCD_BACKLIGHT;
    uint8_t low  = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT;

    write(lcd_fd, &high, 1);
    lcd_toggle_enable(high);
    write(lcd_fd, &low, 1);
    lcd_toggle_enable(low);
}

// Inicializa o LCD I2C
void lcdInit() {
    const char *i2c_file = "/dev/i2c-1";
    lcd_fd = open(i2c_file, O_RDWR);
    if (lcd_fd < 0) {
        std::cerr << "Erro ao abrir /dev/i2c-1. Verifique se I2C está habilitado.\n";
        exit(1);
    }
    if (ioctl(lcd_fd, I2C_SLAVE, I2C_ADDR) < 0) {
        std::cerr << "Erro ao configurar endereço I2C (" << std::hex << I2C_ADDR << std::dec << "). Verifique o endereço e a conexão.\n";
        exit(1);
    }

    // Comandos de inicialização padrão para o HD44780
    lcd_byte(0x33, LCD_CMD);
    lcd_byte(0x32, LCD_CMD);
    lcd_byte(0x06, LCD_CMD);
    lcd_byte(0x0C, LCD_CMD);
    lcd_byte(0x28, LCD_CMD);
    lcd_byte(0x01, LCD_CMD); // Limpa o display
    usleep(2000); // Atraso recomendado após limpar o display
}

void lcdClear() {
    lcd_byte(0x01, LCD_CMD);
    usleep(2000); // Necessário para a operação de limpar
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

// Função para detectar um clique de botão com debounce
bool esperaClique(int pin, int debounceDelay = 50) {
    // Espera o botão ser liberado (HIGH)
    while (digitalRead(pin) == LOW) usleep(1000);
    usleep(debounceDelay * 1000); // Atraso para debounce
    
    // Verifica se o botão foi pressionado (LOW)
    if (digitalRead(pin) == LOW) {
        // Espera o botão ser liberado novamente (fim do clique)
        while (digitalRead(pin) == LOW) usleep(1000);
        usleep(debounceDelay * 1000); // Atraso para debounce
        return true;
    }
    return false;
}

// Lógica de detecção de cores HSV
std::string detectColorHSV(int hue, int saturation, int value) {
    // Para calibração, você pode temporariamente imprimir os valores aqui:
    // std::cout << "H:" << hue << " S:" << saturation << " V:" << value << std::endl;

    // A ordem é importante: preto/branco/cinza primeiro, pois dependem mais de S/V
    if (value < 40) return "PRETO"; // Valor baixo indica escuridão (preto)
    if (saturation < 30 && value > 200) return "BRANCO"; // Saturação baixa e valor alto (branco)
    if (saturation < 40 && value >= 40 && value <= 200) return "CINZA"; // Saturação baixa e valor intermediário (cinza)

    // Cores cromáticas (Matiz - Hue)
    if ((hue >= 0 && hue <= 10) || (hue >= 170 && hue <= 179)) return "VERMELHO"; // Vermelho no início e fim do ciclo
    if (hue >= 15 && hue <= 35) return "AMARELO";
    if (hue >= 40 && hue <= 80) return "VERDE";
    if (hue >= 100 && hue <= 140) return "AZUL";
    if (hue >= 145 && hue <= 165) return "ROXO"; // Exemplo de adição de roxo

    return "INDEFINIDO";
}

// Processa um frame da câmera para detectar a cor
std::string processFrameHSV(cv::VideoCapture& cap, int& hue, int& saturation, int& value) {
    cv::Mat frame;
    cap >> frame; // Captura um frame
    if (frame.empty()) return ""; // Retorna vazio se o frame estiver vazio

    // Aplica um pequeno filtro Gaussiano para suavizar o ruído
    cv::GaussianBlur(frame, frame, cv::Size(5, 5), 0);

    // Define a Região de Interesse (ROI) no centro do frame
    int rectSize = 20; // Tamanho do quadrado de detecção
    int x = frame.cols / 2;
    int y = frame.rows / 2;
    cv::Rect roi(x - rectSize / 2, y - rectSize / 2, rectSize, rectSize);

    cv::Mat region = frame(roi); // Extrai a ROI
    cv::Mat regionHSV;
    cv::cvtColor(region, regionHSV, cv::COLOR_BGR2HSV); // Converte ROI para HSV
    cv::Scalar meanHSV = cv::mean(regionHSV); // Calcula a média HSV da ROI

    hue = static_cast<int>(meanHSV[0]);
    saturation = static_cast<int>(meanHSV[1]);
    value = static_cast<int>(meanHSV[2]);

    return detectColorHSV(hue, saturation, value);
}

// Thread responsável pela leitura e exibição da cor
void colorThread(cv::VideoCapture& cap) {
    while (running) {
        if (leituraAtiva) {
            int h, s, v; // Variáveis para armazenar H, S, V (não serão exibidas)
            std::string cor = processFrameHSV(cap, h, s, v);

            lcdClear();
            lcdSetCursor(0, 0);
            lcdPrint("Cor Detectada:");
            lcdSetCursor(1, 0);
            lcdPrint(cor); // Apenas a cor detectada

            digitalWrite(LED, HIGH); // Liga o LED durante a leitura
            usleep(500000); // Meio segundo ligado
            digitalWrite(LED, LOW);  // Desliga o LED
            usleep(500000); // Meio segundo desligado
        } else {
            // Se a leitura estiver pausada, exibe mensagem
            lcdClear();
            lcdSetCursor(0, 0);
            lcdPrint("Leitura Pausada");
            lcdSetCursor(1, 0);
            lcdPrint("Aperte o Botao");
            digitalWrite(LED, LOW); // LED permanece desligado
            usleep(300000);
        }
    }
    digitalWrite(LED, LOW); // Garante que o LED esteja desligado ao sair
}

// Função principal
int main() {
    std::cout << "Iniciando sistema de detecção de cores...\n";

    if (wiringPiSetup() == -1) {
        std::cerr << "Erro fatal: Não foi possível iniciar WiringPi!\n";
        return 1;
    }

    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW); // Garante que o LED comece desligado

    pinMode(BOTAO, INPUT);
    pullUpDnControl(BOTAO, PUD_UP); // Ativa o pull-up interno para o botão

    lcdInit(); // Inicializa o LCD I2C

    // --- Mensagem de boas-vindas expandida ---
    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("  Bem-vindo(a)! ");
    lcdSetCursor(1, 0);
    lcdPrint("  Sistema ON  ");
    usleep(2000000); // 2 segundos

    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("  Detector de ");
    lcdSetCursor(1, 0);
    lcdPrint("   Cores HSV  ");
    usleep(2000000); // 2 segundos

    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("Aguardando seu");
    lcdSetCursor(1, 0);
    lcdPrint("  primeiro click");
    usleep(2000000); // 2 segundos
    // --- Fim da mensagem de boas-vindas expandida ---

    // Espera pelo primeiro clique para iniciar a detecção
    lcdClear();
    lcdSetCursor(0,0);
    lcdPrint("Pressione o");
    lcdSetCursor(1,0);
    lcdPrint("Botao para Iniciar");
    while (!esperaClique(BOTAO)) {
        usleep(100000); // Pequeno atraso para evitar consumo excessivo da CPU
    }
    leituraAtiva = true; // Inicia a leitura

    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("Camera...");
    lcdSetCursor(1, 0);
    lcdPrint("Inicializando");

    cv::VideoCapture cap(0); // Abre a câmera (0 é geralmente a câmera padrão)
    if (!cap.isOpened()) {
        lcdClear();
        lcdPrint("ERRO: Camera!");
        std::cerr << "Erro fatal: Não foi possível abrir a câmera. Verifique a conexão.\n";
        running = false; // Sinaliza para a thread parar
        usleep(5000000); // Mantém a mensagem de erro por 5 segundos
        return 1;
    }

    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("Camera OK!");
    lcdSetCursor(1, 0);
    lcdPrint("Botao=PAUSA/INICIA");
    usleep(2000000); // Mensagem por 2 segundos

    // Inicia a thread de detecção de cores
    std::thread t(colorThread, std::ref(cap));

    // Loop principal: aguarda o botão para pausar/resumir ou finalizar
    while (running) {
        if (esperaClique(BOTAO)) {
            leituraAtiva = !leituraAtiva; // Alterna o estado de leitura

            lcdClear();
            lcdSetCursor(0, 0);
            if (leituraAtiva) {
                lcdPrint("Leitura ATIVA");
            } else {
                lcdPrint("Leitura PAUSADA");
            }
            lcdSetCursor(1, 0);
            lcdPrint("Aperte o Botao");
            usleep(1500000); // Mensagem por 1.5 segundos
        }
        usleep(100000); // Pequeno atraso para o loop principal
    }

    // Garante que a thread termine antes de sair
    t.join();

    // Finalização
    lcdClear();
    lcdPrint("Encerrando...");
    cap.release(); // Libera a câmera
    std::cout << "Programa finalizado com sucesso.\n";
    return 0;
}
