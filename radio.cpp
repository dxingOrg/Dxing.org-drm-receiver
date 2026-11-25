#
/*
 *    Copyright (C)  2020 .. 2025
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of drm receiver
 *
 *    drm receiver is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    drm receiver is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with drm receiver; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include	<unistd.h>
#include	<QCoreApplication>
#include	<QToolTip>
#include	<QCursor>
#include	<QSettings>
#include	<QDebug>
#include	<QDateTime>
#include	<QObject>
#include	<QDir>
#include	<QColor>
#include	<QMessageBox>
#include	<QFileDialog>
#include	"radio.h"
#include	"fft-scope.h"
#include	"up-converter.h"
#include        "audiosink.h"
#include        "popup-keypad.h"
#include        "program-list.h"
#include	"bandplan.h"
//
//      devices
#include        "device-handler.h"
#include        "filereader.h"
#ifdef	HAVE_SDRPLAY_V2
#include        "sdrplay-handler-v2.h"
#endif
#ifdef	HAVE_SDRPLAY_V3
#include        "sdrplay-handler-v3.h"
#endif
#ifdef	HAVE_HACKRF
#include	"hackrf-handler.h"
#endif
#ifdef	HAVE_AIRSPY
#include	"airspy-handler.h"
#endif
#ifdef	HAVE_AIRSPYHF
#include	"airspyhf-handler.h"
#endif
#ifdef	HAVE_RTLSDR
#include	"rtlsdr-handler.h"
#endif
#ifdef	HAVE_SPYSERVER
#include	"spyserver-client.h"
#endif
#ifdef	HAVE_CARDREADER
#include	"card-reader.h"
#endif
//
#include	"deviceselect.h"
#include	"drm-decoder.h"

#define	D_SDRPLAY_V2	"sdrplay_v2"
#define	D_SDRPLAY_V3	"sdrplay"
#define	D_RTL_TCP	"rtl_tcp"
#define	D_HACKRF	"hackrf"
#define	D_AIRSPY	"AirSpy R2"
#define	D_AIRSPYHF	"AirSpy HF+ Discovery"
#define	D_RTLSDR	"RTL-SDR"
#define	D_SPYSERVER	"spyserver_client"
#define	D_CARDREADER	"Sound card (IQ, 96 kHz)"
static 
const char *deviceTable [] = {
#ifdef	HAVE_SDRPLAY_V2
	D_SDRPLAY_V2,
#endif
#ifdef	HAVE_SDRPLAY_V3
	D_SDRPLAY_V3,
#endif
#ifdef	HAVE_HACKRF
	D_HACKRF,
#endif
#ifdef	HAVE_AIRSPY
	D_AIRSPY,
#endif
#ifdef	HAVE_AIRSPYHF
	D_AIRSPYHF,
#endif
#ifdef	HAVE_RTLSDR
	D_RTLSDR,
#endif
#ifdef	HAVE_RTL_TCP
	D_RTL_TCP,
#endif
#ifdef	HAVE_SPYSERVER
	D_SPYSERVER,
#endif
#ifdef	HAVE_CARDREADER
	D_CARDREADER,
#endif
	nullptr
};

float	Alpha	= 1.0 / 12000;
static int startKnop;
static	QTimer	*starter;

static inline
int twoPower (int a) {
int	res	= 1;
	while (--a >= 0)
	   res <<= 1;
	return res;
}

QString	FrequencytoString (int32_t freq) {
	if (freq < 10)
	   return QString (QChar ('0' + (uint8_t)(freq % 10)));
	return 
	   FrequencytoString (freq / 10). append (QChar ('0' + (uint8_t)(freq % 10)));
}

	RadioInterface::RadioInterface (QSettings	*sI,
	                                QString		stationList,
	                                bandPlan	*my_bandPlan,
	                                QWidget		*parent):
	                                    QMainWindow (parent),
	                                    inputData	(1024 * 1024),
	                                    hfShifter	(96000),
	                                    hfFilter	(2048, 377),
	                                    audioData	(48000),
	                                    theDecimator (96000, 12000) {

	this	-> settings	= sI;
	this	-> my_bandPlan	= my_bandPlan;
	this	-> inputRate	= 96000;
	setupUi (this);
//      and some buffers
	audioRate		= 48000;

//	and the decoders
	displaySize		= 1024;
	scopeWidth		= inputRate;
	centerFrequency		= KHz (14070);
	selectedFrequency	= KHz (14070);

	dcReal                  = 0;
        dcImag                  = 0;

	theBand. currentOffset	= 0;
	theBand. lowF		= -5000;
	theBand. highF		= 5000;
	mouseIncrement		= settings -> value ("mouseIncrement", 5000). toInt ();
	if ((mouseIncrement < 1) || (mouseIncrement > 100000))
	   mouseIncrement	= 5000;
//	scope and settings
//	0 = Auto, 1 .. 100 = % of the level range shown; keyboard and
//	mouse wheel work on it as well as the mouse: arrows 1, PageUp/Down 5
	hfScopeSlider	-> setRange (0, 100);
	hfScopeSlider	-> setSingleStep (1);
	hfScopeSlider	-> setPageStep (5);
	hfScopeSlider	-> setFocusPolicy (Qt::WheelFocus);
	hfScopeSlider	-> setValue (20);
	hfScope		= new fftScope (hfSpectrum,
	                                displaySize,
	                                kHz (1),        // scale
	                                inputRate,
	                                hfScopeSlider -> value (),
	                                8);
	hfScope		-> set_bitDepth (12);	// default
	centerFrequency		= KHz (14070);	// default
	selectedFrequency	= KHz (14070);	// default
	hfScope	-> setScope (centerFrequency, selectedFrequency - centerFrequency);
	connect (hfScope,
	         SIGNAL (clickedwithLeft (int)),
	         this,
	         SLOT (adjustFrequency_khz (int)));
	connect (hfScope,
	         SIGNAL (clickedwithRight (int)),
	         this, SLOT (switch_hfViewMode (int)));
	connect (hfScopeSlider, SIGNAL (valueChanged (int)),
	         this, SLOT (set_hfscopeLevel (int)));

//	output device
	audioHandler            = new audioSink (this -> audioRate, 16384);
	outTable                = new int16_t
	                             [audioHandler -> numberofDevices () + 1];
	for (int i = 0; i < audioHandler -> numberofDevices (); i ++)
	   outTable [i] = -1;

	try {
	   setupSoundOut (streamOutSelector, audioHandler,
	                  audioRate, outTable);
	} catch (int e) {
	   QMessageBox::warning (this, tr ("sdr"),
	                               tr ("Opening audio failed\n"));
	   abort ();
	}
	audioHandler -> selectDefaultDevice ();

	mykeyPad                = new keyPad (this);
	connect (freqButton, SIGNAL (clicked ()),
	         this, SLOT (handle_freqButton ()));

//	step of the up/down buttons, in Hz: 1 Hz .. 100 kHz, 5 kHz at the start
	mouse_Inc	-> setRange (1, 100000);
	mouse_Inc	-> setSuffix (" Hz");
	mouse_Inc	-> setStepType (QAbstractSpinBox::AdaptiveDecimalStepType);
	mouse_Inc	-> setValue (mouseIncrement);
	set_mouseIncrement (mouseIncrement);	// also sets the tooltips
	connect (mouse_Inc, SIGNAL (valueChanged (int)),
	         this, SLOT (set_mouseIncrement (int)));
	connect (upButton, SIGNAL (clicked ()),
	         this, SLOT (handle_upButton ()));
	connect (downButton, SIGNAL (clicked ()),
	         this, SLOT (handle_downButton ()));
	connect (streamOutSelector, SIGNAL (activated (int)),
	         this, SLOT (setStreamOutSelector (int)));

	connect (freqSave, SIGNAL (clicked ()),
	         this, SLOT (set_freqSave ()));

	myList  = new programList (this, stationList);
	myList  -> show ();
	myLine		= NULL;
	myLineContainer	= NULL;
	myLinePrompt	= NULL;
	audioHandler    -> selectDefaultDevice ();

	closing			= false;
	theDevice		= nullptr;
	updateScaleToolTip (false);
	delayCount		= 0;
	dumpfilePointer		= nullptr;

	connect (dumpButton, SIGNAL (clicked (void)),
	         this, SLOT (set_dumpButton (void)));
	secondsTimer. setInterval (1000);
	connect (&secondsTimer, SIGNAL (timeout (void)),
	         this, SLOT (updateTime (void)));
	secondsTimer. start (1000);

//	and off we go
	theDecoder	= new drmDecoder (this, settings,  &audioData);
//	bind the signals in the decoder to some slots
	connect (theDecoder, SIGNAL (audioAvailable (int, int)),
	         this, SLOT (processAudio (int, int)));
//
//	and now for the device
	QString device =
	            settings -> value ("device", "no device").toString ();
	int k = -1;
	fprintf (stderr, "Looking for device %s\n", device. toLatin1 (). data ());
	for (int i = 0; deviceTable [i] != nullptr; i ++)
	   if (deviceTable [i] == device) {
	      k = i;
	      break;
	}
	fprintf (stderr, "K = %d\n", k);
	if (k != -1) {
	   starter	= new QTimer;
	   startKnop	= k;
	   starter -> setSingleShot (true);
	   starter -> setInterval (500);
	   connect (starter, SIGNAL (timeout ()), 
	            this, SLOT (quickStart ()));
	   starter -> start (500);
	}
	else {
	   startKnop	= 0;
	   theDevice 	= setDevice (settings, &inputData);
	   if (theDevice != nullptr) {
	      hfScope	 -> set_bitDepth (theDevice -> bitDepth ());
	      updateScaleToolTip (false);
	      connect (theDevice, SIGNAL (dataAvailable (int)),
	               this, SLOT (sampleHandler (int)));
	      fprintf (stderr, "Device hier?\n");
	      theDevice	-> restartReader ();
	      fprintf (stderr, "Hier??\n");
	   }
	   else
	      throw (24);
	}
	fprintf (stderr, "end van radio start\n");
}

void    RadioInterface::quickStart () {
	disconnect (starter, SIGNAL (timeout ()),
	            this, SLOT (quickStart ()));
	fprintf (stderr, "going for quickStart\n");
	delete starter;
	theDevice = getDevice (deviceTable [startKnop], settings, &inputData);
	if (theDevice == nullptr) {
	   fprintf (stderr,
	            "quick-start device failed to open, showing selector\n");
	   theDevice = setDevice (settings, &inputData);
	   if (theDevice == nullptr)
	      throw (31);
	}
	hfScope	 -> set_bitDepth (theDevice -> bitDepth ());
	      updateScaleToolTip (false);
	connect (theDevice, SIGNAL (dataAvailable (int)),
	         this, SLOT (sampleHandler (int)));
	theDevice	-> restartReader ();
}

//      The end of all
	RadioInterface::~RadioInterface () {
}
//
//	If the user quits before selecting a device ....
void	RadioInterface::handle_quitButton	(void) {
//	sampleHandler () can be on the stack further down (it calls
//	processEvents ()), so from here on it must not touch anything
	if (closing)
	   return;
	closing		= true;
	secondsTimer. stop ();
	myList		-> saveTable ();	// first: closing the receiver may hang
	if (theDevice != NULL) {
	   theDevice	-> stopReader ();
	   disconnect (theDevice, SIGNAL (dataAvailable (int)),
	               this, SLOT (sampleHandler (int)));
	   delete  theDevice;
	}
	fprintf (stderr, "device is gone, go\n");
	fprintf (stderr, "device is deleted\n");
	myList		-> hide ();

	fprintf (stderr, "we kappen ermee\n");
	delete		theDecoder;
	delete		myList;
	delete []	outTable;
	delete		hfScope;
	delete		mykeyPad;
//	data-available signals already queued must not reach the deleted objects
	QCoreApplication::removePostedEvents (this, QEvent::MetaCall);
}

deviceHandler	*RadioInterface::
	            setDevice (QSettings *s,
	                       RingBuffer<std::complex<float>> *hfBuffer) {
deviceSelect	deviceSelect;
deviceHandler	*theDevice	= nullptr;
QStringList devices;

	for (int i = 0; deviceTable [i] != nullptr; i ++)
	   devices += deviceTable [i];
	devices	+= "filereader";
	devices	+= "quit";
	deviceSelect. addList (devices);
	int theIndex = -1;
	while (theDevice == nullptr) {
	   theIndex = deviceSelect. QDialog::exec ();
	   if (theIndex < 0)
	      continue;
	   QString s = devices. at (theIndex);
	   if (s == "quit")
	      return nullptr;
	   fprintf (stderr, "we go for %s\n", s. toLatin1 (). data ());
	   theDevice	= getDevice (s, settings, hfBuffer);
	   if (theDevice == nullptr) {
	      fprintf (stderr, "could not open %s, pick again\n",
	                             s. toLatin1 (). data ());
	      continue;
	   }
	   theDevice -> restartReader ();
	   fprintf (stderr, "Hebben we een device?\n");
	}
	return theDevice;
}

deviceHandler	*RadioInterface::
	                      getDevice (const QString &s,
	                                 QSettings *settings,
	                                 RingBuffer<std::complex<float>> *b) {
#ifdef HAVE_SDRPLAY_V2
	if (s == D_SDRPLAY_V2) {
	   try {
	      return  new sdrplayHandler_v2 (this, settings, b);
	   } catch (int e) {
	   }
	}
	else
#endif
#ifdef HAVE_SDRPLAY_V3
	if (s == D_SDRPLAY_V3) {
	   try {
	      return new sdrplayHandler_v3 (this, settings, b);
	   } catch (int e) {
	   }
	}
	else
#endif
#ifdef HAVE_HACKRF
	if (s == D_HACKRF) {
	   try {
	      return  new hackrfHandler (this, settings, b);
	   } catch (int e) {
	   }
	}
	else
#endif
#ifdef HAVE_AIRSPY
	if (s == D_AIRSPY) {
	   try {
	      return  new airspyHandler (this, settings, b);
	   } catch (int e) {
	   }
	}
	else
#endif
#ifdef HAVE_AIRSPYHF
	if (s == D_AIRSPYHF) {
	   try {
	      return  new airspyhfHandler (this, settings, b);
	   } catch (int e) {
	   }
	}
	else
#endif
#ifdef HAVE_RTLSDR
	if (s == D_RTLSDR) {
	   fprintf (stderr, "going for the stick\n");
	   try {
	      return new rtlsdrHandler (this, settings, b);
	   } catch (int e) {
	   }
	}
	else
#endif
#ifdef HAVE_SPYSERVER
	if (s == D_SPYSERVER) {
	   fprintf (stderr, "going for the spyServer client\n");
	   try {
	      deviceHandler *tt =  new spyServer_client (this, settings, b);
	      tt -> restartReader ();
	      return tt;
	   } catch (int e) {
	   }
	}
	else
#endif
#ifdef HAVE_CARDREADER
	if (s == D_CARDREADER) {
	   fprintf (stderr, "going for the cardreader\n");
	   try {
	      deviceHandler *tt =  new cardReader (this, settings, b);
	      tt -> restartReader ();
	      return tt;
	   } catch (int e) {
	      if (e == 20)
	         QMessageBox::warning (this, tr ("drm"),
	                               tr ("Init of portaudio library failed"));
	      else
	      if (e == 21)
	         QMessageBox::warning (this, tr ("drm"),
	                               tr ("No input channels detected"));
//	      fall through
	   }
	}
	else
#endif
	if (s == "filereader") {
	   try {
	      return new fileReader (this, settings, b);
	   } catch (int e) {
	   }
	}
	else
	QMessageBox::warning (this, tr ("sdr"),
	                               tr ("loading device failed"));
	return nullptr;
}
//
//	
void    RadioInterface::handle_freqButton (void) {
	if (mykeyPad -> isVisible ())
	   mykeyPad -> hidePad ();
	else
	   mykeyPad     -> showPad ();
}

//	setFrequency is called from the frequency panel
//	as well as from inside to change VFO and offset
void	RadioInterface::setFrequency (int32_t frequency) {
	centerFrequency		= frequency;
	selectedFrequency	= frequency;
	theDevice	-> setVFOFrequency (centerFrequency);
	hfScope		-> setScope  (centerFrequency,
	                              centerFrequency - selectedFrequency);
	hfFilter. setBand (theBand. currentOffset + theBand. lowF,
	                   theBand. currentOffset + theBand. highF,
	                                          inputRate);
	fprintf (stderr, "Low %d high %d\n", theBand. lowF, theBand. highF);
	QString ff	= FrequencytoString (selectedFrequency);
	frequencyDisplay	-> display (ff);
	bandLabel		-> setText (my_bandPlan -> getFrequencyLabel (selectedFrequency));
//
//	the user is now listening to a different frequency: drop any
//	"locked on a station" state left over from the previous one,
//	rather than leaving a stale station name/indicators on screen.
	if (theDecoder != nullptr)
	   theDecoder	-> resetDisplay ();
}
//
//	adjustFrequency is called whenever clicking the mouse
void	RadioInterface::adjustFrequency_khz (int32_t n) {
	adjustFrequency_hz (1000 * n);
	fprintf (stderr, "adjusting %d Hz\n", n * 1000);
}

void	RadioInterface::adjustFrequency_hz (int32_t n) {
int	lowF	= theBand. lowF;
int	highF	= theBand. highF;
int	currOff	= theBand. currentOffset;

	if ((currOff + highF + n >= scopeWidth / 2 - scopeWidth / 20) ||
	    (currOff + lowF + n <= - scopeWidth / 2 + scopeWidth / 20) ) {
	   quint64 newFreq = theDevice -> getVFOFrequency () +
	                                   theBand. currentOffset + n;
	   setFrequency ((quint64)newFreq);
	   return;
	}
	else {
	   theBand. currentOffset += n;
	   hfScope -> setScope (theDevice -> getVFOFrequency (),
	                                   theBand. currentOffset);
	   hfFilter. setBand (theBand. currentOffset + theBand. lowF,
	                      theBand. currentOffset + theBand. highF,
	                            inputRate);
	}

	int freq		= theDevice -> getVFOFrequency () +
	                                    theBand. currentOffset;
	QString ff = FrequencytoString ((quint64)freq);
	frequencyDisplay	-> display (ff);
//	bandLabel	-> setAutoFillBackground (true);
//	bandLabel	-> setPalette (labelPalette);
	bandLabel	-> setText (my_bandPlan -> getFrequencyLabel (freq));
//
//	A jump of 1 kHz or more (up/down buttons, click in the spectrum) means
//	a different station is wanted: drop the old station's name and
//	indicators. Fine tuning by a few Hz keeps the lock, so is left alone.
	if (((n >= 1000) || (n <= -1000)) && (theDecoder != nullptr))
	   theDecoder	-> resetDisplay ();
}


//      just a convenience button
void    RadioInterface::set_inMiddle () {
	quint64 newFreq = theDevice -> getVFOFrequency () +
	                                    theBand. currentOffset;
	setFrequency (newFreq);
}

int32_t	RadioInterface::get_selectedFrequency	() {
	return selectedFrequency;
}

int32_t	RadioInterface::get_centerFrequency	() {
	return centerFrequency;
}
//
void    RadioInterface::set_freqSave    () {
	if (myLineContainer == NULL) {
	   myLineContainer	= new QWidget ();
	   myLineContainer	-> setWindowTitle ("Save station");
	   myLinePrompt		= new QLabel ("Enter a name for the radio station");
	   myLine		= new QLineEdit ();
	   QVBoxLayout *lay	= new QVBoxLayout ();
	   lay	-> addWidget (myLinePrompt);
	   lay	-> addWidget (myLine);
	   myLineContainer	-> setLayout (lay);
	   connect (myLine, SIGNAL (returnPressed (void)),
	            this, SLOT (handle_myLine (void)));
	}
//
//	pre-fill with the station name as currently decoded from the
//	signal, if any - the user can still overtype it or append to it
	if (theDecoder != nullptr)
	   myLine	-> setText (theDecoder -> getServiceName ());
	myLine	-> selectAll ();
	myLineContainer	-> show ();
	myLine		-> setFocus ();
}

void    RadioInterface::handle_myLine () {
QString programName     = myLine -> text ();
	myList  -> addRow (programName, QString::number (selectedFrequency / Khz (1)));
	myLineContainer  -> hide ();
}

static
QString stepText (int32_t inc) {
	if (inc >= 1000)
	   return QString::number (inc / 1000.0, 'g', 6) + " kHz";
	return QString::number (inc) + " Hz";
}

void	RadioInterface::set_mouseIncrement (int inc) {
	mouseIncrement = inc;
	settings	-> setValue ("mouseIncrement", inc);
	QString t	= stepText (inc);
	mouse_Inc	-> setToolTip (tr ("Tuning step of the up and down buttons: %1\n"
	                              "Type a value from 1 to 100000 Hz, or use the arrows."). arg (t));
	upButton	-> setToolTip (tr ("Tune up by %1"). arg (t));
	downButton	-> setToolTip (tr ("Tune down by %1"). arg (t));
}

void	RadioInterface::wheelEvent (QWheelEvent *e) {
//	adjustFrequency_hz ((e -> angleDelta (). rx () > 0) ?
//	                        mouseIncrement : -mouseIncrement);
}

void	RadioInterface::handle_upButton	() {
	adjustFrequency_hz (mouseIncrement);
}

void	RadioInterface::handle_downButton	() {
	adjustFrequency_hz (-mouseIncrement);
}


//////////////////////////////////////////////////////////////////
//
void	RadioInterface::sampleHandler (int amount) {
std::complex<float>   buffer [theDecimator. inSize ()]; 
std::complex<float> ifBuffer [theDecimator. outSize ()];
float dumpBuffer [2 * 512];

int	chunkCount	= 0;

	(void)amount;
	if (closing)
	   return;
	while (inputData. GetRingBufferReadAvailable () > 512) {
//
//	On a host that cannot keep up with the incoming sample rate
//	in real time, this loop can end up processing a large and
//	ever-growing backlog in one go, which blocks the Qt event
//	loop for as long as it takes - the window stops repainting
//	and even the close ("X") button stops responding. Yielding
//	back to the event loop every so often keeps the UI alive
//	(and closable) even while catching up on a backlog.
	   if (++chunkCount % 32 == 0) {
	      QCoreApplication::processEvents ();
	      if (closing)		// "X" was answered Yes in there:
	         return;		// the objects below are gone
	   }
	   inputData. getDataFromBuffer (buffer, 512);
	   hfScope	-> addElements (buffer, 512);
	   if (dumpfilePointer != NULL) {
	      for (int i = 0; i < 512; i ++) {
	         dumpBuffer [2 * i]	= real (buffer [i]);
	         dumpBuffer [2 * i + 1] = imag (buffer [i]);
	      }
	      sf_writef_float (dumpfilePointer, dumpBuffer, 512);
	   }

	   for (int i = 0; i < 512; i ++) {
	      std::complex<float> v = buffer [i];
              dcReal            = compute_avg (dcReal, real (v), Alpha);
              dcImag            = compute_avg (dcImag, imag (v), Alpha);
              v = std::complex<float> (real (v) - dcReal, imag (v) - dcImag);

              std::complex<float> temp = hfFilter. Pass (v);

//	      std::complex<float> temp = hfFilter. Pass (buffer [i]);
	      temp = hfShifter. do_shift (temp, theBand. currentOffset);
	      if (theDecimator. add (temp, ifBuffer)) {
	         for (int j = 0; j < theDecimator. outSize (); j ++) {
	            theDecoder -> process (ifBuffer [j]);
	         }
	      }
	   }
	}
}
//
//
void    RadioInterface::set_hfscopeLevel (int level) {
	hfScope -> setLevel (level);
	updateScaleToolTip (true);
}

//	The tooltip of the scale slider: its value in %, what that means in dB
//	for the receiver in use, and how to set it exactly. While the slider is
//	being changed the text is shown at once, so the value can be read.
void	RadioInterface::updateScaleToolTip (bool show) {
int	level	= hfScopeSlider -> value ();
int	bd	= (theDevice != nullptr) ? theDevice -> bitDepth () : 12;
QString	t;

	if (level <= 0)
	   t = tr ("Spectrum scale: Auto\n"
	           "Fits the strongest signal in view, so weak and strong signals "
	           "both show.");
	else
	   t = tr ("Spectrum scale: %1 %\n"
	           "The display shows the lowest %2 dB above the noise floor.\n"
	           "Lower = more zoom, for weak signals.").
	             arg (level).
	             arg (level / 100.0 * bd * 6.0206, 0, 'f', 1);
	t += tr ("\nArrow keys or mouse wheel: 1 step. PageUp / PageDown: 5 steps.\n"
	         "Home (or all the way down): Auto.");
	hfScopeSlider	-> setToolTip (t);
	if (show && (hfScopeSlider -> underMouse () || hfScopeSlider -> hasFocus ())) {
	   QPoint p = hfScopeSlider -> underMouse () ? QCursor::pos () :
	       hfScopeSlider -> mapToGlobal (QPoint (hfScopeSlider -> width (),
	                                             hfScopeSlider -> height () / 2));
	   QToolTip::showText (p, t, hfScopeSlider);
	}
}

//	do not forget that ocnt starts with 1, due
//	to Qt list conventions
void	RadioInterface::setupSoundOut (QComboBox	*streamOutSelector,
	                               audioSink	*our_audioSink,
	                               int32_t		cardRate,
	                               int16_t		*table) {
uint16_t	ocnt	= 1;
uint16_t	i;

	for (i = 0; i < our_audioSink -> numberofDevices (); i ++) {
	   const char *so =
	             our_audioSink -> outputChannelwithRate (i, cardRate);

	   if (so != NULL) {
	      streamOutSelector -> insertItem (ocnt, so, QVariant (i));
	      table [ocnt] = i;
	      ocnt ++;
	   }
	}

	qDebug () << "added items to combobox";
	if (ocnt == 1)
	   throw (22);
}

void	RadioInterface::setStreamOutSelector (int idx) {
int16_t	outputDevice;

	if (idx == 0)
	   return;

	return;
	outputDevice = outTable [idx];
	if (!audioHandler -> isValidDevice (outputDevice)) 
	   return;

	audioHandler	-> stop	();
	if (!audioHandler -> selectDevice (outputDevice)) {
	   QMessageBox::warning (this, tr ("sdr"),
	                               tr ("Selecting  output stream failed\n"));
	   return;
	}

	qWarning () << "selected output device " << idx << outputDevice;
	audioHandler	-> restart ();
}

void    RadioInterface::processAudio  (int amount, int rate) {
DSPCOMPLEX buffer [rate / 10];

	(void)amount;
	usleep (1000);
	while (audioData. GetRingBufferReadAvailable () >
	                                         (uint32_t)rate / 10) {
	   audioData. getDataFromBuffer (buffer, rate / 10);
	   audioHandler      -> putSamples (buffer, rate / 10);
	}
}

void	RadioInterface::switch_hfViewMode	(int d) {
	(void)d;
	hfScope	-> switch_viewMode ();
}

void	RadioInterface::updateTime		(void) {
QDateTime currentTime = QDateTime::currentDateTime ();

	timeDisplay     -> setText (currentTime.
	                            toString (QString ("dd.MM.yy:hh:mm:ss")));
}

void	RadioInterface::set_dumpButton (void) {
SF_INFO	*sf_info	= (SF_INFO *)alloca (sizeof (SF_INFO));

	if (dumpfilePointer != NULL) {
	   sf_close (dumpfilePointer);
	   dumpfilePointer	= NULL;
	   dumpButton	-> setText ("dump");
	   return;
	}

	QString file = QFileDialog::getSaveFileName (this,
	                                        tr ("Save file ..."),
	                                        QDir::homePath (),
	                                        tr ("PCM wave file (*.wav)"));
	if (file == QString (""))
	   return;
	file		= QDir::toNativeSeparators (file);
	if (!file.endsWith (".wav", Qt::CaseInsensitive))
	   file.append (".wav");
	sf_info		-> samplerate	= inputRate;
	sf_info		-> channels	= 2;
	sf_info		-> format	= SF_FORMAT_WAV | SF_FORMAT_FLOAT;

	dumpfilePointer	= sf_open (file. toUtf8 (). data (),
	                                   SFM_WRITE, sf_info);
	if (dumpfilePointer == NULL) {
	   qDebug () << "Cannot open " << file. toUtf8 (). data ();
	   return;
	}

	dumpButton		-> setText ("WRITING");
}

#include <QCloseEvent>
void RadioInterface::closeEvent (QCloseEvent *event) {

	QMessageBox::StandardButton resultButton =
	                QMessageBox::question (this, "Dxing.org DRM radio",
	                                       tr("Are you sure?\n"),
	                                       QMessageBox::No | QMessageBox::Yes,
	                                       QMessageBox::Yes);
	if (resultButton != QMessageBox::Yes) {
	   event -> ignore();
	} else {
	   handle_quitButton ();
	   event -> accept ();
	}
}

