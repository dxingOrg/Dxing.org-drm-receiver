#
/*
 *    Copyright (C) 2025
 *    (based on airspy-handler.cpp by Jan van Katwijk, J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Dxing.org DRM receiver
 *
 *    This software is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation version 2 of the License.
 *
 *    This software is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 */

#include	<QSettings>
#include	<cstdio>
#include	"airspyhf-handler.h"

//	AirSpy HF+ Discovery is designed for the HF (9 kHz - 31 MHz)
//	and VHF (60 - 260 MHz) bands - exactly the range DRM broadcasts
//	live in, with no upconverter/mixer stage needed, unlike a
//	general-purpose receiver such as the AirSpy R2.
#define	DEFAULT_HF_AGC		1
#define	DEFAULT_HF_AGC_THRESHOLD 0
#define	DEFAULT_HF_LNA		0
#define	DEFAULT_HF_ATT		0

	airspyhfHandler::airspyhfHandler (RadioInterface *mr,
	                                   QSettings      *s,
	                                   RingBuffer<std::complex<float>> *r):
	                                              myFrame (nullptr) {
int	res;
uint32_t	i;
uint32_t	rateCount	= 0;
uint32_t	*rateTable	= nullptr;
uint32_t	chosenRate	= 0;

	this	-> outputRate		= 96000;
	this	-> airspyhfSettings	= s;
	setupUi (&myFrame);
	myFrame. show ();
	_I_Buffer			= r;

#ifdef  __MINGW32__
	const char *libraryString = "airspyhf.dll";
	Handle          = LoadLibrary ((wchar_t *)L"airspyhf.dll");
#else
	const char *libraryString = "libairspyhf.so";
	Handle          = dlopen (libraryString, RTLD_NOW);
	if (Handle == NULL)
	   Handle	= dlopen ("libairspyhf.so.1", RTLD_NOW);
	if (Handle == NULL)
	   Handle	= dlopen ("libairspyhf.so.0", RTLD_NOW);
#endif

	if (Handle == NULL) {
	   fprintf (stderr, "failed to open %s\n", libraryString);
	   throw (20);
	}

	if (!load_airspyhfFunctions ()) {
#ifdef __MINGW32__
	   FreeLibrary (Handle);
#else
	   dlclose (Handle);
#endif
	   throw (21);
	}
//
	lastFrequency	= Khz (7295);	// a plausible shortwave DRM default
//
//	restore settings from previous incarnations
	airspyhfSettings	-> beginGroup ("airspyhfSettings");
	hfAgcSelector -> setChecked (
	           airspyhfSettings -> value ("airspyhf_hfAgc",
	                                      DEFAULT_HF_AGC). toInt () != 0);
	hfAgcThresholdSelector -> setChecked (
	           airspyhfSettings -> value ("airspyhf_hfAgcThreshold",
	                                 DEFAULT_HF_AGC_THRESHOLD). toInt () != 0);
	hfLnaSelector -> setChecked (
	           airspyhfSettings -> value ("airspyhf_hfLna",
	                                      DEFAULT_HF_LNA). toInt () != 0);
	hfAttSlider -> setValue (
	           airspyhfSettings -> value ("airspyhf_hfAtt",
	                                      DEFAULT_HF_ATT). toInt ());
	airspyhfSettings	-> endGroup ();

//	First the serial number of the receiver (the number other programs
//	show for it), then open exactly that receiver. If the library has no
//	such function, or it fails, just open the first receiver found.
uint64_t	deviceSerial	= 0;
bool		haveSerial	= false;
	if (this -> airspyhf_list_devices != NULL) {
	   uint64_t	serials [8];
	   int	found	= this -> airspyhf_list_devices (serials, 8);
	   if (found > 0) {
	      deviceSerial	= serials [0];
	      haveSerial	= true;
	   }
	}
	res	= AIRSPYHF_ERROR;
	if (haveSerial && (this -> airspyhf_open_sn != NULL))
	   res	= this -> airspyhf_open_sn (&theDevice, deviceSerial);
	if (res != AIRSPYHF_SUCCESS)
	   res	= this -> airspyhf_open (&theDevice);
	if (res != AIRSPYHF_SUCCESS) {
	   fprintf (stderr, "Problem opening airspyhf device (%d)\n", res);
	   throw (22);
	}

//	ask the device which sample rates it actually offers, and pick
//	the lowest one: the DRM channel is at most 10-20 kHz wide, so
//	there is no benefit in a wider (and more USB/CPU hungry) rate.
	this -> airspyhf_get_samplerates (theDevice, &rateCount, 0);
	if (rateCount > 0) {
	   rateTable	= new uint32_t [rateCount];
	   this -> airspyhf_get_samplerates (theDevice, rateTable, rateCount);
	   chosenRate	= rateTable [0];
	   for (i = 0; i < rateCount; i ++)
	      if (rateTable [i] < chosenRate)
	         chosenRate = rateTable [i];
	   delete[]	rateTable;
	}
	if (chosenRate == 0)
	   chosenRate	= 192000;	// safe, widely supported fallback
	this	-> inputRate	= chosenRate;

	res	= this -> airspyhf_set_samplerate (theDevice, this -> inputRate);
	if (res != AIRSPYHF_SUCCESS) {
	   fprintf (stderr, "Problem with airspyhf_set_samplerate (%d)\n", res);
	   this -> airspyhf_close (theDevice);
	   throw (23);
	}

	res	= this -> airspyhf_set_freq (theDevice, lastFrequency);
	if (res != AIRSPYHF_SUCCESS) {
	   fprintf (stderr, "Problem with airspyhf_set_freq (%d)\n", res);
	   this -> airspyhf_close (theDevice);
	   throw (24);
	}

	setHfAgc	(hfAgcSelector		-> isChecked () ? 1 : 0);
	setHfAgcThreshold (hfAgcThresholdSelector -> isChecked () ? 1 : 0);
	setHfLna	(hfLnaSelector		-> isChecked () ? 1 : 0);
	setHfAtt	(hfAttSlider		-> value ());

	connect (hfAgcSelector, SIGNAL (stateChanged (int)),
	         this, SLOT (setHfAgc (int)));
	connect (hfAgcThresholdSelector, SIGNAL (stateChanged (int)),
	         this, SLOT (setHfAgcThreshold (int)));
	connect (hfLnaSelector, SIGNAL (stateChanged (int)),
	         this, SLOT (setHfLna (int)));
	connect (hfAttSlider, SIGNAL (valueChanged (int)),
	         this, SLOT (setHfAtt (int)));

	if (haveSerial)
	   serial_number_display	-> setText (QString ("serial ") +
	         QString::number ((qulonglong)deviceSerial, 16). toUpper ().
	                                 rightJustified (16, QChar ('0')));
	else
	   serial_number_display	-> setText ("serial not available");
	{
	   char	buf [64];
	   snprintf (buf, sizeof (buf), "samplerate %d S/s", this -> inputRate);
	   samplerate_display	-> setText (QString (buf));
	}

//	just as for the R2 handler: the HF+ only offers a handful of
//	fixed hardware rates, so we use the generic samplerate
//	converter rather than an integer-ratio FIR decimator.
	theConverter	= new rateConverter (this -> inputRate, outputRate,
	                                     this -> inputRate / 10);
	running. store (false);
}

	airspyhfHandler::~airspyhfHandler (void) {
	stopReader ();
	airspyhfSettings	-> beginGroup ("airspyhfSettings");
	airspyhfSettings -> setValue ("airspyhf_hfAgc",
	                                 hfAgcSelector -> isChecked () ? 1 : 0);
	airspyhfSettings -> setValue ("airspyhf_hfAgcThreshold",
	                        hfAgcThresholdSelector -> isChecked () ? 1 : 0);
	airspyhfSettings -> setValue ("airspyhf_hfLna",
	                                 hfLnaSelector -> isChecked () ? 1 : 0);
	airspyhfSettings -> setValue ("airspyhf_hfAtt",
	                                 hfAttSlider -> value ());
	airspyhfSettings	-> endGroup ();
	this	-> airspyhf_close (theDevice);
	delete	theConverter;
}
//

void	airspyhfHandler::setVFOFrequency (int32_t newFrequency) {
int	res;
	res	= this -> airspyhf_set_freq (theDevice, newFrequency);
	if (res != AIRSPYHF_SUCCESS) {
	   fprintf (stderr, "Problem with airspyhf_set_freq (%d)\n", res);
	   return;
	}
	lastFrequency = newFrequency;
}

int32_t	airspyhfHandler::getVFOFrequency (void) {
	return lastFrequency;
}

void	airspyhfHandler::setHfAgc (int state) {
	this -> airspyhf_set_hf_agc (theDevice, state != 0 ? 1 : 0);
}

void	airspyhfHandler::setHfAgcThreshold (int state) {
	this -> airspyhf_set_hf_agc_threshold (theDevice, state != 0 ? 1 : 0);
}

void	airspyhfHandler::setHfLna (int state) {
	this -> airspyhf_set_hf_lna (theDevice, state != 0 ? 1 : 0);
}

void	airspyhfHandler::setHfAtt (int newValue) {
	if ((newValue < 0) || (newValue > 8))
	   return;
	this -> airspyhf_set_hf_att (theDevice, newValue);
	hfAttDisplay -> display (newValue * 6);	// each step is 6 dB
}
//
//	AirSpy HF+ always delivers samples as blocks of interleaved
//	32 bit float I/Q pairs - there is no format selector like on
//	the plain AirSpy, so we can read the buffer directly.
static
int	callback (airspyhf_transfer_t *transfer) {
airspyhfHandler *ctx	= static_cast<airspyhfHandler *>(transfer -> ctx);
float	*fbuf		= reinterpret_cast<float *>(transfer -> samples);
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

bool	airspyhfHandler::restartReader (void) {
int	res;

	if (running. load ())
	   return true;

	res	= this -> airspyhf_start (theDevice, callback, this);
	if (res != AIRSPYHF_SUCCESS) {
	   fprintf (stderr, "Problem with airspyhf_start (%d)\n", res);
	   return false;
	}
	running. store (this -> airspyhf_is_streaming (theDevice) != 0);
	sampleCnt	= 0;
	fprintf (stderr, "airspyhf is %srunning\n",
	                        running. load () ? "" : "not ");
	return running. load ();
}

void	airspyhfHandler::stopReader (void) {
	if (!running. load ())
	   return;
	this -> airspyhf_stop (theDevice);
	running. store (false);
}

void	airspyhfHandler::resetBuffer	() {
	_I_Buffer	-> FlushRingBuffer ();
}

int16_t	airspyhfHandler::bitDepth	() {
	return 18;	// AirSpy HF+ Discovery native ADC resolution
}

bool	airspyhfHandler::load_airspyhfFunctions () {
	this -> airspyhf_open	= (pfn_airspyhf_open)
	                       GETPROCADDRESS (Handle, "airspyhf_open");
	if (this -> airspyhf_open == NULL) {
	   fprintf (stderr, "Could not find airspyhf_open\n");
	   return false;
	}

//	optional, so no "return false" when they are missing
	this -> airspyhf_list_devices	= (pfn_airspyhf_list_devices)
	                       GETPROCADDRESS (Handle, "airspyhf_list_devices");
	this -> airspyhf_open_sn	= (pfn_airspyhf_open_sn)
	                       GETPROCADDRESS (Handle, "airspyhf_open_sn");

	this -> airspyhf_close	= (pfn_airspyhf_close)
	                       GETPROCADDRESS (Handle, "airspyhf_close");
	if (this -> airspyhf_close == NULL) {
	   fprintf (stderr, "Could not find airspyhf_close\n");
	   return false;
	}

	this -> airspyhf_get_samplerates	= (pfn_airspyhf_get_samplerates)
	                       GETPROCADDRESS (Handle, "airspyhf_get_samplerates");
	if (this -> airspyhf_get_samplerates == NULL) {
	   fprintf (stderr, "Could not find airspyhf_get_samplerates\n");
	   return false;
	}

	this -> airspyhf_set_samplerate	= (pfn_airspyhf_set_samplerate)
	                       GETPROCADDRESS (Handle, "airspyhf_set_samplerate");
	if (this -> airspyhf_set_samplerate == NULL) {
	   fprintf (stderr, "Could not find airspyhf_set_samplerate\n");
	   return false;
	}

	this -> airspyhf_set_freq	= (pfn_airspyhf_set_freq)
	                       GETPROCADDRESS (Handle, "airspyhf_set_freq");
	if (this -> airspyhf_set_freq == NULL) {
	   fprintf (stderr, "Could not find airspyhf_set_freq\n");
	   return false;
	}

	this -> airspyhf_set_hf_agc	= (pfn_airspyhf_set_hf_agc)
	                       GETPROCADDRESS (Handle, "airspyhf_set_hf_agc");
	if (this -> airspyhf_set_hf_agc == NULL) {
	   fprintf (stderr, "Could not find airspyhf_set_hf_agc\n");
	   return false;
	}

	this -> airspyhf_set_hf_agc_threshold	=
	                       (pfn_airspyhf_set_hf_agc_threshold)
	                       GETPROCADDRESS (Handle, "airspyhf_set_hf_agc_threshold");
	if (this -> airspyhf_set_hf_agc_threshold == NULL) {
	   fprintf (stderr, "Could not find airspyhf_set_hf_agc_threshold\n");
	   return false;
	}

	this -> airspyhf_set_hf_att	= (pfn_airspyhf_set_hf_att)
	                       GETPROCADDRESS (Handle, "airspyhf_set_hf_att");
	if (this -> airspyhf_set_hf_att == NULL) {
	   fprintf (stderr, "Could not find airspyhf_set_hf_att\n");
	   return false;
	}

	this -> airspyhf_set_hf_lna	= (pfn_airspyhf_set_hf_lna)
	                       GETPROCADDRESS (Handle, "airspyhf_set_hf_lna");
	if (this -> airspyhf_set_hf_lna == NULL) {
	   fprintf (stderr, "Could not find airspyhf_set_hf_lna\n");
	   return false;
	}

	this -> airspyhf_start	= (pfn_airspyhf_start)
	                       GETPROCADDRESS (Handle, "airspyhf_start");
	if (this -> airspyhf_start == NULL) {
	   fprintf (stderr, "Could not find airspyhf_start\n");
	   return false;
	}

	this -> airspyhf_stop	= (pfn_airspyhf_stop)
	                       GETPROCADDRESS (Handle, "airspyhf_stop");
	if (this -> airspyhf_stop == NULL) {
	   fprintf (stderr, "Could not find airspyhf_stop\n");
	   return false;
	}

	this -> airspyhf_is_streaming	= (pfn_airspyhf_is_streaming)
	                       GETPROCADDRESS (Handle, "airspyhf_is_streaming");
	if (this -> airspyhf_is_streaming == NULL) {
	   fprintf (stderr, "Could not find airspyhf_is_streaming\n");
	   return false;
	}

	this -> airspyhf_board_partid_serialno_read =
	                       (pfn_airspyhf_board_partid_serialno_read)
	                       GETPROCADDRESS (Handle,
	                              "airspyhf_board_partid_serialno_read");
	if (this -> airspyhf_board_partid_serialno_read == NULL) {
	   fprintf (stderr, "Could not find airspyhf_board_partid_serialno_read\n");
	   return false;
	}

	fprintf (stderr, "OK, airspyhf functions seem to be loaded\n");
	return true;
}

void	airspyhfHandler::report_dataAvailable () {
	emit dataAvailable (10);
}
