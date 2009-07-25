CC=gcc
CFLAGS=-g -O2 -Wall
CPPFLAGS=-Iinclude -Isrc -DHAVE_VORBIS

SOURCES=src/drivers.c \
        src/fx_man.c \
        src/multivoc.c \
        src/mix.c \
        src/mixst.c \
        src/pitch.c \
        src/vorbis.c \
        src/driver_nosound.c

ifneq (,$(findstring MINGW,$(shell uname -s)))
 CPPFLAGS+= -I/z/sdks/directx/dx7/include
 SOURCES+= src/driver_directsound.c
else
 CPPFLAGS+= -DHAVE_SDL
 SOURCES+= src/driver_sdl.c
endif

OBJECTS=$(SOURCES:%.c=%.o)

libjfaudiolib.a: $(OBJECTS)
	ar cr $@ $^

$(OBJECTS): %.o: %.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@
 
.PHONY: clean
clean:
	-rm -f $(OBJECTS) libjfaudiolib.a
