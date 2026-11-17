#
/*
 *    Dxing.org DRM receiver - MOT decoder (header mode) and SlideShow
 *    GNU General Public License, version 2 or later.
 */
#include	"mot.h"
#include	<string.h>
#include	<ctype.h>
#include	<algorithm>

namespace dxdata {

namespace {
const size_t	maxObjectBytes	= 4 * 1024 * 1024;
const size_t	maxStoreBytes	= 16 * 1024 * 1024;
const size_t	maxStoreObjects	= 64;
const size_t	maxPartials	= 4;
const size_t	maxPending	= 32;
const int	maxSegments	= 4096;
const uint8_t	pngMagic [8]	= {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

uint64_t	fnv64 (const uint8_t *p, size_t n,
	               uint64_t h = 1469598103934665603ULL) {
	for (size_t i = 0; i < n; i ++) {
	   h	^= p [i];
	   h	*= 1099511628211ULL;
	}
	return h;
}

void	appendUtf8 (std::string &s, uint32_t c) {
	if (c < 0x80)
	   s	+= (char)c;
	else
	if (c < 0x800) {
	   s	+= (char)(0xC0 | (c >> 6));
	   s	+= (char)(0x80 | (c & 0x3F));
	}
	else {
	   s	+= (char)(0xE0 | (c >> 12));
	   s	+= (char)(0x80 | ((c >> 6) & 0x3F));
	   s	+= (char)(0x80 | (c & 0x3F));
	}
}

//	control characters would only upset the display
std::string	cleanText (const std::string &s) {
std::string r;
	for (unsigned char c : s)
	   r	+= (c < 0x20 || c == 0x7F) ? ' ' : (char)c;
	return r;
}

//	ContentName: a character set indicator (high nibble) and the text.
//	0xF is UTF-8, 0x6 is UCS-2; everything else (EBU Latin, ISO Latin-1)
//	is taken as Latin-1, which is right for the ASCII part
std::string	nameField (const uint8_t *d, size_t n) {
std::string r;
	if (n < 1)
	   return r;
int	cs	= d [0] >> 4;
	if (cs == 0x0F)
	   r	= std::string ((const char *)d + 1, n - 1);
	else
	if (cs == 0x06) {
	   for (size_t i = 1; i + 1 < n; i += 2)
	      appendUtf8 (r, (uint32_t)((d [i] << 8) | d [i + 1]));
	}
	else
	   for (size_t i = 1; i < n; i ++)
	      appendUtf8 (r, d [i]);
	return cleanText (r);
}
}

bool	decodeMotTime (const uint8_t *d, size_t n, bool &now, int64_t &unixTime) {
	now	= false;
	unixTime = 0;
	if (n < 4)
	   return false;
uint32_t v	= ((uint32_t)d [0] << 24) | (d [1] << 16) | (d [2] << 8) | d [3];
	if (((v >> 31) & 1) == 0) {			// validity flag 0: "now"
	   now	= true;
	   return true;
	}
int64_t mjd	= (v >> 14) & 0x1FFFF;
bool	longForm = (v >> 11) & 1;
int	hours	= (v >> 6) & 0x1F;
int	minutes	= v & 0x3F;
int	seconds	= 0;
	if (longForm) {
	   if (n < 6)
	      return false;
	   seconds	= (((d [4] << 8) | d [5]) >> 10) & 0x3F;
	}
	if (hours > 23 || minutes > 59 || seconds > 59)
	   return false;
	unixTime	= (mjd - 40587) * 86400 + hours * 3600 + minutes * 60 + seconds;
	return true;
}

std::string	sanitizeFileName (const std::string &name,
	                          const std::string &defExt, bool forceExt) {
std::string s	= name;
size_t	sl	= s. find_last_of ("/\\");
	if (sl != std::string::npos)
	   s	= s. substr (sl + 1);
std::string out;
	for (unsigned char c : s)
	   out	+= (isascii (c) && (isalnum (c) || c == '.' || c == '_' || c == '-'))
	           ? (char)c : '_';
size_t	i	= 0;
	while (i < out. size () && out [i] == '.')
	   i ++;
	out	= out. substr (i);
	if (out. size () > 80)
	   out. resize (80);
std::string base	= out;
std::string ext;
size_t	dot	= out. rfind ('.');
	if (dot != std::string::npos) {
	   base	= out. substr (0, dot);
	   ext	= out. substr (dot + 1);
	}
	for (char &c : ext)
	   c	= (char)tolower ((unsigned char)c);
	if (base. empty ())
	   base	= "object";
std::string lb	= base;
	for (char &c : lb)
	   c	= (char)tolower ((unsigned char)c);
static const char *reserved [] = {"con", "prn", "aux", "nul", "com1", "com2",
	   "com3", "com4", "com5", "com6", "com7", "com8", "com9", "lpt1", "lpt2",
	   "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"};
	for (const char *r : reserved)
	   if (lb == r)
	      base	= "_" + base;
	if (forceExt && !defExt. empty ())
	   ext	= defExt;
	else {
static const char *safe [] = {"txt", "htm", "html", "xml", "json", "csv",
	   "log", "pdf", "rtf", "jpg", "jpeg", "png", "gif", "bmp"};
	   bool ok	= false;
	   for (const char *e : safe)
	      if (ext == e)
	         ok	= true;
	   if (ext. empty ())
	      ext	= defExt. empty () ? "dat" : defExt;
	   else
	   if (!ok) {			// an executable or unknown type: keep the
	      base	+= "." + ext;	// name visible but make it harmless
	      ext	= "dat";
	   }
	}
	return base + "." + ext;
}

/*
 *	MOT header: header core (7 bytes) + header extension (parameters)
 */
bool	parseMotHeader (const uint8_t *d, size_t n, MotHeader &out) {
	out	= MotHeader ();
	if (n < 7)
	   return false;
uint64_t core	= 0;
	for (int i = 0; i < 7; i ++)
	   core	= (core << 8) | d [i];
	out. bodySize	= (uint32_t)((core >> 28) & 0x0FFFFFFF);
	out. headerSize	= (uint16_t)((core >> 15) & 0x1FFF);
	out. contentType = (uint8_t)((core >> 9) & 0x3F);
	out. contentSubType = (uint16_t)(core & 0x1FF);
	if (out. headerSize < 7 || out. headerSize != n)
	   return false;
size_t	pos	= 7;
	while (pos < n) {
	   uint8_t b	= d [pos ++];
	   int pli	= b >> 6;
	   int id	= b & 0x3F;
	   size_t len	= 0;
	   if (pli == 1)
	      len	= 1;
	   else
	   if (pli == 2)
	      len	= 4;
	   else
	   if (pli == 3) {
	      if (pos >= n)
	         break;
	      uint8_t l	= d [pos ++];
	      if (l & 0x80) {
	         if (pos >= n)
	            break;
	         len	= ((l & 0x7F) << 8) | d [pos ++];
	      }
	      else
	         len	= l & 0x7F;
	   }
	   if (pos + len > n)
	      break;
	   const uint8_t *f	= d + pos;
	   pos	+= len;
	   MotParams &p	= out. p;
	   switch (id) {
	      case 0x0C:			// ContentName
	         p. hasName	= len >= 1;
	         p. name	= nameField (f, len);
	         break;
	      case 0x10:			// MimeType
	         p. hasMime	= true;
	         p. mime	= cleanText (std::string ((const char *)f, len));
	         break;
	      case 0x11:			// CompressionType
	         p. compressed	= true;
	         break;
	      case 0x23:			// CAInfo
	         p. scrambled	= true;
	         break;
	      case 0x05:			// SlideShow: TriggerTime
	         if (decodeMotTime (f, len, p. triggerNow, p. triggerUnix))
	            p. hasTrigger	= true;
	         break;
	      case 0x04:			// SlideShow: ExpireTime
	         {
	            bool now;
	            if (decodeMotTime (f, len, now, p. expireUnix) && !now)
	               p. hasExpire	= true;
	         }
	         break;
	      case 0x25:			// SlideShow: CategoryID / SlideID
	         if (len >= 2) {
	            p. categoryId	= f [0];
	            p. slideId	= f [1];
	            p. hasCategory	= (f [0] != 0 || f [1] != 0);
	         }
	         break;
	      case 0x26:			// SlideShow: CategoryTitle
	         p. categoryTitle	= cleanText (std::string ((const char *)f,
	                                         std::min<size_t> (len, 128)));
	         break;
	      case 0x27:			// SlideShow: ClickThroughURL
	         p. clickUrl	= cleanText (std::string ((const char *)f,
	                                         std::min<size_t> (len, 512)));
	         break;
	      case 0x29:			// SlideShow: Alert
	         if (len >= 1)
	            p. alert	= f [0];
	         break;
	      default:			// unknown parameters are ignored
	         break;
	   }
	}
	return true;
}

std::string	MotObject::suggestedFileName () const {
std::string name	= hdr. p. hasName ? hdr. p. name : std::string ();
	if (kind == Jpeg)
	   return sanitizeFileName (name. empty () ? "slide" : name, "jpg", true);
	if (kind == Png)
	   return sanitizeFileName (name. empty () ? "slide" : name, "png", true);
	return sanitizeFileName (name. empty () ? "object" : name, "dat", false);
}

/*
 *	the decoder
 */
	MotDecoder::MotDecoder () {
	changes		= 0;
	nextSeq		= 1;
	storeBytes	= 0;
}

void	MotDecoder::clear () {
std::lock_guard<std::mutex> g (lock);
	partials. clear ();
	dirEntries. clear ();
	dirPartial	= DirPartial ();
	store. clear ();
	hash. clear ();
	pending. clear ();
	st		= MotStatus ();
	storeBytes	= 0;
	changes ++;
}

size_t	MotDecoder::findPartial (int stream, uint16_t tid, int appId) {
	for (size_t i = 0; i < partials. size (); i ++)
	   if (partials [i]. tid == tid && partials [i]. stream == stream &&
	       partials [i]. appId == appId)
	      return i;
Partial p;
	p. tid		= tid;
	p. stream	= stream;
	p. appId	= appId;
	p. touched	= time (nullptr);
	p. hdrLast	= -1;
	p. bodyLast	= -1;
	p. bytes	= 0;
	p. haveHeader	= false;
	partials. push_back (p);
	if (partials. size () > maxPartials) {		// drop the stalest one
	   size_t victim	= 0;
	   for (size_t i = 1; i + 1 < partials. size (); i ++)
	      if (partials [i]. touched < partials [victim]. touched)
	         victim	= i;
	   partials. erase (partials. begin () + victim);
	}
	return partials. size () - 1;
}

void	MotDecoder::handleGroup (int stream, const DataGroup &g) {
	if (g. domain != 1 || g. appId == 0x44A)	// DAB domain; not Journaline
	   return;
std::lock_guard<std::mutex> lk (lock);
	if (g. type == 1 || g. type == 5) {
	   st. scrambledGroups ++;
	   return;
	}
	if (g. type != 3 && g. type != 4 && g. type != 6 && g. type != 7)
	   return;
	if (g. crcPresent && !g. crcOk)
	   return;
//	the session header (segment number, TransportId) is not optional
	if (!g. hasSegment || !g. hasTransportId || g. payload. size () < 2) {
	   st. badSegments ++;
	   return;
	}
//	segmentation header: 3 bits repetition count, 13 bits segment size
size_t	segSize	= ((g. payload [0] & 0x1F) << 8) | g. payload [1];
	if (g. payload. size () != 2 + segSize || g. segmentNumber >= maxSegments) {
	   st. badSegments ++;
	   return;
	}
	if (g. type == 7) {				// compressed directory
	   st. directoryGroups ++;
	   st. directoryCompressed ++;
	   return;
	}
	if (g. type == 6) {
	   handleDirectory (stream, g, std::vector<uint8_t> (g. payload. begin () + 2,
	                                                   g. payload. end ()));
	   return;
	}
size_t	idx	= findPartial (stream, g. transportId, g. appId);
Partial &p	= partials [idx];
	p. touched	= time (nullptr);
std::vector<uint8_t> seg (g. payload. begin () + 2, g. payload. end ());
bool	isHeader = (g. type == 3);
auto	&segs	= isHeader ? p. hdr : p. body;
int	&last	= isHeader ? p. hdrLast : p. bodyLast;
	if (!isHeader)
	   st. bodySegments ++;
	if (segs. find (g. segmentNumber) == segs. end ()) {
	   if (p. bytes + seg. size () > maxObjectBytes + 4096) {
	      st. tooBig ++;
	      partials. erase (partials. begin () + idx);
	      return;
	   }
	   p. bytes	+= seg. size ();
	   segs [g. segmentNumber]	= seg;
	}
	if (g. lastSegment)
	   last	= g. segmentNumber;
	tryComplete (idx);
}

//	the header and the body are complete when segments 0 .. last are there
void	MotDecoder::tryComplete (size_t idx) {
Partial &p	= partials [idx];
	if (!p. haveHeader && p. hdrLast >= 0) {
	   bool all	= true;
	   for (int i = 0; i <= p. hdrLast; i ++)
	      if (p. hdr. find ((uint16_t)i) == p. hdr. end ()) {
	         all	= false;
	         break;
	      }
	   if (all) {
	      std::vector<uint8_t> hb;
	      for (int i = 0; i <= p. hdrLast; i ++) {
	         const auto &s	= p. hdr [(uint16_t)i];
	         hb. insert (hb. end (), s. begin (), s. end ());
	      }
	      MotHeader h;
	      if (parseMotHeader (hb. data (), hb. size (), h)) {
	         p. h		= h;
	         p. hdrBytes	= hb;
	         p. haveHeader	= true;
	         st. headers ++;
	      }
	      else {			// a repetition may bring a good one
	         st. badSegments ++;
	         p. hdr. clear ();
	         p. hdrLast	= -1;
	         p. bytes	= 0;
	         for (const auto &b : p. body)
	            p. bytes	+= b. second. size ();
	         return;
	      }
	   }
	}
//	directory mode: the header of an object is in the directory
	if (!p. haveHeader && !p. body. empty ()) {
	   auto it	= dirEntries. find (p. tid);
	   if (it != dirEntries. end ()) {
	      p. h		= it -> second. h;
	      p. hdrBytes	= it -> second. hdrBytes;
	      p. haveHeader	= true;
	   }
	}
	if (!p. haveHeader)
	   return;
//	ContentType 5 is "MOT transport" (TS 101 756, table 17): subtype 0 is a
//	header update (parameters only, no body), subtype 1 "header only".
//	ContentType 1 is plain text and must NOT be treated like that.
	if (p. h. contentType == 5 && p. h. contentSubType == 0) {
	   if (p. h. p. hasName) {
	      st. headerUpdates ++;
	      applyUpdate (p. h. p);
	   }
	   partials. erase (partials. begin () + idx);
	   return;
	}
	if (p. h. contentType == 5) {			// header only: nothing to show
	   partials. erase (partials. begin () + idx);
	   return;
	}
	if (p. h. p. scrambled) {
	   st. scrambledGroups ++;
	   partials. erase (partials. begin () + idx);
	   return;
	}
	if (p. h. bodySize > maxObjectBytes) {
	   st. tooBig ++;
	   partials. erase (partials. begin () + idx);
	   return;
	}
	if (p. bodyLast < 0)
	   return;
	for (int i = 0; i <= p. bodyLast; i ++)
	   if (p. body. find ((uint16_t)i) == p. body. end ())
	      return;
std::vector<uint8_t> body;
	for (int i = 0; i <= p. bodyLast; i ++) {
	   const auto &s	= p. body [(uint16_t)i];
	   body. insert (body. end (), s. begin (), s. end ());
	}
	if (body. size () != p. h. bodySize) {		// clause 6.1: discard it
	   st. sizeMismatch ++;
	   p. body. clear ();
	   p. bodyLast	= -1;
	   p. bytes	= 0;
	   for (const auto &b : p. hdr)
	      p. bytes	+= b. second. size ();
	   return;
	}
	storeObject (p, body);
	partials. erase (partials. begin () + idx);
}

void	MotDecoder::storeObject (const Partial &p, std::vector<uint8_t> &body) {
uint64_t h	= fnv64 (p. hdrBytes. data (), p. hdrBytes. size ());
	h		= fnv64 (body. data (), body. size (), h);
	for (const MotObject &o : store)
	   if (hash [o. seq] == h) {			// the carousel repeats
	      st. duplicates ++;
	      return;
	   }
MotObject o;
	o. seq		= nextSeq ++;
	o. transportId	= p. tid;
	o. appId	= p. appId;
	o. stream	= p. stream;
	o. received	= time (nullptr);
	o. hdr		= p. h;
	o. body		= body;
	if (body. size () >= 3 && body [0] == 0xFF && body [1] == 0xD8 &&
	    body [2] == 0xFF)
	   o. kind	= MotObject::Jpeg;
	else
	if (body. size () >= 8 && memcmp (body. data (), pngMagic, 8) == 0)
	   o. kind	= MotObject::Png;
//	parameters that arrived before the object (header update first)
	if (o. hdr. p. hasName) {
	   auto it	= pending. find (o. hdr. p. name);
	   if (it != pending. end ()) {
	      const MotParams &u	= it -> second;
	      if (u. hasTrigger) {
	         o. hdr. p. hasTrigger	= true;
	         o. hdr. p. triggerNow	= u. triggerNow;
	         o. hdr. p. triggerUnix	= u. triggerUnix;
	      }
	      if (u. hasCategory) {
	         o. hdr. p. hasCategory	= true;
	         o. hdr. p. categoryId	= u. categoryId;
	         o. hdr. p. slideId	= u. slideId;
	      }
	      pending. erase (it);
	   }
//	clause 6.2.2 of SlideShow: the same ContentName replaces the older object
	   for (size_t i = 0; i < store. size (); i ++)
	      if (store [i]. hdr. p. hasName &&
	          store [i]. hdr. p. name == o. hdr. p. name) {
	         storeBytes	-= store [i]. body. size ();
	         hash. erase (store [i]. seq);
	         store. erase (store. begin () + i);
	         break;
	      }
	}
	storeBytes	+= o. body. size ();
	hash [o. seq]	= h;
	store. push_back (o);
	st. objectsTotal ++;
	changes ++;
	while (store. size () > maxStoreObjects || storeBytes > maxStoreBytes) {
	   storeBytes	-= store. front (). body. size ();
	   hash. erase (store. front (). seq);
	   store. pop_front ();
	}
}

//	SlideShow 6.3: only TriggerTime and CategoryID/SlideID can be updated
void	MotDecoder::applyUpdate (const MotParams &u) {
	for (MotObject &o : store)
	   if (o. hdr. p. hasName && o. hdr. p. name == u. name) {
	      if (u. hasTrigger) {
	         o. hdr. p. hasTrigger	= true;
	         o. hdr. p. triggerNow	= u. triggerNow;
	         o. hdr. p. triggerUnix	= u. triggerUnix;
	      }
	      if (u. hasCategory || u. categoryId == 0) {
	         o. hdr. p. hasCategory	= u. hasCategory;
	         o. hdr. p. categoryId	= u. categoryId;
	         o. hdr. p. slideId	= u. slideId;
	      }
	      changes ++;
	      return;
	   }
	if (pending. size () < maxPending || pending. count (u. name))
	   pending [u. name]	= u;
}

//	MOT directory (EN 301 234, 6.2.4): 4 bytes (rfu, CompressionFlag, size),
//	number of objects (2), DataCarouselPeriod (3), rfu + SegmentSize (2),
//	DirectoryExtensionLength (2) + extension, then per object: TransportId
//	(2) and its header (core + parameters, HeaderSize bytes). Checked
//	against a real directory of a Broadcast Web Site.
void	MotDecoder::handleDirectory (int stream, const DataGroup &g,
	                                const std::vector<uint8_t> &seg) {
	st. directoryGroups ++;
	if (!dirPartial. active || dirPartial. tid != g. transportId ||
	    dirPartial. stream != stream) {		// a new directory version
	   dirPartial		= DirPartial ();
	   dirPartial. active	= true;
	   dirPartial. tid	= g. transportId;
	   dirPartial. stream	= stream;
	}
	if (dirPartial. segs. find (g. segmentNumber) == dirPartial. segs. end ()) {
	   if (dirPartial. bytes + seg. size () > 256 * 1024) {
	      st. directoryBad ++;
	      dirPartial	= DirPartial ();
	      return;
	   }
	   dirPartial. bytes	+= seg. size ();
	   dirPartial. segs [g. segmentNumber]	= seg;
	}
	if (g. lastSegment)
	   dirPartial. last	= g. segmentNumber;
	if (dirPartial. last < 0)
	   return;
	for (int i = 0; i <= dirPartial. last; i ++)
	   if (dirPartial. segs. find ((uint16_t)i) == dirPartial. segs. end ())
	      return;
std::vector<uint8_t> d;
	for (int i = 0; i <= dirPartial. last; i ++) {
	   const auto &s	= dirPartial. segs [(uint16_t)i];
	   d. insert (d. end (), s. begin (), s. end ());
	}
	dirPartial	= DirPartial ();
	if (!parseDirectory (d))
	   return;
//	objects whose body segments came first can complete now
	for (size_t i = partials. size (); i -- > 0; )
	   tryComplete (i);
}

bool	MotDecoder::parseDirectory (const std::vector<uint8_t> &d) {
	if (d. size () < 13) {
	   st. directoryBad ++;
	   return false;
	}
uint32_t w	= ((uint32_t)d [0] << 24) | (d [1] << 16) | (d [2] << 8) | d [3];
	if ((w >> 30) & 1) {				// CompressionFlag
	   st. directoryCompressed ++;
	   return false;
	}
size_t	size	= w & 0x3FFFFFFF;
unsigned n	= (d [4] << 8) | d [5];
size_t	extLen	= (d [11] << 8) | d [12];
size_t	pos	= 13 + extLen;
	if (size != d. size () || pos > size || n > 2048) {
	   st. directoryBad ++;
	   return false;
	}
std::map<uint16_t, DirEntry> entries;
	for (unsigned i = 0; i < n; i ++) {
	   if (pos + 2 + 7 > size) {
	      st. directoryBad ++;
	      return false;
	   }
	   uint16_t tid	= (uint16_t)((d [pos] << 8) | d [pos + 1]);
	   uint64_t core	= 0;
	   for (int k = 0; k < 7; k ++)
	      core	= (core << 8) | d [pos + 2 + k];
	   size_t hs	= (core >> 15) & 0x1FFF;
	   if (hs < 7 || pos + 2 + hs > size) {
	      st. directoryBad ++;
	      return false;
	   }
	   DirEntry e;
	   e. hdrBytes. assign (d. begin () + pos + 2, d. begin () + pos + 2 + hs);
	   if (!parseMotHeader (e. hdrBytes. data (), e. hdrBytes. size (), e. h)) {
	      st. directoryBad ++;
	      return false;
	   }
	   entries [tid]	= e;
	   pos	+= 2 + hs;
	}
	dirEntries		= entries;
	st. directories ++;
	st. directoryObjects	= n;
	changes ++;
	return true;
}

uint32_t MotDecoder::changeCounter () const {
std::lock_guard<std::mutex> g (lock);
	return changes;
}

MotStatus MotDecoder::status () const {
std::lock_guard<std::mutex> g (lock);
MotStatus r	= st;
	r. objects	= (uint32_t)store. size ();
	r. partial	= (uint32_t)partials. size ();
	r. images	= r. files = 0;
	for (const MotObject &o : store)
	   if (o. isImage ())
	      r. images ++;
	   else
	      r. files ++;
	return r;
}

std::vector<MotInfo> MotDecoder::list () const {
std::lock_guard<std::mutex> g (lock);
std::vector<MotInfo> r;
	for (const MotObject &o : store) {
	   MotInfo i;
	   i. seq		= o. seq;
	   i. appId		= o. appId;
	   i. received		= o. received;
	   i. kind		= o. kind;
	   i. size		= o. body. size ();
	   i. name		= o. hdr. p. hasName ? o. hdr. p. name : std::string ();
	   i. hasTrigger	= o. hdr. p. hasTrigger;
	   i. triggerNow	= o. hdr. p. triggerNow;
	   i. triggerUnix	= o. hdr. p. triggerUnix;
	   i. hasCategory	= o. hdr. p. hasCategory;
	   i. categoryId	= o. hdr. p. categoryId;
	   i. slideId		= o. hdr. p. slideId;
	   i. categoryTitle	= o. hdr. p. categoryTitle;
	   i. clickUrl		= o. hdr. p. clickUrl;
	   i. compressed	= o. hdr. p. compressed;
	   r. push_back (i);
	}
	return r;
}

bool	MotDecoder::getObject (uint32_t seq, MotObject &out) const {
std::lock_guard<std::mutex> g (lock);
	for (const MotObject &o : store)
	   if (o. seq == seq) {
	      out	= o;
	      return true;
	   }
	return false;
}

bool	MotDecoder::hasImages () const {
std::lock_guard<std::mutex> g (lock);
	for (const MotObject &o : store)
	   if (o. isImage ())
	      return true;
	return false;
}

bool	MotDecoder::hasFiles () const {
std::lock_guard<std::mutex> g (lock);
	for (const MotObject &o : store)
	   if (!o. isImage ())
	      return true;
	return false;
}

}	// namespace dxdata
