#include <stdio.h>
#include <stdlib.h>

#include "fx_man.h"
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
   
   fprintf(stdout, "Format is %dHz %d-bit %d-channel\n", MixRate, NumBits, NumChannels);
   
   playsong(song);
   
   FX_Shutdown();
   
   return 0;
}

void playsong(const char * song)
{
   int voice;
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
   
   voice = FX_PlayAuto(data, length, 0, 255, 255, 255, FX_MUSIC_PRIORITY, 1);
   if (voice >= FX_Ok) {
      while (FX_SoundActive(voice)) {
         ASS_Sleep(500);
      }
   } else {
      fprintf(stderr, "Error playing sound\n");
   }
   
   free(data);
}
