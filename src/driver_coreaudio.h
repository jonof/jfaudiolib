int CoreAudioDrv_GetError(void);
const char *CoreAudioDrv_ErrorString( int ErrorNumber );
int CoreAudioDrv_Init(int mixrate, int numchannels, int samplebits);
void CoreAudioDrv_Shutdown(void);
int CoreAudioDrv_BeginPlayback(char *BufferStart, int BufferSize,
										 int NumDivisions, void ( *CallBackFunc )( void ) );
void CoreAudioDrv_StopPlayback(void);
void CoreAudioDrv_Lock(void);
void CoreAudioDrv_Unlock(void);