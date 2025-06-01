#ifndef CAMERA_H
#define CAMERA_H

#include <opencv2/opencv.hpp>

bool openCamera(cv::VideoCapture& cap, int index = 0);

void showCamera(cv::VideoCapture& cap);

#endif
