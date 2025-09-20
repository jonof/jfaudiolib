/*
 Copyright (C) 2025 Jonathon Fowler <jf@jonof.id.au>
 
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
 * XAudio2 output driver for MultiVoc
 */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#define _WIN32_IE _WIN32_IE_WIN7

#if defined(__MINGW32__)
# define COBJMACROS
# define CASTPROC(x) (void(*)(void))x
#else
# define CASTPROC(x) x
#endif

#include <windows.h>
#include "xaudio2.h"

#include "asssys.h"
#include "driver_xaudio2.h"

#if defined(__MINGW32__)
# if !defined(XAUDIO2_USE_DEFAULT_PROCESSOR)
#  define XAUDIO2_USE_DEFAULT_PROCESSOR XAUDIO2_ANY_PROCESSOR
# endif
#endif

enum {
    XA2Err_Warning = -2,
    XA2Err_Error   = -1,
    XA2Err_Ok      = 0,
    XA2Err_Uninitialised,
    XA2Err_DLLLoadError,
    XA2Err_CoInitializeEx,
    XA2Err_XAudio2Create,
    XA2Err_CreateMasteringVoice,
    XA2Err_CreateSourceVoice,
    XA2Err_AudioBuffer,
    XA2Err_BufferEvent,
    XA2Err_CreateMutex,
    XA2Err_CreateThread,
    XA2Err_VoiceStart,
};

static int ErrorCode = XA2Err_Ok;
static int Initialised = 0;
static int Playing = 0;

static char *MixBuffer = 0;
static int MixBufferSize = 0;
static int MixBufferCount = 0;
static int MixBufferCurrent = 0;
static int MixBufferUsed = 0;
static void ( *MixCallBack )( void ) = 0;

static HANDLE mutex;
static HANDLE xaudiodll;

static IXAudio2 *xaudio;
static IXAudio2MasteringVoice *mastervoice;
static IXAudio2SourceVoice *sourcevoice;

static unsigned char *audiobuffer;
static int buffersize;
#define NUMBUFFERS 2

enum { EVENT_FILL, EVENT_EXIT, MAX_EVENTS };
static HANDLE bufferevents[MAX_EVENTS], bufferthread;
static CRITICAL_SECTION buffercritsec;
static BOOL buffercritsecinited;

static DWORD WINAPI bufferthreadproc(LPVOID lpParam);

static void STDMETHODCALLTYPE svc_OnVoiceProcessingPassStart(IXAudio2VoiceCallback *This, UINT32 BytesRequired) { (void)This; (void)BytesRequired; }
static void STDMETHODCALLTYPE svc_OnVoiceProcessingPassEnd(IXAudio2VoiceCallback *This) { (void)This; }
static void STDMETHODCALLTYPE svc_OnStreamEnd(IXAudio2VoiceCallback *This) { (void)This; }
static void STDMETHODCALLTYPE svc_OnBufferStart(IXAudio2VoiceCallback *This, void *pBufferContext) { (void)This; (void)pBufferContext; }
static void STDMETHODCALLTYPE svc_OnBufferEnd(IXAudio2VoiceCallback *This, void *pBufferContext) { (void)This; *(char*)pBufferContext = 0; SetEvent(bufferevents[EVENT_FILL]); }
static void STDMETHODCALLTYPE svc_OnLoopEnd(IXAudio2VoiceCallback *This, void *pBufferContext) { (void)This; (void)pBufferContext; }
static void STDMETHODCALLTYPE svc_OnVoiceError(IXAudio2VoiceCallback *This, void *pBufferContext, HRESULT Error) { (void)This; (void)pBufferContext; (void)Error; }

static IXAudio2VoiceCallback sourcevoicecallback = {
    .lpVtbl = &(IXAudio2VoiceCallbackVtbl) {
        .OnVoiceProcessingPassStart = svc_OnVoiceProcessingPassStart,
        .OnVoiceProcessingPassEnd = svc_OnVoiceProcessingPassEnd,
        .OnStreamEnd = svc_OnStreamEnd,
        .OnBufferStart = svc_OnBufferStart,
        .OnBufferEnd = svc_OnBufferEnd,
        .OnLoopEnd = svc_OnLoopEnd,
        .OnVoiceError = svc_OnVoiceError,
    }
};


int XAudio2Drv_GetError(void)
{
    return ErrorCode;
}

const char *XAudio2Drv_ErrorString( int ErrorNumber )
{
    const char *ErrorString;

    switch( ErrorNumber )
    {
        case XA2Err_Warning :
        case XA2Err_Error :
            ErrorString = XAudio2Drv_ErrorString( ErrorCode );
            break;

        case XA2Err_Ok :
            ErrorString = "XAudio2 ok.";
            break;

        case XA2Err_DLLLoadError:
            ErrorString = "Error loading xaudio2_9redist.dll";
            break;

        case XA2Err_Uninitialised:
            ErrorString = "XAudio2 uninitialised.";
            break;

        case XA2Err_CoInitializeEx:
            ErrorString = "CoInitializeEx error.";
            break;

        case XA2Err_XAudio2Create:
            ErrorString = "XAudio2Create error.";
            break;

        case XA2Err_CreateMasteringVoice:
            ErrorString = "CreateMasteringVoice error.";
            break;

        case XA2Err_CreateSourceVoice:
            ErrorString = "CreateSourceVoice error.";
            break;

        case XA2Err_AudioBuffer:
            ErrorString = "Audio buffer allocation error.";
            break;

        case XA2Err_BufferEvent:
            ErrorString = "Buffer event creation error.";
            break;

        case XA2Err_CreateMutex:
            ErrorString = "Mutex creation error.";
            break;

        case XA2Err_CreateThread:
            ErrorString = "Thread creation error.";
            break;

        case XA2Err_VoiceStart:
            ErrorString = "Voice start error.";
            break;

        default:
            ErrorString = "Unknown XAudio2 error code.";
            break;
    }

    return ErrorString;
}

static void TeardownXA2(HRESULT err)
{
    if (FAILED(err)) {
        ASS_Message("XAudio2Drv: TeardownXA2 error: %x\n", (unsigned int) err);
    }

    if (bufferthread) {
        SetEvent(bufferevents[EVENT_EXIT]);
        WaitForSingleObject(bufferthread, INFINITE);
        CloseHandle(bufferthread);
        bufferthread = NULL;
    }
    if (buffercritsecinited) {
        DeleteCriticalSection(&buffercritsec);
        buffercritsecinited = FALSE;
    }
    if (bufferevents[EVENT_EXIT]) {
        CloseHandle(bufferevents[EVENT_EXIT]);
        bufferevents[EVENT_EXIT] = NULL;
    }
    if (bufferevents[EVENT_FILL]) {
        CloseHandle(bufferevents[EVENT_FILL]);
        bufferevents[EVENT_FILL] = NULL;
    }

    if (audiobuffer) {
        free(audiobuffer);
        audiobuffer = NULL;
    }

    if (sourcevoice) {
        IXAudio2Voice_DestroyVoice(sourcevoice);
        sourcevoice = NULL;
    }
    if (mastervoice) {
        IXAudio2Voice_DestroyVoice(mastervoice);
        mastervoice = NULL;
    }
    if (xaudio) {
        IXAudio2_Release(xaudio);
        xaudio = NULL;
    }

    if (mutex) {
        CloseHandle(mutex);
        mutex = 0;
    }
}

int XAudio2Drv_PCM_Init(int * mixrate, int * numchannels, int * samplebits, void * initdata)
{
    HRESULT err;
    WAVEFORMATEX wfex = {0};
    HRESULT (WINAPI *procXAudio2Create)(IXAudio2**, UINT32, XAUDIO2_PROCESSOR);

    (void)initdata;

    if (Initialised) {
        XAudio2Drv_PCM_Shutdown();
    }

    if (!xaudiodll) {
        xaudiodll = LoadLibrary("xaudio2_9redist.dll");
        if (!xaudiodll) {
            xaudiodll = LoadLibrary("xaudio2_9.dll");
            if (!xaudiodll) {
                ErrorCode = XA2Err_DLLLoadError;
                return XA2Err_Error;
            }
        }
    }
    procXAudio2Create = (HRESULT (WINAPI *)(IXAudio2**, UINT32, XAUDIO2_PROCESSOR))
        CASTPROC(GetProcAddress(xaudiodll, "XAudio2Create"));
    if (!procXAudio2Create) {
        ErrorCode = XA2Err_DLLLoadError;
        return XA2Err_Error;
    }

    err = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED( err )) {
        ErrorCode = XA2Err_CoInitializeEx;
        return XA2Err_Error;
    }

    err = procXAudio2Create(&xaudio, 0, XAUDIO2_USE_DEFAULT_PROCESSOR);
    if (FAILED( err )) {
        TeardownXA2(err);
        ErrorCode = XA2Err_XAudio2Create;
        return XA2Err_Error;
    }
    err = IXAudio2_CreateMasteringVoice(xaudio,&mastervoice,
        XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE,
        0, NULL, NULL, AudioCategory_GameEffects);
    if (FAILED( err )) {
        TeardownXA2(err);
        ErrorCode = XA2Err_CreateMasteringVoice;
        return XA2Err_Error;
    }

    wfex.wFormatTag = WAVE_FORMAT_PCM;
    wfex.nChannels = *numchannels;
    wfex.nSamplesPerSec = *mixrate;
    wfex.wBitsPerSample = *samplebits;
    wfex.nBlockAlign = wfex.nChannels * wfex.wBitsPerSample / 8;
    wfex.nAvgBytesPerSec = wfex.nBlockAlign * wfex.nSamplesPerSec;
    err = IXAudio2_CreateSourceVoice(xaudio, &sourcevoice, &wfex,
            XAUDIO2_VOICE_NOPITCH, XAUDIO2_DEFAULT_FREQ_RATIO,
            &sourcevoicecallback, NULL, NULL);
    if (FAILED( err )) {
        TeardownXA2(err);
        ErrorCode = XA2Err_CreateSourceVoice;
        return XA2Err_Error;
    }

    buffersize = 4*(((wfex.nAvgBytesPerSec/120)+1)&~1);
    audiobuffer = (unsigned char *)malloc(buffersize * NUMBUFFERS);
    if (!audiobuffer) {
        TeardownXA2(S_OK);
        ErrorCode = XA2Err_AudioBuffer;
        return XA2Err_Error;
    }

    bufferevents[EVENT_FILL] = CreateEvent(NULL, FALSE, TRUE, NULL);    // Signalled.
    bufferevents[EVENT_EXIT] = CreateEvent(NULL, FALSE, FALSE, NULL);   // Unsignalled.
    if (!bufferevents[EVENT_FILL] || !bufferevents[EVENT_EXIT]) {
        TeardownXA2(S_OK);
        ErrorCode = XA2Err_BufferEvent;
        return XA2Err_Error;
    }

    InitializeCriticalSection(&buffercritsec);
    buffercritsecinited = TRUE;

    mutex = CreateMutex(0, FALSE, 0);
    if (!mutex) {
        TeardownXA2(S_OK);
        ErrorCode = XA2Err_CreateMutex;
        return XA2Err_Error;
    }

    Initialised = 1;

    return XA2Err_Ok;
}

void XAudio2Drv_PCM_Shutdown(void)
{
    if (!Initialised) {
        return;
    }

    XAudio2Drv_PCM_StopPlayback();

    TeardownXA2(S_OK);

    // The documentation warns not to let the DLL be unloaded because of the
    // possibility of running threads, so we will not unload it.

    Initialised = 0;
}

int XAudio2Drv_PCM_BeginPlayback(char *BufferStart, int BufferSize,
                        int NumDivisions, void ( *CallBackFunc )( void ) )
{
    HRESULT err;

    if (!Initialised) {
        ErrorCode = XA2Err_Uninitialised;
        return XA2Err_Error;
    }

    XAudio2Drv_PCM_StopPlayback();

    MixBuffer = BufferStart;
    MixBufferSize = BufferSize;
    MixBufferCount = NumDivisions;
    MixBufferCurrent = 0;
    MixBufferUsed = 0;
    MixCallBack = CallBackFunc;

    // prime the buffer
    // FillBuffer(0);

    bufferthread = CreateThread(NULL, 0, bufferthreadproc, NULL, 0, NULL);
    if (!bufferthread) {
        ErrorCode = XA2Err_CreateThread;
        return XA2Err_Error;
    }

    SetThreadPriority(bufferthread, THREAD_PRIORITY_TIME_CRITICAL);

    err = IXAudio2SourceVoice_Start(sourcevoice, 0, XAUDIO2_COMMIT_NOW);
    if (FAILED( err )) {
        ErrorCode = XA2Err_VoiceStart;
        return XA2Err_Error;
    }

    Playing = 1;

    return XA2Err_Ok;
}

void XAudio2Drv_PCM_StopPlayback(void)
{
    HRESULT err;

    if (!Playing) {
        return;
    }

    err = IXAudio2SourceVoice_Stop(sourcevoice, 0, XAUDIO2_COMMIT_NOW);
    if (FAILED( err )) {
        ASS_Message("XAudio2Drv: failed to stop source voice (%08x)\n", err);
    }
    IXAudio2Voice_DestroyVoice(sourcevoice);
    sourcevoice = NULL;

    SetEvent(bufferevents[EVENT_EXIT]);
    WaitForSingleObject(bufferthread, INFINITE);
    CloseHandle(bufferthread);
    bufferthread = NULL;

    Playing = 0;
}

void XAudio2Drv_PCM_Lock(void)
{
    if (!buffercritsecinited) return;
    EnterCriticalSection(&buffercritsec);
}

void XAudio2Drv_PCM_Unlock(void)
{
    if (!buffercritsecinited) return;
    LeaveCriticalSection(&buffercritsec);
}

static void FillBufferPortion(unsigned char * ptr, int remaining)
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

static DWORD WINAPI bufferthreadproc(LPVOID lpParam)
{
    char bufferfull[NUMBUFFERS];

    (void)lpParam;

    while (1) {
        switch (WaitForMultipleObjects(2, bufferevents, FALSE, INFINITE)) {
            case WAIT_OBJECT_0 + EVENT_FILL: {
                XAUDIO2_BUFFER buffer = {0};
                HRESULT hr;

                XAudio2Drv_PCM_Lock();

                for (int i = NUMBUFFERS-1; i >= 0; i--) {
                    if (bufferfull[i]) continue;

                    FillBufferPortion(audiobuffer + buffersize * i, buffersize);
                    buffer.pAudioData = audiobuffer + buffersize * i;
                    buffer.AudioBytes = buffersize;
                    buffer.pContext = (void *)&bufferfull[i];

                    if (FAILED(hr = IXAudio2SourceVoice_SubmitSourceBuffer(sourcevoice, &buffer, NULL))) {
                        ASS_Message("XAudio2Drv: failed to submit source buffer (%08x)\n", hr);
                        break;
                    }

                    bufferfull[i] = 1;
                }

                XAudio2Drv_PCM_Unlock();
                break;
            }
            case WAIT_OBJECT_0 + EVENT_EXIT:
                return 0;
            case WAIT_TIMEOUT:
               break;
            default:
                return -1;
        }
    }

    return 0;
}
