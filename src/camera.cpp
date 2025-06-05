#include "camera.h"
#include <iostream>

bool openCamera(cv::VideoCapture& cap, int device) {
    if (!cap.open(device)) {
        std::cerr << "Erro ao abrir a camera no dispositivo " << device << std::endl;
        return false;
    }
    return true;
}

void closeCamera(cv::VideoCapture& cap) {
    cap.release();
    cv::destroyAllWindows();
}

std::string detectColorRGB(const cv::Vec3b& pixel) {
    int b = pixel[0];
    int g = pixel[1];
    int r = pixel[2];

    if (r > 200 && g < 80 && b < 80) return "Vermelho";
    if (g > 200 && r < 80 && b < 80) return "Verde";
    if (b > 200 && r < 80 && g < 80) return "Azul";
    if (r > 200 && g > 200 && b < 80) return "Amarelo";
    if (r > 200 && g > 100 && b > 100) return "Rosa";
    if (r < 50 && g < 50 && b < 50) return "Preto";
    if (r > 200 && g > 200 && b > 200) return "Branco";
    return "Indefinido";
}

std::string processFrame(cv::VideoCapture& cap, int& r, int& g, int& b) {
    static std::string ultimaCor;

    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) return "";

    int rectSize = 20;
    int x = frame.cols / 2;
    int y = frame.rows / 2;
    cv::Rect roi(x - rectSize / 2, y - rectSize / 2, rectSize, rectSize);

    cv::Mat region = frame(roi);
    cv::Scalar meanColor = cv::mean(region);

    b = static_cast<int>(meanColor[0]);
    g = static_cast<int>(meanColor[1]);
    r = static_cast<int>(meanColor[2]);

    std::string cor = detectColorRGB(cv::Vec3b(b, g, r));

    if (cor != ultimaCor) {
        std::cout << "Cor detectada: " << cor << std::endl;
        ultimaCor = cor;
    }

    return cor;
}
