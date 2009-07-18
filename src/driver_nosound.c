int NoSoundDrv_GetError(void)
{
	return 0;
}

const char *NoSoundDrv_ErrorString( int ErrorNumber )
{
	return "No sound, Ok.";
}

int NoSoundDrv_Init(int mixrate, int numchannels, int samplebits)
{
	return 0;
}

void NoSoundDrv_Shutdown(void)
{
}

int NoSoundDrv_BeginPlayback(char *BufferStart, int BufferSize,
						int NumDivisions, void ( *CallBackFunc )( void ) )
{
	return 0;
}

void NoSoundDrv_StopPlayback(void)
{
}