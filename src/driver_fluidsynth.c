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
#include "driver_fluidsynth.h"
#include <fluidsynth.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/select.h>

enum {
   FSMIDIErr_Warning = -2,
   FSMIDIErr_Error   = -1,
   FSMIDIErr_Ok      = 0,
   FSMIDIErr_Uninitialised,
   FSMIDIErr_NewFluidSettings,
   FSMIDIErr_NewFluidSynth,
   FSMIDIErr_NewFluidAudioDriver,
   FSMIDIErr_NewFluidSequencer,
   FSMIDIErr_RegisterFluidSynth,
   FSMIDIErr_BadSoundFont,
   FSMIDIErr_NewFluidEvent,
   FSMIDIErr_PlayThread
};

static int ErrorCode = FSMIDIErr_Ok;
static char *soundFontName = "/usr/share/sounds/sf2/SGM-V2.01.sf2";
//static char *soundFontName = "/usr/share/sounds/sf2/FluidR3_GM.sf2";

static fluid_settings_t * fluidsettings = 0;
static fluid_synth_t * fluidsynth = 0;
static fluid_audio_driver_t * fluidaudiodriver = 0;
static fluid_sequencer_t * fluidsequencer = 0;
static fluid_event_t * fluidevent = 0;
static short synthseqid = -1;

static pthread_t thread;
static int threadSleepInterval = 0;
static int threadRunning = 0;
static volatile int threadQuit = 0;
static void (* threadService)(void) = 0;


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

        case FSMIDIErr_NewFluidSettings:
            ErrorString = "Failed creating new fluid settings.";
            break;

        case FSMIDIErr_NewFluidSynth:
            ErrorString = "Failed creating new fluid synth.";
            break;

        case FSMIDIErr_NewFluidAudioDriver:
            ErrorString = "Failed creating new fluid audio driver.";
            break;

        case FSMIDIErr_NewFluidSequencer:
            ErrorString = "Failed creating new fluid sequencer.";
            break;

        case FSMIDIErr_RegisterFluidSynth:
            ErrorString = "Failed registering fluid synth with sequencer.";
            break;

        case FSMIDIErr_BadSoundFont:
            ErrorString = "Invalid or non-existent SoundFont.";
            break;

        case FSMIDIErr_NewFluidEvent:
            ErrorString = "Failed creating new fluid event.";
            break;

        case FSMIDIErr_PlayThread:
            ErrorString = "Failed creating playback thread.";
            break;

        default:
            ErrorString = "Unknown FluidSynth error.";
            break;
    }
        
	return ErrorString;
}

static inline void sequence_event(void)
{
    int result = 0;

    fluid_sequencer_send_now(fluidsequencer, fluidevent);
    //result = fluid_sequencer_send_at(fluidsequencer, fluidevent, timestamp, 1);
    if (result < 0) {
        fprintf(stderr, "fluidsynth could not queue event\n");
    }
}

static void Func_NoteOff( int channel, int key, int velocity )
{
    fluid_event_noteoff(fluidevent, channel, key);
    sequence_event();
}

static void Func_NoteOn( int channel, int key, int velocity )
{
    fluid_event_noteon(fluidevent, channel, key, velocity);
    sequence_event();
}

static void Func_PolyAftertouch( int channel, int key, int pressure )
{
    fprintf(stderr, "fluidsynth key %d channel %d aftertouch\n", key, channel);
}

static void Func_ControlChange( int channel, int number, int value )
{
    fluid_event_control_change(fluidevent, channel, number, value);
    sequence_event();
}

static void Func_ProgramChange( int channel, int program )
{
    fluid_event_program_change(fluidevent, channel, program);
    sequence_event();
}

static void Func_ChannelAftertouch( int channel, int pressure )
{
    fprintf(stderr, "fluidsynth channel %d aftertouch\n", channel);
}

static void Func_PitchBend( int channel, int lsb, int msb )
{
    fluid_event_pitch_bend(fluidevent, channel, lsb | (msb << 7));
    sequence_event();
}

static void * threadProc(void * parm)
{
    struct timeval tv;
    
    while (!threadQuit) {
        tv.tv_sec = 0;
        tv.tv_usec = threadSleepInterval;

        select(0, NULL, NULL, NULL, &tv);

        if (threadService) {
            threadService();
        }
    }

    return NULL;
}

int FluidSynthMIDIDrv_MIDI_Init(midifuncs *funcs)
{
    int result;
    
    FluidSynthMIDIDrv_MIDI_Shutdown();
    memset(funcs, 0, sizeof(midifuncs));

    fluidsettings = new_fluid_settings();
    if (!fluidsettings) {
        ErrorCode = FSMIDIErr_NewFluidSettings;
        return FSMIDIErr_Error;
    }
    
    fluidsynth = new_fluid_synth(fluidsettings);
    if (!fluidsettings) {
        FluidSynthMIDIDrv_MIDI_Shutdown();
        ErrorCode = FSMIDIErr_NewFluidSynth;
        return FSMIDIErr_Error;
    }
    
    fluidaudiodriver = new_fluid_audio_driver(fluidsettings, fluidsynth);
    if (!fluidsettings) {
        FluidSynthMIDIDrv_MIDI_Shutdown();
        ErrorCode = FSMIDIErr_NewFluidAudioDriver;
        return FSMIDIErr_Error;
    }
    
    fluidsequencer = new_fluid_sequencer();
    if (!fluidsettings) {
        FluidSynthMIDIDrv_MIDI_Shutdown();
        ErrorCode = FSMIDIErr_NewFluidSequencer;
        return FSMIDIErr_Error;
    }

    fluidevent = new_fluid_event();
    if (!fluidevent) {
        FluidSynthMIDIDrv_MIDI_Shutdown();
        ErrorCode = FSMIDIErr_NewFluidEvent;
        return FSMIDIErr_Error;
    }

    synthseqid = fluid_sequencer_register_fluidsynth(fluidsequencer, fluidsynth);
    if (synthseqid < 0) {
        FluidSynthMIDIDrv_MIDI_Shutdown();
        ErrorCode = FSMIDIErr_RegisterFluidSynth;
        return FSMIDIErr_Error;
    }

    result = fluid_synth_sfload(fluidsynth, soundFontName, 1);
    if (result < 0) {
        FluidSynthMIDIDrv_MIDI_Shutdown();
        ErrorCode = FSMIDIErr_BadSoundFont;
        return FSMIDIErr_Error;
    }

    fluid_event_set_source(fluidevent, -1);
    fluid_event_set_dest(fluidevent, synthseqid);
    
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
    if (fluidevent) {
        delete_fluid_event(fluidevent);
    }
    if (fluidsequencer) {
        delete_fluid_sequencer(fluidsequencer);
    }
    if (fluidaudiodriver) {
        delete_fluid_audio_driver(fluidaudiodriver);
    }
    if (fluidsynth) {
        delete_fluid_synth(fluidsynth);
    }
    if (fluidsettings) {
        delete_fluid_settings(fluidsettings);
    }
    synthseqid = -1;
    fluidevent = 0;
    fluidsequencer = 0;
    fluidaudiodriver = 0;
    fluidsynth = 0;
    fluidsettings = 0;
}

int FluidSynthMIDIDrv_MIDI_StartPlayback(void (*service)(void))
{
    FluidSynthMIDIDrv_MIDI_HaltPlayback();

    threadService = service;
    threadQuit = 0;

    if (pthread_create(&thread, NULL, threadProc, NULL)) {
        fprintf(stderr, "fluidsynth pthread_create returned error\n");
        return FSMIDIErr_PlayThread;
    }

    threadRunning = 1;

    return 0;
}

void FluidSynthMIDIDrv_MIDI_HaltPlayback(void)
{
    void * ret;
    
    if (!threadRunning) {
        return;
    }

    threadQuit = 1;

    if (pthread_join(thread, &ret)) {
        fprintf(stderr, "fluidsynth pthread_join returned error\n");
    }

    threadRunning = 0;
}

unsigned int FluidSynthMIDIDrv_MIDI_GetTick(void)
{
    return fluid_sequencer_get_tick(fluidsequencer);
}

void FluidSynthMIDIDrv_MIDI_SetTempo(int tempo, int division)
{
    double tps;

    tps = ( (double) tempo * (double) division ) / 60.0;
    fluid_sequencer_set_time_scale(fluidsequencer, tps);
    
    threadSleepInterval = (int)(1000000.0 / tps);
}
