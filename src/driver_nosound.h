int NoSoundDrv_GetError(void);
const char *NoSoundDrv_ErrorString( int ErrorNumber );
int NoSoundDrv_Init(int mixrate, int numchannels, int samplebits);
void NoSoundDrv_Shutdown(void);
int NoSoundDrv_BeginPlayback(char *BufferStart, int BufferSize,
									  int NumDivisions, void ( *CallBackFunc )( void ) );
void NoSoundDrv_StopPlayback(void);
void * NoSoundDrv_Lock(void);
void NoSoundDrv_Unlock(void *);