#
/*
 *    Dxing.org DRM receiver - data services inspector window
 *    GNU General Public License, version 2 or later.
 */
#include	"data-inspector.h"
#include	"data-hub.h"
#include	<QPlainTextEdit>
#include	<QCheckBox>
#include	<QSignalBlocker>
#include	<QPushButton>
#include	<QLabel>
#include	<QTimer>
#include	<QVBoxLayout>
#include	<QHBoxLayout>
#include	<QFont>
#include	<QFile>
#include	<QDir>
#include	<QDateTime>
#include	<QTextStream>
#include	<QStringConverter>

	DataInspector::DataInspector (dxdata::DataHub *hub, QWidget *parent):
	                                   QWidget (parent, Qt::Window) {
	this	-> hub	= hub;
	setWindowTitle (tr ("Data services"));
	resize (640, 420);

	text	= new QPlainTextEdit (this);
	text	-> setReadOnly (true);
	text	-> setLineWrapMode (QPlainTextEdit::NoWrap);
QFont	f ("Consolas");
	f. setStyleHint (QFont::Monospace);
	text	-> setFont (f);

QPushButton *saveButton	= new QPushButton (tr ("Save log"), this);
QPushButton *clearButton = new QPushButton (tr ("Clear"), this);
	saveButton	-> setToolTip (tr ("Write this report to Dxing-DRM/data-log.txt in your user folder"));
	clearButton	-> setToolTip (tr ("Restart the statistics"));
	status	= new QLabel (this);

	recordBox	= new QCheckBox (tr ("Record raw data for analysis"), this);
	recordBox	-> setToolTip (tr ("Writes the received data units and the first "
	                             "frames of every data stream to "
	                             "Dxing-DRM/data-groups.txt in your user folder"));
	recordInfo	= new QLabel (this);
QHBoxLayout *row = new QHBoxLayout ();
	row	-> addWidget (saveButton);
	row	-> addWidget (clearButton);
	row	-> addWidget (status, 1);
QHBoxLayout *rec = new QHBoxLayout ();
	rec	-> addWidget (recordBox);
	rec	-> addWidget (recordInfo, 1);
QVBoxLayout *col = new QVBoxLayout (this);
	col	-> addWidget (text, 1);
	col	-> addLayout (row);
	col	-> addLayout (rec);

	timer	= new QTimer (this);
	timer	-> setInterval (1000);
	connect (timer, SIGNAL (timeout ()), this, SLOT (refresh ()));
	connect (saveButton, SIGNAL (clicked ()), this, SLOT (saveLog ()));
	connect (clearButton, SIGNAL (clicked ()), this, SLOT (clearStats ()));
	connect (recordBox, &QCheckBox::toggled, this, &DataInspector::recordToggled);
}

	DataInspector::~DataInspector () {
}

void	DataInspector::recordToggled (bool on) {
	if (hub == nullptr)
	   return;
	if (!on) {
	   hub	-> stopDump ();
	   recordInfo -> setText (QString ());
	   return;
	}
QString	dir	= QDir::homePath () + "/Dxing-DRM";
	QDir (). mkpath (dir);
QString	file	= dir + "/data-groups.txt";
QByteArray name	= QFile::encodeName (file);
	if (hub -> startDump (std::string (name. constData (),
	                                   (size_t)name. size ())))
	   recordInfo -> setText (tr ("Recording to ") +
	                          QDir::toNativeSeparators (file));
	else {
	   recordInfo -> setText (tr ("Cannot write ") +
	                          QDir::toNativeSeparators (file));
	   QSignalBlocker blocker (recordBox);
	   recordBox -> setChecked (false);
	}
}

//	refresh only while the window is visible
void	DataInspector::showEvent (QShowEvent *e) {
	QWidget::showEvent (e);
	refresh ();
	timer	-> start ();
}

void	DataInspector::hideEvent (QHideEvent *e) {
	timer	-> stop ();
	QWidget::hideEvent (e);
}

void	DataInspector::refresh () {
	if (hub == nullptr)
	   return;
QString	s	= QString::fromStdString (hub -> report ());
	if (s != text -> toPlainText ())
	   text	-> setPlainText (s);
}

void	DataInspector::clearStats () {
	if (hub != nullptr)
	   hub	-> reset ();
	refresh ();
}

void	DataInspector::saveLog () {
	if (hub == nullptr)
	   return;
QString	dir	= QDir::homePath () + "/Dxing-DRM";
	QDir (). mkpath (dir);
QFile	file (dir + "/data-log.txt");
	if (!file. open (QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
	   status -> setText (tr ("Cannot write ") +
	                      QDir::toNativeSeparators (file. fileName ()));
	   return;
	}
QTextStream out (&file);
	out. setEncoding (QStringConverter::Utf8);
	out << "==== " << QDateTime::currentDateTimeUtc (). toString (Qt::ISODate)
	    << " UTC ====\n"
	    << QString::fromStdString (hub -> report ()) << "\n";
	status -> setText (tr ("Saved to ") +
	                   QDir::toNativeSeparators (file. fileName ()));
}
