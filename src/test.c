#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fx_man.h"
#include "music.h"
#include "drivers.h"
#include "asssys.h"

void playsong(const char *, int, int);
void listdevs(void);

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void * win_gethwnd()
{
    return (void *) GetForegroundWindow();
}
#endif

#define bound(l,n,u) ((n) < (l) ? (l) : ((n) > (u) ? (u) : (n)))

int main(int argc, char ** argv)
{
    int status = 0;
    int FXDevice = ASS_AutoDetect;
    int MusicDevice = ASS_AutoDetect;
    int NumVoices = 8;
    int NumChannels = 2;
    int NumBits = 16;
    int MixRate = 32000;
    int arg = 0;
    int loopstart = -1, loopend = -1;
    void * initdata = 0;
    const char * musicinit = 0;
    const char * song = "samples/test.wav";

    for (arg = 1; arg < argc; arg++) {
        if (argv[arg][0] == '-') {
            if (argv[arg][1] == 'h') {
                puts("test [options] [song]");
                puts("");
                puts("-h     This text.");
                puts("-l     List drivers.");
                puts("-fn    Set specific FX device (n = device number)");
                puts("-cn    Set 'n' FX output channels (1 or 2)");
                puts("-bn    Set 'n' FX output bits-per-sample (8 or 16)");
                puts("-sx    Set 'n' FX sample rate (8000 to 48000)");
                puts("-mn    Set specific Music device (n = device number)");
                puts("-M...  Specify music device parameter string");
                puts("-rx,y  Loop FX from samples x to y (0,-1 loops fully)");
                return 0;
            } else if (argv[arg][1] == 'l') {
                listdevs();
                return 0;
            } else if (argv[arg][1] == 'f') {
                FXDevice = atoi(argv[arg] + 2);
            } else if (argv[arg][1] == 'c') {
                NumChannels = atoi(argv[arg] + 2);
            } else if (argv[arg][1] == 'b') {
                NumBits = atoi(argv[arg] + 2);
            } else if (argv[arg][1] == 's') {
                MixRate = atoi(argv[arg] + 2);
            } else if (argv[arg][1] == 'm') {
                MusicDevice = atoi(argv[arg] + 2);
            } else if (argv[arg][1] == 'M') {
                musicinit = argv[arg] + 2;
            } else if (argv[arg][1] == 'r') {
                char *comma = NULL;
                loopstart = (int)strtol(argv[arg] + 2, &comma, 10);
                if (comma > argv[arg] + 2 && *comma == ',')
                    loopend = (int)strtol(comma + 1, NULL, 10);
                else
                    loopstart = -1;
            }
        } else {
            song = argv[arg];
        }
    }

    NumChannels = bound(1, NumChannels, 2);
    NumBits = bound(1, (NumBits >> 3), 2) << 3;
    MixRate = bound(8000, MixRate, 48000);

#ifdef _WIN32
    initdata = win_gethwnd();
#endif

    status = FX_Init( FXDevice, NumVoices, &NumChannels, &NumBits, &MixRate, initdata );
    if (status != FX_Ok) {
        fprintf(stderr, "FX_Init error %s\n", FX_ErrorString(status));
        return 1;
    }
    
    status = MUSIC_Init(MusicDevice, musicinit);
    if (status != MUSIC_Ok) {
        fprintf(stderr, "MUSIC_Init error %s\n", MUSIC_ErrorString(status));
        FX_Shutdown();
        return 1;
    }

    fprintf(stdout, "FX driver is %s\n", FX_GetCurrentDriverName());
    fprintf(stdout, "Music driver is %s\n", MUSIC_GetCurrentDriverName());
    fprintf(stdout, "Format is %dHz %d-bit %d-channel\n", MixRate, NumBits, NumChannels);

    playsong(song, loopstart, loopend);

    MUSIC_Shutdown();
    FX_Shutdown();

    return 0;
}

void playsong(const char * song, int loopstart, int loopend)
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
    length = (int)ftell(fp);
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

    printf("Playing %s\n", song);

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
        if (loopstart >= 0) {
            printf("Looping from %d to %d\n", loopstart, loopend);
            status = FX_PlayLoopedAuto(data, length, loopstart, loopend,
                0, 255, 255, 255, FX_MUSIC_PRIORITY, 1);
        } else {
            status = FX_PlayAuto(data, length, 0, 255, 255, 255, FX_MUSIC_PRIORITY, 1);
        }
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

void listdevs(void)
{
    int dev;
    #define YESNO(x) ((x) ? "Yes" : "No ")

    for (dev = 0; dev < ASS_NumSoundCards; dev++) {
        printf("%d) %-15s PCM %s  CD %s  MIDI %s \n",
            dev, SoundDriver_GetName(dev),
            YESNO(SoundDriver_IsPCMSupported(dev)),
            YESNO(SoundDriver_IsCDSupported(dev)),
            YESNO(SoundDriver_IsMIDISupported(dev))
        );
    }
}

/*
 * vim:ts=4:
 */
