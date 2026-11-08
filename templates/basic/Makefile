RUNTIME ?= $(firstword $(wildcard ../../runtime $(HOME)/.local/share/tinyhat-bootsplash/runtime /usr/share/tinyhat-bootsplash/runtime))
CFLAGS ?= -O2 -Wall
ifeq ($(origin CC),default)
CC := $(if $(shell command -v cc 2>/dev/null),cc,tcc)
endif

anim: anim.c $(RUNTIME)/draw.c $(RUNTIME)/main.c $(RUNTIME)/anim.h
	$(CC) $(CFLAGS) -I$(RUNTIME) -o $@ anim.c $(RUNTIME)/draw.c $(RUNTIME)/main.c -lm

run: anim
	tinyhat-boot-animations --preview .

clean:
	$(RM) anim

.PHONY: run clean
