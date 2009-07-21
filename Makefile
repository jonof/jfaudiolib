CC=gcc
CFLAGS=-g -O2 -Wall
CPPFLAGS=-Iinclude -Isrc -DHAVE_SDL

SOURCES=src/drivers.c \
        src/fx_man.c \
        src/multivoc.c \
		src/mix.c \
        src/pitch.c \
        src/driver_nosound.c \
        src/driver_sdl.c

OBJECTS=$(SOURCES:%.c=%.o)

libjfaudiolib.a: $(OBJECTS)
	ar cr $@ $^

$(OBJECTS): %.o: %.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@
 
.PHONY: clean
clean:
	-rm -f $(OBJECTS) libjfaudiolib.a
