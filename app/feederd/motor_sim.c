/* Moteur simulé : affiche ce que ferait le vrai moteur. */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>

#include "motor.h"

int motor_init(void)
{
	fprintf(stderr, "[motor-sim] init\n");
	return 0;
}

int motor_dispense(unsigned int grams)
{
	fprintf(stderr, "[motor-sim] distribution de %u g...\n", grams);
	sleep(1);
	fprintf(stderr, "[motor-sim] termine\n");
	return 0;
}

void motor_cleanup(void)
{
	fprintf(stderr, "[motor-sim] cleanup\n");
}
