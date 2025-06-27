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
    usleep(500); // Atraso para o pulso de enable
    uint8_t enableBits = bits | ENABLE;
    write(lcd_fd, &enableBits, 1);
    usleep(500); // Atraso para manter o enable ativo
    enableBits = bits & ~ENABLE;
    write(lcd_fd, &enableBits, 1);
    usleep(500); // Atraso após desativar enable
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
        close(lcd_fd); // Fecha o descritor em caso de erro
        exit(1);
    }

    // Sequencia de inicializacao do LCD
    lcd_byte(0x33, LCD_CMD);
    lcd_byte(0x32, LCD_CMD);
    lcd_byte(0x06, LCD_CMD);
    lcd_byte(0x0C, LCD_CMD);
    lcd_byte(0x28, LCD_CMD);
    lcd_byte(0x01, LCD_CMD); // Limpa o display
    usleep(2000); // Atraso necessario apos limpar
}

void lcdClear() {
    lcd_byte(0x01, LCD_CMD);
    usleep(2000); // Atraso necessario para a operacao de limpar
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

// === Funções de Leitura do Botão ===
bool esperaClique(int pin, int debounceDelay = 70) { // Aumentado debounceDelay
    // Aguarda o botão ser liberado (estado HIGH) para evitar cliques falsos iniciais
    while (digitalRead(pin) == LOW) {
        usleep(1000);
    }
    usleep(debounceDelay * 1000); // Atraso para debounce (após liberação)

    // Se o botão for pressionado (estado LOW)
    if (digitalRead(pin) == LOW) {
        // Aguarda o botão ser liberado novamente (fim do clique)
        while (digitalRead(pin) == LOW) {
            usleep(1000);
        }
        usleep(debounceDelay * 1000); // Atraso para debounce (após pressionamento)
        return true; // Retorna verdadeiro para um clique válido
    }
    return false; // Nenhum clique válido detectado
}

// === Funções de Detecção de Cores ===
// Calibração dos limites HSV (Matiz, Saturação, Valor)
// Hue: 0-179 (OpenCV), Saturation: 0-255, Value: 0-255
std::string detectColorHSV(int hue, int saturation, int value) {
    // --- Para Calibracao: Descomente para ver os valores no terminal ---
    // std::cout << "H:" << hue << " S:" << saturation << " V:" << value << std::endl;

    // Prioridade: Preto, Branco, Cinza (mais dependentes de S e V)
    if (value < 40) return "PRETO"; // Muito escuro
    if (saturation < 25 && value > 200) return "BRANCO"; // Muito desbotado e claro
    if (saturation < 35 && value >= 40 && value <= 200) return "CINZA"; // Desbotado e brilho intermediário

    // Cores Cromáticas (mais dependentes de H, mas com restrições em S e V)
    // Saturação e Valor mínimos para cores vibrantes (evita cinzas/pretos)
    const int MIN_SATURATION = 60; // Ajuste conforme a pureza das suas cores
    const int MIN_VALUE = 60;     // Ajuste conforme o brilho mínimo das suas cores

    if (saturation < MIN_SATURATION || value < MIN_VALUE) return "INDEFINIDO"; // Não vibrante o suficiente

    // Vermelho (Dividido em duas faixas no ciclo HSV)
    if ((hue >= 0 && hue <= 8) || (hue >= 170 && hue <= 179)) return "VERMELHO";

    // Amarelo
    if (hue >= 18 && hue <= 30) return "AMARELO";

    // Verde
    if (hue >= 45 && hue <= 75) return "VERDE";

    // Ciano (Entre verde e azul)
    if (hue >= 85 && hue <= 95) return "CIANO";
    
    // Azul
    if (hue >= 105 && hue <= 130) return "AZUL";

    // Roxo/Magenta (Entre azul e vermelho)
    if (hue >= 140 && hue <= 160) return "ROXO";

    return "INDEFINIDO";
}

std::string processFrameHSV(cv::VideoCapture& cap, int& hue, int& saturation, int& value) {
    cv::Mat frame;
    cap >> frame; // Captura um frame da câmera
    if (frame.empty()) {
        std::cerr << "ERRO: Frame vazio da camera!\n";
        return "";
    }

    // Aplica filtro Gaussiano para suavizar a imagem e reduzir ruído
    cv::GaussianBlur(frame, frame, cv::Size(5, 5), 0);

    // Define a Região de Interesse (ROI) no centro do frame
    // Tamanho da ROI mais restrito: 10x10 pixels
    int rectSize = 10;
    int x = frame.cols / 2;
    int y = frame.rows / 2;
    cv::Rect roi(x - rectSize / 2, y - rectSize / 2, rectSize, rectSize);

    // Garante que a ROI esteja dentro dos limites da imagem
    if (roi.x < 0) roi.x = 0;
    if (roi.y < 0) roi.y = 0;
    if (roi.x + roi.width > frame.cols) roi.width = frame.cols - roi.x;
    if (roi.y + roi.height > frame.rows) roi.height = frame.rows - roi.y;
    
    cv::Mat region = frame(roi); // Extrai a ROI
    cv::Mat regionHSV;
    cv::cvtColor(region, regionHSV, cv::COLOR_BGR2HSV); // Converte a ROI para HSV
    cv::Scalar meanHSV = cv::mean(regionHSV); // Calcula a média HSV da ROI

    hue = static_cast<int>(meanHSV[0]);
    saturation = static_cast<int>(meanHSV[1]);
    value = static_cast<int>(meanHSV[2]);

    return detectColorHSV(hue, saturation, value);
}

// === Thread de Leitura e Preview de Cores ===
void colorThread(cv::VideoCapture& cap) {
    // Cria a janela de preview
    cv::namedWindow("Preview", cv::WINDOW_AUTOSIZE);

    while (running) {
        cv::Mat frame_full;
        cap >> frame_full; // Captura o frame original
        if (frame_full.empty()) {
            std::cerr << "ERRO: Falha ao capturar frame para preview.\n";
            usleep(100000); // Pequeno atraso para evitar loop muito rápido em erro
            continue;
        }

        // Redimensiona para baixa resolução para preview
        cv::Mat frame_preview;
        cv::resize(frame_full, frame_preview, cv::Size(320, 240));

        // Desenha o quadrado da ROI no preview
        int rectSizePreview = 10 * (320 / cap.get(cv::CAP_PROP_FRAME_WIDTH)); // Escala o tamanho da ROI
        int x_preview = frame_preview.cols / 2;
        int y_preview = frame_preview.rows / 2;
        cv::Rect roi_preview(x_preview - rectSizePreview / 2, y_preview - rectSizePreview / 2, rectSizePreview, rectSizePreview);
        cv::rectangle(frame_preview, roi_preview, cv::Scalar(0, 255, 0), 2); // Desenha um quadrado verde

        cv::imshow("Preview", frame_preview); // Exibe o frame no preview
        
        // Verifica por tecla e atualiza a janela. cv::waitKey(1) é essencial para imshow funcionar.
        if (cv::waitKey(1) == 27) { // Se ESC for pressionado, encerra o programa (opcional)
            running = false;
        }

        if (leituraAtiva) {
            int h, s, v;
            std::string cor = processFrameHSV(cap, h, s, v); // Processa o frame original

            lcdClear();
            lcdSetCursor(0, 0);
            lcdPrint("Cor Detectada:");
            lcdSetCursor(1, 0);
            lcdPrint(cor); // Apenas a cor detectada

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
    cv::destroyWindow("Preview"); // Destroi a janela ao sair da thread
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

    lcdInit(); // Inicializa o LCD

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
        usleep(100000);
    }
    leituraAtiva = true;

    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("Camera...");
    lcdSetCursor(1, 0);
    lcdPrint("Inicializando");

    cv::VideoCapture cap(0); // Abre a câmera
    if (!cap.isOpened()) {
        lcdClear();
        lcdPrint("ERRO: Camera!");
        std::cerr << "ERRO FATAL: Nao foi possivel abrir a camera. Verifique a conexao e drivers.\n";
        running = false;
        usleep(5000000); // Mantém a mensagem de erro por mais tempo
        return 1;
    }

    // Define a resolução da câmera (antes de iniciar a thread)
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 320);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 240);

    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("Camera OK!");
    lcdSetCursor(1, 0);
    lcdPrint("Botao=PAUSA/INICIA");
    usleep(2000000);

    std::thread t(colorThread, std::ref(cap)); // Inicia a thread de cores e preview

    // Loop principal: gerencia o botao para pausar/resumir
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
        usleep(100000);
    }

    t.join(); // Espera a thread de cores terminar

    lcdClear();
    lcdPrint("Encerrando...");
    usleep(1000000);
    cap.release(); // Libera a câmera
    if (lcd_fd != -1) {
        close(lcd_fd); // Fecha o descritor do LCD
    }
    std::cout << "Programa finalizado com sucesso.\n";
    return 0;
}
