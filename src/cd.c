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

#include "cd.h"
#include "drivers.h"

int CD_Init(void)
{
    return SoundDriver_CD_Init();
}

void CD_Shutdown(void)
{
    SoundDriver_CD_Shutdown();
}

int CD_Play(int track, int loop)
{
    return SoundDriver_CD_Play(track, loop);
}

void CD_Stop(void)
{
    SoundDriver_CD_Stop();
}

void CD_Pause(int pauseon)
{
    SoundDriver_CD_Pause(pauseon);
}

int CD_IsPlaying(void)
{
    return SoundDriver_CD_IsPlaying();
}
