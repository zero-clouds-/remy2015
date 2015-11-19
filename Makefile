CC=gcc
CFLAGS=-Wall

SSRC=$(wildcard server/*.c)
SOBJ=$(SSRC:.c=.o)

CSRC=$(wildcard client/*.c)
COBJ=$(CSRC:.c=.o)

LDLIBS=-lm

all: robot_server robot_client

$(SOBJ):$(wildcard server/*.h) Makefile
$(COBJ):$(wildcard client/*.h) Makefile

robot_server: $(SOBJ)
	$(LINK.c) $^ $(LDLIBS) -o $@
robot_client: $(COBJ)
	$(LINK.c) $^ $(LDLIBS) -o $@

.PHONY: clean
clean:
	rm $(EXE) $(OBJ)