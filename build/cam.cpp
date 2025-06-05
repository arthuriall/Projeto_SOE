#include <iostream> // Para std::cout, std::cerr, std::endl
#include <string>   // Para std::string
#include <vector>   // Para std::vector (usado por cv::Vec3b internamente)

// Headers do OpenCV
#include <opencv2/core.hpp>     // Para cv::Mat, cv::Scalar, cv::Rect, cv::Vec3b
#include <opencv2/highgui.hpp>  // Para cv::namedWindow, cv::imshow, cv::waitKey, cv::destroyAllWindows
#include <opencv2/imgproc.hpp>  // Para cv::mean, cv::rectangle
#include <opencv2/videoio.hpp>  // Para cv::VideoCapture

// Declaração das funções antes do main, ou coloque as definições antes do main

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
    int b = pixel[0]; // Componente Azul
    int g = pixel[1]; // Componente Verde
    int r = pixel[2]; // Componente Vermelho

    // Thresholds para cores primárias. Ajuste conforme necessário.
    // Vermelho: R alto, G e B baixos
    if (r > 180 && g < 100 && b < 100) return "Vermelho";
    // Verde: G alto, R e B baixos
    if (g > 180 && r < 100 && b < 100) return "Verde";
    // Azul: B alto, R e G baixos
    if (b > 180 && r < 100 && g < 100) return "Azul";
    
    return "Indefinido";
}

std::string processFrame(cv::VideoCapture& cap, cv::Mat& currentFrame, int& r_val, int& g_val, int& b_val) {
    static std::string ultimaCorDetectada = "Indefinido";

    cap >> currentFrame; // Captura um novo frame da camera
    if (currentFrame.empty()) {
        // std::cerr << "ERRO: Frame capturado esta vazio em processFrame." << std::endl; // Descomente para debug
        return ""; // Retorna string vazia se o frame estiver vazio
    }

    int rectSize = 30; // Tamanho do quadrado da Regiao de Interesse (ROI)
    
    // Calcula o centro do frame atual para o ROI
    int roi_x = currentFrame.cols / 2 - rectSize / 2;
    int roi_y = currentFrame.rows / 2 - rectSize / 2;

    // Garante que o ROI esteja dentro dos limites do frame
    if (roi_x < 0 || roi_y < 0 || roi_x + rectSize > currentFrame.cols || roi_y + rectSize > currentFrame.rows) {
        // std::cerr << "ERRO: ROI (" << rectSize << "x" << rectSize << ") fora dos limites do frame ("
        //           << currentFrame.cols << "x" << currentFrame.rows << ")." << std::endl; // Descomente para debug
        return "Indefinido"; // Retorna indefinido se o ROI for inválido
    }
    cv::Rect roi(roi_x, roi_y, rectSize, rectSize);

    cv::Mat region = currentFrame(roi);      // Extrai a ROI
    cv::Scalar meanColor = cv::mean(region); // Calcula a cor média na ROI

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
    
    cv::rectangle(currentFrame, roi, cv::Scalar(0, 255, 0), 2); // Desenha retângulo verde da ROI

    return corAtual;
}

int main() {
    cv::VideoCapture cameraCap;
    int cameraID = 0; 

    if (!openCamera(cameraCap, cameraID)) {
        std::cerr << "Falha ao abrir a camera. Verifique o ID da camera e as permissoes." << std::endl;
        return -1;
    }

    // Tenta definir a largura do frame para 640 e altura para 480
    bool cap_width_set = cameraCap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    bool cap_height_set = cameraCap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    if (!cap_width_set) {
        std::cout << "AVISO: Nao foi possivel definir a largura do frame para 640." << std::endl;
    }
    if (!cap_height_set) {
        std::cout << "AVISO: Nao foi possivel definir a altura do frame para 480." << std::endl;
    }

    // Verifica e imprime a resolução real obtida
    double actual_width = cameraCap.get(cv::CAP_PROP_FRAME_WIDTH);
    double actual_height = cameraCap.get(cv::CAP_PROP_FRAME_HEIGHT);
    std::cout << "Resolucao atual da camera: " << actual_width << " x " << actual_height << std::endl;


    cv::namedWindow("Camera Feed", cv::WINDOW_AUTOSIZE);
    std::cout << "Pressione 'ESC' ou 'q' para sair." << std::endl;

    cv::Mat frame;
    int r, g, b;

    while (true) {
        std::string corDetectada = processFrame(cameraCap, frame, r, g, b);

        if (frame.empty()) {
            // std::cout << "Frame vazio recebido no loop principal, tentando continuar..." << std::endl; // Descomente para debug
            // Adicionado para evitar loop infinito se a câmera parar de enviar frames
            if (!cameraCap.isOpened()) {
                std::cerr << "ERRO: Camera desconectada ou parou de funcionar." << std::endl;
                break;
            }
            int key = cv::waitKey(30); // Pequena pausa e verificação de tecla
             if (key == 27 || key == 'q' || key == 'Q') {
                std::cout << "Saindo..." << std::endl;
                break;
            }
            continue; 
        }
        
        cv::imshow("Camera Feed", frame);

        int key = cv::waitKey(30); 

        if (key == 27 || key == 'q' || key == 'Q') {
            std::cout << "Tecla ESC ou Q/q pressionada. Saindo..." << std::endl;
            break;
        }
    }

    closeCamera(cameraCap);

    return 0;
}