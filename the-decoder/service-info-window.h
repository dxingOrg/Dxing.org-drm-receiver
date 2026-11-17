#
/*
 *    Dxing.org DRM receiver - "Service info" window
 *    GNU General Public License, version 2 or later.
 *
 *    A non-modal window with what the station says about itself (services,
 *    label, language, programme type, date and time), the alternative
 *    frequencies (double click on a DRM frequency tunes to it) and the
 *    announcements (with a red banner when an alarm is announced).
 *    It only reads copies (snapshots) made by dxdata::SdcInfo.
 *    NOT compiled or run by the author of this patch yet.
 */
#pragma once
#include	<QWidget>
#include	<stdint.h>
#include	"service-info.h"

class	QTabWidget;
class	QTableWidget;
class	QTableWidgetItem;
class	QLabel;
class	QTimer;

class	ServiceInfoWindow: public QWidget {
Q_OBJECT
public:
		ServiceInfoWindow	(dxdata::SdcInfo *, QWidget *parent = nullptr);
		~ServiceInfoWindow	();
signals:
//	the user double clicked a DRM frequency (Hz)
	void	tuneRequest	(int32_t);
protected:
	void	showEvent	(QShowEvent *) override;
	void	hideEvent	(QHideEvent *) override;
private slots:
	void	tick		();
	void	afsDoubleClicked (QTableWidgetItem *);
private:
	dxdata::SdcInfo	*info;
	QTabWidget	*tabs;
	QLabel		*timeLabel;
	QTableWidget	*stationTable;
	QTableWidget	*afsTable;
	QLabel		*alarmBanner;
	QLabel		*announceNote;
	QTableWidget	*announceTable;
	QTimer		*timer;
	uint32_t	lastCounter;
	bool		first;
	void	rebuild		(const dxdata::InfoSnapshot &);
	QString	serviceList	(const dxdata::InfoSnapshot &, int flags);
	QString	regionText	(const dxdata::InfoSnapshot &, int id);
	QString	scheduleOf	(const dxdata::InfoSnapshot &, int id);
};
