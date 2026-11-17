#
/*
 *    Dxing.org DRM receiver - MOT decoder (header mode) and SlideShow
 *    GNU General Public License, version 2 or later.
 *
 *    Pure C++ (no Qt). Written from ETSI EN 301 234 V2.1.1 (MOT),
 *    ETSI TS 101 968 (MOT over DRM: one MSC data group = one DRM data
 *    unit) and ETSI TS 101 499 V3.2.1 (SlideShow). No third party code.
 *
 *    Implemented: MOT header mode (data group types 3 and 4), which is
 *    the mode SlideShow must use. Not implemented: MOT directory mode
 *    (types 6/7, used by e.g. the Broadcast Web Site), scrambled bodies
 *    and compressed bodies. Such groups are only counted.
 */
#ifndef	__MOT_DECODER__
#define	__MOT_DECODER__

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

//	time parameter (EN 301 234, 6.2.4.1): "now" when the validity flag is
//	0, else seconds since 1970 (UTC). false: malformed
bool	decodeMotTime	(const uint8_t *d, size_t n, bool &now, int64_t &unixTime);

//	make a file name that is safe to create on Windows and Linux. Images
//	always get the extension that matches their data (forceExt); other
//	files keep a harmless extension, anything else gets ".dat" appended
std::string	sanitizeFileName	(const std::string &name,
	                                 const std::string &defExt,
	                                 bool forceExt);

struct MotParams {
	bool		hasName		= false;
	std::string	name;			// ContentName, as UTF-8
	bool		hasMime		= false;
	std::string	mime;
	bool		compressed	= false;	// CompressionType present
	bool		scrambled	= false;	// CAInfo present
	bool		hasTrigger	= false;
	bool		triggerNow	= false;
	int64_t		triggerUnix	= 0;
	bool		hasExpire	= false;
	int64_t		expireUnix	= 0;
	bool		hasCategory	= false;
	uint8_t		categoryId	= 0;
	uint8_t		slideId		= 0;
	std::string	categoryTitle;
	std::string	clickUrl;		// shown, never opened by itself
	uint8_t		alert		= 0;
};

struct MotHeader {
	uint32_t	bodySize	= 0;
	uint16_t	headerSize	= 0;
	uint8_t		contentType	= 0;
	uint16_t	contentSubType	= 0;
	MotParams	p;
};

//	n must be the size of the whole reassembled header (core + extension)
bool	parseMotHeader	(const uint8_t *d, size_t n, MotHeader &out);

struct MotObject {
	enum Kind { Other = 0, Jpeg = 1, Png = 2 };
	uint32_t	seq		= 0;
	uint16_t	transportId	= 0;
	int		appId		= 0;
	int		stream		= 0;
	time_t		received	= 0;
	MotHeader	hdr;
	Kind		kind		= Other;
	std::vector<uint8_t> body;
	bool		isImage	() const { return kind != Other; }
	std::string	suggestedFileName	() const;
};

//	what the window needs to list an object (no body copied)
struct MotInfo {
	uint32_t	seq;
	int		appId;
	time_t		received;
	MotObject::Kind	kind;
	size_t		size;
	std::string	name;
	bool		hasTrigger;
	bool		triggerNow;
	int64_t		triggerUnix;
	bool		hasCategory;
	uint8_t		categoryId;
	uint8_t		slideId;
	std::string	categoryTitle;
	std::string	clickUrl;
	bool		compressed;
};

struct MotStatus {
	uint32_t	headers		= 0;	// MOT headers accepted
	uint32_t	bodySegments	= 0;
	uint32_t	objects		= 0;	// stored now
	uint32_t	objectsTotal	= 0;	// ever completed
	uint32_t	duplicates	= 0;	// repeats of an object already held
	uint32_t	badSegments	= 0;
	uint32_t	sizeMismatch	= 0;
	uint32_t	tooBig		= 0;
	uint32_t	headerUpdates	= 0;
	uint32_t	directoryGroups	= 0;	// MOT directory segments (type 6)
	uint32_t	directories	= 0;	// complete directories understood
	uint32_t	directoryObjects = 0;	// objects the last directory lists
	uint32_t	directoryCompressed = 0;	// type 7: not supported
	uint32_t	directoryBad	= 0;
	uint32_t	scrambledGroups	= 0;
	uint32_t	images		= 0;
	uint32_t	files		= 0;
	uint32_t	partial		= 0;	// objects being reassembled
};

class MotDecoder {
public:
		MotDecoder	();
	void	clear		();
	void	handleGroup	(int stream, const DataGroup &);
	uint32_t changeCounter	() const;
	MotStatus status	() const;
	std::vector<MotInfo> list	() const;
	bool	getObject	(uint32_t seq, MotObject &out) const;
	bool	hasImages	() const;
	bool	hasFiles	() const;
private:
	struct Partial {
	   uint16_t	tid;
	   int		stream;
	   int		appId;
	   time_t	touched;
	   std::map<uint16_t, std::vector<uint8_t>> hdr, body;
	   int		hdrLast, bodyLast;
	   size_t	bytes;
	   bool		haveHeader;
	   MotHeader	h;
	   std::vector<uint8_t> hdrBytes;
	};
	struct DirEntry {
	   MotHeader	h;
	   std::vector<uint8_t> hdrBytes;
	};
	struct DirPartial {
	   bool		active	= false;
	   uint16_t	tid	= 0;
	   int		stream	= 0;
	   int		last	= -1;
	   size_t	bytes	= 0;
	   std::map<uint16_t, std::vector<uint8_t>> segs;
	};
	mutable std::mutex	lock;
	std::map<uint16_t, DirEntry>	dirEntries;	// from the MOT directory
	DirPartial		dirPartial;
	std::deque<Partial>	partials;
	std::deque<MotObject>	store;
	std::map<uint32_t, uint64_t>	hash;	// seq -> hash of the object
	std::map<std::string, MotParams> pending;	// updates before the object
	MotStatus		st;
	uint32_t		changes;
	uint32_t		nextSeq;
	size_t			storeBytes;
	size_t	findPartial	(int stream, uint16_t tid, int appId);
	void	tryComplete	(size_t idx);
	void	applyUpdate	(const MotParams &);
	void	handleDirectory	(int stream, const DataGroup &,
	                         const std::vector<uint8_t> &seg);
	bool	parseDirectory	(const std::vector<uint8_t> &);
	void	storeObject	(const Partial &, std::vector<uint8_t> &body);
};

}	// namespace dxdata
#endif
