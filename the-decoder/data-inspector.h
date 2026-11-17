#
/*
 *    Dxing.org DRM receiver - data services inspector window
 *    GNU General Public License, version 2 or later.
 *
 *    A small non-modal window that shows what the data hub currently
 *    sees (announced data services, packet / data group statistics).
 *    It only reads; it never touches the audio path.
 */
#pragma once
#include	<QWidget>

class	QPlainTextEdit;
class	QTimer;
class	QLabel;
class	QCheckBox;
namespace dxdata { class DataHub; }

class	DataInspector: public QWidget {
Q_OBJECT
public:
		DataInspector	(dxdata::DataHub *, QWidget *parent = nullptr);
		~DataInspector	();
protected:
	void	showEvent	(QShowEvent *) override;
	void	hideEvent	(QHideEvent *) override;
private slots:
	void	refresh		();
	void	saveLog		();
	void	clearStats	();
	void	recordToggled	(bool);
private:
	QCheckBox	*recordBox;
	QLabel		*recordInfo;
	dxdata::DataHub	*hub;
	QPlainTextEdit	*text;
	QLabel		*status;
	QTimer		*timer;
};
