#include "camera.h"
#include <iostream>

Camera::Camera(int width, int height)
    : frameWidth(width), frameHeight(height) {}

bool Camera::open() {
    cap.open(0, cv::CAP_V4L2);  // força backend V4L2
    if (!cap.isOpened()) {
        std::cerr << "Erro ao abrir a câmera." << std::endl;
        return false;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, frameWidth);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, frameHeight);
    return true;
}

void Camera::show() {
    cv::Mat frame;
    while (true) {
        cap >> frame;
        if (frame.empty()) break;

        cv::imshow("Webcam", frame);
        if (cv::waitKey(1) == 27) break; // ESC
    }
}

void Camera::close() {
    cap.release();
    cv::destroyAllWindows();
}
