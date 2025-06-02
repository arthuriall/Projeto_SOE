#include "camera.h"
#include <iostream>

bool openCamera(cv::VideoCapture& cap, int device) {
    cap.open(device);
    return cap.isOpened();
}

void closeCamera(cv::VideoCapture& cap) {
    cap.release();
    cv::destroyAllWindows();
}

std::string detectColor(const cv::Scalar& meanColor) {
    int b = static_cast<int>(meanColor[0]);
    int g = static_cast<int>(meanColor[1]);
    int r = static_cast<int>(meanColor[2]);

    if (r >= 140 && g >= 140 && b <= 60) {
        return "Amarelo";
    }
    else if (r >= g && r >= b) {
        return "Vermelho";
    }
    else if (g >= r && g >= b) {
        return "Verde";
    }
    else {
        return "Indefinido";
    }
}

void processFrame(cv::VideoCapture& cap, int offset) {
    static std::string corChange = "";
    std::string corFinal = "";

    cv::Mat img;
    cap >> img;
    if (img.empty()) return;

    int h = img.rows;
    int w = img.cols;

    cv::Rect roi(offset, offset, w - 2*offset, h - 2*offset);
    cv::Mat campo = img(roi);

    cv::rectangle(img, roi, cv::Scalar(255, 0, 0), 3);

    cv::Scalar meanColor = cv::mean(campo);
    int b = static_cast<int>(meanColor[0]);
    int g = static_cast<int>(meanColor[1]);
    int r = static_cast<int>(meanColor[2]);

    std::cout << "RGB médio: (" << r << ", " << g << ", " << b << ")\n";

    corFinal = detectColor(meanColor);

    if (corFinal != corChange) {
        std::cout << "Cor detectada: " << corFinal << std::endl;
        corChange = corFinal;
    }

    cv::imshow("Camera", img);
}
