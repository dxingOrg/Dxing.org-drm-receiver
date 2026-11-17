#
/*
 *    Dxing.org DRM receiver - data services, step 1
 *
 *    Packet-mode data streams of a DRM multiplex: packet extraction,
 *    CRC check, data-unit reassembly, MSC data group parsing and a text
 *    "inspector" report. Pure C++ (no Qt) so it can be tested alone.
 *
 *    Released under the GNU General Public License, version 2 or later,
 *    like the rest of this program.
 *
 *    NOTE: the packet / data group layouts below were written from the
 *    DRM (ETSI ES 201 980) and MOT (ETSI EN 301 234) descriptions as
 *    remembered, and are checked only against synthetic test data.
 *    They must be confirmed with a real off-air recording.
 */
#ifndef	__DATA_HUB__
#define	__DATA_HUB__

#include	<stdint.h>
#include	<stddef.h>
#include	<vector>
#include	<string>
#include	<deque>
#include	<map>
#include	<mutex>
#include	<functional>
#include	<stdio.h>
#include	"service-diag.h"
#include	"data-carousel.h"

namespace dxdata {

//	CRC-16 as used for DRM packets and data groups: polynomial
//	x^16 + x^12 + x^5 + 1, register preset to all ones, result
//	complemented (CRC-16/GENIBUS; check value of "123456789" is 0xD64E)
uint16_t	crc16		(const uint8_t *p, size_t n);

struct DataGroup {
	uint8_t		type		= 0;
	uint8_t		continuity	= 0;
	uint8_t		repetition	= 0;
	bool		crcPresent	= false;
	bool		crcOk		= false;
	bool		hasSegment	= false;
	bool		lastSegment	= false;
	uint16_t	segmentNumber	= 0;
	bool		hasTransportId	= false;
	uint16_t	transportId	= 0;
	std::vector<uint8_t> payload;	// data field, headers and CRC removed
//	set by the hub before the group is handed on
	int		stream		= -1;
	int		appId		= 0;	// application of the stream
	int		domain		= 0;
	int		packetId	= 0;
//	the whole data unit as received (header and CRC included), for
//	applications that do not use MSC data groups
	std::vector<uint8_t> raw;
};

//	false: too short or inconsistent. A group with a wrong CRC is still
//	returned (crcPresent = true, crcOk = false) so the caller can count it
bool		parseDataGroup	(const uint8_t *p, size_t n, DataGroup &out);

//	a readable name for a user application identifier. These are the
//	DAB "user application types" that DRM data applications reuse;
//	treat the names as a hint until verified on air.
std::string	applicationName	(int domain, int appId);

typedef std::function<void (int stream, const DataGroup &)> GroupSink;

class DataHub {
public:
		DataHub		();
		~DataHub	();
	void	reset		();
//	called for every frame for every data stream; harmless to repeat,
//	a change of any parameter restarts that stream's statistics
	void	configureStream	(int stream, bool packetMode, int packetLength,
	                         int packetId, int domain, int appId, bool fec);
//	the bytes of one stream of one (400 ms) frame: HP part, then LP part
	void	feedStream	(int stream, const uint8_t *bytes, size_t n);
	void	setGroupSink	(GroupSink);
//	is a packet-mode stream with this application announced? (stream
//	index in *stream when asked for)
	bool	hasApplication	(int appId, int *stream = nullptr);
//	write every reassembled data unit (also damaged ones) as one hex
//	line to a text file, for offline analysis. false: cannot open it
	bool	startDump	(const std::string &path);
	void	stopDump	();
	bool	dumping		();
	std::string report	();
//	passive recorder of FAC/SDC service information (see service-diag.h)
	ServiceDiag *serviceDiag	() { return &diagData; }
//	carousel combining (see handlePacket): on by default. Off only for
//	comparisons and tests
	void	setCombining	(bool on);
private:
//	A carousel repeats the same data units. Two copies differ only in the
//	continuity / repetition byte of the group header and in the group CRC,
//	so the packets in between can be collected over several repetitions
//	while every single packet has been checked by its own CRC. The first
//	and last packet always come from the same repetition, so the group CRC
//	still checks the whole result.
	struct Cand {
	   int		K		= -1;	// packets in the unit, -1 unknown
	   std::vector<std::vector<uint8_t>> mid;
	   std::vector<char>	have;
	   bool		delivered	= false;
	   uint32_t	used		= 0;
	};
	struct Pass {
	   bool		active	= false;
	   uint64_t	key	= 0;
	   int64_t	sFirst	= 0;
	   int		K	= -1;
	   bool		lastSeen = false;
	   std::vector<std::vector<uint8_t>> pk;
	   std::vector<char>	have;
	   std::vector<uint8_t>	first;
	   std::vector<uint8_t>	last;
	};
	struct Combiner {
	   std::map<uint64_t, Cand>	cands;
	   Pass		pass;
	   bool		haveS	= false;
	   int64_t	S	= 0;		// sequence number of the last good packet
	   int		lastCI	= 0;
	   uint32_t	badSince = 0;	// bad slots since that packet
	   uint32_t	passes	= 0;
	   uint32_t	completed = 0;	// units delivered thanks to combining
	   uint32_t	rejected = 0;	// combined unit failed its CRC
	   uint32_t	conflicts = 0;
	   uint32_t	orphans	= 0;
	};
	struct Assembly {
	   std::vector<uint8_t>	buf;
	   bool			inUnit	= false;
	   int			lastCI	= -1;
	};
	struct Stream {
	   bool		configured	= false;
	   bool		packetMode	= false;
	   int		packetLength	= 0;
	   int		packetId	= 0;
	   int		domain		= 0;
	   int		appId		= 0;
	   bool		fec		= false;
	   int		sizeMode	= -1;	// -1 undecided, 0: L, 1: L + 3
	   uint32_t	scoreTotal	= 0;
	   uint32_t	scorePlus3	= 0;
	   uint32_t	tried		= 0;
	   uint64_t	bytes		= 0;
	   uint32_t	frames		= 0;
	   uint32_t	badRemainder	= 0;
	   uint32_t	packets		= 0;
	   uint32_t	crcOk		= 0;
	   uint32_t	crcBad		= 0;
	   uint32_t	padding		= 0;
	   uint32_t	malformed	= 0;
	   uint32_t	orphans		= 0;
	   uint32_t	ciJumps		= 0;
	   uint32_t	units		= 0;
	   uint32_t	groupsOk	= 0;
	   uint32_t	groupsBad	= 0;
	   uint32_t	groupsNoCrc	= 0;
	   uint32_t	groupsUnparsed	= 0;
	   uint32_t	packetsPerId [4]	= {0, 0, 0, 0};
	   uint32_t	unitsPerId [4]	= {0, 0, 0, 0};
	   uint32_t	unitsOtherId	= 0;	// not on the announced packet id
	   uint32_t	combinedUnits	= 0;	// units completed by combining
	   uint32_t	dcUnits		= 0;	// completed by the carousel assembly
	   std::map<int, uint32_t> byType;
	   Assembly	as [4];
	   Combiner	cb;
	   DbCarousel	dc;		// data broadcast packets from repetitions
	};
	std::mutex	lock;
	ServiceDiag	diagData;
	Stream		streams [4];
	std::deque<std::string>	recent;
	GroupSink	sink;
	uint64_t	totalBytes;
	FILE		*dumpFile;
	uint32_t	dumpCount;
	uint32_t	dumpFrames;
	bool		combining;

	void	handlePacket	(int idx, Stream &s, const uint8_t *p, size_t L);
	bool	finishUnit	(int idx, Stream &s, Assembly &a, int pid,
	                         bool combined = false);
	void	combinerPacket	(int idx, Stream &s, bool first, bool last, int ci,
	                         const uint8_t *data, size_t dlen, size_t fullLen);
	void	combinerEndPass	(int idx, Stream &s, int pid);
	void	clearStream	(Stream &s);
	void	deliverCarousel	(int idx, Stream &s);
};

}	// namespace dxdata
#endif
