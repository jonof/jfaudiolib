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

/**
 * OggVorbis source support for MultiVoc
 */

#ifdef __APPLE__
# include <vorbis/vorbisfile.h>
#else
# include "vorbisfile.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "pitch.h"
#include "multivoc.h"
#include "_multivc.h"

#define min(x,y) ((x) < (y) ? (x) : (y))
#define max(x,y) ((x) > (y) ? (x) : (y))


typedef struct {
   void * ptr;
   size_t length;
   size_t pos;
   
   OggVorbis_File vf;
   
   char block[1024];
   int lastbitstream;
} vorbis_data;

static size_t read_vorbis(void * ptr, size_t size, size_t nmemb, void * datasource)
{
   vorbis_data * vorb = (vorbis_data *) datasource;
   size_t nread = 0;
   size_t bytes;
   
   errno = 0;

   if (vorb->length == vorb->pos) {
      return 0;
   }
   
   for (; nmemb > 0; nmemb--, nread++) {
      bytes = vorb->length - vorb->pos;
      if (size < bytes) {
         bytes = size;
      }
      
      memcpy(ptr, vorb->ptr + vorb->pos, bytes);
      vorb->pos += bytes;
      ptr += bytes;
      
      if (vorb->length == vorb->pos) {
         nread++;
         break;
      }
   }
   
   return nread;
}

static int seek_vorbis(void * datasource, ogg_int64_t offset, int whence)
{
   vorbis_data * vorb = (vorbis_data *) datasource;
   
   switch (whence) {
      case SEEK_SET: vorb->pos = 0; break;
      case SEEK_CUR: break;
      case SEEK_END: vorb->pos = vorb->length; break;
   }
   
   vorb->pos += offset;
   if (vorb->pos < 0) {
      vorb->pos = 0;
   } else if (vorb->pos > vorb->length) {
      vorb->pos = vorb->length;
   }
   
   return vorb->pos;
}

static int close_vorbis(void * datasource)
{
   return 0;
}

static int tell_vorbis(void * datasource)
{
   vorbis_data * vorb = (vorbis_data *) datasource;
   
   return vorb->pos;
}

static ov_callbacks vorbis_callbacks = {
   read_vorbis,
   seek_vorbis,
   close_vorbis,
   tell_vorbis
};


/*---------------------------------------------------------------------
Function: MV_GetNextVorbisBlock

Controls playback of OggVorbis data
---------------------------------------------------------------------*/

playbackstatus MV_GetNextVorbisBlock
(
 VoiceNode *voice
 )

{
   vorbis_data * vd = (vorbis_data *) voice->extra;
   long bytes;
   int bitstream, err;
   
   if ( voice->BlockLength <= 0 )
   {
      do {
         bytes = ov_read(&vd->vf, vd->block, sizeof(vd->block), 0, 2, 1, &bitstream);
         if (bytes == OV_HOLE) continue;
         if (bytes == 0) {
            if (voice->LoopStart) {
               err = ov_pcm_seek_page(&vd->vf, 0);
               if (err != 0) {
                  fprintf(stderr, "MV_GetNextVorbisBlock ov_pcm_seek_page_lap: err %d\n", err);
               } else {
                  break;
               }
            }
            voice->Playing = FALSE;
            return NoMoreData;
         } else if (bytes > 0) {
            break;
         } else {
            fprintf(stderr, "MV_GetNextVorbisBlock ov_read: err %d\n", err);
            voice->Playing = FALSE;
            return NoMoreData;
         }   
      } while (1);
      
      if (bitstream != vd->lastbitstream) {
         vorbis_info * vi = 0;
         
         vi = ov_info(&vd->vf, -1);
         if (!vi) {
            voice->Playing = FALSE;
            return NoMoreData;
         }
         
         if (vi->channels != 1 && vi->channels != 2) {
            voice->Playing = FALSE;
            return NoMoreData;
         }
         
         voice->channels = vi->channels;
         voice->SamplingRate = vi->rate;
         voice->RateScale    = ( voice->SamplingRate * voice->PitchScale ) / MV_MixRate;
         MV_SetVoiceMixMode( voice );
      }
      vd->lastbitstream = bitstream;

      bytes /= 2 * voice->channels;
      
      voice->BlockLength = bytes;
      voice->NextBlock   = vd->block;
      voice->length      = 0;
      voice->position    = 0;
   }
   
   voice->sound        = voice->NextBlock;
   voice->position    -= voice->length;
   voice->length       = min( voice->BlockLength, 0x8000 );
   voice->NextBlock   += voice->length * (voice->channels * voice->bits / 8);
   voice->BlockLength -= voice->length;
   voice->length     <<= 16;
   
   return( KeepPlaying );
}


/*---------------------------------------------------------------------
Function: MV_PlayVorbis3D

Begin playback of sound data at specified angle and distance
from listener.
---------------------------------------------------------------------*/

int MV_PlayVorbis3D
(
 char *ptr,
 unsigned int ptrlength,
 int  pitchoffset,
 int  angle,
 int  distance,
 int  priority,
 unsigned int callbackval
 )

{
   int left;
   int right;
   int mid;
   int volume;
   int status;
   
   if ( !MV_Installed )
   {
      MV_SetErrorCode( MV_NotInstalled );
      return( MV_Error );
   }
   
   if ( distance < 0 )
   {
      distance  = -distance;
      angle    += MV_NumPanPositions / 2;
   }
   
   volume = MIX_VOLUME( distance );
   
   // Ensure angle is within 0 - 31
   angle &= MV_MaxPanPosition;
   
   left  = MV_PanTable[ angle ][ volume ].left;
   right = MV_PanTable[ angle ][ volume ].right;
   mid   = max( 0, 255 - distance );
   
   status = MV_PlayVorbis( ptr, ptrlength, pitchoffset, mid, left, right, priority,
                           callbackval );
   
   return( status );
}


/*---------------------------------------------------------------------
Function: MV_PlayVorbis

Begin playback of sound data with the given sound levels and
priority.
---------------------------------------------------------------------*/

int MV_PlayVorbis
(
 char *ptr,
 unsigned int ptrlength,
 int   pitchoffset,
 int   vol,
 int   left,
 int   right,
 int   priority,
 unsigned int callbackval
 )

{
   int status;
   
   status = MV_PlayLoopedVorbis( ptr, ptrlength, -1, -1, pitchoffset, vol, left, right,
                                 priority, callbackval );
   
   return( status );
}


/*---------------------------------------------------------------------
Function: MV_PlayLoopedVorbis

Begin playback of sound data with the given sound levels and
priority.
---------------------------------------------------------------------*/

int MV_PlayLoopedVorbis
(
 char *ptr,
 unsigned int ptrlength,
 int   loopstart,
 int   loopend,
 int   pitchoffset,
 int   vol,
 int   left,
 int   right,
 int   priority,
 unsigned int callbackval
 )

{
   VoiceNode   *voice;
   int          status;
   vorbis_data * vd = 0;
   vorbis_info * vi = 0;
   
   if ( !MV_Installed )
   {
      MV_SetErrorCode( MV_NotInstalled );
      return( MV_Error );
   }
   
   vd = (vorbis_data *) malloc( sizeof(vorbis_data) );
   if (!vd) {
      MV_SetErrorCode( MV_InvalidVorbisFile );
      return MV_Error;
   }
   
   memset(vd, 0, sizeof(vorbis_data));
   vd->ptr = ptr;
   vd->pos = 0;
   vd->length = ptrlength;
   vd->lastbitstream = -1;
   
   status = ov_open_callbacks((void *) vd, &vd->vf, 0, 0, vorbis_callbacks);
   if (status < 0) {
      fprintf(stderr, "MV_PlayLoopedVorbis: err %d\n", status);
      MV_SetErrorCode( MV_InvalidVorbisFile );
      return MV_Error;
   }
   
   vi = ov_info(&vd->vf, 0);
   if (!vi) {
      ov_clear(&vd->vf);
      free(vd);
      MV_SetErrorCode( MV_InvalidVorbisFile );
      return MV_Error;
   }
   
   if (vi->channels != 1 && vi->channels != 2) {
      ov_clear(&vd->vf);
      free(vd);
      MV_SetErrorCode( MV_InvalidVorbisFile );
      return MV_Error;
   }
   
   // Request a voice from the voice pool
   voice = MV_AllocVoice( priority );
   if ( voice == NULL )
   {
      ov_clear(&vd->vf);
      free(vd);
      MV_SetErrorCode( MV_NoVoices );
      return( MV_Error );
   }
   
   voice->wavetype    = Vorbis;
   voice->bits        = 16;
   voice->channels    = vi->channels;
   voice->extra       = (void *) vd;
   voice->GetSound    = MV_GetNextVorbisBlock;
   voice->NextBlock   = NULL;
   voice->DemandFeed  = NULL;
   voice->LoopStart   = NULL;
   voice->LoopCount   = 0;
   voice->BlockLength = 0;
   voice->PitchScale  = PITCH_GetScale( pitchoffset );
   voice->length      = 0;
   voice->next        = NULL;
   voice->prev        = NULL;
   voice->priority    = priority;
   voice->callbackval = callbackval;
   voice->LoopStart   = (char *) (loopstart >= 0 ? TRUE : FALSE);
   voice->LoopEnd     = 0;
   voice->LoopSize    = 0;
   
   if ( loopstart < 0 )
   {
      voice->LoopStart = NULL;
      voice->LoopEnd   = NULL;
   }

   voice->SamplingRate = vi->rate;
   voice->RateScale    = ( voice->SamplingRate * voice->PitchScale ) / MV_MixRate;
   MV_SetVoiceMixMode( voice );

   MV_SetVoiceVolume( voice, vol, left, right );
   MV_PlayVoice( voice );
   
   return( voice->handle );
}


void MV_ReleaseVorbisVoice( VoiceNode * voice )
{
   vorbis_data * vd = (vorbis_data *) voice->extra;
   
   if (voice->wavetype != Vorbis) {
      return;
   }
   
   ov_clear(&vd->vf);
   free(vd);
   
   voice->extra = 0;
}

