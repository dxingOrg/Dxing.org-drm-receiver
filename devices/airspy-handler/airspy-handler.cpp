#
/*
 *    Copyright (C) 2025
 *    (based on hackrf-handler.cpp by Jan van Katwijk, J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of drm-receiver
 *
 *    drm-receiver is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation version 2 of the License.
 *
 *    drm-receiver is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with drm-receiver, if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include	<QSettings>
#include	<QHBoxLayout>
#include	<QLabel>
#include	<cstdio>
#include	"airspy-handler.h"

#define	DEFAULT_LNA_GAIN	8
#define	DEFAULT_MIXER_GAIN	8
#define	DEFAULT_VGA_GAIN	0

//	AirSpy R2 (stock firmware) offers two fixed hardware sample
//	rates: 10 MS/s and 2.5 MS/s. We pick the lower one: it is
//	kinder on USB bandwidth/CPU and more than wide enough for a
//	DRM channel (10 kHz), while still giving the AirSpy's more
//	sensitive front end room to work with compared to the HackRF.
//	The samplerate table is read at runtime (see below); this is
//	only the value we *ask* for.
#define	REQUESTED_INPUTRATE	2500000

	airspyHandler::airspyHandler	(RadioInterface *mr,
	                                 QSettings      *s,
	                                 RingBuffer<std::complex<float>> *r):
	                                              myFrame (nullptr) {
int	res;
uint32_t	i;
uint32_t	rateCount	= 0;
uint32_t	*rateTable	= nullptr;
uint32_t	chosenRate	= REQUESTED_INPUTRATE;

	this	-> outputRate		= 96000;
	this	-> airspySettings	= s;
	setupUi (&myFrame);
	myFrame. show ();
	_I_Buffer			= r;

#ifdef  __MINGW32__
	const char *libraryString = "airspy.dll";
	Handle          = LoadLibrary ((wchar_t *)L"airspy.dll");
#else
	const char *libraryString = "libairspy.so";
	Handle          = dlopen (libraryString, RTLD_NOW);
	if (Handle == NULL)		// try the versioned name as a fallback
	   Handle	= dlopen ("libairspy.so.0", RTLD_NOW);
#endif

	if (Handle == NULL) {
	   fprintf (stderr, "failed to open %s\n", libraryString);
	   throw (20);
	}

	libraryLoaded	= true;
	if (!load_airspyFunctions ()) {
#ifdef __MINGW32__
	   FreeLibrary (Handle);
#else
	   dlclose (Handle);
#endif
	   throw (21);
	}
//
	lastFrequency	= Khz (220000);
//
//	See if there are settings from previous incarnations
	airspySettings		-> beginGroup ("airspySettings");
	lnagainSlider 		-> setValue (
	            airspySettings -> value ("airspy_lnaGain",
	                                      DEFAULT_LNA_GAIN). toInt ());
	mixergainSlider 	-> setValue (
	            airspySettings -> value ("airspy_mixerGain",
	                                      DEFAULT_MIXER_GAIN). toInt ());
	vgagainSlider 		-> setValue (
	            airspySettings -> value ("airspy_vgaGain",
	                                      DEFAULT_VGA_GAIN). toInt ());
	airspySettings	-> endGroup ();

//
	res	= this -> airspy_init ();
	if (res != AIRSPY_SUCCESS) {
	   fprintf (stderr, "Problem with airspy_init: %s\n",
	                 this -> airspy_error_name (airspy_error (res)));
	   throw (21);
	}

	res	= this	-> airspy_open (&theDevice);
	if (res != AIRSPY_SUCCESS) {
	   fprintf (stderr, "Problem with airspy_open: %s\n",
	                 this -> airspy_error_name (airspy_error (res)));
	   this	-> airspy_exit ();
	   throw (22);
	}

//	Ask the AirSpy which sample rates it actually supports, and
//	pick the one closest to our preferred rate. The very first
//	call (len == 0) returns the number of entries in "buffer[0]".
	this -> airspy_get_samplerates (theDevice, &rateCount, 0);
	if (rateCount > 0) {
	   rateTable	= new uint32_t [rateCount];
	   this -> airspy_get_samplerates (theDevice, rateTable, rateCount);
	   chosenRate	= rateTable [0];
	   for (i = 0; i < rateCount; i ++) {
	      if (rateTable [i] == REQUESTED_INPUTRATE) {
	         chosenRate	= rateTable [i];
	         break;
	      }
	      if ((rateTable [i] < chosenRate) &&
	          (rateTable [i] >= REQUESTED_INPUTRATE / 2))
	         chosenRate	= rateTable [i];
	   }
	   delete[]	rateTable;
	}
	this	-> inputRate	= chosenRate;

	res	= this -> airspy_set_sample_type (theDevice,
	                                           AIRSPY_SAMPLE_FLOAT32_IQ);
	if (res != AIRSPY_SUCCESS) {
	   fprintf (stderr, "Problem with airspy_set_sample_type: %s\n",
	                 this -> airspy_error_name (airspy_error (res)));
	   this -> airspy_close (theDevice);
	   this -> airspy_exit ();
	   throw (23);
	}

	res	= this -> airspy_set_samplerate (theDevice, this -> inputRate);
	if (res != AIRSPY_SUCCESS) {
	   fprintf (stderr, "Problem with airspy_set_samplerate: %s\n",
	                 this -> airspy_error_name (airspy_error (res)));
	   this -> airspy_close (theDevice);
	   this -> airspy_exit ();
	   throw (24);
	}

	res	= this -> airspy_set_freq (theDevice, 220000000);
	if (res != AIRSPY_SUCCESS) {
	   fprintf (stderr, "Problem with airspy_set_freq: %s\n",
	                 this -> airspy_error_name (airspy_error (res)));
	   this -> airspy_close (theDevice);
	   this -> airspy_exit ();
	   throw (25);
	}

//	we do the gain management ourselves, not through hardware AGC
	this -> airspy_set_lna_agc	(theDevice, 0);
	this -> airspy_set_mixer_agc	(theDevice, 0);
	setLNAGain	(lnagainSlider	 -> value ());
	setMixerGain	(mixergainSlider -> value ());
	setVGAGain	(vgagainSlider	 -> value ());
//	and be prepared for future changes in the settings
	connect (lnagainSlider, SIGNAL (valueChanged (int)),
	         this, SLOT (setLNAGain (int)));
	connect (mixergainSlider, SIGNAL (valueChanged (int)),
	         this, SLOT (setMixerGain (int)));
	connect (vgagainSlider, SIGNAL (valueChanged (int)),
	         this, SLOT (setVGAGain (int)));

	airspy_read_partid_serialno_t	partSerial;
	if (this -> airspy_board_partid_serialno_read (theDevice, &partSerial)
	                                              == AIRSPY_SUCCESS) {
	   char	buf [64];
	   snprintf (buf, sizeof (buf), "serial %08X%08X",
	                        partSerial. serial_no [2],
	                        partSerial. serial_no [3]);
	   serial_number_display	-> setText (QString (buf));
	}
	{
	   char	buf [64];
	   snprintf (buf, sizeof (buf), "samplerate %d S/s", this -> inputRate);
	   samplerate_display	-> setText (QString (buf));
	}

//	Rather than a fixed integer FIR decimation (as used for hackrf
//	and rtlsdr, whose sample rate can be set to an arbitrary value
//	that is a clean multiple of 96000), the AirSpy only offers a
//	handful of fixed hardware rates. 2 500 000 / 96 000 is not an
//	integer ratio, so we use the project's generic samplerate
//	converter (libsamplerate-based) instead of decimatingFIR.
	theConverter	= new rateConverter (this -> inputRate, outputRate,
	                                     this -> inputRate / 10);
	running. store (false);
}

	airspyHandler::~airspyHandler	(void) {
	stopReader ();
	airspySettings	-> beginGroup ("airspySettings");
	airspySettings -> setValue ("airspy_lnaGain",
	                                 lnagainSlider -> value ());
	airspySettings -> setValue ("airspy_mixerGain",
	                                 mixergainSlider -> value ());
	airspySettings -> setValue ("airspy_vgaGain",
	                                 vgagainSlider -> value ());
	airspySettings	-> endGroup ();
	this	-> airspy_close (theDevice);
	this	-> airspy_exit ();
	delete	theConverter;
}
//

void	airspyHandler::setVFOFrequency	(int32_t newFrequency) {
int	res;
	if (newFrequency < Mhz (1))
	   return;
	res	= this -> airspy_set_freq (theDevice, newFrequency);
	if (res != AIRSPY_SUCCESS) {
	   fprintf (stderr, "Problem with airspy_set_freq: %s\n",
	                 this -> airspy_error_name (airspy_error (res)));
	   return;
	}
	lastFrequency = newFrequency;
}

int32_t	airspyHandler::getVFOFrequency	(void) {
	return lastFrequency;
}

void	airspyHandler::setLNAGain	(int newGain) {
int	res;
	if ((newGain <= 14) && (newGain >= 0)) {
	   res	= this -> airspy_set_lna_gain (theDevice, newGain);
	   if (res != AIRSPY_SUCCESS) {
	      fprintf (stderr, "Problem with airspy_set_lna_gain: %s\n",
	                 this -> airspy_error_name (airspy_error (res)));
	      return;
	   }
	   lnagainDisplay	-> display (newGain);
	}
}

void	airspyHandler::setMixerGain	(int newGain) {
int	res;
	if ((newGain <= 15) && (newGain >= 0)) {
	   res	= this -> airspy_set_mixer_gain (theDevice, newGain);
	   if (res != AIRSPY_SUCCESS) {
	      fprintf (stderr, "Problem with airspy_set_mixer_gain: %s\n",
	                 this -> airspy_error_name (airspy_error (res)));
	      return;
	   }
	   mixergainDisplay	-> display (newGain);
	}
}

void	airspyHandler::setVGAGain	(int newGain) {
int	res;
	if ((newGain <= 15) && (newGain >= 0)) {
	   res	= this -> airspy_set_vga_gain (theDevice, newGain);
	   if (res != AIRSPY_SUCCESS) {
	      fprintf (stderr, "Problem with airspy_set_vga_gain: %s\n",
	                 this -> airspy_error_name (airspy_error (res)));
	      return;
	   }
	   vgagainDisplay	-> display (newGain);
	}
}
//
//	AirSpy delivers samples as blocks of interleaved 32 bit float
//	I/Q pairs (we asked for AIRSPY_SAMPLE_FLOAT32_IQ), so no manual
//	int-to-float scaling is needed, unlike the int8 samples that
//	hackrf/rtlsdr hand us.
static
int	callback (airspy_transfer_t *transfer) {
airspyHandler *ctx	= static_cast<airspyHandler *>(transfer -> ctx);
float	*fbuf		= static_cast<float *>(transfer -> samples);
int	n		= transfer -> sample_count;
int	i;
RingBuffer<std::complex<float>> * q = ctx -> _I_Buffer;
int32_t	outMax	= ctx -> theConverter -> getOutputsize () + 10;
std::complex<float> localBuf [outMax];
std::complex<float> outBuf [outMax];
int	cnt	= 0;

	for (i = 0; i < n; i ++) {
	   std::complex<float> sample =
	                std::complex<float> (fbuf [2 * i], fbuf [2 * i + 1]);
	   int32_t	amount	= 0;
	   if (ctx -> theConverter -> convert (sample, outBuf, &amount)) {
	      for (int k = 0; k < amount && cnt < outMax; k ++)
	         localBuf [cnt ++] = outBuf [k];
	   }
	}

	q -> putDataIntoBuffer (localBuf, cnt);
	ctx -> sampleCnt += cnt;

	if (ctx -> sampleCnt > ctx -> outputRate / 8) {
	   ctx -> report_dataAvailable ();
	   ctx -> sampleCnt = 0;
	}
	return 0;
}

bool	airspyHandler::restartReader	(void) {
int	res;

	if (running. load ())
	   return true;

	res	= this -> airspy_start_rx (theDevice, callback, this);
	if (res != AIRSPY_SUCCESS) {
	   fprintf (stderr, "Problem with airspy_start_rx: %s\n",
	                 this -> airspy_error_name (airspy_error (res)));
	   return false;
	}
	running. store (this -> airspy_is_streaming (theDevice) == AIRSPY_TRUE);
	sampleCnt	= 0;
	fprintf (stderr, "airspy is %srunning\n",
	                        running. load () ? "" : "not ");
	return running. load ();
}

void	airspyHandler::stopReader	(void) {
int	res;

	if (!running. load ())
	   return;

	res	= this -> airspy_stop_rx (theDevice);
	if (res != AIRSPY_SUCCESS) {
	   fprintf (stderr, "Problem with airspy_stop_rx: %s\n",
	                 this -> airspy_error_name (airspy_error (res)));
	   return;
	}
	running. store (false);
}

void	airspyHandler::resetBuffer	() {
	_I_Buffer	-> FlushRingBuffer ();
}

int16_t	airspyHandler::bitDepth		() {
	return 12;	// AirSpy R2 native ADC resolution
}

bool	airspyHandler::load_airspyFunctions () {
//
//	link the required procedures
	this -> airspy_init	= (pfn_airspy_init)
	                       GETPROCADDRESS (Handle, "airspy_init");
	if (this -> airspy_init == NULL) {
	   fprintf (stderr, "Could not find airspy_init\n");
	   return false;
	}

	this -> airspy_exit	= (pfn_airspy_exit)
	                       GETPROCADDRESS (Handle, "airspy_exit");
	if (this -> airspy_exit == NULL) {
	   fprintf (stderr, "Could not find airspy_exit\n");
	   return false;
	}

	this -> airspy_open	= (pfn_airspy_open)
	                       GETPROCADDRESS (Handle, "airspy_open");
	if (this -> airspy_open == NULL) {
	   fprintf (stderr, "Could not find airspy_open\n");
	   return false;
	}

	this -> airspy_close	= (pfn_airspy_close)
	                       GETPROCADDRESS (Handle, "airspy_close");
	if (this -> airspy_close == NULL) {
	   fprintf (stderr, "Could not find airspy_close\n");
	   return false;
	}

	this -> airspy_get_samplerates	= (pfn_airspy_get_samplerates)
	                       GETPROCADDRESS (Handle, "airspy_get_samplerates");
	if (this -> airspy_get_samplerates == NULL) {
	   fprintf (stderr, "Could not find airspy_get_samplerates\n");
	   return false;
	}

	this -> airspy_set_samplerate	= (pfn_airspy_set_samplerate)
	                       GETPROCADDRESS (Handle, "airspy_set_samplerate");
	if (this -> airspy_set_samplerate == NULL) {
	   fprintf (stderr, "Could not find airspy_set_samplerate\n");
	   return false;
	}

	this -> airspy_set_sample_type	= (pfn_airspy_set_sample_type)
	                       GETPROCADDRESS (Handle, "airspy_set_sample_type");
	if (this -> airspy_set_sample_type == NULL) {
	   fprintf (stderr, "Could not find airspy_set_sample_type\n");
	   return false;
	}

	this -> airspy_set_freq	= (pfn_airspy_set_freq)
	                       GETPROCADDRESS (Handle, "airspy_set_freq");
	if (this -> airspy_set_freq == NULL) {
	   fprintf (stderr, "Could not find airspy_set_freq\n");
	   return false;
	}

	this -> airspy_set_lna_gain	= (pfn_airspy_set_lna_gain)
	                       GETPROCADDRESS (Handle, "airspy_set_lna_gain");
	if (this -> airspy_set_lna_gain == NULL) {
	   fprintf (stderr, "Could not find airspy_set_lna_gain\n");
	   return false;
	}

	this -> airspy_set_mixer_gain	= (pfn_airspy_set_mixer_gain)
	                       GETPROCADDRESS (Handle, "airspy_set_mixer_gain");
	if (this -> airspy_set_mixer_gain == NULL) {
	   fprintf (stderr, "Could not find airspy_set_mixer_gain\n");
	   return false;
	}

	this -> airspy_set_vga_gain	= (pfn_airspy_set_vga_gain)
	                       GETPROCADDRESS (Handle, "airspy_set_vga_gain");
	if (this -> airspy_set_vga_gain == NULL) {
	   fprintf (stderr, "Could not find airspy_set_vga_gain\n");
	   return false;
	}

	this -> airspy_set_lna_agc	= (pfn_airspy_set_lna_agc)
	                       GETPROCADDRESS (Handle, "airspy_set_lna_agc");
	if (this -> airspy_set_lna_agc == NULL) {
	   fprintf (stderr, "Could not find airspy_set_lna_agc\n");
	   return false;
	}

	this -> airspy_set_mixer_agc	= (pfn_airspy_set_mixer_agc)
	                       GETPROCADDRESS (Handle, "airspy_set_mixer_agc");
	if (this -> airspy_set_mixer_agc == NULL) {
	   fprintf (stderr, "Could not find airspy_set_mixer_agc\n");
	   return false;
	}

	this -> airspy_start_rx	= (pfn_airspy_start_rx)
	                       GETPROCADDRESS (Handle, "airspy_start_rx");
	if (this -> airspy_start_rx == NULL) {
	   fprintf (stderr, "Could not find airspy_start_rx\n");
	   return false;
	}

	this -> airspy_stop_rx	= (pfn_airspy_stop_rx)
	                       GETPROCADDRESS (Handle, "airspy_stop_rx");
	if (this -> airspy_stop_rx == NULL) {
	   fprintf (stderr, "Could not find airspy_stop_rx\n");
	   return false;
	}

	this -> airspy_is_streaming	= (pfn_airspy_is_streaming)
	                       GETPROCADDRESS (Handle, "airspy_is_streaming");
	if (this -> airspy_is_streaming == NULL) {
	   fprintf (stderr, "Could not find airspy_is_streaming\n");
	   return false;
	}

	this -> airspy_error_name	= (pfn_airspy_error_name)
	                       GETPROCADDRESS (Handle, "airspy_error_name");
	if (this -> airspy_error_name == NULL) {
	   fprintf (stderr, "Could not find airspy_error_name\n");
	   return false;
	}

	this -> airspy_board_partid_serialno_read =
	                       (pfn_airspy_board_partid_serialno_read)
	                       GETPROCADDRESS (Handle,
	                              "airspy_board_partid_serialno_read");
	if (this -> airspy_board_partid_serialno_read == NULL) {
	   fprintf (stderr, "Could not find airspy_board_partid_serialno_read\n");
	   return false;
	}

	fprintf (stderr, "OK, airspy functions seem to be loaded\n");
	return true;
}

void	airspyHandler::report_dataAvailable () {
	emit dataAvailable (10);
}

