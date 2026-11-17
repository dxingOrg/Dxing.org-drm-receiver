#
/*
 *    Dxing.org DRM receiver - "Files and data" window (slide show, files, data broadcast)
 *    GNU General Public License, version 2 or later.
 */
#include	"mot-window.h"
#include	"data-hub.h"
#include	<QSignalBlocker>
#include	<QStringList>
#include	<QTabWidget>
#include	<QListWidget>
#include	<QPlainTextEdit>
#include	<QLabel>
#include	<QPushButton>
#include	<QCheckBox>
#include	<QTimer>
#include	<QVBoxLayout>
#include	<QHBoxLayout>
#include	<QImage>
#include	<QImageReader>
#include	<QBuffer>
#include	<QByteArray>
#include	<QPixmap>
#include	<QDir>
#include	<QFile>
#include	<QFileInfo>
#include	<QDateTime>
#include	<QUrl>
#include	<QDesktopServices>
#include	<algorithm>

using namespace dxdata;

static QString	downloadDir () {
QString	dir	= QDir::homePath () + "/Dxing-DRM/Download";
	QDir (). mkpath (dir);
	return dir;
}

static QString	displayName (const MotInfo &i) {
	return i. name. empty () ? QString ("(no name)")
	                         : QString::fromUtf8 (i. name. c_str ());
}

static QString	sizeText (size_t n) {
	if (n >= 1024 * 1024)
	   return QString::number (n / (1024.0 * 1024.0), 'f', 1) + " MB";
	if (n >= 1024)
	   return QString::number (n / 1024.0, 'f', 1) + " kB";
	return QString::number ((qulonglong)n) + " bytes";
}

static QString	triggerText (const MotInfo &i) {
	if (!i. hasTrigger)
	   return QString ();
	if (i. triggerNow)
	   return QObject::tr ("TriggerTime: now");
	return QObject::tr ("TriggerTime: ") +
	       QDateTime::fromSecsSinceEpoch ((qint64)i. triggerUnix). toUTC ().
	                                   toString ("yyyy-MM-dd HH:mm:ss") + " UTC";
}

	MotWindow::MotWindow (MotDecoder *m, DataBroadcastDecoder *d,
	                      DataHub *h, QWidget *parent):
	                                     QWidget (parent, Qt::Window) {
	mot		= m;
	db		= d;
	hub		= h;
	slideIndex	= -1;
	shownSeq	= 0;
	lastChange	= 0;
	current		= new QImage ();
	setWindowTitle (tr ("Slideshow and files"));
	resize (560, 620);

	tabs		= new QTabWidget (this);

//	tab 1: the slides
QWidget	*slidePage	= new QWidget ();
	imageLabel	= new QLabel ();
	imageLabel	-> setAlignment (Qt::AlignCenter);
	imageLabel	-> setWordWrap (true);		// long messages must not be cut
	imageLabel	-> setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Ignored);
	imageLabel	-> setMinimumSize (240, 180);
	imageLabel	-> setStyleSheet ("background-color: #000000; color: #cccccc;");
	imageLabel	-> setText (tr ("No slide received yet"));
	caption		= new QLabel ();
	caption	-> setTextFormat (Qt::PlainText);
	caption	-> setWordWrap (true);
	counter		= new QLabel ();
	prevButton	= new QPushButton (tr ("Previous"));
	nextButton	= new QPushButton (tr ("Next"));
	saveSlideButton	= new QPushButton (tr ("Save slide"));
	newestBox	= new QCheckBox (tr ("Show the newest slide automatically"));
	newestBox	-> setChecked (true);
QHBoxLayout *slideButtons = new QHBoxLayout ();
	slideButtons -> addWidget (prevButton);
	slideButtons -> addWidget (nextButton);
	slideButtons -> addWidget (counter, 1);
	slideButtons -> addWidget (saveSlideButton);
QVBoxLayout *slideCol	= new QVBoxLayout (slidePage);
	slideCol -> addWidget (imageLabel, 1);
	slideCol -> addWidget (caption);
	slideCol -> addLayout (slideButtons);
	slideCol -> addWidget (newestBox);
	tabs -> addTab (slidePage, tr ("Slideshow"));

//	tab 2: other MOT objects
QWidget	*filePage	= new QWidget ();
	fileList	= new QListWidget ();
	fileInfo	= new QLabel ();
	fileInfo	-> setTextFormat (Qt::PlainText);
	fileInfo	-> setWordWrap (true);
	filePreview	= new QPlainTextEdit ();
	filePreview	-> setReadOnly (true);
	filePreview	-> setMaximumHeight (140);
	filePreview	-> setPlaceholderText (tr ("Text files are shown here (as plain text)"));
	saveFileButton	= new QPushButton (tr ("Save selected"));
	saveAllButton	= new QPushButton (tr ("Save all"));
QLabel	*note	= new QLabel (tr ("Received files are never opened or run "
	                  "by the program. Unusual file types get the extra "
	                  "extension .dat when saved."));
	note	-> setWordWrap (true);
QHBoxLayout *fileButtons = new QHBoxLayout ();
	fileButtons -> addWidget (saveFileButton);
	fileButtons -> addWidget (saveAllButton);
	fileButtons -> addStretch (1);
QVBoxLayout *fileCol	= new QVBoxLayout (filePage);
	fileCol -> addWidget (fileList, 1);
	fileCol -> addWidget (fileInfo);
	fileCol -> addWidget (filePreview);
	fileCol -> addLayout (fileButtons);
	fileCol -> addWidget (note);
	tabs -> addTab (filePage, tr ("Files"));

//	tab 3: data broadcast (GY/T 423-2025, 6.7), e.g. Chinese emergency messages
QWidget	*dbPage	= new QWidget ();
	dbProgress	= new QLabel ();
	dbProgress	-> setTextFormat (Qt::PlainText);
	dbProgress	-> setWordWrap (true);
	dbProgress	-> setVisible (false);
	dbList		= new QListWidget ();
	dbList		-> setMaximumHeight (110);
	dbInfo		= new QLabel ();
	dbInfo		-> setTextFormat (Qt::PlainText);
	dbInfo		-> setWordWrap (true);
	dbText		= new QPlainTextEdit ();
	dbText		-> setReadOnly (true);
	dbText		-> setPlaceholderText (tr ("No data broadcast message received yet"));
	dbSaveFileButton = new QPushButton (tr ("Save file (as received)"));
	dbSaveTextButton = new QPushButton (tr ("Save text"));
QLabel	*dbNote	= new QLabel (tr ("Shown as plain text only; nothing received is "
	                  "opened or run. The meaning of the fields marked "
	                  "\"not defined\" is not public."));
	dbNote		-> setWordWrap (true);
QHBoxLayout *dbButtons = new QHBoxLayout ();
	dbButtons -> addWidget (dbSaveFileButton);
	dbButtons -> addWidget (dbSaveTextButton);
	dbButtons -> addStretch (1);
QVBoxLayout *dbCol	= new QVBoxLayout (dbPage);
	dbCol -> addWidget (dbProgress);
	dbCol -> addWidget (dbList);
	dbCol -> addWidget (dbInfo);
	dbCol -> addWidget (dbText, 1);
	dbCol -> addLayout (dbButtons);
	dbCol -> addWidget (dbNote);
	tabs -> addTab (dbPage, tr ("Data broadcast"));

//	below the tabs
	autoSaveBox	= new QCheckBox (tr ("Save new slides and files automatically"));
QPushButton *folderButton = new QPushButton (tr ("Open download folder"));
	status		= new QLabel ();
	saved		= new QLabel ();
	saved	-> setTextFormat (Qt::PlainText);
	saved	-> setWordWrap (true);
QHBoxLayout *low	= new QHBoxLayout ();
	low -> addWidget (autoSaveBox, 1);
	low -> addWidget (folderButton);
QVBoxLayout *col	= new QVBoxLayout (this);
	col -> addWidget (tabs, 1);
	col -> addLayout (low);
	col -> addWidget (status);
	col -> addWidget (saved);

	timer	= new QTimer (this);
	timer	-> setInterval (500);
	connect (timer, &QTimer::timeout, this, &MotWindow::tick);
	connect (prevButton, &QPushButton::clicked, this, &MotWindow::prevSlide);
	connect (nextButton, &QPushButton::clicked, this, &MotWindow::nextSlide);
	connect (saveSlideButton, &QPushButton::clicked,
	         this, &MotWindow::saveSlide);
	connect (saveFileButton, &QPushButton::clicked,
	         this, &MotWindow::saveSelectedFile);
	connect (saveAllButton, &QPushButton::clicked,
	         this, &MotWindow::saveAllFiles);
	connect (folderButton, &QPushButton::clicked, this, &MotWindow::openFolder);
	connect (dbList, &QListWidget::currentItemChanged,
	         this, &MotWindow::dbSelectionChanged);
	connect (dbSaveFileButton, &QPushButton::clicked, this, &MotWindow::saveDbFile);
	connect (dbSaveTextButton, &QPushButton::clicked, this, &MotWindow::saveDbText);
	connect (fileList, &QListWidget::currentItemChanged,
	         this, &MotWindow::fileSelectionChanged);
}

	MotWindow::~MotWindow () {
	if (hub != nullptr)
	   hub -> stopDump ();
	delete current;
}

void	MotWindow::showTab (int tab) {
	tabs -> setCurrentIndex (tab);
}

void	MotWindow::showEvent (QShowEvent *e) {
	QWidget::showEvent (e);
	lastChange	= mot -> changeCounter () + db -> changeCounter () - 1;	// force a first refresh
	tick ();
	timer	-> start ();
}

void	MotWindow::hideEvent (QHideEvent *e) {
	timer	-> stop ();
	QWidget::hideEvent (e);
}

void	MotWindow::resizeEvent (QResizeEvent *e) {
	QWidget::resizeEvent (e);
	scaleImage ();
}

void	MotWindow::tick () {
MotStatus st	= mot -> status ();
QString	s	= tr ("Slides: %1   Files: %2").arg (st. images). arg (st. files);
	if (st. partial > 0)
	   s	+= tr ("   Receiving: %1").arg (st. partial);
	DbStatus ds	= db -> status ();
	if (ds. packets > 0)
	   s	+= tr ("   Data broadcast: %1 received").arg (ds. resources);
	if (st. directoryObjects > 0)
	   s	+= tr ("   Directory lists %1 objects"). arg (st. directoryObjects);
	else
	if (st. directoryCompressed > 0)
	   s	+= tr ("   (compressed MOT directory: not supported)");
	status -> setText (s);
	status -> setToolTip (tr ("Duplicates skipped: %1, damaged or incomplete "
	                          "segments: %2, wrong size: %3, too big: %4")
	                      . arg (st. duplicates). arg (st. badSegments)
	                      . arg (st. sizeMismatch). arg (st. tooBig));
uint32_t cc	= mot -> changeCounter () + db -> changeCounter ();
	if (cc != lastChange) {
	   lastChange	= cc;
	   refresh ();
	}
}

void	MotWindow::refresh () {
std::vector<MotInfo> all	= mot -> list ();
	slides. clear ();
	files. clear ();
	for (const MotInfo &i : all)
	   if (i. kind != MotObject::Other) {
SlideRef r;
	      r. mi	= i;
	      slides. push_back (r);
	   }
	   else
	      files. push_back (i);
//	pictures received as data broadcast files (after the slide show ones)
	for (const DbInfo &d : db -> list ())
	   if (d. kind == DbResource::Jpeg || d. kind == DbResource::Png ||
	       d. kind == DbResource::Gif) {
SlideRef r;
	      r. fromDb	= true;
	      r. di	= d;
	      slides. push_back (r);
	   }

//	automatic saving: everything new, once
	if (autoSaveBox -> isChecked ())
	   for (const MotInfo &i : all)
	      if (autoSaved. find (i. seq) == autoSaved. end ()) {
	         autoSaved. insert (i. seq);
	         saveObject (i. seq);
	      }

//	the slide on screen: the newest, or the one the user is looking at
	int	idx	= -1;
	if (newestBox -> isChecked () && !slides. empty ()) {
	   if (slides. back (). key () != shownSeq)
	      idx	= (int)slides. size () - 1;
	}
	if (idx < 0)
	   for (size_t k = 0; k < slides. size (); k ++)
	      if (slides [k]. key () == shownSeq)
	         idx	= (int)k;
	if (idx < 0 && !slides. empty ())
	   idx	= (int)slides. size () - 1;
bool	changed	= (idx >= 0) &&
	                  (slides [(size_t)idx]. key () != shownSeq || slideIndex != idx);
	slideIndex	= idx;
	if (slideIndex < 0) {
	   shownSeq	= 0;
	   *current	= QImage ();
	   imageLabel -> setPixmap (QPixmap ());
	   imageLabel -> setText (tr ("No slide received yet"));
	   caption -> setText (QString ());
	   counter -> setText (QString ());
	}
	else
	if (changed)
	   showSlide ();
	else
	   counter -> setText (tr ("%1 / %2"). arg (slideIndex + 1). arg (slides. size ()));
	prevButton -> setEnabled (slideIndex > 0);
	nextButton -> setEnabled (slideIndex >= 0 &&
	                          slideIndex + 1 < (int)slides. size ());
	saveSlideButton -> setEnabled (slideIndex >= 0);

//	the file list, keeping the selection
uint32_t selected	= 0;
	if (fileList -> currentItem () != nullptr)
	   selected	= fileList -> currentItem () -> data (Qt::UserRole). toUInt ();
	fileList -> blockSignals (true);
	fileList -> clear ();
	int	row	= -1;
	for (size_t k = 0; k < files. size (); k ++) {
	   const MotInfo &i	= files [k];
	   QListWidgetItem *it = new QListWidgetItem (
	      displayName (i) + "   (" + sizeText (i. size) + ")");
	   it -> setData (Qt::UserRole, (uint)i. seq);
	   fileList -> addItem (it);
	   if (i. seq == selected)
	      row	= (int)k;
	}
	if (row < 0 && !files. empty ())
	   row	= (int)files. size () - 1;
	if (row >= 0)
	   fileList -> setCurrentRow (row);
	fileList -> blockSignals (false);
	fileSelectionChanged ();
	saveFileButton -> setEnabled (!files. empty ());
	saveAllButton -> setEnabled (!files. empty ());
	refreshDb ();
}

void	MotWindow::showSlide () {
	if (slideIndex < 0 || slideIndex >= (int)slides. size ())
	   return;
const SlideRef &sl	= slides [(size_t)slideIndex];
QByteArray data;
QString	cap;
	shownSeq	= sl. key ();
	if (sl. fromDb) {
DbResource r;
	   if (!db -> getResource (sl. di. seq, r))
	      return;
	   data	= QByteArray ((const char *)r. file. data (), (int)r. file. size ());
	   cap	= QString::fromStdString (r. suggestedFileName ()) + "   " +
	          sizeText (r. file. size ());
	   if (r. hasDescription) {
	      if (!r. desc. title. empty ())
	         cap += "\n" + QString::fromUtf8 (r. desc. title. c_str ());
	      cap += "\n" + tr ("Data broadcast file. Type in the description: %1 "
	               "(the picture format is recognised from the content)").
	             arg (QString::fromUtf8 (r. desc. fileType. c_str ()));
	   }
	   else
	      cap += "\n" + tr ("Data broadcast file (its description has not "
	                       "been received yet)");
	   if (r. lengthChecked && !r. lengthOk)
	      cap += "\n" + tr ("LENGTH DOES NOT MATCH the description!");
	   if (r. md5Checked)
	      cap += "\n" + (r. md5Ok ? tr ("The file name is the MD5 sum of "
	                                    "the file: OK")
	                              : tr ("The file name looks like an MD5 sum "
	                                    "but does NOT match!"));
	}
	else {
const MotInfo &i	= sl. mi;
MotObject o;
	   if (!mot -> getObject (i. seq, o))
	      return;
	   data	= QByteArray ((const char *)o. body. data (), (int)o. body. size ());
	   cap	= displayName (i) + "   " + sizeText (i. size);
	   if (!i. categoryTitle. empty ())
	      cap	+= "\n" + tr ("Category: ") +
	               QString::fromUtf8 (i. categoryTitle. c_str ()) +
	               tr (" (slide %1)"). arg (i. slideId);
QString	tt	= triggerText (i);
	   if (!tt. isEmpty ())
	      cap	+= "\n" + tt;
	   if (!i. clickUrl. empty ())
	      cap	+= "\n" + tr ("Link (not opened): ") +
	              QString::fromUtf8 (i. clickUrl. c_str ());
	}
QBuffer	buf (&data);
	buf. open (QIODevice::ReadOnly);
QImageReader reader (&buf);
QSize	sz	= reader. size ();
//	the size is checked before decoding: a small file can describe a huge image
bool	ok	= sz. isValid () && sz. width () <= 4096 && sz. height () <= 4096 &&
	            (qint64)sz. width () * sz. height () <= 8000000;
	if (ok) {
	   *current	= reader. read ();
	   ok	= !current -> isNull ();
	}
	if (!ok) {
	   *current	= QImage ();
	   imageLabel -> setPixmap (QPixmap ());
	   if (reader. error () == QImageReader::UnsupportedFormatError)
	      imageLabel -> setText (tr ("This slide cannot be shown: JPEG support "
	                    "is missing. The folder imageformats with qjpeg.dll "
	                    "(and the libjpeg DLL next to the program) must be "
	                    "beside the program. The slide can still be saved.")
	                    + "\n(" + reader. errorString () + ")");
	   else
	      imageLabel -> setText (tr ("This slide cannot be shown "
	                                 "(unsupported or damaged image)"));
	}
	else
	   scaleImage ();
	caption -> setText (cap);
	counter -> setText (tr ("%1 / %2"). arg (slideIndex + 1). arg (slides. size ()));
}

void	MotWindow::scaleImage () {
	if (current -> isNull ())
	   return;
QPixmap	pm	= QPixmap::fromImage (*current);
QSize	avail	= imageLabel -> size () - QSize (4, 4);
	if (avail. width () < 16 || avail. height () < 16)
	   return;
//	keep the aspect ratio; small slides are enlarged only by whole steps
	pm	= pm. scaled (avail, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	imageLabel -> setPixmap (pm);
}

void	MotWindow::prevSlide () {
	if (slideIndex > 0) {
	   slideIndex --;
	   newestBox -> setChecked (false);
	   showSlide ();
	   prevButton -> setEnabled (slideIndex > 0);
	   nextButton -> setEnabled (slideIndex + 1 < (int)slides. size ());
	}
}

void	MotWindow::nextSlide () {
	if (slideIndex >= 0 && slideIndex + 1 < (int)slides. size ()) {
	   slideIndex ++;
	   showSlide ();
	   prevButton -> setEnabled (slideIndex > 0);
	   nextButton -> setEnabled (slideIndex + 1 < (int)slides. size ());
	}
}

//	writes one received object into the download folder; never
//	overwrites a file, never opens it
bool	MotWindow::saveObject (uint32_t seq) {
MotObject o;
	if (!mot -> getObject (seq, o))
	   return false;
QString	dir	= downloadDir ();
QString	name	= QString::fromStdString (o. suggestedFileName ());
QFileInfo fi (name);
QString	base	= fi. completeBaseName ();
QString	suffix	= fi. suffix ();
QString	path	= dir + "/" + name;
	for (int n = 2; QFile::exists (path) && n < 1000; n ++)
	   path	= dir + "/" + base + "_" + QString::number (n) + "." + suffix;
	if (QFile::exists (path)) {
	   saved -> setText (tr ("Not saved: too many files with the name ") + name);
	   return false;
	}
QFile	f (path);
	if (!f. open (QIODevice::WriteOnly)) {
	   saved -> setText (tr ("Cannot write ") + QDir::toNativeSeparators (path));
	   return false;
	}
qint64	w	= f. write ((const char *)o. body. data (), (qint64)o. body. size ());
	f. close ();
	if (w != (qint64)o. body. size ()) {
	   saved -> setText (tr ("Writing failed: ") + QDir::toNativeSeparators (path));
	   return false;
	}
	saved -> setText (tr ("Saved: ") + QDir::toNativeSeparators (path));
	return true;
}

void	MotWindow::saveSlide () {
	if (slideIndex < 0 || slideIndex >= (int)slides. size ())
	   return;
const SlideRef &sl	= slides [(size_t)slideIndex];
	if (!sl. fromDb) {
	   saveObject (sl. mi. seq);
	   return;
	}
DbResource r;
	if (db -> getResource (sl. di. seq, r))
	   saveData (QString::fromStdString (r. suggestedFileName ()),
	             QByteArray ((const char *)r. file. data (), (int)r. file. size ()));
}

void	MotWindow::fileSelectionChanged () {
QListWidgetItem *it	= fileList -> currentItem ();
	filePreview -> clear ();
	if (it == nullptr) {
	   fileInfo -> setText (QString ());
	   return;
	}
uint32_t seq	= it -> data (Qt::UserRole). toUInt ();
	for (const MotInfo &i : files)
	   if (i. seq == seq) {
	      QString t	= displayName (i) + "   " + sizeText (i. size) + "\n" +
	                   tr ("Received ") + QDateTime::fromSecsSinceEpoch (
	                          (qint64)i. received). toString ("HH:mm:ss");
	      if (i. compressed)
	         t	+= "\n" + tr ("Compressed on the air (saved as received).");
	      fileInfo -> setText (t);
//	a text object is shown as plain text, never interpreted (an HTML
//	page is shown as its source). UTF-8 if it is valid, else Latin-1
	      MotObject o;
	      if (mot -> getObject (seq, o) &&
	          (o. hdr. contentType == 1 ||
	           (o. hdr. p. hasMime && o. hdr. p. mime. compare (0, 5, "text/") == 0))) {
	         QByteArray ba ((const char *)o. body. data (),
	                        (int)std::min<size_t> (o. body. size (), 8192));
	         QString u	= QString::fromUtf8 (ba);
	         if (u. contains (QChar (0xFFFD)))
	            u	= QString::fromLatin1 (ba);
	         filePreview -> setPlainText (u);
	      }
	      return;
	   }
}

void	MotWindow::saveSelectedFile () {
QListWidgetItem *it	= fileList -> currentItem ();
	if (it != nullptr)
	   saveObject (it -> data (Qt::UserRole). toUInt ());
}

void	MotWindow::saveAllFiles () {
int	n	= 0;
	for (const MotInfo &i : files)
	   if (saveObject (i. seq))
	      n ++;
	saved -> setText (tr ("Saved %1 file(s) in ").arg (n) +
	                  QDir::toNativeSeparators (downloadDir ()));
}

void	MotWindow::openFolder () {
	QDesktopServices::openUrl (QUrl::fromLocalFile (downloadDir ()));
}

/*
 *	data broadcast (GY/T 423-2025, 6.7)
 */
static QString	kindName (DbResource::Kind k) {
	switch (k) {
	   case DbResource::Emergency:	return QObject::tr ("emergency message");
	   case DbResource::Html:	return QObject::tr ("HTML");
	   case DbResource::Text:	return QObject::tr ("text");
	   case DbResource::Jpeg:	return QObject::tr ("JPEG");
	   case DbResource::Png:	return QObject::tr ("PNG");
	   case DbResource::Gif:	return QObject::tr ("GIF");
	   default:		return QObject::tr ("file");
	}
}

static QString	serviceName (int t) {
	if (t == 1)
	   return QObject::tr ("1 (emergency broadcast)");
	if (t == 2)
	   return QObject::tr ("2 (data broadcast)");
	return QString::number (t);
}

//	writes bytes into the download folder; never overwrites, never opens
bool	MotWindow::saveData (const QString &name, const QByteArray &data) {
QString	dir	= downloadDir ();
QFileInfo fi (name);
QString	base	= fi. completeBaseName ();
QString	suffix	= fi. suffix ();
QString	path	= dir + "/" + name;
	for (int n = 2; QFile::exists (path) && n < 1000; n ++)
	   path	= dir + "/" + base + "_" + QString::number (n) + "." + suffix;
	if (QFile::exists (path)) {
	   saved -> setText (tr ("Not saved: too many files with the name ") + name);
	   return false;
	}
QFile	f (path);
	if (!f. open (QIODevice::WriteOnly)) {
	   saved -> setText (tr ("Cannot write ") + QDir::toNativeSeparators (path));
	   return false;
	}
qint64	w	= f. write (data);
	f. close ();
	if (w != (qint64)data. size ()) {
	   saved -> setText (tr ("Writing failed: ") + QDir::toNativeSeparators (path));
	   return false;
	}
	saved -> setText (tr ("Saved: ") + QDir::toNativeSeparators (path));
	return true;
}

void	MotWindow::refreshDb () {
//	files that are on their way (a big file needs several repetitions of the
//	carousel, the missing pieces are filled in from the next ones)
	{  QStringList lines;
	   for (const DbProgress &d : db -> progress ()) {
QString	l;
	      if (d. packetType == 1) {
	         l = tr ("File, resource %1, update %2").arg (d. resourceId).
	                                                  arg (d. updateNo);
	         if (d. hasDescription)
	            l += tr (": %1, %2 bytes").
	                 arg (QString::fromStdString (d. name)).
	                 arg (QString::fromStdString (d. fileLength));
	         if (d. count > 0)
	            l += tr (". Received %1 of %2 packets (%3 bytes)").
	                 arg (d. have). arg (d. count). arg ((qulonglong)d. bytes);
	         else
	            l += tr (". Nothing of the file received yet");
	      }
	      else
	         l = tr ("Resource %1, update %2: %3 of %4 packets").
	             arg (d. resourceId). arg (d. updateNo).
	             arg (d. have). arg (d. count);
	      lines << l;
	   }
	   if (lines. isEmpty ())
	      dbProgress -> setVisible (false);
	   else {
	      dbProgress -> setText (tr ("Being received:\n") + lines. join ("\n") +
	         tr ("\nThe station repeats its files in a loop; with a weak "
	             "signal it takes several repetitions to get all pieces."));
	      dbProgress -> setVisible (true);
	   }
	}
	dbItems	= db -> list ();
	if (autoSaveBox -> isChecked ())
	   for (const DbInfo &i : dbItems)
	      if (dbAutoSaved. find (i. seq) == dbAutoSaved. end ()) {
	         dbAutoSaved. insert (i. seq);
	         DbResource r;
	         if (db -> getResource (i. seq, r))
	            saveData (QString::fromStdString (r. suggestedFileName ()),
	                      QByteArray ((const char *)r. file. data (),
	                                  (int)r. file. size ()));
	      }
uint32_t selected	= 0;
	if (dbList -> currentItem () != nullptr)
	   selected	= dbList -> currentItem () -> data (Qt::UserRole). toUInt ();
	dbList -> blockSignals (true);
	dbList -> clear ();
int	row	= -1;
	for (size_t k = 0; k < dbItems. size (); k ++) {
	   const DbInfo &i	= dbItems [k];
QString label	= kindName (i. kind) + tr ("   resource %1, update %2, %3")
	                      . arg (i. resourceId). arg (i. updateNo). arg (sizeText (i. size));
	   QListWidgetItem *it = new QListWidgetItem (label);
	   it -> setData (Qt::UserRole, (uint)i. seq);
	   dbList -> addItem (it);
	   if (i. seq == selected)
	      row	= (int)k;
	}
	if (row < 0 && !dbItems. empty ())
	   row	= (int)dbItems. size () - 1;
	if (row >= 0)
	   dbList -> setCurrentRow (row);
	dbList -> blockSignals (false);
	dbSelectionChanged ();
	dbSaveFileButton -> setEnabled (!dbItems. empty ());
	dbSaveTextButton -> setEnabled (!dbItems. empty ());
}

void	MotWindow::dbSelectionChanged () {
	dbText -> clear ();
QListWidgetItem *it	= dbList -> currentItem ();
	if (it == nullptr) {
	   dbInfo -> setText (QString ());
	   return;
	}
DbResource r;
	if (!db -> getResource (it -> data (Qt::UserRole). toUInt (), r))
	   return;
QString	t	= tr ("Service type: %1\n").arg (serviceName (r. serviceType)) +
		  tr ("Resource %1, update %2, %3, received %4\n")
		  . arg (r. resourceId). arg (r. updateNo). arg (sizeText (r. file. size ()))
		  . arg (QDateTime::fromSecsSinceEpoch ((qint64)r. received). toString ("HH:mm:ss"));
	if (r. hasDescription) {
	   t	+= tr ("File name: %1\n").arg (QString::fromUtf8 (r. desc. name. c_str ()));
	   if (!r. desc. title. empty ())
	      t	+= tr ("Title: %1\n").arg (QString::fromUtf8 (r. desc. title. c_str ()));
	   t	+= tr ("Declared file type: %1\n").arg (QString::fromUtf8 (r. desc. fileType. c_str ()));
	}
	else
	   t	+= tr ("(the description file has not been received yet)\n");
	if (r. lengthChecked)
	   t	+= r. lengthOk ? tr ("Length matches the description. ")
	                    : tr ("LENGTH DOES NOT MATCH the description! ");
	if (r. md5Checked)
	   t	+= r. md5Ok ? tr ("The file name is the MD5 sum of the file: OK.")
	                  : tr ("The file name looks like an MD5 sum but does NOT match!");
	if (r. kind == DbResource::Emergency) {
	   t	+= "\n" + tr ("15-byte header (not defined): %1")
	           . arg (QString::fromStdString (r. subHeaderHex));
	   if (r. hasTime)
	      t	+= "\n" + tr ("Time field in it (assumed UTC): %1")
	              . arg (QDateTime::fromSecsSinceEpoch ((qint64)r. timeUnix). toUTC ().
	                                   toString ("yyyy-MM-dd HH:mm:ss"));
	}
	dbInfo -> setText (t);
	if (!r. text. empty ())
	   dbText -> setPlainText (QString::fromUtf8 (r. text. c_str (), (int)r. text. size ()));
	else
	   dbText -> setPlainText (tr ("(nothing to show as text: %1 bytes of %2)")
	                           . arg (r. file. size ()). arg (kindName (r. kind)));
}

void	MotWindow::saveDbFile () {
QListWidgetItem *it	= dbList -> currentItem ();
	if (it == nullptr)
	   return;
DbResource r;
	if (db -> getResource (it -> data (Qt::UserRole). toUInt (), r))
	   saveData (QString::fromStdString (r. suggestedFileName ()),
	             QByteArray ((const char *)r. file. data (), (int)r. file. size ()));
}

void	MotWindow::saveDbText () {
QListWidgetItem *it	= dbList -> currentItem ();
	if (it == nullptr)
	   return;
DbResource r;
	if (!db -> getResource (it -> data (Qt::UserRole). toUInt (), r))
	   return;
QString	base	= QFileInfo (QString::fromStdString (r. suggestedFileName ())).
	                                                         completeBaseName ();
//	UTF-8 with a byte order mark, so that Notepad shows Chinese text right
	saveData (base + ".txt", QByteArray ("\xEF\xBB\xBF") +
	          QByteArray (r. text. data (), (int)r. text. size ()));
}
