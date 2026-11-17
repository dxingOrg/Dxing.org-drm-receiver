#
/*
 *    Dxing.org DRM receiver - "Files and data" window (slide show, files, data broadcast)
 *    GNU General Public License, version 2 or later.
 *
 *    A non-modal window (the audio keeps playing) with two tabs: the
 *    slides that were received (MOT SlideShow) and other MOT objects
 *    (files), which are only saved when the user asks for it or turns on
 *    automatic saving. Nothing received is ever opened or run.
 *    NOT compiled or run by the author of this patch yet.
 */
#pragma once
#include	<QWidget>
#include	<stdint.h>
#include	<vector>
#include	<set>
#include	"mot.h"
#include	"data-broadcast.h"

class	QImage;
namespace dxdata { class DataHub; }
class	QTabWidget;
class	QListWidget;
class	QPlainTextEdit;
class	QLabel;
class	QPushButton;
class	QCheckBox;
class	QTimer;

//	A picture to show: a MOT slide show slide or an image file of the
//	Chinese data broadcast (JPEG, PNG or GIF, recognised by its content)
struct SlideRef {
	bool		fromDb;
	dxdata::MotInfo	mi;
	dxdata::DbInfo	di;
	SlideRef	(): fromDb (false), mi (), di () {}
	uint32_t key	() const { return fromDb ? (0x80000000u | di. seq) : mi. seq; }
};

class	MotWindow: public QWidget {
Q_OBJECT
public:
		MotWindow	(dxdata::MotDecoder *, dxdata::DataBroadcastDecoder *,
	                         dxdata::DataHub *, QWidget *parent = nullptr);
		~MotWindow	();
	void	showTab		(int tab);	// 0: slides, 1: files, 2: data broadcast
protected:
	void	showEvent	(QShowEvent *) override;
	void	hideEvent	(QHideEvent *) override;
	void	resizeEvent	(QResizeEvent *) override;
private slots:
	void	tick		();
	void	prevSlide	();
	void	nextSlide	();
	void	saveSlide	();
	void	saveSelectedFile ();
	void	saveAllFiles	();
	void	openFolder	();
	void	fileSelectionChanged ();
	void	dbSelectionChanged ();
	void	saveDbFile	();
	void	saveDbText	();
private:
	dxdata::DataBroadcastDecoder	*db;
	QListWidget		*dbList;
	QPlainTextEdit		*dbText;
	QLabel			*dbInfo;
	QPushButton		*dbSaveFileButton;
	QPushButton		*dbSaveTextButton;
	std::vector<dxdata::DbInfo>	dbItems;
	std::set<uint32_t>	dbAutoSaved;
	bool	saveData	(const QString &name, const QByteArray &data);
	void	refreshDb	();
	dxdata::MotDecoder	*mot;
	dxdata::DataHub		*hub;
	QLabel			*dbProgress;
	QTabWidget		*tabs;
	QLabel			*imageLabel;
	QLabel			*caption;
	QLabel			*counter;
	QPushButton		*prevButton;
	QPushButton		*nextButton;
	QPushButton		*saveSlideButton;
	QCheckBox		*newestBox;
	QListWidget		*fileList;
	QLabel			*fileInfo;
	QPlainTextEdit		*filePreview;
	QPushButton		*saveFileButton;
	QPushButton		*saveAllButton;
	QCheckBox		*autoSaveBox;
	QLabel			*status;
	QLabel			*saved;
	QTimer			*timer;
	std::vector<SlideRef>	slides;
	std::vector<dxdata::MotInfo>	files;
	int			slideIndex;
	uint32_t		shownSeq;
	uint32_t		lastChange;
	std::set<uint32_t>	autoSaved;
	QImage			*current;		// decoded slide on screen
	void	refresh		();
	void	showSlide	();
	void	scaleImage	();
	bool	saveObject	(uint32_t seq);
};
