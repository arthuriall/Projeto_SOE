#include "camera.h"
#include <iostream>

int main() {
    cv::VideoCapture cap;

    if (!openCamera(cap)) {
        std::cerr << "Erro ao abrir a camera\n";
        return -1;
    }

    while (true) {
        processFrame(cap);
        if (cv::waitKey(1) == 27) break;  // ESC para sair
    }

    closeCamera(cap);
    return 0;
}
