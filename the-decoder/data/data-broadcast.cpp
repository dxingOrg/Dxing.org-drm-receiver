#
/*
 *    Dxing.org DRM receiver - data broadcast packets (GY/T 423-2025, 6.7)
 *    GNU General Public License, version 2 or later.
 */
#include	"data-broadcast.h"
#include	"mot.h"
#include	<string.h>
#include	<ctype.h>
#include	<algorithm>

namespace dxdata {

/*
 * ---- MD5 (RFC 1321), only to check the file names that are MD5 sums --
 */
namespace {
const uint32_t md5K [64] = {
0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
const int md5S [64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
	5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
	4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
	6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

uint32_t	rol (uint32_t x, int c) {
	return (x << c) | (x >> (32 - c));
}

void	md5Block (uint32_t st [4], const uint8_t *p) {
uint32_t m [16];
	for (int i = 0; i < 16; i ++)
	   m [i]	= (uint32_t)p [4 * i] | ((uint32_t)p [4 * i + 1] << 8) |
		         ((uint32_t)p [4 * i + 2] << 16) | ((uint32_t)p [4 * i + 3] << 24);
uint32_t a = st [0], b = st [1], c = st [2], d = st [3];
	for (int i = 0; i < 64; i ++) {
	   uint32_t f;
	   int g;
	   if (i < 16) {
	      f	= (b & c) | (~b & d);
	      g	= i;
	   }
	   else
	   if (i < 32) {
	      f	= (d & b) | (~d & c);
	      g	= (5 * i + 1) & 15;
	   }
	   else
	   if (i < 48) {
	      f	= b ^ c ^ d;
	      g	= (3 * i + 5) & 15;
	   }
	   else {
	      f	= c ^ (b | ~d);
	      g	= (7 * i) & 15;
	   }
	   uint32_t t	= d;
	   d	= c;
	   c	= b;
	   b	= b + rol (a + f + md5K [i] + m [g], md5S [i]);
	   a	= t;
	}
	st [0] += a;
	st [1] += b;
	st [2] += c;
	st [3] += d;
}

void	appendUtf8 (std::string &s, uint32_t c) {
	if (c < 0x80)
	   s	+= (char)c;
	else
	if (c < 0x800) {
	   s	+= (char)(0xC0 | (c >> 6));
	   s	+= (char)(0x80 | (c & 0x3F));
	}
	else
	if (c < 0x10000) {
	   s	+= (char)(0xE0 | (c >> 12));
	   s	+= (char)(0x80 | ((c >> 6) & 0x3F));
	   s	+= (char)(0x80 | (c & 0x3F));
	}
	else {
	   s	+= (char)(0xF0 | (c >> 18));
	   s	+= (char)(0x80 | ((c >> 12) & 0x3F));
	   s	+= (char)(0x80 | ((c >> 6) & 0x3F));
	   s	+= (char)(0x80 | (c & 0x3F));
	}
}

size_t	findNoCase (const std::string &h, const std::string &needle, size_t from) {
	if (needle. empty () || h. size () < needle. size ())
	   return std::string::npos;
	for (size_t i = from; i + needle. size () <= h. size (); i ++) {
	   size_t k	= 0;
	   while (k < needle. size () &&
	          tolower ((unsigned char)h [i + k]) == needle [k])
	      k ++;
	   if (k == needle. size ())
	      return i;
	}
	return std::string::npos;
}

std::string	beHex (const uint8_t *d, size_t n) {
static const char *x = "0123456789abcdef";
std::string r;
	for (size_t i = 0; i < n; i ++) {
	   r	+= x [d [i] >> 4];
	   r	+= x [d [i] & 15];
	}
	return r;
}

uint64_t	fnv64 (const uint8_t *p, size_t n, uint64_t h) {
	for (size_t i = 0; i < n; i ++) {
	   h	^= p [i];
	   h	*= 1099511628211ULL;
	}
	return h;
}

const size_t	maxResources	= 64;
const size_t	maxStoreBytes	= 8 * 1024 * 1024;
const size_t	maxParts	= 16;
const size_t	maxPartBytes	= 2 * 1024 * 1024;
}

std::string	md5Hex (const uint8_t *d, size_t n) {
uint32_t st [4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
size_t	i;
	for (i = 0; i + 64 <= n; i += 64)
	   md5Block (st, d + i);
uint8_t tail [128];
size_t	rest	= n - i;
	memset (tail, 0, sizeof (tail));
	if (rest > 0)
	   memcpy (tail, d + i, rest);
	tail [rest]	= 0x80;
size_t	total	= (rest < 56) ? 64 : 128;
uint64_t bits	= (uint64_t)n * 8;
	for (int k = 0; k < 8; k ++)
	   tail [total - 8 + k]	= (uint8_t)(bits >> (8 * k));
	md5Block (st, tail);
	if (total == 128)
	   md5Block (st, tail + 64);
uint8_t out [16];
	for (int k = 0; k < 4; k ++)
	   for (int b = 0; b < 4; b ++)
	      out [4 * k + b]	= (uint8_t)(st [k] >> (8 * b));
	return beHex (out, 16);
}

/*
 * ---- HTML to text ------------------------------------------------
 */
std::string	htmlToPlainText (const std::string &h) {
std::string out;
size_t	i	= 0;
size_t	n	= h. size ();
auto newline	= [&] () {
	   if (!out. empty () && out. back () != '\n')
	      out	+= '\n';
	};
	while (i < n) {
	   char c	= h [i];
	   if (c == '<') {
	      if (h. compare (i, 4, "<!--") == 0) {
	         size_t e	= h. find ("-->", i + 4);
	         i	= (e == std::string::npos) ? n : e + 3;
	         continue;
	      }
	      size_t j	= h. find ('>', i);
	      if (j == std::string::npos)
	         break;
	      std::string tag	= h. substr (i + 1, j - i - 1);
	      size_t k	= 0;
	      bool closing	= false;
	      if (k < tag. size () && tag [k] == '/') {
	         closing	= true;
	         k ++;
	      }
	      std::string name;
	      while (k < tag. size () && isalnum ((unsigned char)tag [k]))
	         name	+= (char)tolower ((unsigned char)tag [k ++]);
	      if (!closing && (name == "script" || name == "style")) {
	         size_t e	= findNoCase (h, "</" + name, j + 1);
	         if (e == std::string::npos)
	            break;
	         size_t e2	= h. find ('>', e);
	         i	= (e2 == std::string::npos) ? n : e2 + 1;
	         continue;
	      }
	      if (name == "br" || name == "hr" || name == "p" || name == "div" ||
	          name == "li" || name == "tr" || name == "h1" || name == "h2" ||
	          name == "h3" || name == "table")
	         newline ();
	      i	= j + 1;
	      continue;
	   }
	   if (c == '&') {
	      size_t e	= h. find (';', i);
	      if (e != std::string::npos && e - i <= 10) {
	         std::string ent	= h. substr (i + 1, e - i - 1);
	         bool ok	= true;
	         if (ent == "amp")
	            out	+= '&';
	         else if (ent == "lt")
	            out	+= '<';
	         else if (ent == "gt")
	            out	+= '>';
	         else if (ent == "quot")
	            out	+= '"';
	         else if (ent == "apos")
	            out	+= '\'';
	         else if (ent == "nbsp")
	            out	+= ' ';
	         else if (ent. size () > 1 && ent [0] == '#') {
	            long v	= (ent [1] == 'x' || ent [1] == 'X')
	                      ? strtol (ent. c_str () + 2, nullptr, 16)
	                      : strtol (ent. c_str () + 1, nullptr, 10);
	            if (v > 0 && v < 0x110000)
	               appendUtf8 (out, (uint32_t)v);
	            else
	               ok	= false;
	         }
	         else
	            ok	= false;
	         if (ok) {
	            i	= e + 1;
	            continue;
	         }
	      }
	   }
	   if ((unsigned char)c < 0x20 && c != '\n' && c != '\t')
	      out	+= ' ';
	   else
	      out	+= c;
	   i ++;
	}
//	tidy up: no blank-line runs, no leading or trailing blanks
std::string r;
int	nl	= 0;
	for (char c : out) {
	   if (c == '\n') {
	      if (++ nl <= 2 && !r. empty ())
	         r	+= c;
	   }
	   else {
	      nl	= 0;
	      r	+= c;
	   }
	}
	while (!r. empty () && (r. back () == '\n' || r. back () == ' '))
	   r. pop_back ();
	return r;
}

/*
 * ---- the packet ------------------------------------------------------
 */
bool	parseDbPacket (const uint8_t *d, size_t n, DbPacket &o) {
	if (n < 10 || d [0] != 0x49 || d [1] != 0x59)
	   return false;
uint64_t v	= 0;
	for (int i = 0; i < 8; i ++)
	   v	= (v << 8) | d [i];
	o. serviceType	= (int)((v >> 44) & 0xF);
	o. resourceId	= (int)((v >> 41) & 7);
	o. extFlag	= ((v >> 40) & 1) != 0;
	o. packetNo	= (int)((v >> 30) & 0x3FF);
	o. updateNo	= (int)((v >> 26) & 0xF);
	o. packetType	= (int)((v >> 24) & 3);
	o. packetCount	= (int)((v >> 14) & 0x3FF);
	o. fec		= (int)((v >> 12) & 3);
size_t	plen	= (size_t)(v & 0xFFF);
size_t	pos	= 8;
	o. extId		= 0;
	o. ebmId		= 0;
	if (o. extFlag) {
	   if (pos + 2 > n)
	      return false;
	   o. extId	= (uint16_t)((d [pos] << 8) | d [pos + 1]);
	   pos	+= 2;
	}
	if (o. serviceType == 1) {
	   if (pos + 2 > n)
	      return false;
	   o. ebmId	= (uint16_t)((d [pos] << 8) | d [pos + 1]);
	   pos	+= 2;
	}
	if (pos + plen + 2 != n)
	   return false;
	if (crc16 (d, pos + plen) != (uint16_t)((d [pos + plen] << 8) | d [pos + plen + 1]))
	   return false;
	if (o. packetCount < 1 || o. packetNo >= o. packetCount)
	   return false;
	o. payload. assign (d + pos, d + pos + plen);
	return true;
}

//	clause 6.7: 15 lines, UTF-8, one attribute each
bool	parseDbDescription (const std::string &text, DbDescription &o) {
std::vector<std::string> v;
std::string cur;
	for (char c : text) {
	   if (c == '\n') {
	      v. push_back (cur);
	      cur. clear ();
	   }
	   else
	   if (c != '\r')
	      cur	+= c;
	}
	v. push_back (cur);
	if (v. size () < 12)
	   return false;
	v. resize (15);
	o. serviceId	= v [0];
	o. mode	= v [1];
	o. resourceId	= v [2];
	o. updateNo	= v [3];
	o. name	= v [4];
	o. fileType	= v [5];
	o. title	= v [6];
	o. summary	= v [7];
	o. keywords	= v [8];
	o. encoding	= v [9];
	o. path	= v [10];
	o. fileLength	= v [11];
	o. startTime	= v [12];
	o. expiryTime	= v [13];
	o. deleteFlag	= v [14];
	return true;
}

std::string	DbResource::suggestedFileName () const {
std::string base	= hasDescription && !desc. name. empty () ? desc. name :
	                     "resource_" + std::to_string (resourceId) + "_" +
	                     std::to_string (updateNo);
	switch (kind) {
	   case Jpeg:	return sanitizeFileName (base, "jpg", true);
	   case Png:	return sanitizeFileName (base, "png", true);
	   case Gif:	return sanitizeFileName (base, "gif", true);
	   case Html:	return sanitizeFileName (base, "html", true);
	   case Text:	return sanitizeFileName (base, "txt", true);
	   default:	return sanitizeFileName (base, "bin", true);
	}
}

/*
 * ---- the decoder ---------------------------------------------------
 */
	DataBroadcastDecoder::DataBroadcastDecoder () {
	changes		= 0;
	nextSeq		= 1;
	storeBytes	= 0;
}

void	DataBroadcastDecoder::clear () {
std::lock_guard<std::mutex> g (lock);
	parts. clear ();
	store. clear ();
	hash. clear ();
	st		= DbStatus ();
	storeBytes	= 0;
	changes ++;
}

namespace {
bool	partKey (int a, int b, int c, int d, int e, int f, int g, int h) {
	return a == e && b == f && c == g && d == h;
}
}

void	DataBroadcastDecoder::handleGroup (int stream, const DataGroup &gr) {
	(void)stream;
	const std::vector<uint8_t> &r	= gr. raw;
	if (r. size () < 10 || r [0] != 0x49 || r [1] != 0x59)
	   return;
std::lock_guard<std::mutex> g (lock);
DbPacket p;
	if (!parseDbPacket (r. data (), r. size (), p)) {
	   st. badPackets ++;
	   return;
	}
	st. packets ++;
	st. serviceType	= (uint32_t)p. serviceType;
//	not handled: FEC coded payloads, files with extension packets, streams
	if (p. fec != 0 || (p. extFlag && p. extId != 0) || p. packetType == 0 ||
	    p. packetType > 2) {
	   st. unsupported ++;
	   return;
	}
size_t	idx	= parts. size ();
	for (size_t i = 0; i < parts. size (); i ++)
	   if (partKey (parts [i]. serviceType, parts [i]. resourceId,
	                parts [i]. updateNo, parts [i]. packetType,
	                p. serviceType, p. resourceId, p. updateNo, p. packetType)) {
	      idx	= i;
	      break;
	   }
	if (idx < parts. size () && parts [idx]. count != p. packetCount) {
	   parts. erase (parts. begin () + idx);	// a different layout: start over
	   idx	= parts. size ();
	}
	if (idx == parts. size ()) {
//	a newer update of the same resource makes the older parts obsolete
	   for (size_t i = parts. size (); i -- > 0; )
	      if (parts [i]. serviceType == p. serviceType &&
	          parts [i]. resourceId == p. resourceId &&
	          parts [i]. packetType == p. packetType)
	         parts. erase (parts. begin () + i);
	   Part n;
	   n. serviceType	= p. serviceType;
	   n. resourceId	= p. resourceId;
	   n. updateNo	= p. updateNo;
	   n. packetType	= p. packetType;
	   n. count		= p. packetCount;
	   n. touched	= time (nullptr);
	   parts. push_back (n);
	   if (parts. size () > maxParts)
	      parts. pop_front ();
	   idx	= parts. size () - 1;
	}
Part	&pt	= parts [idx];
	pt. touched	= time (nullptr);
size_t	bytes	= 0;
	for (const auto &e : pt. pieces)
	   bytes	+= e. second. size ();
	if (bytes + p. payload. size () > maxPartBytes) {
	   parts. erase (parts. begin () + idx);
	   return;
	}
	if (pt. pieces. find (p. packetNo) == pt. pieces. end ())
	   pt. pieces [p. packetNo]	= p. payload;
	if ((int)pt. pieces. size () < pt. count)
	   return;
std::vector<uint8_t> data;
	for (int i = 0; i < pt. count; i ++) {
	   auto it	= pt. pieces. find (i);
	   if (it == pt. pieces. end ())
	      return;
	   data. insert (data. end (), it -> second. begin (), it -> second. end ());
	}
Part done	= pt;
	parts. erase (parts. begin () + idx);
	partDone (done, data);
}

void	DataBroadcastDecoder::settle (DbResource &r) {
	r. lengthChecked	= false;
	r. md5Checked	= false;
	if (r. hasDescription) {
	   const std::string &len	= r. desc. fileLength;
	   if (!len. empty () && len. find_first_not_of ("0123456789") == std::string::npos) {
	      r. lengthChecked	= true;
	      r. lengthOk	= (unsigned long long)strtoull (len. c_str (), nullptr, 10) ==
	                         (unsigned long long)r. file. size ();
	   }
	   const std::string &nm	= r. desc. name;
	   if (nm. size () == 32 &&
	       nm. find_first_not_of ("0123456789abcdefABCDEF") == std::string::npos) {
	      std::string low	= nm;
	      for (char &c : low)
	         c	= (char)tolower ((unsigned char)c);
	      r. md5Checked	= true;
	      r. md5Ok	= (md5Hex (r. file. data (), r. file. size ()) == low);
	   }
	}
const std::vector<uint8_t> &f	= r. file;
	r. kind		= DbResource::Unknown;
	r. text. clear ();
	r. subHeaderHex. clear ();
	r. hasTime	= false;
	if (f. size () >= 3 && f [0] == 0xFF && f [1] == 0xD8 && f [2] == 0xFF)
	   r. kind	= DbResource::Jpeg;
	else
	if (f. size () >= 8 && f [0] == 0x89 && f [1] == 'P' && f [2] == 'N' && f [3] == 'G')
	   r. kind	= DbResource::Png;
	else
	if (f. size () >= 6 && memcmp (f. data (), "GIF8", 4) == 0)
	   r. kind	= DbResource::Gif;
	else
	if (r. serviceType == 1 && f. size () > 15 && f [15] == '<') {
//	emergency broadcast: 15 bytes whose meaning is defined elsewhere, then HTML
	   r. kind	= DbResource::Emergency;
	   r. subHeaderHex	= beHex (f. data (), 15);
	   uint32_t t	= ((uint32_t)f [1] << 24) | (f [2] << 16) | (f [3] << 8) | f [4];
	   if (f [0] == 0 && t > 946684800u && t < 4102444800u) {	// 2000 .. 2100
	      r. hasTime	= true;
	      r. timeUnix	= t;
	   }
	   r. text	= htmlToPlainText (std::string ((const char *)f. data () + 15,
	                                           f. size () - 15));
	}
	else {
size_t	k	= 0;
	   while (k < f. size () && (f [k] == ' ' || f [k] == '\n' || f [k] == '\r' ||
	                              f [k] == '\t'))
	      k ++;
	   bool printable	= !f. empty ();
	   for (size_t i = 0; i < f. size () && i < 4096; i ++)
	      if (f [i] < 0x09 || (f [i] > 0x0D && f [i] < 0x20))
	         printable	= false;
	   if (printable && k < f. size () && f [k] == '<') {
	      r. kind	= DbResource::Html;
	      r. text	= htmlToPlainText (std::string ((const char *)f. data (), f. size ()));
	   }
	   else
	   if (printable) {
	      r. kind	= DbResource::Text;
	      r. text	= std::string ((const char *)f. data (), f. size ());
	   }
	}
	if (r. text. size () > 65536)
	   r. text. resize (65536);
}

void	DataBroadcastDecoder::partDone (const Part &pt,
	                                  const std::vector<uint8_t> &data) {
	if (pt. packetType == 2) {			// information description file
	   DbDescription d;
	   if (!parseDbDescription (std::string ((const char *)data. data (),
	                                         data. size ()), d))
	      return;
	   for (DbResource &r : store)
	      if (r. serviceType == pt. serviceType && r. resourceId == pt. resourceId &&
	          r. updateNo == pt. updateNo) {
	         bool same	= r. hasDescription && r. desc. name == d. name &&
	                      r. desc. fileLength == d. fileLength;
	         r. hasDescription	= true;
	         r. desc		= d;
	         if (!same) {
	            settle (r);
	            changes ++;
	         }
	         return;
	      }
//	the file may come later: keep the description as a resource without
//	a file (empty), completed when the file arrives
	   DbResource r;
	   r. seq		= nextSeq ++;
	   r. serviceType	= pt. serviceType;
	   r. resourceId	= pt. resourceId;
	   r. updateNo	= pt. updateNo;
	   r. received	= time (nullptr);
	   r. hasDescription	= true;
	   r. desc		= d;
	   store. push_back (r);
	   changes ++;
	   return;
	}
//	a file
uint64_t h	= fnv64 (data. data (), data. size (), 1469598103934665603ULL);
	for (DbResource &r : store)
	   if (r. serviceType == pt. serviceType && r. resourceId == pt. resourceId &&
	       r. updateNo == pt. updateNo) {
	      if (!r. file. empty () && fnv64 (r. file. data (), r. file. size (),
	                                       1469598103934665603ULL) == h) {
	         st. duplicates ++;			// the carousel repeats
	         return;
	      }
	      storeBytes	-= r. file. size ();
	      r. file	= data;
	      storeBytes	+= r. file. size ();
	      r. received	= time (nullptr);
	      settle (r);
	      st. resourcesTotal ++;
	      changes ++;
	      return;
	   }
//	a newer update of the same resource replaces the older one
	for (size_t i = 0; i < store. size (); i ++)
	   if (store [i]. serviceType == pt. serviceType &&
	       store [i]. resourceId == pt. resourceId) {
	      storeBytes	-= store [i]. file. size ();
	      store. erase (store. begin () + i);
	      break;
	   }
DbResource r;
	r. seq		= nextSeq ++;
	r. serviceType	= pt. serviceType;
	r. resourceId	= pt. resourceId;
	r. updateNo	= pt. updateNo;
	r. received	= time (nullptr);
	r. file		= data;
	settle (r);
	storeBytes	+= r. file. size ();
	store. push_back (r);
	st. resourcesTotal ++;
	changes ++;
	while (store. size () > maxResources || storeBytes > maxStoreBytes) {
	   storeBytes	-= store. front (). file. size ();
	   store. pop_front ();
	}
}

uint32_t DataBroadcastDecoder::changeCounter () const {
std::lock_guard<std::mutex> g (lock);
	return changes;
}

//	what is on its way: files that are not complete yet, and descriptions
//	of files from which nothing has arrived
std::vector<DbProgress> DataBroadcastDecoder::progress () const {
std::lock_guard<std::mutex> g (lock);
std::vector<DbProgress> r;
	for (const Part &p : parts) {
DbProgress d;
	   d. serviceType	= p. serviceType;
	   d. resourceId	= p. resourceId;
	   d. updateNo		= p. updateNo;
	   d. packetType	= p. packetType;
	   d. count		= p. count;
	   d. have		= (int)p. pieces. size ();
	   d. bytes		= 0;
	   for (auto &x : p. pieces)
	      d. bytes += x. second. size ();
	   d. hasDescription = false;
	   for (const DbResource &x : store)
	      if (x. hasDescription && x. serviceType == p. serviceType &&
	          x. resourceId == p. resourceId && x. updateNo == p. updateNo) {
	         d. hasDescription = true;
	         d. name		= x. desc. name;
	         d. fileLength	= x. desc. fileLength;
	      }
	   if (p. packetType == 2 && d. have >= d. count)
	      continue;
	   r. push_back (d);
	}
	for (const DbResource &x : store) {
	   if (!x. hasDescription || !x. file. empty ())
	      continue;
bool covered = false;
	   for (const DbProgress &d : r)
	      if (d. packetType == 1 && d. serviceType == x. serviceType &&
	          d. resourceId == x. resourceId && d. updateNo == x. updateNo)
	         covered = true;
	   if (covered)
	      continue;
DbProgress d;
	   d. serviceType	= x. serviceType;
	   d. resourceId	= x. resourceId;
	   d. updateNo		= x. updateNo;
	   d. packetType	= 1;
	   d. count		= 0;
	   d. have		= 0;
	   d. bytes		= 0;
	   d. hasDescription = true;
	   d. name		= x. desc. name;
	   d. fileLength	= x. desc. fileLength;
	   r. push_back (d);
	}
	return r;
}

bool DataBroadcastDecoder::hasImage () const {
std::lock_guard<std::mutex> g (lock);
	for (const DbResource &r : store)
	   if (r. kind == DbResource::Jpeg || r. kind == DbResource::Png ||
	       r. kind == DbResource::Gif)
	      return true;
	return false;
}

DbStatus DataBroadcastDecoder::status () const {
std::lock_guard<std::mutex> g (lock);
DbStatus r	= st;
	r. partial	= (uint32_t)parts. size ();
	r. resources	= 0;
	for (const DbResource &x : store)
	   if (!x. file. empty ())
	      r. resources ++;
	return r;
}

std::vector<DbInfo> DataBroadcastDecoder::list () const {
std::lock_guard<std::mutex> g (lock);
std::vector<DbInfo> v;
	for (const DbResource &r : store) {
	   if (r. file. empty ())
	      continue;
	   DbInfo i;
	   i. seq		= r. seq;
	   i. serviceType	= r. serviceType;
	   i. resourceId	= r. resourceId;
	   i. updateNo	= r. updateNo;
	   i. received	= r. received;
	   i. kind		= r. kind;
	   i. size		= r. file. size ();
	   i. title		= r. hasDescription ? r. desc. title : std::string ();
	   i. hasDescription	= r. hasDescription;
	   i. lengthOk	= r. lengthOk;
	   i. md5Checked	= r. md5Checked;
	   i. md5Ok		= r. md5Ok;
	   v. push_back (i);
	}
	return v;
}

bool	DataBroadcastDecoder::getResource (uint32_t seq, DbResource &out) const {
std::lock_guard<std::mutex> g (lock);
	for (const DbResource &r : store)
	   if (r. seq == seq && !r. file. empty ()) {
	      out	= r;
	      return true;
	   }
	return false;
}

bool	DataBroadcastDecoder::hasContent () const {
std::lock_guard<std::mutex> g (lock);
	for (const DbResource &r : store)
	   if (!r. file. empty ())
	      return true;
	return false;
}

}	// namespace dxdata
