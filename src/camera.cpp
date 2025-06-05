#include "camera.h"
#include <iostream>

bool openCamera(cv::VideoCapture& cap, int device) {
    return cap.open(device);
}

void closeCamera(cv::VideoCapture& cap) {
    cap.release();
    cv::destroyAllWindows();
}

std::string detectColor(const cv::Vec3b& pixel) {
    int b = pixel[0];
    int g = pixel[1];
    int r = pixel[2];

    if (r < 40 && g < 40 && b < 40)
        return "Preto";
    if (r > 200 && g > 200 && b > 200)
        return "Branco";
    if (abs(r - g) < 15 && abs(g - b) < 15 && r > 80 && r < 200)
        return "Cinza";

    if (r > 200 && g < 100 && b < 100)
        return "Vermelho";
    if (g > 200 && r < 100 && b < 100)
        return "Verde";
    if (b > 200 && r < 100 && g < 100)
        return "Azul";

    if (r > 200 && g > 200 && b < 100)
        return "Amarelo";
    if (g > 200 && b > 200 && r < 100)
        return "Ciano";
    if (r > 200 && b > 200 && g < 100)
        return "Magenta";

    if (r > 200 && g > 100 && g < 180 && b < 60)
        return "Laranja";

    return "Indefinido";
}

std::string processFrame(cv::VideoCapture& cap) {
    static std::string ultimaCor = "";

    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) return "";

    int x = frame.cols / 2;
    int y = frame.rows / 2;
    cv::Vec3b pixel = frame.at<cv::Vec3b>(y, x);
    std::string cor = detectColor(pixel);

    if (cor != ultimaCor) {
        std::cout << "Cor detectada: " << cor << "\n";
        ultimaCor = cor;
    }

    cv::circle(frame, cv::Point(x, y), 5, cv::Scalar(255, 0, 0), -1);
    cv::putText(frame, cor, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0,
                cv::Scalar(255, 255, 255), 2);

    cv::imshow("Camera", frame);
    return cor;
}