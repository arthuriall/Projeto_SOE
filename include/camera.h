#ifndef CAMERA_H
#define CAMERA_H

#include <opencv2/opencv.hpp>
#include <string>

bool openCamera(cv::VideoCapture& cap, int device = 0);
void closeCamera(cv::VideoCapture& cap);
void processFrame(cv::VideoCapture& cap, int offset);
std::string detectColor(const cv::Scalar& meanColor);

#endif
