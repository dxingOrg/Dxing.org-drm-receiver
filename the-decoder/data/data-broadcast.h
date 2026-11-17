#
/*
 *    Dxing.org DRM receiver - data broadcast packets (GY/T 423-2025, 6.7)
 *    GNU General Public License, version 2 or later.
 *
 *    Pure C++ (no Qt). Written from the Chinese national standard
 *    GY/T 423-2025 (clause 6.7, data broadcasting), checked against real
 *    off-air data of a Chinese service (application 0x44B). The standard
 *    leaves the meaning of the packet types and of EBM_id of an
 *    emergency broadcast service (service type 1) to another document;
 *    those parts are shown raw.
 *
 *    Packet (all fields most significant bit first):
 *      16  start marker 0x4959
 *       4  service type (1: emergency broadcast, 2: data broadcast)
 *       3  resource id
 *       1  extension packet flag
 *      10  packet number
 *       4  update number
 *       2  packet type (0 stream, 1 file, 2 description file)
 *      10  number of packets of this part
 *       2  FEC indicator
 *      12  payload length
 *      16  extension packet id (only with the extension flag)
 *      16  EBM_id check (only for service type 1)
 *       n  payload
 *      16  CRC-16 (x^16+x^12+x^5+1, start 0xFFFF, inverted) over all before it
 */
#ifndef	__DATA_BROADCAST__
#define	__DATA_BROADCAST__

#include	<stdint.h>
#include	<stddef.h>
#include	<time.h>
#include	<vector>
#include	<string>
#include	<map>
#include	<deque>
#include	<mutex>
#include	"data-hub.h"

namespace dxdata {

std::string	md5Hex		(const uint8_t *d, size_t n);

//	HTML to plain text, for display only: tags are dropped (script and
//	style with their content), <br> and block ends become line breaks,
//	the common entities are decoded. Nothing is ever loaded or executed.
std::string	htmlToPlainText	(const std::string &html);

struct DbPacket {
	int		serviceType;
	int		resourceId;
	bool		extFlag;
	int		packetNo;
	int		updateNo;
	int		packetType;	// 0 stream, 1 file, 2 description file
	int		packetCount;
	int		fec;
	uint16_t	extId;		// high byte: this one, low byte: the last one
	uint16_t	ebmId;
	std::vector<uint8_t> payload;
};

//	false: not a data broadcast packet, or damaged (checks the CRC)
bool	parseDbPacket	(const uint8_t *d, size_t n, DbPacket &out);

//	the 15 attributes of an information description file (one per line)
struct DbDescription {
	std::string	serviceId, mode, resourceId, updateNo, name, fileType;
	std::string	title, summary, keywords, encoding, path;
	std::string	fileLength, startTime, expiryTime, deleteFlag;
};
bool	parseDbDescription (const std::string &text, DbDescription &out);

struct DbResource {
	enum Kind { Unknown = 0, Text, Html, Jpeg, Png, Gif, Emergency };
	uint32_t	seq		= 0;
	int		serviceType	= 0;
	int		resourceId	= 0;
	int		updateNo	= 0;
	time_t		received	= 0;
	bool		hasDescription	= false;
	DbDescription	desc;
	std::vector<uint8_t> file;
	bool		lengthChecked	= false;
	bool		lengthOk	= false;
	bool		md5Checked	= false;	// the name is 32 hex digits: an MD5?
	bool		md5Ok		= false;
	Kind		kind		= Unknown;
	std::string	subHeaderHex;		// emergency service: not defined here
	bool		hasTime		= false;	// the 4 bytes after the first one
	int64_t		timeUnix	= 0;	// (a guess: they fit a UTC time)
	std::string	text;			// plain text of text / HTML
	std::string	suggestedFileName () const;
};

struct DbInfo {
	uint32_t	seq;
	int		serviceType, resourceId, updateNo;
	time_t		received;
	DbResource::Kind kind;
	size_t		size;
	std::string	title;
	bool		hasDescription, lengthOk, md5Checked, md5Ok;
};

//	A file or message that is still being received (or only described)
struct DbProgress {
	int		serviceType, resourceId, updateNo, packetType;
	int		count;		// data broadcast packets in the file; 0: not known
	int		have;		// of them received
	size_t		bytes;		// received so far
	bool		hasDescription;
	std::string	name, fileLength;
};

struct DbStatus {
	uint32_t	packets		= 0;	// valid packets
	uint32_t	badPackets	= 0;	// start marker but damaged
	uint32_t	unsupported	= 0;	// FEC / extension packets: not handled
	uint32_t	resources	= 0;	// stored now
	uint32_t	resourcesTotal	= 0;
	uint32_t	duplicates	= 0;
	uint32_t	partial		= 0;
	uint32_t	serviceType	= 0;	// of the last packet
};

class DataBroadcastDecoder {
public:
		DataBroadcastDecoder	();
	void	clear		();
	void	handleGroup	(int stream, const DataGroup &);
	uint32_t changeCounter	() const;
	DbStatus status		() const;
	std::vector<DbInfo> list	() const;
	bool	getResource	(uint32_t seq, DbResource &out) const;
	bool	hasContent	() const;
	bool	hasImage	() const;		// a JPEG, PNG or GIF file
	std::vector<DbProgress> progress	() const;
private:
	struct Part {				// one file or description file
	   int		serviceType, resourceId, updateNo, packetType, count;
	   time_t	touched;
	   std::map<int, std::vector<uint8_t>> pieces;
	};
	mutable std::mutex	lock;
	std::deque<Part>	parts;
	std::deque<DbResource>	store;
	std::map<uint32_t, uint64_t>	hash;
	DbStatus		st;
	uint32_t		changes;
	uint32_t		nextSeq;
	size_t			storeBytes;
	void	partDone	(const Part &, const std::vector<uint8_t> &data);
	void	settle		(DbResource &);
};

}	// namespace dxdata
#endif
