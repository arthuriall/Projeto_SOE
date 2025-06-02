#include "camera.h"
#include <iostream>

bool camera_abrir(cv::VideoCapture &cap, int largura, int altura) {
    cap.open(0);
    if (!cap.isOpened()) {
        std::cerr << "Erro ao abrir a câmera\n";
        return false;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH, largura);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, altura);
    return true;
}

void camera_mostrar(cv::VideoCapture &cap) {
    cv::namedWindow("Webcam", cv::WINDOW_NORMAL);
    cv::resizeWindow("Webcam", 640, 480);

    cv::Mat frame;
    while (true) {
        cap >> frame;
        if (frame.empty()) break;
        cv::imshow("Webcam", frame);
        if (cv::waitKey(30) == 27) break; // ESC para sair
    }
}
