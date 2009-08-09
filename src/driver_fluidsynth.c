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
 * Stub driver for no output
 */

#include "midifuncs.h"
#include <fluidsynth.h>
#include <string.h>

enum {
   FSMIDIErr_Warning = -2,
   FSMIDIErr_Error   = -1,
   FSMIDIErr_Ok      = 0,
   FSMIDIErr_Uninitialised,
};

static int ErrorCode = FSMIDIErr_Ok;

int FluidSynthMIDIDrv_GetError(void)
{
	return ErrorCode;
}

const char *FluidSynthMIDIDrv_ErrorString( int ErrorNumber )
{
	const char *ErrorString;
	
    switch( ErrorNumber )
	{
        case FSMIDIErr_Warning :
        case FSMIDIErr_Error :
            ErrorString = FluidSynthMIDIDrv_ErrorString( ErrorCode );
            break;

        case FSMIDIErr_Ok :
            ErrorString = "FluidSynth ok.";
            break;
			
		case FSMIDIErr_Uninitialised:
			ErrorString = "FluidSynth uninitialised.";
			break;

        default:
            ErrorString = "Unknown FluidSynth error.";
            break;
    }
        
	return ErrorString;
}

static void Func_NoteOff( int channel, int key, int velocity )
{
}

static void Func_NoteOn( int channel, int key, int velocity )
{
}

static void Func_PolyAftertouch( int channel, int key, int pressure )
{
}

static void Func_ControlChange( int channel, int number, int value )
{
}

static void Func_ProgramChange( int channel, int program )
{
}

static void Func_ChannelAftertouch( int channel, int pressure )
{
}

static void Func_PitchBend( int channel, int lsb, int msb )
{
}

int FluidSynthMIDIDrv_MIDI_Init(midifuncs *funcs)
{
    memset(funcs, 0, sizeof(midifuncs));
    funcs->NoteOff = Func_NoteOff;
    funcs->NoteOn  = Func_NoteOn;
    funcs->PolyAftertouch = Func_PolyAftertouch;
    funcs->ControlChange = Func_ControlChange;
    funcs->ProgramChange = Func_ProgramChange;
    funcs->ChannelAftertouch = Func_ChannelAftertouch;
    funcs->PitchBend = Func_PitchBend;
    
    return FSMIDIErr_Ok;
}

void FluidSynthMIDIDrv_MIDI_Shutdown(void)
{
}
