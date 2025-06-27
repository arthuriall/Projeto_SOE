#include <wiringPi.h>
#include <iostream>
#include <unistd.h>

#define BOTAO 7  // WiringPi 7 = GPIO4 = pino físico 7

int main() {
    if (wiringPiSetup() == -1) {
        std::cerr << "Erro ao iniciar WiringPi\n";
        return 1;
    }

    pinMode(BOTAO, INPUT);
    pullUpDnControl(BOTAO, PUD_UP); // Pull-up interno ativado

    std::cout << "Teste do botão no GPIO4 (WiringPi 7)\nPressione Ctrl+C para sair.\n";

    while (true) {
        if (digitalRead(BOTAO) == LOW) { // botão pressionado (ligado ao GND)
            std::cout << "Botão pressionado!\n";
            // Espera o botão ser solto para evitar mensagens repetidas
            while (digitalRead(BOTAO) == LOW) {
                usleep(100000);
            }
        }
        usleep(100000); // evita poluir a CPU
    }

    return 0;
}
