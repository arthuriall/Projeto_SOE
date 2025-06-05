
// main.cpp
#include "camera.h"
#include "lcd.h"
#include <wiringPi.h>
#include <opencv2/opencv.hpp>
#include <iostream>

int main() {
    wiringPiSetup();
    lcdInit();

    cv::VideoCapture cap;
    if (!openCamera(cap)) {
        std::cerr << "Erro ao abrir a camera\n";
        return -1;
    }

    std::string ultimaCor = "";

    while (true) {
        std::string cor = processFrame(cap);
        if (!cor.empty() && cor != ultimaCor) {
            lcdClear();
            lcdSetCursor(0, 0);
            lcdPrint("Cor detectada:");
            lcdSetCursor(1, 0);
            lcdPrint(cor);
            ultimaCor = cor;
        }

        if (cv::waitKey(1) == 27) break; // ESC
    }

    closeCamera(cap);
    return 0;
}
