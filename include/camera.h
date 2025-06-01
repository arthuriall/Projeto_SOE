#ifndef CAMERA_H
#define CAMERA_H

#include <atomic>
#include <thread>
#include <opencv2/opencv.hpp>

class Camera {
public:
    Camera(int width = 320, int height = 240);
    ~Camera();

    void start();
    void stop();

private:
    void captureLoop();

    std::atomic<bool> running;
    std::thread captureThread;
    int frameWidth;
    int frameHeight;
};

#endif
