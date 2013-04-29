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
 * CoreAudio output driver for MultiVoc
 *
 * Inspired by the example set by the Audiere sound library available at
 *   https://audiere.svn.sourceforge.net/svnroot/audiere/trunk/audiere/
 *
 */

#include <AudioUnit/AudioUnit.h>
#include <pthread.h>
#include "driver_coreaudio.h"

enum {
    CAErr_Warning = -2,
    CAErr_Error   = -1,
    CAErr_Ok      = 0,
    CAErr_Uninitialised,
    CAErr_FindNextComponent,
    CAErr_OpenAComponent,
    CAErr_AudioUnitInitialize,
    CAErr_AudioUnitSetFormat,
    CAErr_AudioUnitSetCallback,
    CAErr_Mutex
};

static int ErrorCode = CAErr_Ok;
static int Initialised = 0;
static int Playing = 0;
static AudioComponentInstance output_audio_unit;
static pthread_mutex_t mutex;

static char *MixBuffer = 0;
static int MixBufferSize = 0;
static int MixBufferCount = 0;
static int MixBufferCurrent = 0;
static int MixBufferUsed = 0;
static void ( *MixCallBack )( void ) = 0;

static OSStatus fillInput(
                    void                         *inRefCon,
                    AudioUnitRenderActionFlags   *inActionFlags,
                    const AudioTimeStamp         *inTimeStamp,
                    UInt32                       inBusNumber,
                    UInt32                       inNumberFrames,
                    AudioBufferList              *ioData)
{
    UInt32 remaining, len, bufn;
    char *ptr, *sptr;

    for (bufn = 0; bufn < ioData->mNumberBuffers; bufn++) {
        remaining = ioData->mBuffers[bufn].mDataByteSize;
        ptr = ioData->mBuffers[bufn].mData;

        while (remaining > 0) {
            if (MixBufferUsed == MixBufferSize) {
                CoreAudioDrv_PCM_Lock();
                MixCallBack();
                CoreAudioDrv_PCM_Unlock();
                
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

    return noErr;
}

int CoreAudioDrv_GetError(void)
{
    return ErrorCode;
}

const char *CoreAudioDrv_ErrorString( int ErrorNumber )
{
    const char *ErrorString;
    
    switch( ErrorNumber )
    {
        case CAErr_Warning :
        case CAErr_Error :
            ErrorString = CoreAudioDrv_ErrorString( ErrorCode );
            break;
            
        case CAErr_Ok :
            ErrorString = "CoreAudio ok.";
            break;
            
        case CAErr_Uninitialised:
            ErrorString = "CoreAudio uninitialised.";
            break;
            
        case CAErr_FindNextComponent:
            ErrorString = "CoreAudio error: FindNextComponent returned NULL.";
            break;
            
        case CAErr_OpenAComponent:
            ErrorString = "CoreAudio error: OpenAComponent failed.";
            break;
            
        case CAErr_AudioUnitInitialize:
            ErrorString = "CoreAudio error: AudioUnitInitialize failed.";
            break;
            
        case CAErr_AudioUnitSetFormat:
            ErrorString = "CoreAudio error: AudioUnitSetProperty format failed.";
            break;

        case CAErr_AudioUnitSetCallback:
            ErrorString = "CoreAudio error: AudioUnitSetProperty callback failed.";
            break;

        case CAErr_Mutex:
            ErrorString = "CoreAudio error: could not initialise mutex.";
            break;
            
        default:
            ErrorString = "Unknown CoreAudio error code.";
            break;
    }
    
    return ErrorString;
}

int CoreAudioDrv_PCM_Init(int * mixrate, int * numchannels, int * samplebits, void * initdata)
{
    OSStatus result = noErr;
    AudioComponent comp;
    AudioComponentDescription desc;
    AudioStreamBasicDescription requestedDesc;
    AURenderCallbackStruct callback;
    
    if (Initialised) {
        CoreAudioDrv_PCM_Shutdown();
    }
    
    if (pthread_mutex_init(&mutex, 0) < 0) {
        ErrorCode = CAErr_Mutex;
        return CAErr_Error;
    }

    // Setup a AudioStreamBasicDescription with the requested format
    requestedDesc.mFormatID = kAudioFormatLinearPCM;
    requestedDesc.mFormatFlags = kLinearPCMFormatFlagIsPacked;
    requestedDesc.mChannelsPerFrame = *numchannels;
    requestedDesc.mSampleRate = *mixrate;
    
    requestedDesc.mBitsPerChannel = *samplebits;
    if (*samplebits == 16) {
        requestedDesc.mFormatFlags |= kLinearPCMFormatFlagIsSignedInteger;
#ifdef __POWERPC__
        requestedDesc.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;
#endif
    }
    
    requestedDesc.mFramesPerPacket = 1;
    requestedDesc.mBytesPerFrame = requestedDesc.mBitsPerChannel * \
        requestedDesc.mChannelsPerFrame / 8;
    requestedDesc.mBytesPerPacket = requestedDesc.mBytesPerFrame * \
        requestedDesc.mFramesPerPacket;

    // Locate the default output audio unit
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    comp = AudioComponentFindNext(NULL, &desc);
    if (comp == NULL) {
        ErrorCode = CAErr_FindNextComponent;
        pthread_mutex_destroy(&mutex);
        return CAErr_Error;
    }

    // Open and initialize the default output audio unit
    result = AudioComponentInstanceNew(comp, &output_audio_unit);
    if (result != noErr) {
        ErrorCode = CAErr_OpenAComponent;
        pthread_mutex_destroy(&mutex);
        return CAErr_Error;
    }
    
    result = AudioUnitInitialize(output_audio_unit);
    if (result != noErr) {
        ErrorCode = CAErr_AudioUnitInitialize;
        AudioComponentInstanceDispose(output_audio_unit);
        pthread_mutex_destroy(&mutex);
        return CAErr_Error;
    }
    
    // Set the input format of the audio unit.
    result = AudioUnitSetProperty(output_audio_unit,
                        kAudioUnitProperty_StreamFormat,
                        kAudioUnitScope_Input,
                        0,
                        &requestedDesc,
                        sizeof(requestedDesc));
    if (result != noErr) {
        ErrorCode = CAErr_AudioUnitSetFormat;
        AudioComponentInstanceDispose(output_audio_unit);
        pthread_mutex_destroy(&mutex);
        return CAErr_Error;
    }
    
    // Set the audio callback
    callback.inputProc = fillInput;
    callback.inputProcRefCon = 0;
    result = AudioUnitSetProperty(output_audio_unit,
                        kAudioUnitProperty_SetRenderCallback,
                        kAudioUnitScope_Input,
                        0,
                        &callback,
                        sizeof(callback));
    if (result != noErr) {
        ErrorCode = CAErr_AudioUnitSetCallback;
        AudioComponentInstanceDispose(output_audio_unit);
        pthread_mutex_destroy(&mutex);
        return CAErr_Error;
    }

    Initialised = 1;
    
    return CAErr_Ok;
}

void CoreAudioDrv_PCM_Shutdown(void)
{
    OSStatus result;
    AURenderCallbackStruct callback;

    if (!Initialised) {
        return;
    }
    
    // stop processing the audio unit
    CoreAudioDrv_PCM_StopPlayback();
    
    // Remove the input callback
    callback.inputProc = 0;
    callback.inputProcRefCon = 0;
    result = AudioUnitSetProperty(output_audio_unit,
                                kAudioUnitProperty_SetRenderCallback,
                                kAudioUnitScope_Input,
                                0,
                                &callback,
                                sizeof(callback));
    if (result != noErr) {
        ErrorCode = CAErr_AudioUnitSetCallback;
        AudioComponentInstanceDispose(output_audio_unit);
    }

    pthread_mutex_destroy(&mutex);
    
    Initialised = 0;
}

int CoreAudioDrv_PCM_BeginPlayback(char *BufferStart, int BufferSize,
                                  int NumDivisions, void ( *CallBackFunc )( void ) )
{
    if (!Initialised) {
        ErrorCode = CAErr_Uninitialised;
        return CAErr_Error;
    }
    
    if (Playing) {
        CoreAudioDrv_PCM_StopPlayback();
    }
    
    MixBuffer = BufferStart;
    MixBufferSize = BufferSize;
    MixBufferCount = NumDivisions;
    MixBufferCurrent = 0;
    MixBufferUsed = 0;
    MixCallBack = CallBackFunc;
    
    // prime the buffer
    MixCallBack();
    
    AudioOutputUnitStart(output_audio_unit);
    
    Playing = 1;
    
    return CAErr_Ok;
}

void CoreAudioDrv_PCM_StopPlayback(void)
{
    if (!Initialised || !Playing) {
        return;
    }
    
    CoreAudioDrv_PCM_Lock();
    AudioOutputUnitStop(output_audio_unit);
    CoreAudioDrv_PCM_Unlock();
    
    Playing = 0;
}

void CoreAudioDrv_PCM_Lock(void)
{
    pthread_mutex_lock(&mutex);
}

void CoreAudioDrv_PCM_Unlock(void)
{
    pthread_mutex_unlock(&mutex);
}

