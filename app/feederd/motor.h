/*
 * Interface du moteur de distribution.
 *
 * feederd ne connaît que ces trois fonctions. L'implémentation est choisie
 * à la compilation : motor_sim.c (PC, simulation) ou, plus tard,
 * motor_gpio.c (Raspberry Pi, vrai moteur).
 */
#ifndef MOTOR_H
#define MOTOR_H

int motor_init(void);
int motor_dispense(unsigned int grams);	/* 0 si succès */
void motor_cleanup(void);

#endif
