#include <stdio.h>
#include <stdlib.h>

#include "fx_man.h"
#include "music.h"
#include "sndcards.h"
#include "asssys.h"

void playsong(const char *);

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void * win_gethwnd()
{
    return (void *) GetForegroundWindow();
}
#endif

int main(int argc, char ** argv)
{
    int status = 0;
    int NumVoices = 8;
    int NumChannels = 2;
    int NumBits = 16;
    int MixRate = 32000;
    void * initdata = 0;
    int voice = FX_Error;
    const char * song = "test.ogg";

    if (argc > 1) {
        song = argv[1];
    }

#ifdef _WIN32
    initdata = win_gethwnd();
#endif

    status = FX_Init( ASS_AutoDetect, NumVoices, &NumChannels, &NumBits, &MixRate, initdata );
    if (status != FX_Ok) {
        fprintf(stderr, "FX_Init error %s\n", FX_ErrorString(status));
        return 1;
    }
    
    status = MUSIC_Init(ASS_AutoDetect, 0);
    if (status != MUSIC_Ok) {
        fprintf(stderr, "MUSIC_Init error %s\n", MUSIC_ErrorString(status));
        FX_Shutdown();
        return 1;
    }

    fprintf(stdout, "FX driver is %s\n", FX_GetCurrentDriverName());
    fprintf(stdout, "Music driver is %s\n", MUSIC_GetCurrentDriverName());
    fprintf(stdout, "Format is %dHz %d-bit %d-channel\n", MixRate, NumBits, NumChannels);

    playsong(song);

    MUSIC_Shutdown();
    FX_Shutdown();

    return 0;
}

void playsong(const char * song)
{
    int status;
    int length;
    char * data;
    FILE * fp;

    fp = fopen(song, "rb");
    if (!fp) {
        fprintf(stderr, "Error opening %s\n", song);
        return;
    }

    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    if (length <= 0) {
        fclose(fp);
        return;
    }

    data = (char *) malloc(length);
    if (!data) {
        fclose(fp);
        return;
    }

    fseek(fp, 0, SEEK_SET);

    if (fread(data, length, 1, fp) != 1) {
        fclose(fp);
        free(data);
        return;
    }

    fclose(fp);

    if (memcmp(data, "MThd", 4) == 0) {
        status = MUSIC_PlaySong(data, length, 0);
        if (status != MUSIC_Ok) {
            fprintf(stderr, "Error playing music: %s\n", MUSIC_ErrorString(MUSIC_ErrorCode));
        } else {
            while (MUSIC_SongPlaying()) {
                ASS_Sleep(500);
            }
        }
    } else {
        status = FX_PlayAuto(data, length, 0, 255, 255, 255, FX_MUSIC_PRIORITY, 1);
        if (status >= FX_Ok) {
            while (FX_SoundActive(status)) {
                ASS_Sleep(500);
            }
        } else {
            fprintf(stderr, "Error playing sound: %s\n", FX_ErrorString(FX_ErrorCode));
        }
    }

    free(data);
}
