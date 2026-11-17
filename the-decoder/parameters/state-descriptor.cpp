#
/*
 *    Copyright (C) 2019
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the SDRuno plugin for drm
 *
 *    drm plugin is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    drm plugin is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with drm plugin; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#
#include	"state-descriptor.h"
#include	<stdio.h>
#include	<string.h>
#include	"service-diag.h"

	stateDescriptor::stateDescriptor (uint8_t Mode, uint8_t Spectrum) {
	this	-> Mode		= Mode;
	this	-> Spectrum	= Spectrum;
	this	-> diag		= nullptr;
	this	-> info		= nullptr;
	this	-> chosenService	= 0;
	clearServices ();
	this	-> RMflag	= 0;
	this	-> sdcMode	= 0;
	this	-> numofStreams	= 0;
	this	-> set		= false;
	for (int i = 0; i < 4; i ++)
	   memset (&streams [i], 0, sizeof (streams [0]));
}

	stateDescriptor::~stateDescriptor	(void) {
}

void	stateDescriptor::clearServices	() {
	for (int i = 0; i < 4; i ++) {
	   svc [i]. audioStream	= -1;
	   svc [i]. dataStream	= -1;
	   svc [i]. label [0]	= 0;
	}
}

//	The stream (index in streams []) to decode as audio: the audio
//	stream of the chosen service. As long as the SDC did not tell (just
//	after a re-synchronisation) the first service, then stream 0.
int	stateDescriptor::getAudioChannel	() {
int s;
	if (chosenService < 0 || chosenService > 3)
	   chosenService = 0;
	s = svc [chosenService]. audioStream;
	if (s >= 0 && s < 4 && streams [s]. inUse &&
	    streams [s]. soort == AUDIO_STREAM)
	   return s;
	s = svc [0]. audioStream;
	if (s >= 0 && s < 4 && streams [s]. inUse &&
	    streams [s]. soort == AUDIO_STREAM)
	   return s;
	return 0;
}

void	stateDescriptor::cleanUp	() {
	if (diag != nullptr)
	   diag -> onResync ();
	clearServices ();	// chosenService is kept on purpose
        this    -> RMflag       = 0;
        this    -> sdcMode      = 0;
        this    -> numofStreams = 0;
        this    -> set          = false;
        for (int i = 0; i < 4; i ++)
           memset (&streams [i], 0, sizeof (streams [0]));
}

void	stateDescriptor::activate_channel_1	() {
	chosenService	= 0;
}

//	The second service can only be listened to when it is an audio stream
//	that the SDC has announced. (A zeroed stream counts as AUDIO_STREAM
//	because that is 0, so "in use" must be checked too: a data service,
//	or a stream that does not exist, must not switch the sound off.)
bool	stateDescriptor::activate_channel_2	() {
int s = svc [1]. audioStream;
	if (s >= 0 && s < 4 && streams [s]. inUse &&
	    streams [s]. soort == AUDIO_STREAM) {
	   chosenService	= 1;
	   return true;
	}
	return false;
}


void	stateDescriptor::publishDiag	() {
dxdata::SlotInfo	slotTab [4];
	if (diag == nullptr)
	   return;
	for (int i = 0; i < 4; i ++) {
	   slotTab [i]. inUse		= streams [i]. inUse;
	   slotTab [i]. soort		= streams [i]. soort;
	   slotTab [i]. shortId		= streams [i]. shortId;
	   slotTab [i]. streamId		= streams [i]. streamId;
	   slotTab [i]. serviceId		= streams [i]. serviceId;
	   slotTab [i]. isAudio		= streams [i]. isAudio;
	   slotTab [i]. lengthHigh	= streams [i]. lengthHigh;
	   slotTab [i]. lengthLow		= streams [i]. lengthLow;
	   slotTab [i]. audioCoding	= streams [i]. audioCoding;
	   slotTab [i]. audioMode		= streams [i]. audioMode;
	   slotTab [i]. samplingRate	= streams [i]. audioSamplingRate;
	   streams [i]. serviceName [255] = 0;
	   slotTab [i]. name		= std::string (streams [i]. serviceName);
	}
dxdata::SvcInfo	svcTab [4];
	for (int i = 0; i < 4; i ++) {
	   svc [i]. label [127] = 0;
	   svcTab [i]. audioStream	= svc [i]. audioStream;
	   svcTab [i]. dataStream	= svc [i]. dataStream;
	   svcTab [i]. label		= std::string (svc [i]. label);
	}
	diag -> setSlots (numofStreams, slotTab, getAudioChannel (),
	                  chosenService, svcTab);
}
