# CC, CFLAGS et LDFLAGS peuvent être fournis par Buildroot (compilation croisée).
CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wextra -std=c11

# Implémentation du moteur : sim (PC) ou gpio (Raspberry Pi, plus tard)
MOTOR ?= sim

OBJS = feederd.o motor_$(MOTOR).o

feederd: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c motor.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f feederd *.o feederd.state feederd.state.tmp

.PHONY: clean
