#include "camera.h"

int main() {
    cv::VideoCapture cap;

    if (!openCamera(cap)) return -1;

    showCamera(cap);

    return 0;
}
