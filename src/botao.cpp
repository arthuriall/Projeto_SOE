#include "botao.h"
#include <wiringPi.h>

void configurarBotao(int pino) {
    pinMode(pino, INPUT);
    pullUpDnControl(pino, PUD_UP);  // Ativa pull-up interno
}

bool verificarPressionado(int pino, bool& ultimoEstado) {
    bool estadoAtual = digitalRead(pino);
    bool pressionado = (ultimoEstado == HIGH && estadoAtual == LOW);
    ultimoEstado = estadoAtual;
    return pressionado;
}
