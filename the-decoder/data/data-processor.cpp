#
/*
 *    Copyright (C) 2020
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the SDRunoPlugin_drm
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
#include	<stdio.h>
#include	<float.h>
#include	<math.h>
#include	"basics.h"
#include	"drm-decoder.h"
#include	"state-descriptor.h"
#include	"data-processor.h"
#include	"data-hub.h"
#include	"service-diag.h"
#include	<vector>


	dataProcessor::dataProcessor	(stateDescriptor *theState,
	                                 drmDecoder *m_form,
#ifdef	__WITH_FDK_AAC__
#ifdef	__MINGW32__
	                                 aacHandler	*aacFunctions,
#endif
#endif
	                                 RingBuffer<std::complex<float>> *b):
	                                      my_messageProcessor (m_form),
#ifdef	__WITH_FDK_AAC__
	                                      my_aacProcessor (theState,
	                                                       m_form,
#ifdef	__MINGW32__
	                                                       aacFunctions,
#endif
	                                                       b),
	                                      my_xheaacProcessor (theState,
	                                                          m_form,
#ifdef	__MINGW32__
	                                                          aacFunctions,
#endif
	                                                          b)
#else
	                                      my_aacProcessor (theState,
	                                                       m_form, b)
#endif
	{
	
	this	-> theState		= theState;
	this	-> m_form		= m_form;
	this	-> audioChannel		= 1;
	connect (this, SIGNAL (set_audioModeLabel (const QString &)),
	         m_form, SLOT (set_audioModeLabel (const QString &)));
	set_audioModeLabel (" ");
}

	dataProcessor::~dataProcessor	(void) {
}

//	The first thing we do is to define the borders for the
//	streams and dispatch on the kind of stream
void	dataProcessor::process		(uint8_t *v, int16_t size) {
int16_t	i;
int16_t	startPosA	= 0;
int16_t	startPosB	= 0;

//	we first compute the length of the HP part, which - apparently -
//	is the start of the LP part
	for (i = 0; i < theState -> numofStreams; i ++) 
	   startPosB += theState -> streams [i]. lengthHigh;

	for (i = 0; i < theState -> numofStreams && i < 4; i ++) {
	   int16_t lengthA = theState -> streams [i]. lengthHigh;
	   int16_t lengthB = theState -> streams [i]. lengthLow;
	   if ((theState -> streams [i]. soort ==
	                       stateDescriptor::AUDIO_STREAM) && 
	                         (theState -> getAudioChannel () == i)) {
	      if (theState -> diag != nullptr)
	         theState -> diag -> noteAudio (i);
	      process_audio (v, i, startPosA, lengthA,
	                           startPosB, lengthB);
	   }
	   else
	   if (theState -> streams [i]. soort == stateDescriptor::DATA_STREAM)
	      process_data (v, i, startPosA, lengthA,
	                          startPosB, lengthB, size);
	   startPosA	+= lengthA;
	   startPosB	+= lengthB;
	}
}
//
//	A data stream: the bits of the high protected part and then of the
//	low protected part are packed into bytes and handed to the data hub,
//	which lives in the decoder (it must survive reconfigurations).
void	dataProcessor::process_data (uint8_t *v, int16_t mscIndex,
	                              int16_t startHigh, int16_t lengthHigh,
	                              int16_t startLow,  int16_t lengthLow,
	                              int16_t totalBytes) {
dxdata::DataHub	*hub	= m_form -> dataHub ();
	if (hub == nullptr)
	   return;
	if (lengthHigh < 0 || lengthLow < 0 || startHigh < 0 || startLow < 0)
	   return;
	if (startHigh + lengthHigh > totalBytes ||
	    startLow + lengthLow > totalBytes)
	   return;				// would read outside the frame
auto &st	= theState -> streams [mscIndex];
	hub -> configureStream (mscIndex, st. packetModeInd != 0,
	                        st. dataPacketLength, st. packetId,
	                        st. domain, st. applicationId, st. FEC != 0);
std::vector<uint8_t> bytes ((size_t)lengthHigh + (size_t)lengthLow);
size_t	n	= 0;
	for (int16_t i = 0; i < lengthHigh; i ++) {
	   uint8_t b = 0;
	   for (int k = 0; k < 8; k ++)
	      b = (b << 1) | (v [(startHigh + i) * 8 + k] & 01);
	   bytes [n ++] = b;
	}
	for (int16_t i = 0; i < lengthLow; i ++) {
	   uint8_t b = 0;
	   for (int k = 0; k < 8; k ++)
	      b = (b << 1) | (v [(startLow + i) * 8 + k] & 01);
	   bytes [n ++] = b;
	}
	hub -> feedStream (mscIndex, bytes. data (), bytes. size ());
}
//
//	Finally, we are here. We have a segment v with length l
//	that contains audio. Parameters are to be found
//	at the mscIndex's description in the mscConfig
void	dataProcessor::process_audio (uint8_t *v, int16_t mscIndex,
	                              int16_t startHigh, int16_t lengthHigh,
	                              int16_t startLow,  int16_t lengthLow) {
	
uint8_t	audioCoding		= theState -> streams [mscIndex]. audioCoding;

//	fprintf (stderr, "mscIndex %d, startHigh %d, startLow %d, lengthH %d, length Low %d\n", mscIndex, startHigh, startLow, lengthHigh, lengthLow);
	switch (audioCoding) { 
	   case 0:		// AAC
	      set_audioModeLabel ("AAC");
	      my_aacProcessor. process_aac (v, mscIndex,
	                                    startHigh, lengthHigh,
	                                    startLow,  lengthLow);
	      return;

	   case 1:		// CELP
	      return;

	   case 2:		// HVXC
	      return;

	   default:
              set_audioModeLabel (QString ("xHE-AAC"));
#ifdef  __WITH_FDK_AAC__
              my_xheaacProcessor. process_usac (v, mscIndex,
                                                startHigh, lengthHigh,
                                                startLow, lengthLow);
#endif
	      return;
	}
}

