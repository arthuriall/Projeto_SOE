#include "camera.h"
#include <iostream>

bool openCamera(cv::VideoCapture& cap, int index) {
    cap.open(index);
    if (!cap.isOpened()) {
        std::cerr << "Erro ao abrir a câmera." << std::endl;
        return false;
    }
    return true;
}

void showCamera(cv::VideoCapture& cap) {
    cv::Mat frame;
    while (true) {
        cap >> frame;
        if (frame.empty()) {
            std::cerr << "Frame vazio!" << std::endl;
            break;
        }

        cv::imshow("Webcam", frame);
        if (cv::waitKey(30) == 'q') break;
    }
    cap.release();
    cv::destroyAllWindows();
}
