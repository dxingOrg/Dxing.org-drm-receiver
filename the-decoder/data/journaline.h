#
/*
 *    Dxing.org DRM receiver - Journaline decoder (ETSI TS 102 979)
 *    GNU General Public License, version 2 or later.
 *
 *    Pure C++ (no Qt). Written from the published specification
 *    ETSI TS 102 979 V1.1.1: clause 5 (JML object format), clause 6.2
 *    (TOC management data) and clause 8 (transport as MSC data groups,
 *    in DRM packet mode). It does not use any third party decoder code.
 *
 *    A Journaline object arrives as one MSC data group (type 0). A TOC
 *    block (the list of all objects of the service) arrives as a data
 *    group of type 6. Both must have a CRC, no segmentation and no
 *    transport id; anything else is discarded, as the specification
 *    demands.
 */
#ifndef	__JOURNALINE__
#define	__JOURNALINE__

#include	<stdint.h>
#include	<stddef.h>
#include	<vector>
#include	<string>
#include	<map>
#include	<mutex>
#include	"data-hub.h"

namespace dxdata {

//	raw DEFLATE (RFC 1951, no zlib header). false on any error or when
//	the result would exceed maxOut bytes
bool	inflateRaw	(const uint8_t *in, size_t inLen,
	                 std::vector<uint8_t> &out, size_t maxOut);

enum {
	JML_MENU	= 1,
	JML_PLAIN	= 2,
	JML_TITLE	= 3,
	JML_LIST	= 4
};

struct JmlLink {
	uint16_t	target	= 0;
	std::string	text;
};

//	Text fields are UTF-8. Escape sequence 0x12/0x13 (highlight) are kept
//	as the control bytes 0x02 / 0x03 inside the text, line breaks as '\n'.
struct JmlObject {
	uint16_t	id		= 0;
	uint8_t		type		= 0;
	bool		isStatic	= false;
	bool		compressed	= false;
	uint8_t		revision	= 0;
	size_t		rawSize		= 0;
	std::string	title;
	std::string	body;
	std::vector<JmlLink>			links;	// menu
	std::vector<std::vector<std::string>>	rows;	// list message
	bool	sameContent	(const JmlObject &o) const;
};

//	extHeader: length of the extended header (from the application
//	signalling; 0 in every current service)
bool	parseJmlObject	(const uint8_t *p, size_t n, size_t extHeader,
	                         JmlObject &out);

struct JournalineStatus {
	uint32_t	objects		= 0;	// objects in the cache
	uint32_t	objectsSeen	= 0;	// valid objects received (with repeats)
	uint32_t	objectsRejected	= 0;	// valid group, unusable object
	uint32_t	groupsRejected	= 0;	// not a valid Journaline data group
	uint32_t	tocBlocks	= 0;
	uint32_t	tocObjectCount	= 0;	// objects the TOC announces
	uint32_t	tocListed	= 0;	// of them described by TOC blocks so far
	uint32_t	tocReceived	= 0;	// TOC-listed objects present in the cache
	int		tocRevision	= -1;
};

class JournalineDecoder {
public:
	static const int kAppId	= 0x44A;
		JournalineDecoder	();
	void	clear			();
	void	setExtendedHeaderLength	(size_t n);
//	called for every data group of every stream; takes only the
//	Journaline ones. Safe to call from the decoder thread.
	void	handleGroup		(int stream, const DataGroup &);
//	safe to call from another thread
	bool	getObject		(uint16_t id, JmlObject &out) const;
	bool	hasObject		(uint16_t id) const;
	uint32_t changeCounter		() const;
	bool	hasContent		() const;
	JournalineStatus status		() const;
private:
	mutable std::mutex	lock;
	std::map<uint16_t, JmlObject>	cache;
	std::map<uint16_t, uint64_t>	signature;	// of the raw object
	std::map<uint16_t, uint8_t>	toc;
	JournalineStatus	st;
	uint32_t		changes;
	int			activeStream;
	size_t			extHeader;
	void	handleToc	(const std::vector<uint8_t> &);
};

}	// namespace dxdata
#endif
