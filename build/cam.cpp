#include <iostream>
#include <string>
#include <vector>
#include <unistd.h> // Para usleep
#include <cstdio>   // Para snprintf

// Headers do OpenCV
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

// Header do WiringPi (para Raspberry Pi)
#include <wiringPi.h>

// --- Início do Código do LCD ---
// Definições dos pinos do LCD (use números BCM GPIO se usar wiringPiSetupGpio())
#define LCD_RS 0  // Pino RS do LCD conectado ao GPIO 0 (BCM)
#define LCD_E  1  // Pino E  do LCD conectado ao GPIO 1 (BCM)
#define LCD_D4 4  // Pino D4 do LCD conectado ao GPIO 4 (BCM)
#define LCD_D5 5  // Pino D5 do LCD conectado ao GPIO 5 (BCM)
#define LCD_D6 6  // Pino D6 do LCD conectado ao GPIO 6 (BCM)
#define LCD_D7 7  // Pino D7 do LCD conectado ao GPIO 7 (BCM)

// Protótipos das funções do LCD (se definidas após o uso, ou para clareza)
void pulseEnable();
void sendNibble(int data);
void sendByte(int data, int mode);
void lcdInit();
void lcdClear();
void lcdSetCursor(int row, int col);
void lcdPrint(const std::string& text);

void pulseEnable() {
    digitalWrite(LCD_E, HIGH);
    usleep(500); // 500 microssegundos
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
    digitalWrite(LCD_RS, mode); // mode=0 para comando, mode=1 para dado (char)
    sendNibble(data >> 4);      // Envia o nibble mais significativo
    sendNibble(data & 0x0F);    // Envia o nibble menos significativo
    usleep(100); // Pequena espera após enviar byte
}

void lcdInit() {
    pinMode(LCD_RS, OUTPUT);
    pinMode(LCD_E, OUTPUT);
    pinMode(LCD_D4, OUTPUT);
    pinMode(LCD_D5, OUTPUT);
    pinMode(LCD_D6, OUTPUT);
    pinMode(LCD_D7, OUTPUT);

    usleep(50000);      // Espera >15ms após VCC subir para 4.5V
    sendNibble(0x03); usleep(4500); // Espera >4.1ms
    sendNibble(0x03); usleep(4500); // Espera >4.1ms
    sendNibble(0x03); usleep(150);  // Espera >100us
    sendNibble(0x02);      // Define para modo de 4 bits

    sendByte(0x28, 0); // Função Set: DL=0 (4 bits), N=1 (2 linhas), F=0 (fonte 5x8)
    sendByte(0x0C, 0); // Display Control: D=1 (display ON), C=0 (cursor OFF), B=0 (blink OFF)
    sendByte(0x06, 0); // Entry Mode Set: I/D=1 (incrementa cursor), S=0 (sem shift)
    sendByte(0x01, 0); // Clear Display
    usleep(2000);      // Comando clear display leva mais tempo
}

void lcdClear() {
    sendByte(0x01, 0); // Comando para limpar o display
    usleep(2000);      // Este comando leva mais tempo
}

void lcdSetCursor(int row, int col) {
    // Endereços base para um LCD 16x2 ou 20x2 (ou primeiras 2 linhas de um 20x4)
    // Linha 0: 0x00, Linha 1: 0x40
    // Para um LCD 20x4, Linha 2: 0x14, Linha 3: 0x54 (não usado neste exemplo simples)
    int rowAddr[] = {0x00, 0x40}; // Para LCD 16x2 ou 20x2
    if (row < 0) row = 0;
    if (row > 1) row = 1; // Limita a 2 linhas para este exemplo

    sendByte(0x80 | (rowAddr[row] + col), 0); // Comando para definir posição do cursor (DDRAM address)
}

void lcdPrint(const std::string& text) {
    for (char c : text) {
        sendByte(c, 1); // Envia cada caractere como dado
    }
}
// --- Fim do Código do LCD ---


// --- Início do Código da Câmera OpenCV ---
bool openCamera(cv::VideoCapture& cap, int device) {
    if (!cap.open(device)) {
        std::cerr << "ERRO: Nao foi possivel abrir a camera no dispositivo " << device << std::endl;
        return false;
    }
    std::cout << "Camera aberta no dispositivo: " << device << std::endl;
    return true;
}

void closeCamera(cv::VideoCapture& cap) {
    if (cap.isOpened()) {
        cap.release();
    }
    cv::destroyAllWindows();
    std::cout << "Camera fechada e janelas destruidas." << std::endl;
}

std::string detectColorRGB(const cv::Vec3b& pixel) {
    int b = pixel[0];
    int g = pixel[1];
    int r = pixel[2];

    if (r > 180 && g < 100 && b < 100) return "Vermelho";
    if (g > 180 && r < 100 && b < 100) return "Verde";
    if (b > 180 && r < 100 && g < 100) return "Azul";
    
    return "Indefinido";
}

std::string processFrame(cv::VideoCapture& cap, cv::Mat& currentFrame, int& r_val, int& g_val, int& b_val) {
    static std::string ultimaCorDetectada = "Indefinido";
    r_val = 0; g_val = 0; b_val = 0; // Reseta os valores RGB para este frame

    cap >> currentFrame;
    if (currentFrame.empty()) {
        return ""; 
    }

    int rectSize = 30;
    int roi_x = currentFrame.cols / 2 - rectSize / 2;
    int roi_y = currentFrame.rows / 2 - rectSize / 2;

    if (roi_x < 0 || roi_y < 0 || roi_x + rectSize > currentFrame.cols || roi_y + rectSize > currentFrame.rows) {
        return "Indefinido"; 
    }
    cv::Rect roi(roi_x, roi_y, rectSize, rectSize);
    cv::Mat region = currentFrame(roi);
    cv::Scalar meanColor = cv::mean(region);

    b_val = static_cast<int>(meanColor[0]);
    g_val = static_cast<int>(meanColor[1]);
    r_val = static_cast<int>(meanColor[2]);

    std::string corAtual = detectColorRGB(cv::Vec3b(static_cast<unsigned char>(b_val),
                                                   static_cast<unsigned char>(g_val),
                                                   static_cast<unsigned char>(r_val)));

    if (corAtual != ultimaCorDetectada) {
        if (corAtual != "Indefinido") {
            std::cout << "Cor detectada: " << corAtual << " (R:" << r_val << ", G:" << g_val << ", B:" << b_val << ")" << std::endl;
        } else if (ultimaCorDetectada != "Indefinido") {
            std::cout << "Cor detectada: Indefinido (R:" << r_val << ", G:" << g_val << ", B:" << b_val << ")" << std::endl;
        }
        ultimaCorDetectada = corAtual;
    }
    
    cv::rectangle(currentFrame, roi, cv::Scalar(0, 255, 0), 2);
    return corAtual;
}
// --- Fim do Código da Câmera OpenCV ---


int main() {
    // Inicializa wiringPi (IMPORTANTE: ESCOLHA A CORRETA!)
    // Use wiringPiSetup() para numeração wiringPi, wiringPiSetupGpio() para BCM, wiringPiSetupPhys() para física.
    // Os pinos definidos (RS 0, E 1, D4 4, etc.) devem corresponder ao esquema de numeração escolhido.
    // Usando BCM GPIO como exemplo:
    if (wiringPiSetupGpio() == -1) {
        std::cerr << "ERRO: Falha ao inicializar wiringPi (GPIO)." << std::endl;
        return 1;
    }
    std::cout << "WiringPi (GPIO) inicializado." << std::endl;

    // Inicializa o LCD
    lcdInit();
    std::cout << "LCD inicializado." << std::endl;
    lcdPrint("Iniciando...");
    usleep(1000000); // Espera 1 segundo
    lcdClear();


    cv::VideoCapture cameraCap;
    int cameraID = 0; 

    if (!openCamera(cameraCap, cameraID)) {
        lcdClear();
        lcdPrint("Erro Camera!");
        return -1;
    }

    cameraCap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cameraCap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    double actual_width = cameraCap.get(cv::CAP_PROP_FRAME_WIDTH);
    double actual_height = cameraCap.get(cv::CAP_PROP_FRAME_HEIGHT);
    std::cout << "Resolucao atual da camera: " << actual_width << " x " << actual_height << std::endl;
    
    char lcd_line_buffer[17]; // Buffer para 16 caracteres + terminador nulo
    snprintf(lcd_line_buffer, sizeof(lcd_line_buffer), "Res:%.0fx%.0f", actual_width, actual_height);
    lcdSetCursor(0,0);
    lcdPrint(std::string(lcd_line_buffer).substr(0,16)); // Garante que não excede 16 chars
    lcdSetCursor(1,0);
    lcdPrint("Aguardando cor...");
    

    // Só cria a janela se não for um ambiente headless (sem display X11)
    // Em um Raspberry Pi sem desktop, cv::namedWindow e cv::imshow podem falhar ou não ser úteis.
    // Você pode comentar estas linhas se estiver rodando headless.
    #ifndef HEADLESS_MODE 
        cv::namedWindow("Camera Feed", cv::WINDOW_AUTOSIZE);
        std::cout << "Pressione 'ESC' ou 'q' na janela da camera para sair." << std::endl;
    #else
        std::cout << "Rodando em modo headless. Pressione Ctrl+C no terminal para sair." << std::endl;
    #endif


    cv::Mat frame;
    int r_val = 0, g_val = 0, b_val = 0;

    while (true) {
        std::string corDetectadaStr = processFrame(cameraCap, frame, r_val, g_val, b_val);

        if (frame.empty()) {
            if (!cameraCap.isOpened()) {
                std::cerr << "ERRO: Camera desconectada ou parou de funcionar." << std::endl;
                lcdClear();
                lcdPrint("Erro Cam!");
                break;
            }
            usleep(30000); // Pequena pausa se o frame estiver vazio mas a câmera ainda aberta
            continue; 
        }
        
        // Atualiza LCD
        // Linha 1: Nome da Cor
        std::string displayTextLine1 = "Cor: ";
        if (!corDetectadaStr.empty() && corDetectadaStr != "Indefinido") {
            displayTextLine1 += corDetectadaStr;
        } else if (corDetectadaStr == "Indefinido") {
            displayTextLine1 += "Nenhuma";
        } else { // frame estava vazio ou erro em processFrame
            displayTextLine1 += "---";
        }
        snprintf(lcd_line_buffer, sizeof(lcd_line_buffer), "%-16s", displayTextLine1.c_str()); // Preenche com espaços
        lcdSetCursor(0, 0);
        lcdPrint(lcd_line_buffer);

        // Linha 2: Valores RGB
        snprintf(lcd_line_buffer, sizeof(lcd_line_buffer), "R%3d G%3d B%3d", r_val, g_val, b_val); // Formata para RXXX GYYY BZZZ (14 chars)
        lcdSetCursor(1, 0);
        lcdPrint(std::string(lcd_line_buffer).substr(0,16)); // Garante que não excede 16 chars
        

        #ifndef HEADLESS_MODE
            cv::imshow("Camera Feed", frame);
            int key = cv::waitKey(30); 
            if (key == 27 || key == 'q' || key == 'Q') {
                std::cout << "Tecla pressionada. Saindo..." << std::endl;
                break;
            }
        #else
            // Se em modo headless, podemos adicionar um delay ou outra lógica de saída
            usleep(30000); // Pequeno delay para não sobrecarregar a CPU
        #endif
    }

    closeCamera(cameraCap);
    lcdClear();
    lcdPrint("Desligando...");
    usleep(1000000);
    lcdClear();
    // Opcional: desligar pinos do LCD ou definir como entrada
    // digitalWrite(LCD_RS, LOW); ... etc.

    return 0;
}