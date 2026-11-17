#
/*
 *    Dxing.org DRM receiver - "Service info" window (see the header)
 *    GNU General Public License, version 2 or later.
 */
#include	"service-info-window.h"
#include	<QTabWidget>
#include	<QTableWidget>
#include	<QHeaderView>
#include	<QLabel>
#include	<QTimer>
#include	<QVBoxLayout>
#include	<QString>

using namespace dxdata;

static QString u8 (const std::string &s) {
	return QString::fromStdString (s);
}

static void setCell (QTableWidget *t, int row, int col, const QString &text) {
QTableWidgetItem *it = new QTableWidgetItem (text);
	it -> setFlags (it -> flags () & ~Qt::ItemIsEditable);
	t -> setItem (row, col, it);
}

static QTableWidget *makeTable (const QStringList &heads, QWidget *parent) {
QTableWidget *t = new QTableWidget (parent);
	t -> setColumnCount (heads. size ());
	t -> setHorizontalHeaderLabels (heads);
	t -> setSelectionBehavior (QAbstractItemView::SelectRows);
	t -> setSelectionMode (QAbstractItemView::SingleSelection);
	t -> setEditTriggers (QAbstractItemView::NoEditTriggers);
	t -> verticalHeader () -> setVisible (false);
	t -> horizontalHeader () -> setStretchLastSection (true);
	return t;
}

	ServiceInfoWindow::ServiceInfoWindow (SdcInfo *info, QWidget *parent):
	                                            QWidget (parent, Qt::Window) {
	this	-> info	= info;
	lastCounter	= 0;
	first		= true;
	setWindowTitle ("Service info");
	resize (760, 420);
QVBoxLayout *top = new QVBoxLayout (this);
	tabs	= new QTabWidget (this);
	top -> addWidget (tabs);

//	tab 1: the services
QWidget *p1 = new QWidget ();
QVBoxLayout *l1 = new QVBoxLayout (p1);
	timeLabel	= new QLabel ("Time and date: not received yet", p1);
	timeLabel -> setTextInteractionFlags (Qt::TextSelectableByMouse);
	l1 -> addWidget (timeLabel);
	stationTable = makeTable (QStringList () << "Short id" << "Service id"
	   << "Type" << "Label" << "Language (FAC)" << "Programme type"
	   << "Language / country (SDC)", p1);
	l1 -> addWidget (stationTable);
	tabs -> addTab (p1, "Station");

//	tab 2: alternative frequencies
QWidget *p2 = new QWidget ();
QVBoxLayout *l2 = new QVBoxLayout (p2);
QLabel *n2 = new QLabel (
	   "Double click a DRM frequency to tune to it. FM frequencies are "
	   "computed as 87.1 MHz + 0.1 MHz x code (GY/T 423); the code is shown "
	   "too.", p2);
	n2 -> setWordWrap (true);
	l2 -> addWidget (n2);
	afsTable = makeTable (QStringList () << "System" << "Frequency"
	   << "Service id" << "Applies to" << "Region" << "Schedule" << "Note", p2);
	l2 -> addWidget (afsTable);
	tabs -> addTab (p2, "Alternative frequencies");
	connect (afsTable, SIGNAL (itemDoubleClicked (QTableWidgetItem *)),
	         this, SLOT (afsDoubleClicked (QTableWidgetItem *)));

//	tab 3: announcements
QWidget *p3 = new QWidget ();
QVBoxLayout *l3 = new QVBoxLayout (p3);
	alarmBanner	= new QLabel (p3);
	alarmBanner -> setStyleSheet (
	   "QLabel {background: #d00000; color: white; font-weight: bold; "
	   "padding: 6px;}");
	alarmBanner -> setVisible (false);
	l3 -> addWidget (alarmBanner);
	announceNote = new QLabel (
	   "The meaning of the 10 announcement bits is an assumption (bit 0 "
	   "travel, 1 news, 2 weather, 3 warning/alarm); the raw values are "
	   "shown too. A programme type 30 in the FAC is a reliable alarm "
	   "signal.", p3);
	announceNote -> setWordWrap (true);
	l3 -> addWidget (announceNote);
	announceTable = makeTable (QStringList () << "Services"
	   << "Same multiplex" << "Id" << "Supported" << "Switched on (active)"
	   << "Raw support" << "Raw switching", p3);
	l3 -> addWidget (announceTable);
	tabs -> addTab (p3, "Announcements");

	timer	= new QTimer (this);
	timer -> setInterval (1000);
	connect (timer, SIGNAL (timeout ()), this, SLOT (tick ()));
}

	ServiceInfoWindow::~ServiceInfoWindow () {
}

void	ServiceInfoWindow::showEvent (QShowEvent *e) {
	QWidget::showEvent (e);
	first	= true;
	tick ();
	timer -> start ();
}

void	ServiceInfoWindow::hideEvent (QHideEvent *e) {
	timer -> stop ();
	QWidget::hideEvent (e);
}

void	ServiceInfoWindow::tick () {
uint32_t c = info -> changeCounter ();
	if (!first && c == lastCounter)
	   return;
	first		= false;
	lastCounter	= c;
	rebuild (info -> snapshot ());
}

//	names of the services for a 4-bit service flag field (MSB = short id 3)
QString	ServiceInfoWindow::serviceList (const InfoSnapshot &s, int flags) {
QStringList l;
	for (int i = 0; i < 4; i ++)
	   if (flags & (0x8 >> (3 - i))) {
	      QString n = QString ("%1").arg (i);
	      if (s. service [i]. haveLabel)
	         n += " (" + u8 (s. service [i]. label) + ")";
	      l << n;
	   }
	return l. isEmpty () ? QString ("-") : "short id " + l. join (", ");
}

QString	ServiceInfoWindow::regionText (const InfoSnapshot &s, int id) {
	for (auto &r : s. regions)
	   if (r. id == id) {
QString t = QString ("%1: lat %2..%3, lon %4..%5").arg (id).
	         arg (r. lat). arg (r. lat + r. latExt).
	         arg (r. lon). arg (r. lon + r. lonExt);
	      if (!r. ciraf. empty ()) {
	         QStringList z;
	         for (int c : r. ciraf)
	            z << QString::number (c);
	         t += ", CIRAF " + z. join (",");
	      }
	      return t;
	   }
	return QString ("%1 (area not described)").arg (id);
}

QString	ServiceInfoWindow::scheduleOf (const InfoSnapshot &s, int id) {
QStringList l;
	for (auto &x : s. schedules)
	   if (x. id == id)
	      l << u8 (scheduleText (x));
	return l. isEmpty () ? QString ("%1 (not described)").arg (id) :
	                       QString ("%1: ").arg (id) + l. join ("; ");
}

void	ServiceInfoWindow::rebuild (const InfoSnapshot &s) {
//	time
	if (s. time. valid) {
QString t = "UTC: " + u8 (formatDateTime (s. time. mjd, s. time. hour,
	                                           s. time. minute, 0));
	   if (s. time. hasOffset) {
int half = s. time. offsetHalfHours;
	      t += "      Local: " + u8 (formatDateTime (s. time. mjd,
	                  s. time. hour, s. time. minute, half * 30)) +
	           QString ("  (UTC%1%2 h)").arg (half < 0 ? "-" : "+").
	                  arg (qAbs (half) / 2.0);
	   } else
	      t += "      (no local time offset sent)";
	   timeLabel -> setText (t);
	} else
	   timeLabel -> setText ("Time and date: not received yet");

//	services
int rows = 0;
	stationTable -> setRowCount (4);
	for (int i = 0; i < 4; i ++) {
const SvcEntry &e = s. service [i];
	   if (!e. haveFac && !e. haveLabel && !e. haveLang)
	      continue;
	   setCell (stationTable, rows, 0, QString::number (i));
	   setCell (stationTable, rows, 1, e. haveFac ?
	      QString ("0x") + QString::number (e. serviceId, 16).
	                       rightJustified (6, QChar ('0')). toUpper () :
	      QString ("-"));
	   setCell (stationTable, rows, 2, !e. haveFac ? QString ("-") :
	      e. isAudio ? QString ("audio") : QString ("data"));
	   setCell (stationTable, rows, 3, e. haveLabel ? u8 (e. label) : QString ("-"));
	   setCell (stationTable, rows, 4, e. haveFac ?
	      u8 (languageName (e. language)) : QString ("-"));
	   setCell (stationTable, rows, 5, e. haveFac ?
	      u8 (programmeTypeName (e. programmeType)) : QString ("-"));
	   setCell (stationTable, rows, 6, e. haveLang ?
	      u8 (e. lang3) + " / " + u8 (e. country2) : QString ("-"));
	   rows ++;
	}
	stationTable -> setRowCount (rows);
	stationTable -> resizeColumnsToContents ();

//	alternative frequencies
int r = 0;
	afsTable -> setRowCount (0);
	for (auto &m : s. afs) {
	   for (int f : m. freqsKHz) {
	      afsTable -> insertRow (r);
	      setCell (afsTable, r, 0, "DRM (this multiplex)");
	      QTableWidgetItem *fi = new QTableWidgetItem (QString ("%1 kHz").arg (f));
	      fi -> setData (Qt::UserRole, f);
	      fi -> setFlags (fi -> flags () & ~Qt::ItemIsEditable);
	      afsTable -> setItem (r, 1, fi);
	      setCell (afsTable, r, 2, "-");
	      setCell (afsTable, r, 3, m. restricted ?
	         serviceList (s, m. serviceFlags) : QString ("all services"));
	      setCell (afsTable, r, 4, m. hasRegSched ?
	         regionText (s, m. regionId) : QString ("-"));
	      setCell (afsTable, r, 5, m. hasRegSched ?
	         scheduleOf (s, m. scheduleId) : QString ("-"));
	      setCell (afsTable, r, 6, QString ("%1, %2").
	         arg (m. sync ? "synchronous" : "not synchronous").
	         arg (m. layer ? "layer 2" : "layer 1"));
	      r ++;
	   }
	}
	for (auto &o : s. other) {
QString idText = o. hasServiceId ?
	         QString ("0x") + QString::number (o. serviceId, 16). toUpper () :
	         QString ("-");
	   if (!o. parsed) {
	      afsTable -> insertRow (r);
	      setCell (afsTable, r, 0, u8 (systemName (o. systemId)));
	      setCell (afsTable, r, 1, "?");
	      setCell (afsTable, r, 2, "-");
	      setCell (afsTable, r, 3, "-");
	      setCell (afsTable, r, 4, "-");
	      setCell (afsTable, r, 5, "-");
	      setCell (afsTable, r, 6, "not understood, raw: " + u8 (o. raw));
	      r ++;
	      continue;
	   }
	   for (int f : o. freqs) {
	      afsTable -> insertRow (r);
	      setCell (afsTable, r, 0, u8 (systemName (o. systemId)));
	      QString ft;
bool tunable = false;
	      if (o. systemId <= 2) {
	         ft = QString ("%1 kHz").arg (f);
	         tunable = (o. systemId == 0);
	      } else
	         ft = QString ("%1 MHz (code %2)").arg (87.1 + 0.1 * f, 0, 'f', 1).
	              arg (f);
	      QTableWidgetItem *fi = new QTableWidgetItem (ft);
	      if (tunable)
	         fi -> setData (Qt::UserRole, f);
	      fi -> setFlags (fi -> flags () & ~Qt::ItemIsEditable);
	      afsTable -> setItem (r, 1, fi);
	      setCell (afsTable, r, 2, idText);
	      setCell (afsTable, r, 3, o. sameService ? QString ("same service") :
	                                               QString ("other service"));
	      setCell (afsTable, r, 4, o. hasRegSched ?
	         regionText (s, o. regionId) : QString ("-"));
	      setCell (afsTable, r, 5, o. hasRegSched ?
	         scheduleOf (s, o. scheduleId) : QString ("-"));
	      setCell (afsTable, r, 6, o. announce ?
	         QString ("announcement id %1").arg (o. announceId) : QString (""));
	      r ++;
	   }
	}
	afsTable -> resizeColumnsToContents ();

//	announcements
	announceTable -> setRowCount (0);
int a = 0;
	for (auto &x : s. announcements) {
	   announceTable -> insertRow (a);
	   setCell (announceTable, a, 0, serviceList (s, x. serviceFlags));
	   setCell (announceTable, a, 1, x. sameMux ? "yes" : "no");
	   setCell (announceTable, a, 2, QString::number (x. id));
	   setCell (announceTable, a, 3, u8 (announcementTypes (x. support)));
	   setCell (announceTable, a, 4, u8 (announcementTypes (x. switching)));
	   setCell (announceTable, a, 5, QString ("0x%1").arg (x. support, 3, 16, QChar ('0')));
	   setCell (announceTable, a, 6, QString ("0x%1").arg (x. switching, 3, 16, QChar ('0')));
	   a ++;
	}
	announceTable -> resizeColumnsToContents ();
	if (s. alarmFac || s. alarmSdc) {
QString t = "ALARM: a warning / alarm announcement is active";
	   if (s. alarmFac)
	      t += " (signalled in the FAC, programme type 30)";
	   if (s. alarmSdc)
	      t += " (announcement switching flag set)";
	   alarmBanner -> setText (t);
	   alarmBanner -> setVisible (true);
	} else
	   alarmBanner -> setVisible (false);
}

void	ServiceInfoWindow::afsDoubleClicked (QTableWidgetItem *item) {
QTableWidgetItem *f = afsTable -> item (item -> row (), 1);
	if (f == nullptr)
	   return;
QVariant v = f -> data (Qt::UserRole);
	if (!v. isValid ())
	   return;			// not a DRM frequency
int kHz = v. toInt ();
	if (kHz < 100 || kHz > 30000)
	   return;
	emit tuneRequest ((int32_t)kHz * 1000);
}
