#
/*
 *    Dxing.org DRM receiver - Journaline window
 *    GNU General Public License, version 2 or later.
 */
#include	"journaline-window.h"
#include	"data-hub.h"
#include	<QTextBrowser>
#include	<QScrollBar>
#include	<QPushButton>
#include	<QLabel>
#include	<QCheckBox>
#include	<QTimer>
#include	<QUrl>
#include	<QDir>
#include	<QFile>
#include	<QFont>
#include	<QSignalBlocker>
#include	<QPalette>
#include	<QColor>
#include	<QVBoxLayout>
#include	<QHBoxLayout>

using namespace dxdata;

//	text of an object -> HTML. 0x02 / 0x03 are the highlight markers
//	from the decoder, '\n' is a preferred line break
static QString	formatText (const std::string &s, bool lineBreaks) {
QString	t	= QString::fromUtf8 (s. data (), (qsizetype)s. size ());
QString	out;
int	bold	= 0;
	for (QChar ch : t) {
	   ushort u	= ch. unicode ();
	   if (u == 2) {
	      out	+= "<b>";
	      bold ++;
	   }
	   else
	   if (u == 3) {
	      if (bold > 0) {
	         out	+= "</b>";
	         bold --;
	      }
	   }
	   else
	   if (u == '\n')
	      out	+= lineBreaks ? QString ("<br>") : QString (" ");
	   else
	   if (u == '<')
	      out	+= "&lt;";
	   else
	   if (u == '>')
	      out	+= "&gt;";
	   else
	   if (u == '&')
	      out	+= "&amp;";
	   else
	   if (u < 0x20)
	      out	+= ' ';
	   else
	      out	+= ch;
	}
	while (bold -- > 0)
	   out	+= "</b>";
	return out;
}

static QString	pageHtml (JournalineDecoder *dec, const JmlObject &o,
	                       bool updatePending,
	                       const QString &linkColor,
	                       const QString &dimColor) {
QString	r;
	if (updatePending)
	   r	+= "<p><a href=\"jml:refresh\" style=\"color:" + linkColor + "\"><b>"
	         "A newer version of this page was received - click here to show it."
	         "</b></a></p>";
	switch (o. type) {
	   case JML_MENU:
	      r	+= "<h3>" + formatText (o. title, false) + "</h3>";
	      for (const JmlLink &l : o. links) {
	         QString text	= formatText (l. text, false);
	         QString color	= linkColor;
	         if (!dec -> hasObject (l. target)) {	// not received yet
	            text	= "[" + text + "]";
	            color	= dimColor;
	         }
	         r	+= "<p><a href=\"jml:" + QString::number (l. target) +
	               "\" style=\"color:" + color + "\">" + text + "</a></p>";
	      }
	      break;
	   case JML_PLAIN:
	      r	+= "<h3>" + formatText (o. title, false) + "</h3>";
	      r	+= "<p>" + formatText (o. body, true) + "</p>";
	      break;
	   case JML_TITLE:
	      r	+= "<h2 align=\"center\">" + formatText (o. title, false) + "</h2>";
	      break;
	   case JML_LIST:
	      r	+= "<h3>" + formatText (o. title, false) + "</h3>";
	      r	+= "<table cellspacing=\"0\" cellpadding=\"3\">";
	      for (const auto &row : o. rows) {
	         r	+= "<tr>";
	         for (const std::string &cell : row)
	            r	+= "<td><tt>" + formatText (cell, false) + "</tt></td>";
	         r	+= "</tr>";
	      }
	      r	+= "</table>";
	      break;
	   default:
	      break;
	}
	return r;
}

	JournalineWindow::JournalineWindow (JournalineDecoder *d,
	                                    DataHub *h, QWidget *parent):
	                                    QWidget (parent, Qt::Window) {
	dec		= d;
	hub		= h;
	shownValid	= false;
	updatePending	= false;
	lastChange	= 0;
	path. push_back (0);				// the root object
	setWindowTitle (tr ("Journaline"));
	resize (520, 580);

	backButton	= new QPushButton (tr ("Back"), this);
	homeButton	= new QPushButton (tr ("Main menu"), this);
	status		= new QLabel (this);
	browser		= new QTextBrowser (this);
	browser	-> setOpenLinks (false);
	browser	-> setOpenExternalLinks (false);
QFont	f	= browser -> font ();
	if (f. pointSize () > 0) {
	   f. setPointSize (f. pointSize () + 2);
	   browser -> setFont (f);
	}
	recordBox	= new QCheckBox (tr ("Record raw data for analysis"), this);
	recordBox	-> setToolTip (tr ("Writes every received data unit to "
	                             "Dxing-DRM/data-groups.txt in your user folder"));
	recordInfo	= new QLabel (this);

QHBoxLayout *top	= new QHBoxLayout ();
	top -> addWidget (backButton);
	top -> addWidget (homeButton);
	top -> addWidget (status, 1);
QHBoxLayout *bottom	= new QHBoxLayout ();
	bottom -> addWidget (recordBox);
	bottom -> addWidget (recordInfo, 1);
QVBoxLayout *col	= new QVBoxLayout (this);
	col -> addLayout (top);
	col -> addWidget (browser, 1);
	col -> addLayout (bottom);

	timer	= new QTimer (this);
	timer	-> setInterval (500);
	connect (timer, &QTimer::timeout, this, &JournalineWindow::tick);
	connect (backButton, &QPushButton::clicked, this, &JournalineWindow::goBack);
	connect (homeButton, &QPushButton::clicked, this, &JournalineWindow::goHome);
	connect (browser, &QTextBrowser::anchorClicked,
	         this, &JournalineWindow::linkClicked);
	connect (recordBox, &QCheckBox::toggled,
	         this, &JournalineWindow::recordToggled);
}

	JournalineWindow::~JournalineWindow () {
	if (hub != nullptr)
	   hub -> stopDump ();
}

void	JournalineWindow::showEvent (QShowEvent *e) {
	QWidget::showEvent (e);
	lastChange	= dec -> changeCounter () - 1;	// force a first render
	tick ();
	timer	-> start ();
}

void	JournalineWindow::hideEvent (QHideEvent *e) {
	timer	-> stop ();
	QWidget::hideEvent (e);
}

void	JournalineWindow::tick () {
JournalineStatus st	= dec -> status ();
QString	s;
	if (st. objects == 0)
	   s	= tr ("Waiting for Journaline data ...");
	else
	if (st. tocObjectCount > 0)		// the main menu is not in the list
	   s	= tr ("%1 of %2 pages received").arg (st. tocReceived)
	                                       . arg (st. tocObjectCount);
	else
	   s	= tr ("%1 pages received").arg (st. objects);
	status -> setText (s);
	status -> setToolTip (tr ("Rejected data units: %1, unusable pages: %2")
	                      . arg (st. groupsRejected). arg (st. objectsRejected));
	backButton	-> setEnabled (path. size () > 1);
uint32_t cc	= dec -> changeCounter ();
	if (cc != lastChange) {
	   lastChange	= cc;
	   render (true);
	}
}

void	JournalineWindow::render (bool keepScroll) {
int	vpos	= browser -> verticalScrollBar () -> value ();
uint16_t id	= path. back ();
JmlObject cur;
bool	have	= dec -> getObject (id, cur);
QString	page;
	if (!have) {
	   shownValid	= false;
	   updatePending = false;
	   if (dec -> status (). objects == 0)
	      page	= tr ("<p><i>Waiting for Journaline data.</i></p>"
	                  "<p>The station sends its pages in a loop. The main menu "
	                  "can take a minute or more to arrive, and a weak signal "
	                  "makes it slower.</p>");
	   else
	      page	= tr ("<p><i>Waiting for page %1 ...</i></p>"
	                  "<p>It will appear here as soon as it is received.</p>")
	                  . arg (id);
	}
	else {
//	a text page that changed under the reader is not replaced silently
	   if (shownValid && shown. id == id && cur. type == JML_PLAIN &&
	       !cur. sameContent (shown))
	      updatePending	= true;
	   else {
	      shown		= cur;
	      shownValid	= true;
	      updatePending	= false;
	   }
//	the default blue of a link is unreadable on a dark window
	   bool dark	= browser -> palette (). color (QPalette::Base). lightness () < 128;
	   page	= pageHtml (dec, shown, updatePending,
	                    dark ? QString ("#ffffff") : QString ("#0645ad"),
	                    dark ? QString ("#b4b4b4") : QString ("#666666"));
	}
	browser -> setHtml (page);
	if (keepScroll)
	   browser -> verticalScrollBar () -> setValue (vpos);
}

//	clause 4.3: following a link to a page that is already in the history
//	path shortens the path to it
void	JournalineWindow::navigate (uint16_t id) {
bool	found	= false;
	for (size_t i = 0; i < path. size (); i ++)
	   if (path [i] == id) {
	      path. resize (i + 1);
	      found	= true;
	      break;
	   }
	if (!found) {
	   path. push_back (id);
	   if (path. size () > 64)
	      path. erase (path. begin ());
	}
	shownValid	= false;
	updatePending	= false;
	render (false);
	backButton	-> setEnabled (path. size () > 1);
}

void	JournalineWindow::linkClicked (const QUrl &url) {
QString	s	= url. toString ();
	if (s == "jml:refresh") {
	   shownValid	= false;
	   updatePending = false;
	   render (true);
	   return;
	}
	if (!s. startsWith ("jml:"))
	   return;
bool	ok;
uint	v	= s. mid (4). toUInt (&ok);
	if (!ok || v > 0xFFFF)
	   return;
	navigate ((uint16_t)v);
}

void	JournalineWindow::goBack () {
	if (path. size () > 1)
	   path. pop_back ();
	shownValid	= false;
	updatePending	= false;
	render (false);
	backButton	-> setEnabled (path. size () > 1);
}

void	JournalineWindow::goHome () {
	path. assign (1, 0);
	shownValid	= false;
	updatePending	= false;
	render (false);
	backButton	-> setEnabled (false);
}

void	JournalineWindow::recordToggled (bool on) {
	if (hub == nullptr)
	   return;
	if (!on) {
	   hub -> stopDump ();
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
