
#include	<vector>
#include	<chrono>
#include	<QEvent>
#include	<QTimer>
#include	<QMessageBox>

#include	"drm-decoder.h"

//
//	drm specifics

#include	"utilities.h"

#include	"timesync.h"
#include	"freqsyncer.h"
#include	"word-collector.h"
#include	"correlator.h"
#include	"my-array.h"
#include	"referenceframe.h"
#include	"equalizer-1.h"
#include	"fac-processor.h"
#include	"fac-tables.h"
#include	"sdc-processor.h"
#include	"eqdisplay.h"
#include	"iqdisplay.h"
#include	"data-inspector.h"
#include	"journaline-window.h"
#include	"service-info-window.h"
#include	"mot-window.h"
#include	<QTimer>
#include	<QToolTip>
#include	<QCursor>

#define  _USE_MATH_DEFINES
#include	<math.h>

	drmDecoder::drmDecoder (RadioInterface *theRadio,
	                        QSettings *s,
	                        RingBuffer<std::complex<float>> *audioBuffer):
	                                 myFrame (nullptr),
	                                 m_worker (nullptr),
	                                 inputBuffer  (16 * 32768),
	                                 iqBuffer (32768),
	                                 eqBuffer (32768),
	                                 my_Reader (&inputBuffer,
	                                           2 * 16384, this),
#ifdef	__WITH_FDK_AAC__
#ifdef	__MINGW32__
	                                 aacFunctions (0),
#endif
#endif
	                                 my_backendController (this, 4,
#ifdef	__WITH_FDK_AAC__
#ifdef	__MINGW32__
	                                                       &aacFunctions,
#endif
#endif
	                                                       audioBuffer,
	                                                       &iqBuffer),
	                                theState (1, 3) {
	this	-> theRadio	= theRadio;
	theState. diag	= theDataHub. serviceDiag ();
	theState. info	= &theSdcInfo;
#ifdef	__WITH_FDK_AAC__
#ifdef	__MINGW32__
//	no libfdk-aac-2.dll next to the program: say so (after the windows are up)
	if (!aacFunctions. ok)
	   QTimer::singleShot (1500, this, [this] () { showAacMissing (); });
#endif
#endif
	drmSettings		= s;
	setupUi (&myFrame);
	my_eqDisplay            = new EQDisplay (equalizerDisplay);
	my_iqDisplay            = new IQDisplay (iqPlotter, 512);

	scopeMode		= SHOW_PILOTS;
	myFrame. installEventFilter (this);
	myFrame. show ();
	running. store (false);

	drmError		= false;
	nSymbols		= 25;
	modeInf. Mode		= 2;
	modeInf. Spectrum	= 3;

//	connect (strengthSelector, SIGNAL (valueChanged (int)),
//	         this, SLOT (handle_strengthSelector (int)));
//	connect (f_cutSelector, SIGNAL (valueChanged (int)),
//	         this, SLOT (handle_f_cutSelector (int)));
	connect (modeSelector, SIGNAL (textActivated (const QString &)),
                 this, SLOT (handle_modeSelector (const QString &)));

	connect (this, SIGNAL (setTimeSync (bool)),
                 this, SLOT (executeTimeSync (bool)));
        connect (this, SIGNAL (setFACSync (bool)),
                 this, SLOT (executeFACSync (bool)));
        connect (this, SIGNAL (setSDCSync (bool)),
                 this, SLOT (executeSDCSync (bool)));
        connect (this, SIGNAL (show_Mode (int)),
                 this, SLOT (execute_showMode (int)));
        connect (this, SIGNAL(show_Spectrum (int)),
                 this, SLOT (execute_showSpectrum (int)));
	connect (channel_1, SIGNAL (clicked ()),
                 this, SLOT (select_channel_1 ()));
        connect (channel_2, SIGNAL (clicked ()),
                 this, SLOT (select_channel_2 ()));

	connect (resetButton, SIGNAL (clicked ()),
	         this, SLOT (handle_reset ()));
	dataInspector	= new DataInspector (&theDataHub, &myFrame);
	connect (dataButton, SIGNAL (clicked ()),
	         this, SLOT (handle_dataButton ()));
//	every complete data unit of the announced packet id goes to the
//	Journaline decoder (it takes only its own application)
	theDataHub. setGroupSink ([this] (int stream, const dxdata::DataGroup &g) {
	                              theJournaline. handleGroup (stream, g);
	                              theMot. handleGroup (stream, g);
	                              theDb. handleGroup (stream, g);
	                           });
	journalineWindow = new JournalineWindow (&theJournaline, &theDataHub,
	                                         &myFrame);
	journalineButton -> setEnabled (false);
	connect (journalineButton, SIGNAL (clicked ()),
	         this, SLOT (handle_journalineButton ()));
	serviceInfoWindow = new ServiceInfoWindow (&theSdcInfo, &myFrame);
	connect (serviceInfoButton, SIGNAL (clicked ()),
	         this, SLOT (handle_serviceInfoButton ()));
//	double click on a DRM frequency of the alternative frequency list
	connect (serviceInfoWindow, SIGNAL (tuneRequest (int32_t)),
	         theRadio, SLOT (setFrequency (int32_t)));
	motWindow	= new MotWindow (&theMot, &theDb, &theDataHub, &myFrame);
	slideshowButton -> setEnabled (false);
	motButton	-> setEnabled (false);
	connect (slideshowButton, SIGNAL (clicked ()),
	         this, SLOT (handle_slideshowButton ()));
	connect (motButton, SIGNAL (clicked ()),
	         this, SLOT (handle_motButton ()));
	dataTimer	= new QTimer (this);
	dataTimer	-> setInterval (1000);
	connect (dataTimer, SIGNAL (timeout ()),
	         this, SLOT (updateDataButtons ()));
	dataTimer	-> start ();
	m_worker	=
	       new std::thread (&drmDecoder::WorkerFunction, this);
}

	drmDecoder::~drmDecoder () {	
	running. store (false);
	my_Reader. stop ();
	m_worker        -> join ();
	delete m_worker;
	m_worker = nullptr;
	dataTimer	-> stop ();
	delete	journalineWindow;
	journalineWindow = nullptr;
	delete	serviceInfoWindow;
	serviceInfoWindow = nullptr;
	delete	motWindow;
	motWindow	= nullptr;
	delete	dataInspector;
	dataInspector	= nullptr;
	delete	my_eqDisplay;
	delete	my_iqDisplay;
}
//
//	Called by the underlying system
//
void	drmDecoder::
	         processBuffer (std::complex<float>* buffer, int length) {
	inputBuffer. putDataIntoBuffer (buffer, length);
}

void	drmDecoder::process (std::complex<float> s) {
	inputBuffer. putDataIntoBuffer (&s, 1);
}
//

void	drmDecoder::WorkerFunction () {
int16_t	blockCount      = 0;
bool	inSync;
int16_t	symbol_no       = 0;
bool	frameReady;
int counter = 0;
float     deltaFreqOffset         = 0;
float     sampleclockOffset       = 0;


	running. store (true);
	while (running. load ()) {
	   try {
	      if (!running. load ())
	         throw (21);
	      counter++;
	      emit setTimeSync (false);
	      emit setFACSync (false);
	      emit setSDCSync (false);
	      theState. cleanUp ();
	      my_Reader. waitfor (Ts_of (Mode_A));
	  
//      First step: find mode and starting point
	      modeInf. Mode = -1;
	      while (running. load () && (modeInf. Mode == -1)) {
	         my_Reader. shiftBuffer (Ts_of (Mode_A) / 3);
	         getMode (&my_Reader, &modeInf);
	      }

	      if (!running. load ())
	         throw (20);

	      setTimeSync (true);
	      my_Reader. shiftBuffer (modeInf. timeOffset_integer);
	      frequencySync (&my_Reader, &modeInf);

	      show_Mode		(modeInf. Mode);
	      show_Spectrum	(modeInf. Spectrum);
//	      show_coarseOffset	(modeInf. freqOffset_integer);
//	      show_fineOffset	(modeInf. freqOffset_fractional);

	      theState. Mode		= modeInf. Mode;
	      theState. Spectrum	= modeInf. Spectrum;
	      int nrSymbols		= symbolsperFrame (modeInf. Mode);
	      int nrCarriers       = Kmax (modeInf. Mode, modeInf. Spectrum) -
	                             Kmin (modeInf. Mode, modeInf. Spectrum) + 1;

	      myArray<std::complex<float>> inbank (nrSymbols, nrCarriers);
	      myArray<theSignal> outbank (nrSymbols, nrCarriers);
	      correlator myCorrelator (&modeInf);
	      equalizer_1 my_Equalizer (this,
	                                modeInf.Mode,
	                                modeInf.Spectrum,
	                                &eqBuffer);
	
	      my_Equalizer. set_scopeMode (scopeMode);
	      std::vector<std::complex<float>> displayVector;
	      displayVector. resize (Kmax (modeInf. Mode, modeInf. Spectrum) -
	                             Kmin (modeInf. Mode, modeInf. Spectrum) + 1);
	      wordCollector my_wordCollector (this,
	                                      &my_Reader,
	                                      &modeInf,
	                                      WORKING_RATE);
	      facProcessor my_facProcessor (this, &modeInf);

//	   we know that - when starting - we are not "in sync" yet
	      inSync	= false;
//
//	   we read one full frame after which we start looking for a 
//	   match
	      for (int symbol = 0; symbol < nrSymbols - 1; symbol ++) {
	         my_wordCollector. getWord (inbank. element (symbol),
	                                    modeInf. freqOffset_integer,
	                                    modeInf. timeOffset_fractional,
	                                    modeInf. freqOffset_fractional
	                                   );
	         myCorrelator. correlate (inbank. element (symbol), symbol);
	       }

	      int  lc      = nrSymbols - 1;
//      We keep on reading here until we are satisfied that the
//      frame that is in, looks like a decent frame, just by the
//      correlation on the first word
	      while (running. load ()) {
	         my_wordCollector. getWord (inbank. element (lc),
	                                    modeInf. freqOffset_integer,
	                                    modeInf. timeOffset_fractional,
	                                    modeInf. freqOffset_fractional
	                                   );
	         myCorrelator. correlate (inbank. element (lc), lc);
	         lc = (lc + 1) % symbolsperFrame (modeInf. Mode);
	         if (myCorrelator. bestIndex (lc))  {
	            break;
	         }
	      }
		
//      from here on, we know that in the input bank, the frames occupy the
//      rows "lc" ... "(lc + symbolsinFrame) % symbolsinFrame"
//      so, once here, we know that the frame starts with index lc,
//      so let us equalize the last symbolsinFrame words in the buffer
	      for (symbol_no = 0; symbol_no < nrSymbols; symbol_no ++)
	         (void) my_Equalizer.
	            equalize (inbank. element ((lc + symbol_no) % nrSymbols),
	                      symbol_no,
	                      &outbank,
                              &modeInf. timeOffset_fractional,
                              &deltaFreqOffset,
                              &sampleclockOffset,
	                      displayVector);

	      lc           = (lc + symbol_no) % symbol_no;
	      symbol_no    = 0;
	      frameReady   = false;
	      while (running. load () && !frameReady) {
	         my_Equalizer. set_scopeMode (scopeMode);
		  my_wordCollector.getWord (inbank.element(lc),
				   modeInf.freqOffset_integer,
				  lc == 0,        // no-op
				  modeInf.timeOffset_fractional,
				  deltaFreqOffset,  // tracking value
				  sampleclockOffset); // tracking value

	         frameReady = my_Equalizer. 
	                            equalize (inbank. element (lc),
	                                      symbol_no,
	                                      &outbank,
	                                      &modeInf. timeOffset_fractional,
                                              &deltaFreqOffset,
                                              &sampleclockOffset,
	                                      displayVector);
			
	         lc = (lc + 1) % nrSymbols;
	         symbol_no = (symbol_no + 1) % nrSymbols;
	      }

	      if (!running.load())
	         throw (37);

//	when we are here, we do have  our first full "frame".
//	so, we will be convinced that we are OK when we have a decent FAC
	      inSync = my_facProcessor.  processFAC  (&outbank, &theState);

	  //	one test:
	      if (!inSync)
	         throw (33);
	      if (modeInf. Spectrum != getSpectrum (&theState))
	         throw (34);
	      emit setFACSync (true);
		
//
//	prepare for sdc processing
//	Since computing the position of the sdc Cells depends (a.o)
//	on FAC and other data cells, we better create the table here.
	      sdcTable. resize (sdcCells (&modeInf));
	      set_sdcCells (&modeInf);
	      sdcProcessor my_sdcProcessor (this, &modeInf,
	                                    sdcTable, &theState);

	      bool	superframer		= false;
	      int	missers			= 0;
	      bool	firstTime		= true;
	      float	deltaFreqOffset		= 0;
	      float	sampleclockOffset	= 0;
		  
		
	      while (true) {
	         my_Equalizer. set_scopeMode (scopeMode);
//	when we are here, we can start thinking about  SDC's and superframes
//	The first frame of a superframe has an SDC part
	         if (isFirstFrame (&theState)) {
	            bool sdcOK = my_sdcProcessor. processSDC (&outbank);
	            emit setSDCSync (sdcOK);
	            if (sdcOK) {
	               blockCount	= 0;
	            }
//
//	if we seem to have the start of a superframe, we
//	re-create a backend with the right parameters
	            if (!superframer && sdcOK)
	               my_backendController. reset (&theState);
	            superframer	= sdcOK;
	         }
//
//	when here, add the current frame to the superframe.
//	Obviously, we cannot garantee that all data is in order
	         if (superframer)
	            addtoSuperFrame (&modeInf, blockCount ++, &outbank);

//	when we are here, it is time to build the next frame
	         frameReady	= false;
	         for (int i = 0; !frameReady && (i < nrSymbols); i ++) {
	            my_wordCollector.
	               getWord (inbank. element ((lc + i) % nrSymbols),
	                        modeInf. freqOffset_integer,	// initial value
	                        (lc + i) % nrSymbols == 0,
	                        modeInf. timeOffset_fractional,
	                        deltaFreqOffset,	// tracking value
	                        sampleclockOffset	// tracking value
	                      );
	            firstTime	= false;
	            frameReady =
	                  my_Equalizer.
	                         equalize (inbank. element ((lc + i) % nrSymbols),
	                                   (symbol_no + i) % nrSymbols,
	                                   &outbank,
	                                   &modeInf. timeOffset_fractional,
	                                   &deltaFreqOffset,
	                                   &sampleclockOffset,
	                                   displayVector);
	         }
	
	         if (!frameReady)	// should not happen???
	            throw (36);
			 
//	OK, let us check the FAC
	         bool success  = my_facProcessor.
	                          processFAC (&outbank, &theState);
	         if (success) {
	            emit setFACSync	(true);
	            missers = 0;
	         }
	         else {
	            emit setFACSync	(false);
	            emit setSDCSync	(false);
	            superframer		= false;
	            if (missers++ < 2)
	               continue;
	            throw (35);	// ... or give up and start all over
	         }
	      }	// end of main loop
	   } catch (int e) {
	      if (!running. load ())
	         return;
	   }
	}
}

//      just for readability
uint8_t drmDecoder::getSpectrum     (stateDescriptor *f) {
uint8_t val = f -> spectrumBits;
	return val <= 5 ? val : 3;
}
//

void	drmDecoder::getMode (Reader *my_Reader, smodeInfo *m) {
timeSyncer  my_Syncer (my_Reader, WORKING_RATE,  nSymbols);
	my_Syncer. getMode (m);
}

void    drmDecoder::frequencySync (Reader *my_Reader, smodeInfo *m) {
freqSyncer my_Syncer (my_Reader, m, WORKING_RATE, this);
	my_Syncer. frequencySync (m);
}

int16_t	drmDecoder::sdcCells (smodeInfo *m) {
static
int m1_table []	= {167, 190, 359, 405, 754, 846};
static
int m2_table [] = {130, 150, 282, 322, 588, 1500};

	switch (m -> Mode) {
	   case Mode_A:
	      return m1_table [m -> Spectrum];

	   default:
	   case Mode_B:
	      return m2_table [m -> Spectrum];

	   case Mode_C:
	      return 288;

	   case Mode_D:
	      return 152;
	}
	return 288;
}

void	drmDecoder::set_sdcCells (smodeInfo *modeInf) {
uint8_t	Mode	= modeInf -> Mode;
uint8_t	Spectrum = modeInf -> Spectrum;
int	carrier;
int	cnt	= 0;
	for (carrier = Kmin(Mode, Spectrum);
	     carrier <= Kmax(Mode, Spectrum); carrier++) {
	   if (isSDCcell (modeInf, 0, carrier)) {
		  sdcTable[cnt].symbol = 0;
		  sdcTable[cnt].carrier = carrier;
		  cnt++;
	   }
	}
	
	for (carrier = Kmin (Mode, Spectrum);
	     carrier <= Kmax (Mode, Spectrum); carrier ++) {
	   if (isSDCcell (modeInf, 1, carrier)) {
	      sdcTable [cnt]. symbol = 1;
	      sdcTable [cnt]. carrier = carrier;
	      cnt ++;
	   }
	}
	   
	if ((Mode == Mode_C) || (Mode == Mode_D)) {
	   for (carrier = Kmin (Mode, Spectrum);
	        carrier <= Kmax (Mode, Spectrum); carrier ++) 
	      if (isSDCcell (modeInf, 2, carrier)) {
	         sdcTable [cnt]. symbol = 2;
	         sdcTable [cnt]. carrier = carrier;
	         cnt ++;
	      }
	}

	fprintf (stderr, "for Mode %d, spectrum %d, we have %d sdc cells\n",
	                       Mode, Spectrum, cnt);
}

bool	drmDecoder::isFACcell (smodeInfo *m,
	                             int16_t symbol, int16_t carrier) {
int16_t	i;
struct facElement *facTable     = getFacTableforMode (m -> Mode);

//	we know that FAC cells are always in positive carriers
	if (carrier < 0)
	   return false;
	for (i = 0; facTable [i]. symbol != -1; i ++) {
	   if (facTable [i]. symbol > symbol)
	      return false;
	   if ((facTable [i]. symbol == symbol) &&
	       (facTable [i]. carrier == carrier))
	      return true;
	}
	return false;
}

bool	drmDecoder::isSDCcell (smodeInfo *m,
	                             int16_t symbol, int16_t carrier) {
	if (carrier == 0)
	   return false;
	if ((m -> Mode == 1) && ((carrier == -1) || (carrier == 1)))
	   return false;

	if (symbol > 2)
	   return false;
	if (isTimeCell (m -> Mode, symbol, carrier))
	   return false;
	if (isFreqCell (m -> Mode, symbol, carrier))
	   return false;
	if (isPilotCell (m -> Mode, symbol, carrier))
	   return false;
	if (isFACcell (m, symbol, carrier))
	   return false;
	return true;
}

bool	drmDecoder::isDatacell (smodeInfo *m,
	                              int16_t symbol,
	                              int16_t carrier, int16_t blockno) {
	if (carrier == 0)
	   return false;
	if (m -> Mode == 1 && (carrier == -1 || carrier == 1))
	   return false;
//
//	these are definitely SDC cells
	if ((blockno == 0) && ((symbol == 0) || (symbol == 1)))
	   return false;
//
	if ((blockno == 0) && ((m -> Mode == 3 ) || (m -> Mode == 4)))
	   if (symbol == 2)
	      return false;
	if (isFreqCell (m -> Mode, symbol, carrier))
	   return false;
	if (isPilotCell (m -> Mode, symbol, carrier))
	   return false;
	if (isTimeCell (m -> Mode, symbol, carrier))
	   return false;
	if (isFACcell (m, symbol, carrier))
	   return false;

	return true;
}

bool	drmDecoder::isFirstFrame (stateDescriptor *f) {
uint8_t val     = f -> frameIdentity;
	return ((val & 03) == 0) || ((val & 03) == 03);
}

bool	drmDecoder::isLastFrame (stateDescriptor *f) {
uint8_t val     = f -> frameIdentity;
	return ((val & 03) == 02);
}

//
//	adding the contents of a frame to a superframe
void	drmDecoder::addtoSuperFrame (smodeInfo *m,
	                                 int16_t blockno, 
	                                 myArray<theSignal> *outbank) {
static	int	teller	= 0;
int16_t	symbol, carrier;
int16_t	   K_min		= Kmin (m -> Mode, m -> Spectrum);
int16_t	   K_max		= Kmax (m -> Mode, m -> Spectrum);

	if (theState. numofStreams <= 0) 
	   return;

	if (isFirstFrame (&theState))
	   my_backendController. newFrame (&theState);

	for (symbol = 0; symbol < symbolsperFrame (m -> Mode); symbol ++) {
	   for (carrier = K_min; carrier <= K_max; carrier ++)
	      if (isDatacell (&modeInf, symbol, carrier, blockno)) {
	         my_backendController. addtoMux (blockno, teller ++,
	                                     outbank -> element (symbol)[carrier - K_min]);
	      }
	}

	if (isLastFrame (&theState)) {
	   my_backendController. endofFrame ();
	   teller = 0;
	}
}

void	drmDecoder::executeTimeSync	(bool f) {
	if (f)
	   timeSyncLabel -> setStyleSheet ("QLabel {background-color:green}");
	else {
	   timeSyncLabel -> setStyleSheet ("QLabel {background-color:red}");
	   faadSyncLabel -> setStyleSheet ("QLabel {background-color:red}");
	}
}

void	drmDecoder::executeFACSync	(bool f) {
	if (f)
	   facSyncLabel -> setStyleSheet ("QLabel {background-color:green}");
	else {
	   facSyncLabel -> setStyleSheet ("QLabel {background-color:red}");
	   faadSyncLabel -> setStyleSheet ("QLabel {background-color:red}");
	}
}

void	drmDecoder::executeSDCSync	(bool f) {
	if (f)
	   sdcSyncLabel -> setStyleSheet ("QLabel {background-color:green}");
	else {
	   sdcSyncLabel -> setStyleSheet ("QLabel {background-color:red}");
	   faadSyncLabel -> setStyleSheet ("QLabel {background-color:red}");
	}
}

void    drmDecoder::execute_showMode            (int l) {
        if (1 <= l && l <= 4)
           modeIndicator        -> setText (QString (char ('A' + (l - 1))));
}

void    drmDecoder::execute_showSpectrum        (int l) {
        if (0 <= l && l < 6)
           spectrumIndicator    -> setText (QString (char ('0' + l)));
}


void    drmDecoder::set_audioModeLabel	(const QString &s) {
        audioModelabel  -> setText (s);
}

void	drmDecoder::show_fineOffset	(float f) {
	show_small_offset -> display (f);
}

void	drmDecoder::show_coarseOffset	(float f) {
	show_int_offset	-> display (f);
}

void	drmDecoder::set_faadSyncLabel	(bool b) {
static  int     faadCounter     = 0;
static  int     goodfaad        = 0;
        faadCounter ++;
        if (b)
           faadSyncLabel -> setStyleSheet ("QLabel {background-color:green}");
        else
           faadSyncLabel -> setStyleSheet ("QLabel {background-color:red}");
        if (b)
           goodfaad ++;
        if (faadCounter > 500) {
           float ratio = goodfaad / 500.0;
           channel_4 -> setText (QString::number (ratio));
           goodfaad     = 0;
           faadCounter  = 0;
        }
}

void	drmDecoder::set_messageLabel	(const QString &s) {
	messageLabel	-> setText (s);
}

void	drmDecoder::set_timeLabel	(const QString &s) {
	timeLabel	-> setText (s);
}

void	drmDecoder::show_fac_mer	(float f) {
	fac_mer	-> display (f);;
}

void	drmDecoder::show_sdc_mer	(float f) {
	sdc_mer -> display (f);
}

void    drmDecoder::show_msc_mer        (float f) {
        msc_mer -> display (f);
}

void	drmDecoder::set_aacDataLabel	(const QString &s) {
	aacDataLabel	-> setText (s);
}

//	The name of the service the listener has chosen goes to the field
//	that the station list uses ("frequency save"); the button of that
//	service is marked.
void	drmDecoder::showChosenService	() {
int c = theState. chosenService;
	if (c < 0 || c > 3)
	   c = 0;
	selectedService	-> setText (serviceLabel [c]);
const QString mark = "QPushButton {font-weight: bold; border: 2px solid #1a8a3a;}";
	channel_1	-> setStyleSheet (c == 0 ? mark : QString (""));
	channel_2	-> setStyleSheet (c == 1 ? mark : QString (""));
}
void	drmDecoder::set_channel_1	(const QString &s) {
	serviceLabel [0]	= s;
	channel_1	-> setText (s);
	showChosenService ();
}

void	drmDecoder::set_channel_2	(const QString &s) {
	serviceLabel [1]	= s;
	channel_2	-> setText (s);
	showChosenService ();
}

void	drmDecoder::set_channel_3	(const QString &s) {
	serviceLabel [2]	= s;
	channel_3	-> setText (s);
}

void	drmDecoder::set_channel_4	(const QString &s) {
	channel_4	-> setText (s);
}

void	drmDecoder::set_datacoding	(const QString &s) {
	datacoding	-> setText (s);
}

void	drmDecoder::audioAvailable	() {
	audioAvailable (0, 48000);
}

void    drmDecoder::show_eqsymbol       (int amount) {
std::complex<float> line [amount];

	eqBuffer. getDataFromBuffer (line, amount);
        if (scopeMode == SHOW_PILOTS)
           my_eqDisplay    -> show_pilots (line, amount);
        else
	if (scopeMode == SHOW_CHANNEL)
           my_eqDisplay    -> show_channel (line, amount);
	else
	   my_eqDisplay	 -> show_pilots (line, amount);
}

void    drmDecoder::showIQ  (int amount) {
std::complex<float> Values [amount];
int16_t i;
int16_t t;
double  avg     = 0;
int     scopeWidth      = scopeSlider -> value();

	if (iqBuffer. GetRingBufferReadAvailable () < amount)
	   return;
        t = iqBuffer. getDataFromBuffer (Values, amount);
	for (i = 0; i < t; i ++) {
	   float x = abs (Values [i]);
	   if (!std::isnan (x) && !std::isinf (x))
	   avg += abs (Values [i]);
	}
	avg     /= t;
	my_iqDisplay -> DisplayIQ (Values, scopeWidth / avg);
}

void    drmDecoder::handle_modeSelector (const QString &m) {
        scopeMode = m == "Pilots" ?  SHOW_PILOTS :
	                  m == "Channel" ? SHOW_CHANNEL : SHOW_ERROR;
}


void	drmDecoder::select_channel_1	() {
	theState. activate_channel_1 ();
	showChosenService ();
}

void	drmDecoder::select_channel_2	() {
	if (theState. activate_channel_2 ()) {
	   showChosenService ();
	   return;
	}
//	not an audio service: the sound simply goes on, so tell why
bool	dataService	= theState. svc [1]. dataStream >= 0 &&
	                 theState. svc [1]. audioStream < 0;
	QToolTip::showText (QCursor::pos (),
	   dataService ?
	      tr ("This service carries data, not audio, so the sound stays on "
	          "the first service. See the Data info button.") :
	      tr ("This service is not an audio service (or not received yet), "
	          "so the sound stays on the first service."),
	   &myFrame, QRect (), 6000);
}

bool	drmDecoder::eventFilter	(QObject *obj, QEvent *ev) {
	if ((obj == &myFrame) && (ev -> type () == QEvent::Close)) {
	   ev -> ignore ();
	   return true;
	}
	return QObject::eventFilter (obj, ev);
}

QString	drmDecoder::getServiceName	() const {
	return selectedService -> text ();
}

void	drmDecoder::handle_reset	() {
	resetDisplay ();
}

//	Without the AAC decoder library the stations with AAC or xHE-AAC sound are
//	silent. The library is not part of the program (its licence is separate);
//	README.txt in the program folder says where to get it and where to put it.
void	drmDecoder::showAacMissing	() {
QMessageBox *m = new QMessageBox (QMessageBox::Warning,
	   tr ("AAC decoder library not found"),
	   tr ("The file libfdk-aac-2.dll was not found next to the program, so "
	       "stations with AAC or xHE-AAC audio will be silent.\n\n"
	       "Data services, Journaline, the slide show and the service "
	       "information still work.\n\n"
	       "Please read the section \"AAC decoder\" in README.txt in the "
	       "program folder: it tells where to get libfdk-aac-2.dll and where "
	       "to put it. Then restart the program."),
	   QMessageBox::Ok, &myFrame);
	m -> setAttribute (Qt::WA_DeleteOnClose);
	m -> setWindowModality (Qt::NonModal);
	m -> show ();
}

void	drmDecoder::handle_serviceInfoButton	() {
	serviceInfoWindow	-> show ();
	serviceInfoWindow	-> raise ();
	serviceInfoWindow	-> activateWindow ();
}

void	drmDecoder::handle_journalineButton	() {
	journalineWindow	-> show ();
	journalineWindow	-> raise ();
	journalineWindow	-> activateWindow ();
}
//
//	The Journaline button is usable as soon as the SDC announces a
//	Journaline service (the pages then arrive slowly, in a loop), or when
//	pages have been received
void	drmDecoder::handle_slideshowButton	() {
	motWindow	-> showTab (0);
	motWindow	-> show ();
	motWindow	-> raise ();
	motWindow	-> activateWindow ();
}

void	drmDecoder::handle_motButton	() {
//	data broadcast packets (e.g. Chinese emergency messages) have a tab of
//	their own; open on it when that is what this station sends
	motWindow	-> showTab ((!theMot. hasFiles () && theDb. status (). packets > 0) ? 2 : 1);
	motWindow	-> show ();
	motWindow	-> raise ();
	motWindow	-> activateWindow ();
}

void	drmDecoder::updateDataButtons	() {
bool	have	= theJournaline. hasContent () ||
	              theDataHub. hasApplication (dxdata::JournalineDecoder::kAppId);
	if (journalineButton -> isEnabled () != have)
	   journalineButton -> setEnabled (have);
//	the button of the service information turns red during an alarm
bool alarm = theSdcInfo. alarmActive ();
	if (alarm != serviceInfoAlarm) {
	   serviceInfoAlarm = alarm;
	   serviceInfoButton -> setStyleSheet (alarm ?
	      "QPushButton {background: #d00000; color: white; "
	      "font-weight: bold;}" : "");
	}
//	SlideShow is application 0x002 (DAB domain); other MOT objects are
//	files. A station that uses MOT directory mode (not supported yet)
//	also enables the files button, so the window can say so
bool	slides	= theMot. hasImages () || theDataHub. hasApplication (0x002) ||
	              theDb. hasImage ();
bool	motFiles	= theMot. hasFiles () || theDataHub. hasApplication (0x003) ||
	              theMot. status (). directoryGroups > 0 ||
	              theDb. hasContent () || theDb. status (). packets > 0;
	if (slideshowButton -> isEnabled () != slides)
	   slideshowButton -> setEnabled (slides);
	if (motButton -> isEnabled () != motFiles)
	   motButton -> setEnabled (motFiles);
}

void	drmDecoder::handle_dataButton	() {
	dataInspector	-> show ();
	dataInspector	-> raise ();
	dataInspector	-> activateWindow ();
}
//
//	Clear every "we are locked on a station" indication on screen
//	and force the OFDM/FAC/SDC search to start over. Called both
//	by the reset button and automatically whenever the user tunes
//	to a new frequency, so a previous station's name/indicators
//	are not left showing while a new (possibly absent) signal is
//	being searched for.
void	drmDecoder::resetDisplay	() {
	theDataHub. reset ();
	theJournaline. clear ();
	theSdcInfo. clear ();
	theMot. clear ();
	theDb. clear ();
	timeSyncLabel	-> setStyleSheet ("QLabel {background-color:red}");
	facSyncLabel	-> setStyleSheet ("QLabel {background-color:red}");
	sdcSyncLabel	-> setStyleSheet ("QLabel {background-color:red}");
	faadSyncLabel	-> setStyleSheet ("QLabel {background-color:red}");

	messageLabel	-> setText ("");
	timeLabel	-> setText ("");
	aacDataLabel	-> setText ("");
	audioModelabel	-> setText ("");
	datacoding	-> setText ("");
	channel_1	-> setText ("");
	channel_2	-> setText ("");
	channel_3	-> setText ("");
	channel_4	-> setText ("");
	for (int i = 0; i < 4; i ++)
	   serviceLabel [i]. clear ();
	theState. chosenService	= 0;	// a new station: back to the first service
	selectedService	-> setText ("");
	channel_1	-> setStyleSheet ("");
	channel_2	-> setStyleSheet ("");
	modeIndicator		-> setText ("");
	spectrumIndicator	-> setText ("");

	fac_mer		-> display (0);
	sdc_mer		-> display (0);
	msc_mer		-> display (0);
	show_small_offset	-> display (0);
	show_int_offset		-> display (0);

	fprintf (stderr, "Going to signal\n");
	my_Reader.signal ();
}

