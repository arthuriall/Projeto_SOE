#ifndef LASER_H
#define LASER_H

void configurarLaser(int pino);
void ligarLaser(int pino);
void desligarLaser(int pino);
void alternarLaser(int pino, bool& estado, int intervaloMs = 100);

#endif
