#ifndef CAMERA_H
#define CAMERA_H

#include <opencv2/opencv.hpp>

class Camera {
public:
    Camera(int width = 320, int height = 240);
    bool open();
    void show();
    void close();

private:
    cv::VideoCapture cap;
    int frameWidth;
    int frameHeight;
};

#endif
