CC=gcc
CFLAGS=-g -O2 -Wall
CPPFLAGS=-Iinclude -Isrc

USE_SDLMIXER=1

SOURCES=src/drivers.c \
        src/fx_man.c \
        src/cd.c \
        src/multivoc.c \
        src/mix.c \
        src/mixst.c \
        src/pitch.c \
        src/vorbis.c \
        src/music.c \
        src/midi.c \
        src/driver_nosound.c \
        src/asssys.c
		
include Makefile.shared

ifeq (1,$(JFAUDIOLIB_HAVE_SDL))
 CPPFLAGS+= -DHAVE_VORBIS $(shell pkg-config --cflags vorbisfile)
endif

ifneq (,$(findstring MINGW,$(shell uname -s)))
 CPPFLAGS+= -I/z/sdks/directx/dx7/include -Ithird-party/mingw32/include
 SOURCES+= src/driver_directsound.c src/driver_winmm.c
else
 ifeq (1,$(JFAUDIOLIB_HAVE_SDL))
  CPPFLAGS+= -DHAVE_SDL $(shell pkg-config --cflags sdl)
  SOURCES+= src/driver_sdl.c
  ifeq (1,$(USE_SDLMIXER))
   CPPFLAGS+= -DUSE_SDLMIXER
  endif
 endif
 ifeq (1,$(JFAUDIOLIB_HAVE_ALSA))
  CPPFLAGS+= -DHAVE_ALSA $(shell pkg-config --cflags alsa)
  SOURCES+= src/driver_alsa.c
 endif
 ifeq (1,$(JFAUDIOLIB_HAVE_FLUIDSYNTH))
  CPPFLAGS+= -DHAVE_FLUIDSYNTH $(shell pkg-config --cflags fluidsynth)
  SOURCES+= src/driver_fluidsynth.c
 endif
endif

OBJECTS=$(SOURCES:%.c=%.o)

$(JFAUDIOLIB): $(OBJECTS)
	ar cr $@ $^

$(OBJECTS): %.o: %.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@
 
.PHONY: clean
clean:
	-rm -f $(OBJECTS) $(JFAUDIOLIB)
