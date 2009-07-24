/*
 Copyright (C) 2009 Jonathon Fowler <jf@jonof.id.au>
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 
 See the GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
 */
 
/**
 * libSDL output driver for MultiVoc
 */


#include <SDL/SDL.h>
#include "driver_sdl.h"

enum {
   SDLErr_Warning = -2,
   SDLErr_Error   = -1,
   SDLErr_Ok      = 0,
   SDLErr_Uninitialised,
   SDLErr_InitSubSystem,
   SDLErr_OpenAudio
};

static int ErrorCode = SDLErr_Ok;
static int Initialised = 0;
static int Playing = 0;
static int StartedSDL = -1;

static char *MixBuffer = 0;
static int MixBufferSize = 0;
static int MixBufferCount = 0;
static int MixBufferCurrent = 0;
static int MixBufferUsed = 0;
static void ( *MixCallBack )( void ) = 0;

static void fillData(void * userdata, Uint8 * ptr, int remaining)
{
	int len;
	char *sptr;

	while (remaining > 0) {
		if (MixBufferUsed == MixBufferSize) {
			MixCallBack();
			
			MixBufferUsed = 0;
			MixBufferCurrent++;
			if (MixBufferCurrent >= MixBufferCount) {
				MixBufferCurrent -= MixBufferCount;
			}
		}
		
		while (remaining > 0 && MixBufferUsed < MixBufferSize) {
			sptr = MixBuffer + (MixBufferCurrent * MixBufferSize) + MixBufferUsed;
			
			len = MixBufferSize - MixBufferUsed;
			if (remaining < len) {
				len = remaining;
			}
			
			memcpy(ptr, sptr, len);
			
			ptr += len;
			MixBufferUsed += len;
			remaining -= len;
		}
	}
}


int SDLDrv_GetError(void)
{
    return ErrorCode;
}

const char *SDLDrv_ErrorString( int ErrorNumber )
{
    const char *ErrorString;
	
    switch( ErrorNumber ) {
        case SDLErr_Warning :
        case SDLErr_Error :
            ErrorString = SDLDrv_ErrorString( ErrorCode );
            break;

        case SDLErr_Ok :
            ErrorString = "SDL Audio ok.";
            break;
			
        case SDLErr_Uninitialised:
            ErrorString = "SDL Audio uninitialised.";
            break;

        case SDLErr_InitSubSystem:
            ErrorString = "SDL Audio: error in Init or InitSubSystem.";
            break;

        case SDLErr_OpenAudio:
            ErrorString = "SDL Audio: error in OpenAudio.";
            break;

        default:
            ErrorString = "Unknown SDL Audio error code.";
            break;
    }

    return ErrorString;
}

int SDLDrv_Init(int mixrate, int numchannels, int samplebits, void * initdata)
{
    Uint32 inited;
    Uint32 err = 0;
    SDL_AudioSpec spec;

    if (Initialised) {
        SDLDrv_Shutdown();
    }

    inited = SDL_WasInit(SDL_INIT_EVERYTHING);

    if (inited == 0) {
        // nothing was initialised
        err = SDL_Init(SDL_INIT_AUDIO);
        StartedSDL = 0;
    } else if (inited & SDL_INIT_AUDIO) {
        err = SDL_InitSubSystem(SDL_INIT_AUDIO);
        StartedSDL = 1;
    }

    if (err < 0) {
        ErrorCode = SDLErr_InitSubSystem;
        return SDLErr_Error;
    }

    spec.freq = mixrate;
    spec.format = (samplebits == 8) ? AUDIO_U8 : AUDIO_S16SYS;
    spec.channels = numchannels;
    spec.samples = 256;
    spec.callback = fillData;
    spec.userdata = 0;

    err = SDL_OpenAudio(&spec, NULL);
    if (err < 0) {
        ErrorCode = SDLErr_OpenAudio;
        return SDLErr_Error;
    }

    Initialised = 1;

    return SDLErr_Ok;
}

void SDLDrv_Shutdown(void)
{
    if (!Initialised) {
        return;
    }

    if (StartedSDL > 0) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    } else if (StartedSDL == 0) {
        SDL_Quit();
    }

    StartedSDL = -1;
}

int SDLDrv_BeginPlayback(char *BufferStart, int BufferSize,
						int NumDivisions, void ( *CallBackFunc )( void ) )
{
	if (!Initialised) {
		ErrorCode = SDLErr_Uninitialised;
		return SDLErr_Error;
	}
	
	if (Playing) {
		SDLDrv_StopPlayback();
	}
    
	MixBuffer = BufferStart;
	MixBufferSize = BufferSize;
	MixBufferCount = NumDivisions;
	MixBufferCurrent = 0;
	MixBufferUsed = 0;
	MixCallBack = CallBackFunc;
	
	// prime the buffer
	MixCallBack();
    
	SDL_PauseAudio(0);
    
    Playing = 1;
    
	return SDLErr_Ok;
}

void SDLDrv_StopPlayback(void)
{
	if (!Initialised || !Playing) {
		return;
	}

    SDL_PauseAudio(1);
	
	Playing = 0;
}

void SDLDrv_Lock(void)
{
    SDL_LockAudio();
}

void SDLDrv_Unlock(void)
{
    SDL_UnlockAudio();
}

