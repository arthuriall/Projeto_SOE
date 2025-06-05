#ifndef CAMERA_H
#define CAMERA_H

#include <opencv2/opencv.hpp>
#include <string>

bool openCamera(cv::VideoCapture& cap, int device = 0);
void closeCamera(cv::VideoCapture& cap);
std::string processFrame(cv::VideoCapture& cap);
std::string detectColor(const cv::Vec3b& pixel);

#endif
