#ifndef DRIVERS_H
#define DRIVERS_H

#include "sndcards.h"

extern int ASS_SoundDriver;

int SoundDriver_IsSupported(int driver);

int SoundDriver_GetError(void);
const char * SoundDriver_ErrorString( int ErrorNumber );
int SoundDriver_Init(int mixrate, int numchannels, int samplebits);
void SoundDriver_Shutdown(void);
int SoundDriver_BeginPlayback( char *BufferStart,
			 int BufferSize, int NumDivisions, 
			 void ( *CallBackFunc )( void ) );
void SoundDriver_StopPlayback(void);

#endif