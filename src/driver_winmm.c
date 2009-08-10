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
 * WinMM CD and MIDI output driver
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "midifuncs.h"
#include "driver_winmm.h"

enum {
   WinMMErr_Warning = -2,
   WinMMErr_Error   = -1,
   WinMMErr_Ok      = 0,
	WinMMErr_Uninitialised,
    WinMMErr_NotifyWindow,
    WinMMErr_CDMCIOpen,
    WinMMErr_CDMCISetTimeFormat,
    WinMMErr_CDMCIPlay,
    WinMMErr_MIDIStreamOpen,
    WinMMErr_MIDIStreamRestart,
    WinMMErr_MIDICreateEvent,
    WinMMErr_MIDIPlayThread
};

static int ErrorCode = WinMMErr_Ok;

enum {
    UsedByNothing = 0,
    UsedByMIDI = 1,
    UsedByCD = 2
};

static HWND notifyWindow = 0;
static int notifyWindowClassRegistered = 0;
static int notifyWindowUsedBy = UsedByNothing;

static UINT cdDeviceID = 0;
static DWORD cdPausePosition = 0;
static int cdPaused = 0;
static int cdLoop = 0;
static int cdPlayTrack = 0;

static HMIDISTRM midiStream = 0;
static UINT midiDeviceID = MIDI_MAPPER;
static void (*midiThreadService)(void) = 0;
static unsigned int midiThreadTimer = 0;
static unsigned int midiLastEventTime = 0;
static unsigned int midiThreadQueueTimer = 0;
static unsigned int midiThreadQueueTicks = 0;
static HANDLE midiThread = 0;
static HANDLE midiThreadQuit = 0;
static BOOL midiPlaying = FALSE;
static int midiLastDivision = 0;
#define THREAD_QUEUE_INTERVAL 20    // 1/20 sec

#define MIDI_NOTE_OFF         0x80
#define MIDI_NOTE_ON          0x90
#define MIDI_POLY_AFTER_TCH   0xA0
#define MIDI_CONTROL_CHANGE   0xB0
#define MIDI_PROGRAM_CHANGE   0xC0
#define MIDI_AFTER_TOUCH      0xD0
#define MIDI_PITCH_BEND       0xE0
#define MIDI_META_EVENT       0xFF
#define MIDI_END_OF_TRACK     0x2F
#define MIDI_TEMPO_CHANGE     0x51
#define MIDI_MONO_MODE_ON     0x7E
#define MIDI_ALL_NOTES_OFF    0x7B




int WinMMDrv_GetError(void)
{
	return ErrorCode;
}

const char *WinMMDrv_ErrorString( int ErrorNumber )
{
	const char *ErrorString;
	
   switch( ErrorNumber )
	{
      case WinMMErr_Warning :
      case WinMMErr_Error :
         ErrorString = WinMMDrv_ErrorString( ErrorCode );
         break;
			
      case WinMMErr_Ok :
         ErrorString = "DirectSound ok.";
         break;
			
		case WinMMErr_Uninitialised:
			ErrorString = "DirectSound uninitialised.";
			break;
			
        case WinMMErr_NotifyWindow:
            ErrorString = "Failed creating notification window for CD/MIDI.";
            break;

        case WinMMErr_CDMCIOpen:
            ErrorString = "MCI error: failed opening CD audio device.";
            break;

        case WinMMErr_CDMCISetTimeFormat:
            ErrorString = "MCI error: failed setting time format for CD audio device.";
            break;

        case WinMMErr_CDMCIPlay:
            ErrorString = "MCI error: failed playing CD audio track.";
            break;

        case WinMMErr_MIDIStreamOpen:
            ErrorString = "MIDI error: failed opening stream.";
            break;

        case WinMMErr_MIDIStreamRestart:
            ErrorString = "MIDI error: failed starting stream.";
            break;

		default:
			ErrorString = "Unknown DirectSound error code.";
			break;
	}
	
	return ErrorString;

}


static LRESULT CALLBACK notifyWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case MM_MCINOTIFY:
            if (wParam == MCI_NOTIFY_SUCCESSFUL && lParam == cdDeviceID) {
                if (cdLoop && cdPlayTrack) {
                    WinMMDrv_CD_Play(cdPlayTrack, 1);
                }
            }
            break;
        case MM_MOM_DONE:
            midiOutUnprepareHeader( (HMIDIOUT) wParam, (LPMIDIHDR) lParam, sizeof(MIDIHDR) );
            free( (void *) lParam );
            break;
        default: break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static int openNotifyWindow(int useby)
{
    if (!notifyWindow) {
        if (!notifyWindowClassRegistered) {
            WNDCLASS wc;

            memset(&wc, 0, sizeof(wc));
            wc.lpfnWndProc = notifyWindowProc;
            wc.hInstance = GetModuleHandle(NULL);
            wc.lpszClassName = "JFAudiolibNotifyWindow";

            if (!RegisterClass(&wc)) {
                return 0;
            }

            notifyWindowClassRegistered = 1;
        }

        notifyWindow = CreateWindow("JFAudiolibNotifyWindow", "", WS_POPUP,
                0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), NULL);
        if (!notifyWindow) {
            return 0;
        }
    }

    notifyWindowUsedBy |= useby;

    return 1;
}

static void closeNotifyWindow(int useby)
{
    notifyWindowUsedBy &= ~useby;

    if (!notifyWindowUsedBy && notifyWindow) {
        DestroyWindow(notifyWindow);
        notifyWindow = 0;
    }
}


int WinMMDrv_CD_Init(void)
{
    MCI_OPEN_PARMS mciopenparms;
    MCI_SET_PARMS mcisetparms;
    DWORD rv;

    WinMMDrv_CD_Shutdown();

    mciopenparms.lpstrDeviceType = "cdaudio";
    rv = mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE, (DWORD)(LPVOID) &mciopenparms);
    if (rv) {
        fprintf(stderr, "Init MCI_OPEN err %d\n", (int) rv);
        ErrorCode = WinMMErr_CDMCIOpen;
        return WinMMErr_Error;
    }

    cdDeviceID = mciopenparms.wDeviceID;

    mcisetparms.dwTimeFormat = MCI_FORMAT_TMSF;
    rv = mciSendCommand(cdDeviceID, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD)(LPVOID) &mcisetparms);
    if (rv) {
        fprintf(stderr, "Init MCI_SET err %d\n", (int) rv);
        mciSendCommand(cdDeviceID, MCI_CLOSE, 0, 0);
        cdDeviceID = 0;

        ErrorCode = WinMMErr_CDMCISetTimeFormat;
        return WinMMErr_Error;
    }

    if (!openNotifyWindow(UsedByCD)) {
        mciSendCommand(cdDeviceID, MCI_CLOSE, 0, 0);
        cdDeviceID = 0;

        ErrorCode = WinMMErr_NotifyWindow;
        return WinMMErr_Error;
    }

    return WinMMErr_Ok;
}

void WinMMDrv_CD_Shutdown(void)
{
    if (cdDeviceID) {
        WinMMDrv_CD_Stop();

        mciSendCommand(cdDeviceID, MCI_CLOSE, 0, 0);
    }
    cdDeviceID = 0;

    closeNotifyWindow(UsedByCD);
}

int WinMMDrv_CD_Play(int track, int loop)
{
    MCI_PLAY_PARMS mciplayparms;
    DWORD rv;

    if (!cdDeviceID) {
        ErrorCode = WinMMErr_Uninitialised;
        return WinMMErr_Error;
    }

    cdPlayTrack = track;
    cdLoop = loop;
    cdPaused = 0;

    mciplayparms.dwFrom = MCI_MAKE_TMSF(track, 0, 0, 0);
    mciplayparms.dwTo   = MCI_MAKE_TMSF(track + 1, 0, 0, 0);
    mciplayparms.dwCallback = (DWORD) notifyWindow;
    rv = mciSendCommand(cdDeviceID, MCI_PLAY, MCI_FROM | MCI_TO | MCI_NOTIFY, (DWORD)(LPVOID) &mciplayparms);
    if (rv) {
        fprintf(stderr, "Play MCI_PLAY err %d\n", (int) rv);
        ErrorCode = WinMMErr_CDMCIPlay;
        return WinMMErr_Error;
    }

    return WinMMErr_Ok;
}

void WinMMDrv_CD_Stop(void)
{
    MCI_GENERIC_PARMS mcigenparms;
    DWORD rv;

    if (!cdDeviceID) {
        return;
    }

    cdPlayTrack = 0;
    cdLoop = 0;
    cdPaused = 0;

    rv = mciSendCommand(cdDeviceID, MCI_STOP, 0, (DWORD)(LPVOID) &mcigenparms);
    if (rv) {
        fprintf(stderr, "Stop MCI_STOP err %d\n", (int) rv);
    }
}

void WinMMDrv_CD_Pause(int pauseon)
{
    if (!cdDeviceID) {
        return;
    }

    if (cdPaused == pauseon) {
        return;
    }

    if (pauseon) {
        MCI_STATUS_PARMS mcistatusparms;
        MCI_GENERIC_PARMS mcigenparms;
        DWORD rv;

        mcistatusparms.dwItem = MCI_STATUS_POSITION;
        rv = mciSendCommand(cdDeviceID, MCI_STATUS, MCI_WAIT | MCI_STATUS_ITEM, (DWORD)(LPVOID) &mcistatusparms);
        if (rv) {
            fprintf(stderr, "Pause MCI_STATUS err %d\n", (int) rv);
            return;
        }

        cdPausePosition = mcistatusparms.dwReturn;

        rv = mciSendCommand(cdDeviceID, MCI_STOP, 0, (DWORD)(LPVOID) &mcigenparms);
        if (rv) {
            fprintf(stderr, "Pause MCI_STOP err %d\n", (int) rv);
        }
    } else {
        MCI_PLAY_PARMS mciplayparms;
        DWORD rv;

        mciplayparms.dwFrom = cdPausePosition;
        mciplayparms.dwTo   = MCI_MAKE_TMSF(cdPlayTrack + 1, 0, 0, 0);
        mciplayparms.dwCallback = (DWORD) notifyWindow;
        rv = mciSendCommand(cdDeviceID, MCI_PLAY, MCI_FROM | MCI_TO | MCI_NOTIFY, (DWORD)(LPVOID) &mciplayparms);
        if (rv) {
            fprintf(stderr, "Pause MCI_PLAY err %d\n", (int) rv);
            return;
        }

        cdPausePosition = 0;
    }

    cdPaused = pauseon;
}

int WinMMDrv_CD_IsPlaying(void)
{
    MCI_STATUS_PARMS mcistatusparms;
    DWORD rv;

    if (!cdDeviceID) {
        return 0;
    }

    mcistatusparms.dwItem = MCI_STATUS_MODE;
    rv = mciSendCommand(cdDeviceID, MCI_STATUS, MCI_WAIT | MCI_STATUS_ITEM, (DWORD)(LPVOID) &mcistatusparms);
    if (rv) {
        fprintf(stderr, "IsPlaying MCI_STATUS err %d\n", (int) rv);
        return 0;
    }

    return (mcistatusparms.dwReturn == MCI_MODE_PLAY);
}

void WinMMDrv_CD_SetVolume(int volume)
{
}


static MIDIHDR * new_header(int length, unsigned char ** data)
{
    MIDIHDR * ptr;
    unsigned char * cdata;
    int datalen;

    if (length <= 3) {
        datalen = 12;
    } else {
        datalen = 12 + length;
    }

    ptr = (MIDIHDR *) malloc(sizeof(MIDIHDR) + datalen);
    memset(ptr, 0, sizeof(MIDIHDR) + datalen);

    ptr->lpData = (LPSTR) (ptr + 1);
    ptr->dwBufferLength = ptr->dwBytesRecorded = datalen;

    cdata = (unsigned char *) (ptr + 1);

    *((int *)cdata) = midiThreadTimer - midiLastEventTime;
    cdata += 4;

    *((int *)cdata) = 0;
    cdata += 4;

    if (length <= 3) {
        cdata[3] = MEVT_SHORTMSG;
        *data = cdata;
    } else {
        *((int *)cdata) = length & 0x00ffffff;
        cdata[3] = MEVT_LONGMSG;
        *data = cdata + 4;
    }

    return ptr;
}

static inline void sequence_event(MIDIHDR * hdr)
{
    MMRESULT rv;

    rv = midiOutPrepareHeader( (HMIDIOUT) midiStream, hdr, sizeof(MIDIHDR) );
    if (rv != MMSYSERR_NOERROR) {
        fprintf(stderr, "sequence_event midiOutPrepareHeader err %d\n", (int) rv);
        free(hdr);
        return;
    }

    rv = midiStreamOut(midiStream, hdr, sizeof(MIDIHDR));
    if (rv != MMSYSERR_NOERROR) {
        fprintf(stderr, "sequence_event midiStreamOut err %d\n", (int) rv);
        midiOutUnprepareHeader( (HMIDIOUT) midiStream, hdr, sizeof(MIDIHDR) );
        free(hdr);
        return;
    }

    midiLastEventTime = midiThreadTimer;
}

static void Func_NoteOff( int channel, int key, int velocity )
{
    MIDIHDR * hdr;
    unsigned char * data;

    hdr = new_header(3, &data);
    data[0] = MIDI_NOTE_OFF | channel;
    data[1] = key;
    data[2] = velocity;
    sequence_event(hdr);
}

static void Func_NoteOn( int channel, int key, int velocity )
{
    MIDIHDR * hdr;
    unsigned char * data;

    hdr = new_header(3, &data);
    data[0] = MIDI_NOTE_ON | channel;
    data[1] = key;
    data[2] = velocity;
    sequence_event(hdr);
}

static void Func_PolyAftertouch( int channel, int key, int pressure )
{
    MIDIHDR * hdr;
    unsigned char * data;

    hdr = new_header(3, &data);
    data[0] = MIDI_POLY_AFTER_TCH | channel;
    data[1] = key;
    data[2] = pressure;
    sequence_event(hdr);
}

static void Func_ControlChange( int channel, int number, int value )
{
    MIDIHDR * hdr;
    unsigned char * data;

    hdr = new_header(3, &data);
    data[0] = MIDI_CONTROL_CHANGE | channel;
    data[1] = number;
    data[2] = value;
    sequence_event(hdr);
}

static void Func_ProgramChange( int channel, int program )
{
    MIDIHDR * hdr;
    unsigned char * data;

    hdr = new_header(2, &data);
    data[0] = MIDI_PROGRAM_CHANGE | channel;
    data[1] = program;
    sequence_event(hdr);
}

static void Func_ChannelAftertouch( int channel, int pressure )
{
    MIDIHDR * hdr;
    unsigned char * data;

    hdr = new_header(2, &data);
    data[0] = MIDI_AFTER_TOUCH | channel;
    data[1] = pressure;
    sequence_event(hdr);
}

static void Func_PitchBend( int channel, int lsb, int msb )
{
    MIDIHDR * hdr;
    unsigned char * data;

    hdr = new_header(3, &data);
    data[0] = MIDI_PITCH_BEND | channel;
    data[1] = lsb;
    data[2] = msb;
    sequence_event(hdr);
}

int WinMMDrv_MIDI_Init(midifuncs * funcs)
{
    MMRESULT rv;

    WinMMDrv_MIDI_Shutdown();
    memset(funcs, 0, sizeof(midifuncs));

    if (!openNotifyWindow(UsedByMIDI)) {
        ErrorCode = WinMMErr_NotifyWindow;
        return WinMMErr_Error;
    }

    rv = midiStreamOpen(&midiStream, &midiDeviceID, 1, (DWORD_PTR) notifyWindow, (DWORD_PTR) 0, CALLBACK_WINDOW);
    if (rv != MMSYSERR_NOERROR) {
        fprintf(stderr, "MIDI_Init midiStreamOpen err %d\n", (int) rv);
        ErrorCode = WinMMErr_MIDIStreamOpen;
        return WinMMErr_Error;
    }
    
    funcs->NoteOff = Func_NoteOff;
    funcs->NoteOn  = Func_NoteOn;
    funcs->PolyAftertouch = Func_PolyAftertouch;
    funcs->ControlChange = Func_ControlChange;
    funcs->ProgramChange = Func_ProgramChange;
    funcs->ChannelAftertouch = Func_ChannelAftertouch;
    funcs->PitchBend = Func_PitchBend;
    
    return WinMMErr_Ok;
}

void WinMMDrv_MIDI_Shutdown(void)
{
    MMRESULT rv;

    WinMMDrv_MIDI_HaltPlayback();

    if (midiStream) {
        rv = midiStreamStop(midiStream);
        if (rv != MMSYSERR_NOERROR) {
            fprintf(stderr, "MIDI_Shutdown midiStreamStop err %d\n", (int) rv);
        }

        rv = midiStreamClose(midiStream);
        if (rv != MMSYSERR_NOERROR) {
            fprintf(stderr, "MIDI_Shutdown midiStreamClose err %d\n", (int) rv);
        }
    }

    midiStream = 0;
}

static DWORD get_tick(void)
{
    MMRESULT rv;
    MMTIME mmtime;

    mmtime.wType = TIME_TICKS;

    rv = midiStreamPosition(midiStream, &mmtime, sizeof(MMTIME));
    if (rv != MMSYSERR_NOERROR) {
        fprintf(stderr, "get_tick midiStreamPosition err %d\n", (int) rv);
        return 0;
    }

    return mmtime.u.ticks;
}

static DWORD WINAPI midiDataThread(LPVOID lpParameter)
{
    DWORD waitret;
    DWORD sequenceTime;
    DWORD sleepAmount = 100 / THREAD_QUEUE_INTERVAL;
    
    fprintf(stderr, "midiDataThread started\n");

    midiThreadTimer = get_tick();
    midiLastEventTime = midiThreadTimer;
    midiThreadQueueTimer = midiThreadTimer + midiThreadQueueTicks;
    while (midiThreadTimer < midiThreadQueueTimer) {
        if (midiThreadService) {
            midiThreadService();
        }
        midiThreadTimer++;
    }

    do {
        waitret = WaitForSingleObject(midiThreadQuit, sleepAmount);
        if (waitret == WAIT_OBJECT_0) {
            fprintf(stderr, "WinMM midiDataThread: exiting\n");
            break;
        } else if (waitret == WAIT_TIMEOUT) {
            // queue a tick
            sequenceTime = get_tick();

            sleepAmount = 100 / THREAD_QUEUE_INTERVAL;
            if ((int)(midiThreadTimer - sequenceTime) > midiThreadQueueTicks) {
                // we're running ahead, so sleep for half the usual
                // amount and try again
                sleepAmount /= 2;
                continue;
            }

            midiThreadQueueTimer = sequenceTime + midiThreadQueueTicks;
            while (midiThreadTimer < midiThreadQueueTimer) {
                if (midiThreadService) {
                    midiThreadService();
                }
                midiThreadTimer++;
            }

        } else {
            fprintf(stderr, "WinMM midiDataThread: wfmo err %d\n", (int) waitret);
        }
    } while (1);

    return 0;
}

int  WinMMDrv_MIDI_StartPlayback(void (*service)(void))
{
    MMRESULT rv;

    WinMMDrv_MIDI_HaltPlayback();

    midiThreadService = service;

    midiThreadQuit = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!midiThreadQuit) {
        ErrorCode = WinMMErr_MIDICreateEvent;
        return WinMMErr_Error;
    }

    if (!midiPlaying) {
        rv = midiStreamRestart(midiStream);
        if (rv != MMSYSERR_NOERROR) {
            fprintf(stderr, "MIDI_StartPlayback midiStreamRestart err %d\n", (int) rv);
            WinMMDrv_MIDI_HaltPlayback();
            ErrorCode = WinMMErr_MIDIStreamRestart;
            return WinMMErr_Error;
        }
    }

    midiThread = CreateThread(NULL, 0, midiDataThread, 0, 0, 0);
    if (!midiThread) {
        CloseHandle(midiThreadQuit);
        midiThreadQuit = 0;
        ErrorCode = WinMMErr_MIDIPlayThread;
        return WinMMErr_Error;
    }

    midiPlaying = TRUE;

    return WinMMErr_Ok;
}

void WinMMDrv_MIDI_HaltPlayback(void)
{
    MMRESULT rv;

    if (midiThread) {
        SetEvent(midiThreadQuit);

        WaitForSingleObject(midiThread, 10000);

        CloseHandle(midiThread);
    }

    if (midiThreadQuit) {
        CloseHandle(midiThreadQuit);
    }

    midiThread = 0;
    midiThreadQuit = 0;
}

void WinMMDrv_MIDI_SetTempo(int tempo, int division)
{
    MMRESULT rv;
    MIDIPROPTEMPO propTempo;
    MIDIPROPTIMEDIV propTimediv;

    fprintf(stderr, "MIDI_SetTempo %d/%d\n", tempo, division);

    propTempo.cbStruct = sizeof(MIDIPROPTEMPO);
    propTempo.dwTempo = 60000000l / tempo;
    propTimediv.cbStruct = sizeof(MIDIPROPTIMEDIV);
    propTimediv.dwTimeDiv = division;

    if (midiLastDivision != division) {
        // changing the division means halting the stream, and apparently
        // when pausing the stream, Windows also closes the handle, so
        // we'll just close and reopen the stream
        if (midiPlaying) {
            WinMMDrv_MIDI_HaltPlayback();

            midiStreamStop(midiStream);
            midiStreamClose(midiStream);
            midiStream = 0;

            rv = midiStreamOpen(&midiStream, &midiDeviceID, 1,
                    (DWORD_PTR) notifyWindow, (DWORD_PTR) 0, CALLBACK_WINDOW);
            if (rv != MMSYSERR_NOERROR) {
                fprintf(stderr, "MIDI_SetTempo midiStreamOpen err %d\n", (int) rv);
                return;
            }
        }

        rv = midiStreamProperty(midiStream, (LPBYTE) &propTimediv, MIDIPROP_SET | MIDIPROP_TIMEDIV);
        if (rv != MMSYSERR_NOERROR) {
            fprintf(stderr, "MIDI_SetTempo midiStreamProperty timediv err %d\n", (int) rv);
        }
    }

    rv = midiStreamProperty(midiStream, (LPBYTE) &propTempo, MIDIPROP_SET | MIDIPROP_TEMPO);
    if (rv != MMSYSERR_NOERROR) {
        fprintf(stderr, "MIDI_SetTempo midiStreamProperty tempo err %d\n", (int) rv);
    }

    if (midiLastDivision != division) {
        if (midiPlaying) {
            midiPlaying = FALSE;
            
            if (WinMMDrv_MIDI_StartPlayback(midiThreadService) != WinMMErr_Ok) {;
                return;
            }
        }

        midiLastDivision = division;
    }

    midiThreadQueueTicks = (int) ceil( ( ( (double) tempo * (double) division ) / 60.0 ) /
            (double) THREAD_QUEUE_INTERVAL );
}

// vim:ts=4:sw=4:expandtab:
