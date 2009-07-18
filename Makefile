CC=gcc
CFLAGS=-g -O2 -Wall
CPPFLAGS=-Iinclude -Isrc

SOURCES=src/drivers.c \
        src/fx_man.c \
        src/multivoc.c \
        src/driver_nosound.c \
        src/driver_coreaudio.c

OBJECTS=$(SOURCES:%.c=%.o)

jfaudiolib.a: $(OBJECTS)
	ar cr $@ $^

$(OBJECTS): %.o: %.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@
 