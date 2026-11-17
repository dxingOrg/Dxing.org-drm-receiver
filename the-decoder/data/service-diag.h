#
/*
 *    Dxing.org DRM receiver - service diagnostics
 *
 *    A passive recorder: it only WRITES down what the FAC and SDC
 *    processors see (which service, which stream, which label arrived and
 *    what was done with it) so that it can be shown in the "Data info"
 *    window. It never changes the decoding. Pure C++ (no Qt).
 *
 *    Released under the GNU General Public License, version 2 or later,
 *    like the rest of this program.
 */
#ifndef	__SERVICE_DIAG__
#define	__SERVICE_DIAG__

#include	<stdint.h>
#include	<stddef.h>
#include	<string>
#include	<vector>
#include	<set>
#include	<map>
#include	<mutex>

namespace dxdata {

//	one entry of stateDescriptor::streams [], copied
struct SlotInfo {
	bool		inUse		= false;
	int		soort		= 0;		// 0 audio, 1 data
	int		shortId		= 0;
	int		streamId	= 0;
	int		serviceId	= 0;
	bool		isAudio		= false;
	int		lengthHigh	= 0;
	int		lengthLow	= 0;
	int		audioCoding	= 0;
	int		audioMode	= 0;
	int		samplingRate	= 0;
	std::string	name;
};

//	one entry of stateDescriptor::svc [], copied
struct SvcInfo {
	int		audioStream	= -1;
	int		dataStream	= -1;
	std::string	label;
};

class ServiceDiag {
public:
		ServiceDiag	();
		~ServiceDiag	();
	void	clear		();
//	the decoder lost sync and starts again (stateDescriptor::cleanUp)
	void	onResync	();
//	one SDC block was interpreted
	void	onSdcBlock	(int afs);
//	one SDC entity. bits: one bit per byte, MSB first, starting at the
//	first bit of the body; the body has 8 * lengthofBody + 4 bits
	void	onSdcEntity	(int type, int versionFlag, int lengthofBody,
	                         const uint8_t *bits);
//	one FAC service description
	void	onFacService	(int shortId, int serviceId, bool isAudio,
	                         int language, int descriptor);
//	one label entity and what was done with it
	void	onLabel		(int shortId, const std::string &label,
	                         const std::string &verdict);
//	the stream table after an SDC block or a FAC block
	void	setSlots	(int numofStreams, const SlotInfo *slotTab,
	                         int audioStream, int chosenService,
	                         const SvcInfo *svcTab);
//	the audio decoder was given this stream
	void	noteAudio	(int streamIndex);
	std::string report	();
private:
	void	addLine		(const std::string &key, const std::string &text);
	std::mutex	lock;
	int		resyncs;
	int		sdcBlocks;
	int		facBlocks;
	int		seq;
	std::map<int, int>	entityCount;
	int		typeLines [16];
	std::set<std::string>	seen;		// this sync cycle only
	std::vector<std::string> lines;
	bool		linesFull;
	struct FacSvc {
	   int		count	= 0;
	   int		serviceId = 0;
	   bool	isAudio	= false;
	   int		language = 0;
	   int		descriptor = 0;
	}		facSvc [4];
	SlotInfo	slotTab [4];
	int		numofStreams;
	int		audioChannel;		// stream index that is decoded
	int		chosenService;
	SvcInfo		svcTab [4];
	int		audioCount [4];
	int		labelCount [4];
	int		labelDropped [4];
};

}
#endif
