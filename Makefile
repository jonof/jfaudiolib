-include Makefile.user

ifeq (0,$(RELEASE))
 OPTLEVEL=-Og
else
 OPTLEVEL=-fomit-frame-pointer -O2
endif

CC?=gcc
AR?=ar
CFLAGS=-g $(OPTLEVEL) -W -Wall
JFAUDIOLIB_CPPFLAGS=-Iinclude -Isrc
JFAUDIOLIB_CFLAGS=-std=c99
JFAUDIOLIB_LDFLAGS=
o=o

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

ifeq (mingw32,$(findstring mingw32,$(TARGETMACHINE)))
 SOURCES+= src/driver_directsound.c src/driver_winmm.c
 JFAUDIOLIB_CPPFLAGS+= -Ithird-party/mingw/include -DHAVE_VORBIS
else
 ifeq (-darwin,$(findstring -darwin,$(TARGETMACHINE)))
  SOURCES+= src/driver_coreaudio.c
  JFAUDIOLIB_LDFLAGS+= -framework Foundation
 endif
 ifneq (0,$(JFAUDIOLIB_HAVE_SDL))
  JFAUDIOLIB_CPPFLAGS+= -DHAVE_SDL=2 $(shell $(SDL2CONFIG) --cflags)
  ifeq (1,$(JFAUDIOLIB_USE_SDLMIXER))
   JFAUDIOLIB_CPPFLAGS+= -DUSE_SDLMIXER
   SOURCES+= src/driver_sdlmixer.c
  else
   SOURCES+= src/driver_sdl.c
  endif
 endif
 ifeq (1,$(JFAUDIOLIB_HAVE_ALSA))
  JFAUDIOLIB_CPPFLAGS+= -DHAVE_ALSA $(shell $(PKGCONFIG) --cflags alsa)
  SOURCES+= src/driver_alsa.c
 endif
 ifeq (1,$(JFAUDIOLIB_HAVE_FLUIDSYNTH))
  JFAUDIOLIB_CPPFLAGS+= -DHAVE_FLUIDSYNTH $(shell $(PKGCONFIG) --cflags fluidsynth)
  SOURCES+= src/driver_fluidsynth.c
 endif
 ifeq (1,$(JFAUDIOLIB_HAVE_VORBIS))
  JFAUDIOLIB_CPPFLAGS+= -DHAVE_VORBIS $(shell $(PKGCONFIG) --cflags vorbisfile)
 endif
endif

OBJECTS=$(SOURCES:%.c=%.o)

.PHONY: all
all: $(JFAUDIOLIB) test

include Makefile.deps

$(JFAUDIOLIB): $(OBJECTS)
	$(AR) cr $@ $^

$(OBJECTS) src/test.o: %.o: %.c
	$(CC) -c $(JFAUDIOLIB_CPPFLAGS) $(CPPFLAGS) $(CFLAGS) $(JFAUDIOLIB_CFLAGS) $< -o $@

test: src/test.o $(JFAUDIOLIB);
	$(CC) $(JFAUDIOLIB_CPPFLAGS) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(JFAUDIOLIB_LDFLAGS) -lm

.PHONY: clean
clean:
	-rm -f $(OBJECTS) $(JFAUDIOLIB)
