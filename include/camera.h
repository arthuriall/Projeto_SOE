#ifndef CAMERA_H
#define CAMERA_H

#include <opencv2/opencv.hpp>
#include <string>

bool openCamera(cv::VideoCapture& cap, int device = 0);
void closeCamera(cv::VideoCapture& cap);
std::string detectColorRGB(const cv::Vec3b& pixel);
std::string processFrame(cv::VideoCapture& cap, int& r, int& g, int& b);

#endif // CAMERA_H
