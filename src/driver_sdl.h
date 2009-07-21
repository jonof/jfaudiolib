int SDLDrv_GetError(void);
const char *SDLDrv_ErrorString( int ErrorNumber );
int SDLDrv_Init(int mixrate, int numchannels, int samplebits);
void SDLDrv_Shutdown(void);
int SDLDrv_BeginPlayback(char *BufferStart, int BufferSize,
									  int NumDivisions, void ( *CallBackFunc )( void ) );
void SDLDrv_StopPlayback(void);
void SDLDrv_Lock(void);
void SDLDrv_Unlock(void);
