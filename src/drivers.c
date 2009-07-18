#include "drivers.h"

#include "driver_nosound.h"

#ifdef HAVE_SDL
# include "driver_sdl.h"
#endif

#ifdef __APPLE__
# include "driver_coreaudio.h"
#endif

#ifdef WIN32
# include "driver_directsound.h"
#endif

int ASS_SoundDevice = -1;

ASS_SoundDriver ASS_SoundDrivers[ASS_NumSoundCards] = {
{
	NoSoundDrv_GetError,
	NoSoundDrv_ErrorString,
},
{
#ifdef HAVE_SDL
	SDLDrv_GetError,
	SDLDrv_ErrorString,
#else
	0,
	0,
#endif
},
{
#ifdef __APPLE__
	CoreAudioDrv_GetError,
	CoreAudioDrv_ErrorString,
#else
	0,
	0,
#endif
},
{
#ifdef WIN32
	DirectSoundDrv_GetError,
	DirectSoundDrv_ErrorString,
#else
	0,
	0,
#endif
},
};

