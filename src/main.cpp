#include <opencv2/opencv.hpp>
#include "camera.h"

int main() {
    cv::VideoCapture cap;

    if (!camera_abrir(cap, 640, 480)) return -1;
    camera_mostrar(cap);
    return 0;
}
