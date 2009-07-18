#ifndef DRIVERS_H
#define DRIVERS_H

#include "sndcards.h"

typedef struct {
	int          (* GetError)();
	const char * (* ErrorString)( int ErrorNumber );
	int          (* Init)();
} ASS_SoundDriver;

extern int ASS_SoundDevice;
extern ASS_SoundDriver ASS_SoundDrivers[ASS_NumSoundCards];

#define SoundDriver_GetError    ASS_SoundDrivers[ASS_SoundDevice].GetError
#define SoundDriver_ErrorString ASS_SoundDrivers[ASS_SoundDevice].ErrorString
#define SoundDriver_Init        ASS_SoundDrivers[ASS_SoundDevice].Init

#endif