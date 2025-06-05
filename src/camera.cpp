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

    if (r > g && r > b) return "Vermelho";
    if (g > r && g > b) return "Verde";
    if (b > r && b > g) return "Azul";
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

    // OpenCV usa BGR
    b = static_cast<int>(meanColor[0]);
    g = static_cast<int>(meanColor[1]);
    r = static_cast<int>(meanColor[2]);

    cv::Vec3b meanPixel(b, g, r);

    std::string cor = detectColorRGB(meanPixel);

    if (cor != ultimaCor) {
        std::cout << "Cor detectada: " << cor << std::endl;
        ultimaCor = cor;
    }

    cv::rectangle(frame, roi, cv::Scalar(255, 0, 0), 2);
    cv::putText(frame, cor, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0,
                cv::Scalar(255, 255, 255), 2);

    // Redimensionar o frame antes de mostrar
    cv::Mat resizedFrame;
    double scale = 0.5;  // diminui para 50% do tamanho original, por exemplo
    cv::resize(frame, resizedFrame, cv::Size(), scale, scale);

    cv::imshow("Camera", resizedFrame);
    return cor;
}

