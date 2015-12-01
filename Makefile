CC=gcc
CFLAGS=-Wall -g

SSRC=$(wildcard server/*.c)
SOBJ=$(SSRC:.c=.o)

CSRC=$(wildcard client/*.c)
COBJ=$(CSRC:.c=.o)

PSRC=$(wildcard protocol/*c)
POBJ=$(PSRC:.c=.o)

LDLIBS=-lm

all: robot_server robot_client

$(SOBJ):$(wildcard server/*.h) Makefile
$(COBJ):$(wildcard client/*.h) Makefile
$(POBJ):$(wildcard protocol/*.h) Makefile

robot_server: $(SOBJ) $(POBJ)
	$(LINK.c) $^ $(LDLIBS) -o $@
robot_client: $(COBJ) $(POBJ)
	$(LINK.c) $^ $(LDLIBS) -o $@

.PHONY: clean
clean:
	rm $(EXE) $(SOBJ) $(COBJ) $(POBJ)
