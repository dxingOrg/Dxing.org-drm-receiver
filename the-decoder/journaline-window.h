#
/*
 *    Dxing.org DRM receiver - Journaline window
 *    GNU General Public License, version 2 or later.
 *
 *    A non-modal window (so the audio keeps playing, the window only
 *    reads what the decoder thread has collected) that shows the pages
 *    of a Journaline service: menus, text pages, tickers and lists.
 *    NOT compiled or run by the author of this patch yet.
 */
#pragma once
#include	<QWidget>
#include	<QUrl>
#include	<stdint.h>
#include	<vector>
#include	"journaline.h"

class	QTextBrowser;
class	QPushButton;
class	QLabel;
class	QTimer;
class	QCheckBox;
namespace dxdata { class DataHub; }

class	JournalineWindow: public QWidget {
Q_OBJECT
public:
		JournalineWindow	(dxdata::JournalineDecoder *,
	                                 dxdata::DataHub *,
	                                 QWidget *parent = nullptr);
		~JournalineWindow	();
protected:
	void	showEvent	(QShowEvent *) override;
	void	hideEvent	(QHideEvent *) override;
private slots:
	void	tick		();
	void	goBack		();
	void	goHome		();
	void	linkClicked	(const QUrl &);
	void	recordToggled	(bool);
private:
	dxdata::JournalineDecoder	*dec;
	dxdata::DataHub		*hub;
	QTextBrowser		*browser;
	QPushButton		*backButton;
	QPushButton		*homeButton;
	QLabel			*status;
	QCheckBox		*recordBox;
	QLabel			*recordInfo;
	QTimer			*timer;
	std::vector<uint16_t>	path;		// back () is the page on screen
	dxdata::JmlObject	shown;
	bool			shownValid;
	bool			updatePending;
	uint32_t		lastChange;
	void	navigate	(uint16_t);
	void	render		(bool keepScroll);
};
