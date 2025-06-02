#include "camera.h"
#include <iostream>
#include <opencv2/opencv.hpp>

int main() {
    cv::VideoCapture cap;
    int offset = 100;

    if (!openCamera(cap)) {
        std::cerr << "Erro ao abrir a câmera\n";
        return -1;
    }

    while (true) {
        processFrame(cap, offset);
        if (cv::waitKey(1) == 27) break;  // ESC para sair
    }

    closeCamera(cap);
    return 0;
}
