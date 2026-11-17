#
/*
 *    Dxing.org DRM receiver - Journaline decoder (ETSI TS 102 979)
 *    GNU General Public License, version 2 or later.
 */
#include	"journaline.h"
#include	<string.h>

namespace dxdata {

/*
 * ---- raw DEFLATE (RFC 1951) -------------------------------------
 *      A small canonical-Huffman inflater written for this program.
 */
namespace {

struct InState {
	const uint8_t	*in;
	size_t		inLen;
	size_t		inPos;
	uint32_t	bitBuf;
	int		bitCnt;
	std::vector<uint8_t>	*out;
	size_t		maxOut;
	bool		failed;
};

int	getBits (InState &s, int need) {
uint32_t val	= s. bitBuf;
	while (s. bitCnt < need) {
	   if (s. inPos >= s. inLen) {
	      s. failed	= true;
	      return 0;
	   }
	   val		|= (uint32_t)s. in [s. inPos ++] << s. bitCnt;
	   s. bitCnt	+= 8;
	}
	s. bitBuf	= val >> need;
	s. bitCnt	-= need;
	return (int)(val & ((1u << need) - 1));
}

struct Huffman {
	uint16_t	count [16];
	uint16_t	symbol [320];
};

//	0: complete code, >0: incomplete, <0: over-subscribed
int	buildHuffman (Huffman &h, const uint16_t *length, int n) {
	for (int len = 0; len <= 15; len ++)
	   h. count [len] = 0;
	for (int sym = 0; sym < n; sym ++)
	   h. count [length [sym]] ++;
	if (h. count [0] == n)
	   return 0;
int	left	= 1;
	for (int len = 1; len <= 15; len ++) {
	   left	<<= 1;
	   left	-= h. count [len];
	   if (left < 0)
	      return left;
	}
uint16_t offs [16];
	offs [1]	= 0;
	for (int len = 1; len < 15; len ++)
	   offs [len + 1]	= offs [len] + h. count [len];
	for (int sym = 0; sym < n; sym ++)
	   if (length [sym] != 0)
	      h. symbol [offs [length [sym]] ++] = (uint16_t)sym;
	return left;
}

int	decodeSymbol (InState &s, const Huffman &h) {
int code	= 0;
int first	= 0;
int index	= 0;
	for (int len = 1; len <= 15; len ++) {
	   code	|= getBits (s, 1);
	   if (s. failed)
	      return -1;
	   int count	= h. count [len];
	   if (code - count < first)
	      return h. symbol [index + (code - first)];
	   index	+= count;
	   first	+= count;
	   first	<<= 1;
	   code		<<= 1;
	}
	return -1;
}

const uint16_t lenBase [29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19,
	   23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const uint16_t lenExtra [29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
	   2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const uint16_t distBase [30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65,
	   97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
	   8193, 12289, 16385, 24577};
const uint16_t distExtra [30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5,
	   6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

bool	inflateCodes (InState &s, const Huffman &lit, const Huffman &dist) {
std::vector<uint8_t> &out	= *s. out;
	for ( ; ; ) {
	   int sym	= decodeSymbol (s, lit);
	   if (sym < 0)
	      return false;
	   if (sym < 256) {
	      if (out. size () >= s. maxOut)
	         return false;
	      out. push_back ((uint8_t)sym);
	      continue;
	   }
	   if (sym == 256)
	      return true;
	   sym	-= 257;
	   if (sym >= 29)
	      return false;
	   int len	= lenBase [sym] + getBits (s, lenExtra [sym]);
	   int ds	= decodeSymbol (s, dist);
	   if (ds < 0 || ds >= 30 || s. failed)
	      return false;
	   size_t d	= distBase [ds] + getBits (s, distExtra [ds]);
	   if (s. failed || d > out. size () || out. size () + len > s. maxOut)
	      return false;
	   for (int i = 0; i < len; i ++)
	      out. push_back (out [out. size () - d]);
	}
}

bool	inflateStored (InState &s) {
	s. bitBuf	= 0;
	s. bitCnt	= 0;
	if (s. inPos + 4 > s. inLen)
	   return false;
unsigned len	= s. in [s. inPos] | (s. in [s. inPos + 1] << 8);
unsigned nlen	= s. in [s. inPos + 2] | (s. in [s. inPos + 3] << 8);
	s. inPos	+= 4;
	if (len != (~nlen & 0xFFFF))
	   return false;
	if (s. inPos + len > s. inLen || s. out -> size () + len > s. maxOut)
	   return false;
	s. out -> insert (s. out -> end (), s. in + s. inPos, s. in + s. inPos + len);
	s. inPos	+= len;
	return true;
}

bool	inflateFixed (InState &s) {
static bool	ready = false;
static Huffman	lit, dist;
	if (!ready) {
	   uint16_t l [288];
	   int i;
	   for (i = 0; i < 144; i ++) l [i] = 8;
	   for ( ; i < 256; i ++) l [i] = 9;
	   for ( ; i < 280; i ++) l [i] = 7;
	   for ( ; i < 288; i ++) l [i] = 8;
	   buildHuffman (lit, l, 288);
	   for (i = 0; i < 30; i ++) l [i] = 5;
	   buildHuffman (dist, l, 30);
	   ready	= true;
	}
	return inflateCodes (s, lit, dist);
}

bool	inflateDynamic (InState &s) {
static const uint8_t order [19] =
	   {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
uint16_t lengths [320];
int nlen	= getBits (s, 5) + 257;
int ndist	= getBits (s, 5) + 1;
int ncode	= getBits (s, 4) + 4;
	if (s. failed || nlen > 286 || ndist > 30)
	   return false;
int idx;
	for (idx = 0; idx < ncode; idx ++)
	   lengths [order [idx]]	= (uint16_t)getBits (s, 3);
	for ( ; idx < 19; idx ++)
	   lengths [order [idx]]	= 0;
Huffman	lencode;
	if (buildHuffman (lencode, lengths, 19) != 0)
	   return false;
	idx	= 0;
	while (idx < nlen + ndist) {
	   int sym	= decodeSymbol (s, lencode);
	   if (sym < 0)
	      return false;
	   if (sym < 16)
	      lengths [idx ++]	= (uint16_t)sym;
	   else {
	      int len	= 0;
	      int rep;
	      if (sym == 16) {
	         if (idx == 0)
	            return false;
	         len	= lengths [idx - 1];
	         rep	= 3 + getBits (s, 2);
	      }
	      else
	      if (sym == 17)
	         rep	= 3 + getBits (s, 3);
	      else
	         rep	= 11 + getBits (s, 7);
	      if (s. failed || idx + rep > nlen + ndist)
	         return false;
	      while (rep --)
	         lengths [idx ++]	= (uint16_t)len;
	   }
	}
	if (lengths [256] == 0)
	   return false;
Huffman	lit, dist;
int err	= buildHuffman (lit, lengths, nlen);
	if (err < 0 || (err > 0 && nlen - lit. count [0] != 1))
	   return false;
	err	= buildHuffman (dist, lengths + nlen, ndist);
	if (err < 0 || (err > 0 && ndist - dist. count [0] != 1))
	   return false;
	return inflateCodes (s, lit, dist);
}

}	// anonymous namespace

bool	inflateRaw (const uint8_t *in, size_t inLen,
	              std::vector<uint8_t> &out, size_t maxOut) {
InState s;
	s. in		= in;
	s. inLen	= inLen;
	s. inPos	= 0;
	s. bitBuf	= 0;
	s. bitCnt	= 0;
	s. out		= &out;
	s. maxOut	= maxOut;
	s. failed	= false;
	out. clear ();
int	last;
	do {
	   last		= getBits (s, 1);
	   int type	= getBits (s, 2);
	   if (s. failed)
	      return false;
	   bool ok;
	   switch (type) {
	      case 0:	ok = inflateStored (s);	break;
	      case 1:	ok = inflateFixed (s);	break;
	      case 2:	ok = inflateDynamic (s);	break;
	      default:	return false;
	   }
	   if (!ok || s. failed)
	      return false;
	} while (!last);
	return true;
}

/*
 * ---- JML objects (clause 5) -------------------------------------
 */
bool	JmlObject::sameContent (const JmlObject &o) const {
	return id == o. id && type == o. type && revision == o. revision &&
	       title == o. title && body == o. body &&
	       links. size () == o. links. size () &&
	       rows == o. rows &&
	       [&] () {
	          for (size_t i = 0; i < links. size (); i ++)
	             if (links [i]. target != o. links [i]. target ||
	                 links [i]. text != o. links [i]. text)
	                return false;
	          return true;
	       } ();
}

bool	parseJmlObject (const uint8_t *p, size_t n, size_t extHeader,
	                  JmlObject &out) {
	out	= JmlObject ();
	if (n < 3)
	   return false;
	out. id		= (uint16_t)((p [0] << 8) | p [1]);
uint8_t	desc	= p [2];
	out. type	= desc >> 5;
	out. isStatic	= (desc >> 4) & 1;
	out. compressed	= (desc >> 3) & 1;
	out. revision	= desc & 7;
	out. rawSize	= n;
	if (out. type < JML_MENU || out. type > JML_LIST)
	   return false;			// unknown type: ignored
size_t	pos	= 3 + extHeader;
	if (pos > n)
	   return false;

std::vector<uint8_t> content;
	if (out. compressed) {
	   if (pos >= n)
	      return false;
	   if (p [pos ++] != 0x08)		// only deflate is defined
	      return false;
	   if (!inflateRaw (p + pos, n - pos, content, 16384))
	      return false;
	}
	else
	   content. assign (p + pos, p + n);

//	the content section is a sequence of blocks started by JML codes
//	(0x00 .. 0x0F) with escape sequences (0x10 .. 0x1F) inside the text
std::string *cur	= nullptr;
bool	inItem	= false;
size_t	n2	= content. size ();
	for (size_t i = 0; i < n2; i ++) {
	   uint8_t b	= content [i];
	   if (b >= 0x20) {
	      if (cur != nullptr)
	         cur -> push_back ((char)b);
	      continue;
	   }
	   switch (b) {
	      case 0x00:			// end of block
	         cur	= nullptr;
	         inItem	= false;
	         break;
	      case 0x01:			// title
	         cur	= &out. title;
	         inItem	= false;
	         break;
	      case 0x02: {			// link item: object id + text
	         if (i + 2 >= n2)
	            return out. type == JML_MENU && !out. links. empty ();
	         JmlLink l;
	         l. target	= (uint16_t)((content [i + 1] << 8) | content [i + 2]);
	         i	+= 2;
	         out. links. push_back (l);
	         cur	= &out. links. back (). text;
	         inItem	= false;
	         break;
	      }
	      case 0x03:			// body text
	         cur	= &out. body;
	         inItem	= false;
	         break;
	      case 0x04:			// list item
	         out. rows. push_back (std::vector<std::string> (1));
	         cur	= &out. rows. back (). back ();
	         inItem	= true;
	         break;
	      case 0x05:			// next column of the list item
	         if (inItem) {
	            out. rows. back (). push_back (std::string ());
	            cur	= &out. rows. back (). back ();
	         }
	         else
	         if (cur != nullptr)
	            cur -> push_back (' ');
	         break;
	      case 0x10:			// preferred line break
	         if (cur != nullptr)
	            cur -> push_back ('\n');
	         break;
	      case 0x12:			// highlight start
	         if (cur != nullptr)
	            cur -> push_back (0x02);
	         break;
	      case 0x13:			// highlight stop
	         if (cur != nullptr)
	            cur -> push_back (0x03);
	         break;
	      case 0x14:			// end of introductory section
	         if (cur != nullptr)
	            cur -> push_back ('\n');
	         break;
	      case 0x1A:			// data section start / continuation:
	      case 0x1B: {			// not shown, length byte = size - 1
	         if (i + 1 >= n2)
	            i	= n2;
	         else
	            i	+= 1 + ((size_t)content [i + 1] + 1);
	         break;
	      }
	      case 0x1C:			// extended codes: one more byte
	      case 0x1D:
	         i	+= 1;
	         break;
	      default:			// 0x06-0x0F unknown JML code, 0x11 word
	         break;			// break, other escapes: ignored
	   }
	}
	if (out. title. empty () && out. type != JML_LIST)
	   return false;			// a title block is mandatory
	return true;
}

/*
 * ---- the decoder ---------------------------------------------------
 */
namespace {
uint64_t	fnv64 (const uint8_t *p, size_t n) {
uint64_t h	= 1469598103934665603ULL;
	for (size_t i = 0; i < n; i ++) {
	   h	^= p [i];
	   h	*= 1099511628211ULL;
	}
	return h;
}
const size_t	maxObjects	= 6000;
}

	JournalineDecoder::JournalineDecoder () {
	changes		= 0;
	activeStream	= -1;
	extHeader	= 0;
}

void	JournalineDecoder::clear () {
std::lock_guard<std::mutex> g (lock);
	cache. clear ();
	signature. clear ();
	toc. clear ();
	st		= JournalineStatus ();
	activeStream	= -1;
	changes ++;
}

void	JournalineDecoder::setExtendedHeaderLength (size_t n) {
std::lock_guard<std::mutex> g (lock);
	extHeader	= n;
}

void	JournalineDecoder::handleGroup (int stream, const DataGroup &gr) {
	if (gr. appId != kAppId)
	   return;
std::lock_guard<std::mutex> g (lock);
//	the specification fixes the data group header: CRC present, no
//	segmentation, no user access (transport id), type 0 or 6
	if (!gr. crcPresent || !gr. crcOk || gr. hasSegment ||
	    gr. hasTransportId || (gr. type != 0 && gr. type != 6) ||
	    gr. payload. size () > 4092) {
	   st. groupsRejected ++;
	   return;
	}
	if (activeStream >= 0 && activeStream != stream) {
	   st. groupsRejected ++;		// a second Journaline stream: ignored
	   return;
	}
	activeStream	= stream;
	if (gr. type == 6) {
	   handleToc (gr. payload);
	   return;
	}
JmlObject o;
	if (!parseJmlObject (gr. payload. data (), gr. payload. size (),
	                     extHeader, o)) {
	   st. objectsRejected ++;
	   return;
	}
	st. objectsSeen ++;
uint64_t sig	= fnv64 (gr. payload. data (), gr. payload. size ());
auto it	= signature. find (o. id);
	if (it != signature. end () && it -> second == sig)
	   return;				// the carousel repeats; nothing new
	if (it == signature. end () && cache. size () >= maxObjects) {
	   st. objectsRejected ++;
	   return;
	}
	cache [o. id]		= o;
	signature [o. id]	= sig;
	st. objects		= (uint32_t)cache. size ();
	changes ++;
}

//	clause 6.2.2: 13 header bytes (type, revision, block count, block
//	index, preceding id, object count, timeout, entity length, extended
//	header length), then entities of "entity length" bytes: id + description
void	JournalineDecoder::handleToc (const std::vector<uint8_t> &d) {
	if (d. size () < 13 || d [0] != 0x54) {
	   st. groupsRejected ++;
	   return;
	}
int	revision	= d [1];
unsigned objectCount	= (d [6] << 8) | d [7];
unsigned entityLen	= d [10];
unsigned extLen		= (d [11] << 8) | d [12];
size_t	pos		= 13 + (size_t)extLen;
	if (entityLen < 3 || pos > d. size ()) {
	   st. groupsRejected ++;
	   return;
	}
	st. tocBlocks ++;
	if (revision != st. tocRevision) {
	   toc. clear ();
	   st. tocRevision	= revision;
	}
	st. tocObjectCount	= objectCount;
	for ( ; pos + entityLen <= d. size (); pos += entityLen) {
	   uint16_t id	= (uint16_t)((d [pos] << 8) | d [pos + 1]);
	   toc [id]	= d [pos + 2];
	}
	st. tocListed	= (uint32_t)toc. size ();
	changes ++;
}

bool	JournalineDecoder::getObject (uint16_t id, JmlObject &out) const {
std::lock_guard<std::mutex> g (lock);
auto it	= cache. find (id);
	if (it == cache. end ())
	   return false;
	out	= it -> second;
	return true;
}

bool	JournalineDecoder::hasObject (uint16_t id) const {
std::lock_guard<std::mutex> g (lock);
	return cache. find (id) != cache. end ();
}

uint32_t JournalineDecoder::changeCounter () const {
std::lock_guard<std::mutex> g (lock);
	return changes;
}

bool	JournalineDecoder::hasContent () const {
std::lock_guard<std::mutex> g (lock);
	return !cache. empty ();
}

JournalineStatus JournalineDecoder::status () const {
std::lock_guard<std::mutex> g (lock);
JournalineStatus r	= st;
	r. tocReceived	= 0;
	for (const auto &e : toc)
	   if (cache. find (e. first) != cache. end ())
	      r. tocReceived ++;
	return r;
}

}	// namespace dxdata
