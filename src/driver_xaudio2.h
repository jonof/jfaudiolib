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

int XAudio2Drv_GetError(void);
const char *XAudio2Drv_ErrorString( int ErrorNumber );

int  XAudio2Drv_PCM_Init(int * mixrate, int * numchannels, int * samplebits, void * initdata);
void XAudio2Drv_PCM_Shutdown(void);
int  XAudio2Drv_PCM_BeginPlayback(char *BufferStart, int BufferSize,
                 int NumDivisions, void ( *CallBackFunc )( void ) );
void XAudio2Drv_PCM_StopPlayback(void);
void XAudio2Drv_PCM_Lock(void);
void XAudio2Drv_PCM_Unlock(void);

