#
/*
 *    Copyright (C) 2014
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the  drm2
 *
 *    drm2 is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    drm2 is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include	<cstdio>
#include	<QFile>
#include	<QByteArray>
#include	<QStringList>
#include	"program-list.h"
#include	"radio.h"

	programList::programList (RadioInterface *mr,
	                          QString saveName) {
	this	-> saveName	= saveName;
	myWidget	= new QScrollArea (NULL);
	myWidget	-> resize (240, 200);
	myWidget	-> setWidgetResizable(true);
	myWidget	-> setWindowTitle ("Station list");

	tableWidget 	= new QTableWidget (0, 2);
	myWidget	-> setWidget(tableWidget);
	tableWidget 	-> setHorizontalHeaderLabels (
	            QStringList () << tr ("station") << tr ("frequency"));
	connect (tableWidget, SIGNAL (cellClicked (int, int)),
	         this, SLOT (tableSelect (int, int)));
	connect (tableWidget, SIGNAL (cellDoubleClicked (int, int)),
	         this, SLOT (removeRow (int, int)));
	connect (this, SIGNAL (newFrequency (int32_t)),
	         mr, SLOT (setFrequency (int32_t)));
	loadTable ();
}

	programList::~programList (void) {
int16_t	i;
int16_t	rows	= tableWidget -> rowCount ();

	for (i = rows; i > 0; i --)
	   tableWidget -> removeRow (i);
	delete	tableWidget;
	delete	myWidget;
}

void	programList::show	(void) {
	myWidget	-> show ();
}

void	programList::hide	(void) {
	myWidget	-> hide ();
}

//	One row in the table, without touching the file.
void	programList::appendRow (const QString &name, const QString &freq) {
int	row	= tableWidget -> rowCount ();

	tableWidget	-> insertRow (row);
	QTableWidgetItem *item0	= new QTableWidgetItem (name);
	item0		-> setTextAlignment (Qt::AlignRight | Qt::AlignVCenter);
	tableWidget	-> setItem (row, 0, item0);

	QTableWidgetItem *item1	= new QTableWidgetItem (freq);
	item1		-> setTextAlignment (Qt::AlignRight | Qt::AlignVCenter);
	tableWidget	-> setItem (row, 1, item1);
	tableWidget	-> setCurrentItem (item0);
}

void	programList::addRow (const QString &name, const QString &freq) {
	appendRow (name, freq);
	saveTable ();	// at once: do not depend on a clean exit
}
//
//	Locally we dispatch the "click" and "translate"
//	it into a frequency and a call to the main gui to change
//	the frequency

void	programList::tableSelect (int row, int column) {
QTableWidgetItem* theItem = tableWidget  -> item (row, 1);

	(void)column;
	if (theItem == nullptr)
	   return;
	int32_t	freq	= theItem -> text (). toInt ();
	if (freq <= 0)		// an empty or damaged entry: never tune to 0
	   return;
	emit newFrequency (Khz (freq));
}

void	programList::removeRow (int row, int column) {
	tableWidget	-> removeRow (row);
	(void)column;
	saveTable ();
}
//
//	The list is kept in a plain UTF-8 text file, one station per line:
//		name <TAB> frequency in kHz
//	(lines starting with # are comments). Qt's own streaming of table
//	items proved unreliable - items came back empty - and a text file
//	can also simply be opened in Notepad to see what is stored.
void	programList::saveTable (void) {
QFile	file (saveName);

	if (!file. open (QIODevice::WriteOnly | QIODevice::Truncate)) {
	   fprintf (stderr, "cannot write the station list %s\n",
	                          saveName. toLocal8Bit (). data ());
	   return;
	}

QByteArray data	= "# Dxing.org DRM receiver - station list: name <TAB> frequency in kHz\n";
int	count	= 0;
	for (int i = 0; i < tableWidget -> rowCount (); i ++) {
	   QTableWidgetItem *n	= tableWidget -> item (i, 0);
	   QTableWidgetItem *f	= tableWidget -> item (i, 1);
	   QString name		= (n != nullptr) ? n -> text () : QString ();
	   QString freq		= (f != nullptr) ? f -> text () : QString ();
	   name. replace (QChar ('\t'), QChar (' '));
	   name. replace (QChar ('\n'), QChar (' '));
	   name. replace (QChar ('\r'), QChar (' '));
	   name		= name. trimmed ();
	   freq		= freq. trimmed ();
	   if (freq. toInt () <= 0)	// nothing useful to store
	      continue;
	   data. append (name. toUtf8 ());
	   data. append ('\t');
	   data. append (freq. toUtf8 ());
	   data. append ('\n');
	   count ++;
	}
	file. write (data);
	file. close ();
	fprintf (stderr, "station list: %d stations saved to %s\n",
	                        count, saveName. toLocal8Bit (). data ());
}

void	programList::loadTable (void) {
QFile	file (saveName);

	if (!file. open (QIODevice::ReadOnly))
	   return;			// no list yet
QByteArray data	= file. readAll ();
	file. close ();

	const QStringList lines = QString::fromUtf8 (data). split (QChar ('\n'));
int	count	= 0;
	for (const QString &raw : lines) {
	   QString line	= raw;
	   line. remove (QChar ('\r'));
	   if (line. isEmpty () || line. startsWith (QChar ('#')))
	      continue;
	   int tab	= line. lastIndexOf (QChar ('\t'));
	   if (tab < 0)
	      continue;
	   QString name	= line. left (tab). trimmed ();
	   QString freq	= line. mid (tab + 1). trimmed ();
	   if (freq. toInt () <= 0)	// damaged line: skip it
	      continue;
	   appendRow (name, freq);
	   count ++;
	}
	fprintf (stderr, "station list: %d stations read from %s\n",
	                        count, saveName. toLocal8Bit (). data ());
}
