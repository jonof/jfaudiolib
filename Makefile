-include Makefile.user

DXROOT ?= $(USERPROFILE)/sdks/directx/dx81

ifeq (0,$(RELEASE))
 OPTLEVEL=-Og
else
 OPTLEVEL=-O2
endif

CC?=gcc
AR?=ar
CFLAGS=-g $(OPTLEVEL) -Wall
CPPFLAGS=-Iinclude -Isrc

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

ifeq (mingw32,$(findstring mingw32,$(machine)))
 CPPFLAGS+= -I$(DXROOT)/include -Ithird-party/mingw32/include
 SOURCES+= src/driver_directsound.c src/driver_winmm.c

 CPPFLAGS+= -DHAVE_VORBIS
else
 ifeq (1,$(JFAUDIOLIB_HAVE_SDL))
  CPPFLAGS+= -DHAVE_SDL $(shell pkg-config --cflags sdl)
  ifeq (1,$(JFAUDIOLIB_USE_SDLMIXER))
   CPPFLAGS+= -DUSE_SDLMIXER
   SOURCES+= src/driver_sdlmixer.c
  else
   SOURCES+= src/driver_sdl.c
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
 ifeq (1,$(JFAUDIOLIB_HAVE_VORBIS))
  CPPFLAGS+= -DHAVE_VORBIS $(shell pkg-config --cflags vorbisfile)
 endif
endif

OBJECTS=$(SOURCES:%.c=%.o)

$(JFAUDIOLIB): $(OBJECTS)
	$(AR) cr $@ $^

$(OBJECTS): %.o: %.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

test: src/test.o $(JFAUDIOLIB);
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(JFAUDIOLIB_LDFLAGS)

.PHONY: clean
clean:
	-rm -f $(OBJECTS) $(JFAUDIOLIB)
