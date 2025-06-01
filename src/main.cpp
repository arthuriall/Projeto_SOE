#include "camera.h"

int main() {
    Camera cam(320, 240);
    if (!cam.open()) return -1;

    cam.show();
    cam.close();

    return 0;
}
