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
    WinMMErr_CDMCIPlay
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

int WinMMDrv_MIDI_Init(midifuncs * funcs)
{
    memset(funcs, 0, sizeof(midifuncs));
    return 0;
}

void WinMMDrv_MIDI_Shutdown(void)
{
}

int  WinMMDrv_MIDI_StartPlayback(void (*service)(void))
{
    return 0;
}

void WinMMDrv_MIDI_HaltPlayback(void)
{
}

unsigned int WinMMDrv_MIDI_GetTick(void)
{
    return 0;
}

void WinMMDrv_MIDI_SetTempo(int tempo, int division)
{
}

// vim:ts=4:sw=4:expandtab:
