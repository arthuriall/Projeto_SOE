#ifndef CAMERA_H
#define CAMERA_H

#include <opencv2/opencv.hpp>

// Função para abrir a câmera, retorna true se abriu
bool camera_abrir(cv::VideoCapture &cap, int largura, int altura);

// Função para mostrar vídeo da câmera (loop)
void camera_mostrar(cv::VideoCapture &cap);

#endif
