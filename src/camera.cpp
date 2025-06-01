#include "camera.h"
#include <iostream>

Camera::Camera(int width, int height)
    : running(false), frameWidth(width), frameHeight(height) {}

Camera::~Camera() {
    stop();
}

void Camera::start() {
    if (running) return;
    running = true;
    captureThread = std::thread(&Camera::captureLoop, this);
}

void Camera::stop() {
    if (running) {
        running = false;
        if (captureThread.joinable())
            captureThread.join();
    }
}

void Camera::captureLoop() {
    cv::VideoCapture cap(0);
    cap.set(cv::CAP_PROP_FRAME_WIDTH, frameWidth);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, frameHeight);

    if (!cap.isOpened()) {
        std::cerr << "Erro ao abrir a câmera." << std::endl;
        return;
    }

    cv::Mat frame;
    while (running) {
        cap >> frame;
        if (frame.empty()) break;

        cv::imshow("Webcam", frame);
        if (cv::waitKey(1) == 27) { // ESC
            running = false;
            break;
        }
    }

    cap.release();
    cv::destroyAllWindows();
}
