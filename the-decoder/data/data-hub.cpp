#
/*
 *    Dxing.org DRM receiver - data services, step 1
 *    GNU General Public License, version 2 or later.
 */
#include	"data-hub.h"
#include	<stdio.h>
#include	<time.h>

namespace dxdata {

uint16_t crc16 (const uint8_t *p, size_t n) {
uint16_t	reg = 0xFFFF;
	for (size_t i = 0; i < n; i ++) {
	   reg ^= (uint16_t)p [i] << 8;
	   for (int k = 0; k < 8; k ++)
	      reg = (reg & 0x8000) ? (uint16_t)((reg << 1) ^ 0x1021)
	                           : (uint16_t)(reg << 1);
	}
	return (uint16_t)~reg;
}

bool	parseDataGroup (const uint8_t *p, size_t n, DataGroup &out) {
	out		= DataGroup ();
	if (n < 2)
	   return false;
	bool ext	= (p [0] >> 7) & 1;
	bool crcFlag	= (p [0] >> 6) & 1;
	bool segFlag	= (p [0] >> 5) & 1;
	bool uaFlag	= (p [0] >> 4) & 1;
	out. type	= p [0] & 0x0F;
	out. continuity	= p [1] >> 4;
	out. repetition	= p [1] & 0x0F;
	size_t pos	= 2;
	if (ext)
	   pos += 2;
	if (segFlag) {
	   if (pos + 2 > n)
	      return false;
	   out. hasSegment	= true;
	   out. lastSegment	= (p [pos] >> 7) & 1;
	   out. segmentNumber	= ((p [pos] & 0x7F) << 8) | p [pos + 1];
	   pos += 2;
	}
	if (uaFlag) {
	   if (pos + 1 > n)
	      return false;
	   uint8_t b		= p [pos ++];
	   bool tidFlag		= (b >> 4) & 1;
	   size_t li		= b & 0x0F;
	   if (pos + li > n)
	      return false;
	   if (tidFlag) {
	      if (li < 2)
	         return false;
	      out. hasTransportId	= true;
	      out. transportId	= (p [pos] << 8) | p [pos + 1];
	   }
	   pos += li;
	}
	size_t end	= n;
	if (crcFlag) {
	   if (n < pos + 2)
	      return false;
	   out. crcPresent	= true;
	   out. crcOk		= crc16 (p, n - 2) ==
	                          (uint16_t)((p [n - 2] << 8) | p [n - 1]);
	   end		= n - 2;
	}
	if (pos > end)
	   return false;
	out. payload. assign (p + pos, p + end);
	return true;
}

std::string applicationName (int domain, int appId) {
	if (domain != 1)
	   return "DRM-specific / unknown";
	switch (appId) {
	   case 0x002:	return "MOT Slide Show";
	   case 0x003:	return "MOT Broadcast Web Site";
	   case 0x004:	return "TPEG";
	   case 0x005:	return "DGPS";
	   case 0x006:	return "TMC";
	   case 0x007:	return "EPG";
	   case 0x44A:	return "Journaline";
	   default:	return "unknown";
	}
}

	DataHub::DataHub	() {
	totalBytes	= 0;
	dumpFile	= nullptr;
	dumpCount	= 0;
	dumpFrames	= 0;
	combining	= true;
}

	DataHub::~DataHub	() {
	stopDump ();
}

void	DataHub::clearStream (Stream &s) {
	s	= Stream ();
}

void	DataHub::reset () {
std::lock_guard<std::mutex> g (lock);
	for (int i = 0; i < 4; i ++)
	   clearStream (streams [i]);
	recent. clear ();
	totalBytes	= 0;
	diagData. clear ();
}

bool	DataHub::hasApplication (int appId, int *stream) {
std::lock_guard<std::mutex> g (lock);
	for (int i = 0; i < 4; i ++)
	   if (streams [i]. configured && streams [i]. packetMode &&
	       streams [i]. appId == appId) {
	      if (stream != nullptr)
	         *stream	= i;
	      return true;
	   }
	return false;
}

bool	DataHub::startDump (const std::string &path) {
std::lock_guard<std::mutex> g (lock);
	if (dumpFile != nullptr) {
	   fclose (dumpFile);
	   dumpFile	= nullptr;
	}
	dumpFile	= fopen (path. c_str (), "ab");
	if (dumpFile == nullptr)
	   return false;
	dumpCount	= 0;
	dumpFrames	= 0;
	fprintf (dumpFile, "# Dxing.org DRM data units. One line per unit: "
	                   "G <unix time> <stream> <packet id> <application, hex> "
	                   "<frame no> <bytes> <hex of the unit>\n"
	                   "# and, for the first 600 frames of a data stream: "
	                   "F <unix time> <stream> <frame no> <bytes> <hex of the frame>\n");
	fflush (dumpFile);
	return true;
}

void	DataHub::stopDump () {
std::lock_guard<std::mutex> g (lock);
	if (dumpFile != nullptr) {
	   fclose (dumpFile);
	   dumpFile	= nullptr;
	}
}

bool	DataHub::dumping () {
std::lock_guard<std::mutex> g (lock);
	return dumpFile != nullptr;
}

void	DataHub::setGroupSink (GroupSink s) {
std::lock_guard<std::mutex> g (lock);
	sink	= s;
}

void	DataHub::configureStream (int idx, bool packetMode, int packetLength,
	                          int packetId, int domain, int appId,
	                          bool fec) {
	if (idx < 0 || idx > 3)
	   return;
std::lock_guard<std::mutex> g (lock);
Stream &s = streams [idx];
	if (s. configured && s. packetMode == packetMode &&
	    s. packetLength == packetLength && s. packetId == packetId &&
	    s. domain == domain && s. appId == appId && s. fec == fec)
	   return;
	clearStream (s);
	s. configured	= true;
	s. packetMode	= packetMode;
	s. packetLength	= packetLength;
	s. packetId	= packetId;
	s. domain	= domain;
	s. appId	= appId;
	s. fec		= fec;
}

void	DataHub::feedStream (int idx, const uint8_t *bytes, size_t n) {
	if (idx < 0 || idx > 3)
	   return;
std::lock_guard<std::mutex> g (lock);
Stream &s = streams [idx];
	if (!s. configured)
	   return;
	s. frames	++;
	s. bytes	+= n;
	totalBytes	+= n;
	if (dumpFile != nullptr && dumpFrames < 600) {	// raw frame, to see
	   dumpFrames ++;				// how the packets are laid out
	   fprintf (dumpFile, "F %lld %d %u %u ", (long long)time (nullptr),
	            idx, s. frames, (unsigned)n);
	   for (size_t i = 0; i < n; i ++)
	      fprintf (dumpFile, "%02X", bytes [i]);
	   fputc ('\n', dumpFile);
	}
	if (!s. packetMode)
	   return;				// synchronous stream: only counted
	if (s. packetLength < 4 || s. packetLength > 255)
	   return;

//	The SDC "packet length" may mean the whole packet (header + data +
//	CRC) or only the data field. Until it is clear which, we score both
//	readings by how many packets pass the CRC, then keep the better one.
	if (s. sizeMode < 0) {
	   for (int mode = 0; mode < 2; mode ++) {
	      size_t L = s. packetLength + (mode == 1 ? 3 : 0);
	      for (size_t off = 0; off + L <= n; off += L) {
	         s. tried ++;
	         if (crc16 (bytes + off, L - 2) ==
	             (uint16_t)((bytes [off + L - 2] << 8) | bytes [off + L - 1])) {
	            if (mode == 0) s. scoreTotal ++;
	            else s. scorePlus3 ++;
	         }
	      }
	   }
	   if (s. scoreTotal >= 3 || s. scorePlus3 >= 3)
	      s. sizeMode = (s. scorePlus3 > s. scoreTotal) ? 1 : 0;
	   return;				// frames used for the decision are not assembled
	}

size_t L = s. packetLength + (s. sizeMode == 1 ? 3 : 0);
	if (n % L != 0)
	   s. badRemainder ++;
	s. dc. setCapacity ((int)L - 3);
	for (size_t off = 0; off + L <= n; off += L)
	   handlePacket (idx, s, bytes + off, L);
	s. dc. endFrame ();
	deliverCarousel (idx, s);
}

//	Data broadcast packets that were completed from several repetitions of
//	the carousel (see data-carousel.h): each one has passed its own CRC
void	DataHub::deliverCarousel (int idx, Stream &s) {
std::vector<uint8_t> u;
	while (s. dc. take (u)) {
	   s. dcUnits ++;
	   if (dumpFile != nullptr) {			// kept for later analysis
	      fprintf (dumpFile, "C %lld %d %03X %u %u ",
	               (long long)time (nullptr), idx, s. appId, s. frames,
	               (unsigned)u. size ());
	      for (size_t i = 0; i < u. size (); i ++)
	         fprintf (dumpFile, "%02X", u [i]);
	      fputc ('\n', dumpFile);
	   }
DataGroup g;
	   parseDataGroup (u. data (), u. size (), g);	// type etc., if it fits
	   g. crcPresent	= true;
	   g. crcOk		= true;
	   g. stream	= idx;
	   g. appId	= s. appId;
	   g. domain	= s. domain;
	   g. packetId	= s. packetId;
	   g. raw		= u;
char line [96];
	   snprintf (line, sizeof (line),
	             "stream %d  assembled from repetitions  %u bytes", idx,
	             (unsigned)u. size ());
	   recent. push_back (line);
	   while (recent. size () > 12)
	      recent. pop_front ();
	   if (sink)
	      sink (idx, g);
	}
}

void	DataHub::handlePacket (int idx, Stream &s, const uint8_t *p, size_t L) {
	s. packets ++;
	if (crc16 (p, L - 2) != (uint16_t)((p [L - 2] << 8) | p [L - 1])) {
	   s. crcBad ++;
	   s. cb. badSince ++;			// a packet lost for the combiner
	   s. dc. slotBad ();
	   return;
	}
	s. crcOk ++;
bool	first	= (p [0] >> 7) & 1;
bool	last	= (p [0] >> 6) & 1;
int	pid	= (p [0] >> 4) & 3;
bool	ppi	= (p [0] >> 3) & 1;
int	ci	= p [0] & 7;
const uint8_t *data	= p + 1;
size_t	dlen	= L - 3;
	s. packetsPerId [pid] ++;
	if (ppi) {
	   if (dlen < 1) {
	      s. malformed ++;
	      return;
	   }
	   size_t useful = data [0];
	   if (useful > dlen - 1) {
	      s. malformed ++;
	      return;
	   }
	   data ++;
	   dlen = useful;
	}
	if (pid == s. packetId)
	   s. dc. slotOk (first, last, ci, data, dlen);
	if (combining && pid == s. packetId)
	   combinerPacket (idx, s, first, last, ci, data, dlen, L - 3);
	if (dlen == 0) {
	   s. padding ++;
	   return;
	}
Assembly &a = s. as [pid];
//	continuity is only a diagnostic here: the data group has its own CRC
	if (a. lastCI >= 0 && ci == a. lastCI)
	   return;				// repeated packet
	if (a. lastCI >= 0 && ci != ((a. lastCI + 1) & 7))
	   s. ciJumps ++;
	a. lastCI = ci;
	if (first) {
	   a. buf. clear ();
	   a. inUnit = true;
	}
	else
	if (!a. inUnit) {
	   s. orphans ++;
	   return;
	}
	a. buf. insert (a. buf. end (), data, data + dlen);
	if (a. buf. size () > 65536) {		// runaway: drop it
	   a. buf. clear ();
	   a. inUnit = false;
	   s. malformed ++;
	   return;
	}
	if (last) {
	   finishUnit (idx, s, a, pid);
	   a. buf. clear ();
	   a. inUnit = false;
	}
}

bool	DataHub::finishUnit (int idx, Stream &s, Assembly &a, int pid,
	                         bool combined) {
	(void)combined;				// counted by the caller
	s. units ++;
	s. unitsPerId [pid] ++;
bool	onAnnounced	= (pid == s. packetId);
	if (!onAnnounced)
	   s. unitsOtherId ++;
	if (dumpFile != nullptr) {			// the raw unit, whatever it is
	   fprintf (dumpFile, "G %lld %d %d %03X %u %u ",
	            (long long)time (nullptr), idx, pid, s. appId, s. frames,
	            (unsigned)a. buf. size ());
	   for (size_t i = 0; i < a. buf. size (); i ++)
	      fprintf (dumpFile, "%02X", a. buf [i]);
	   fputc ('\n', dumpFile);
	   if ((++ dumpCount & 7) == 0)
	      fflush (dumpFile);
	}
DataGroup g;
	if (!parseDataGroup (a. buf. data (), a. buf. size (), g)) {
	   s. groupsUnparsed ++;
	   return false;
	}
	s. byType [g. type] ++;
	if (!g. crcPresent)
	   s. groupsNoCrc ++;
	else
	if (g. crcOk)
	   s. groupsOk ++;
	else
	   s. groupsBad ++;
char line [160];
	snprintf (line, sizeof (line),
	          "stream %d  type %2u  %s%s  seg %s%u  tid %s  %u bytes",
	          idx, g. type,
	          g. crcPresent ? (g. crcOk ? "CRC ok  " : "CRC BAD ") : "no CRC  ",
	          "",
	          g. hasSegment ? (g. lastSegment ? "last " : "     ") : "-",
	          g. hasSegment ? g. segmentNumber : 0u,
	          g. hasTransportId ? "yes" : "-",
	          (unsigned)g. payload. size ());
	recent. push_back (line);
	while (recent. size () > 12)
	   recent. pop_front ();
//	only groups that passed their CRC (or have none) go on
	if (g. crcPresent && !g. crcOk)
	   return false;
	if (!onAnnounced)
	   return false;			// not the announced packet id (FEC? other use?)
	g. stream	= idx;
	g. appId	= s. appId;
	g. domain	= s. domain;
	g. packetId	= pid;
	g. raw		= a. buf;
	if (sink)
	   sink (idx, g);
	return true;
}

std::string DataHub::report () {
std::lock_guard<std::mutex> g (lock);
std::string	r;
char	b [256];
bool	any = false;
	for (int i = 0; i < 4; i ++) {
	   Stream &s = streams [i];
	   if (!s. configured)
	      continue;
	   any = true;
	   snprintf (b, sizeof (b),
	             "Stream %d: %s, application 0x%03X (%s), domain %d%s\n",
	             i, s. packetMode ? "packet mode" : "synchronous",
	             s. appId, applicationName (s. domain, s. appId). c_str (),
	             s. domain, s. fec ? ", FEC announced" : "");
	   r += b;
	   snprintf (b, sizeof (b), "   %u frames, %llu bytes\n",
	             s. frames, (unsigned long long)s. bytes);
	   r += b;
	   if (!s. packetMode) {
	      r += "   synchronous data stream: not decoded yet\n";
	      continue;
	   }
	   snprintf (b, sizeof (b), "   packet id %d, packet length %d (SDC)\n",
	             s. packetId, s. packetLength);
	   r += b;
	   if (s. sizeMode < 0) {
	      snprintf (b, sizeof (b),
	                "   packet size: undecided (CRC ok as total %u / as data field %u of %u tried)\n",
	                s. scoreTotal, s. scorePlus3, s. tried);
	      r += b;
	      continue;
	   }
	   snprintf (b, sizeof (b), "   packet size: %d bytes (SDC value %s)\n",
	             s. packetLength + (s. sizeMode == 1 ? 3 : 0),
	             s. sizeMode == 1 ? "+ 3" : "as is");
	   r += b;
	   snprintf (b, sizeof (b),
	             "   packets %u: CRC ok %u, CRC bad %u, padding %u, malformed %u,"
	             " orphan %u, continuity jumps %u, frame remainder %u\n",
	             s. packets, s. crcOk, s. crcBad, s. padding, s. malformed,
	             s. orphans, s. ciJumps, s. badRemainder);
	   r += b;
	   snprintf (b, sizeof (b),
	             "   data units %u: groups CRC ok %u, CRC bad %u, no CRC %u, unreadable %u\n",
	             s. units, s. groupsOk, s. groupsBad, s. groupsNoCrc,
	             s. groupsUnparsed);
	   r += b;
	   snprintf (b, sizeof (b),
	             "   packets per packet id: 0:%u 1:%u 2:%u 3:%u;"
	             " data units per id: 0:%u 1:%u 2:%u 3:%u (%u not on id %d)\n",
	             s. packetsPerId [0], s. packetsPerId [1], s. packetsPerId [2],
	             s. packetsPerId [3], s. unitsPerId [0], s. unitsPerId [1],
	             s. unitsPerId [2], s. unitsPerId [3], s. unitsOtherId,
	             s. packetId);
	   r += b;
	   if (s. cb. passes > 0 || s. cb. completed > 0) {
	      snprintf (b, sizeof (b),
	                "   carousel combining: %u repetitions followed, %u units"
	                " completed by it, %u rejected by CRC, %u conflicts\n",
	                s. cb. passes, s. cb. completed, s. cb. rejected,
	                s. cb. conflicts);
	      r += b;
	   }
	   if (s. dc. nodeCount () > 0)
	      r += s. dc. stats ();
	   if (!s. byType. empty ()) {
	      r += "   data group types:";
	      for (auto &t : s. byType) {
	         snprintf (b, sizeof (b), "  %d x%u", t. first, t. second);
	         r += b;
	      }
	      r += "\n";
	   }
	}
	if (!any)
	   r += "No data services announced in the SDC (yet).\n";
	if (!recent. empty ()) {
	   r += "\nLatest data groups:\n";
	   for (auto &l : recent)
	      r += "   " + l + "\n";
	}
	r += "\n" + diagData. report ();
	return r;
}

void	DataHub::setCombining (bool on) {
std::lock_guard<std::mutex> g (lock);
	combining	= on;
}

/*
 *	Carousel combining
 */
namespace {
//	the length of a data unit that is known from its first packet: only
//	MOT groups say how long they are (segmentation header). 0: not known
size_t	expectedUnitLength (const uint8_t *d, size_t n) {
	if (n < 2)
	   return 0;
bool	ext	= d [0] & 0x80;
bool	crc	= d [0] & 0x40;
bool	seg	= d [0] & 0x20;
bool	ua	= d [0] & 0x10;
int	type	= d [0] & 0x0F;
size_t	h	= 2;
	if (ext)
	   h	+= 2;
	if (seg)
	   h	+= 2;
	if (ua) {
	   if (n <= h)
	      return 0;
	   h	+= 1 + (d [h] & 0x0F);
	}
	if (type == 3 || type == 4 || type == 6 || type == 7) {
	   if (n < h + 2)
	      return 0;
	   size_t segSize	= ((d [h] & 0x1F) << 8) | d [h + 1];
	   return h + 2 + segSize + (crc ? 2 : 0);
	}
	return 0;
}

uint64_t	unitKey (const uint8_t *d, size_t n) {
uint64_t h	= 1469598103934665603ULL;
	for (size_t i = 0; i < n; i ++) {
	   h	^= (i == 1) ? 0 : d [i];	// byte 1 changes with every repetition
	   h	*= 1099511628211ULL;
	}
	return h;
}

const size_t	maxCands	= 96;
const size_t	maxCandBytes	= 4 * 1024 * 1024;
const int	maxUnitPackets	= 2048;
}

//	called for every good packet of the announced packet id (also padding)
void	DataHub::combinerPacket (int idx, Stream &s, bool first, bool last,
	                            int ci, const uint8_t *data, size_t dlen,
	                            size_t fullLen) {
Combiner &cb	= s. cb;
//	where this packet is in the sequence of the packets of this packet id:
//	the continuity index counts them all (mod 8); the bad slots since the
//	previous good one say how many of the missing ones were really ours
	if (!cb. haveS) {
	   cb. haveS	= true;
	   cb. S		= 0;
	}
	else {
	   int delta	= (ci - cb. lastCI) & 7;
//	delta 1 is the next packet: nothing lost. In general (delta - 1) mod 8
//	packets are missing, or that plus a multiple of 8 when more bad slots
//	between the two say so
	   int lostMin	= (delta + 7) & 7;
	   if (delta == 0 && cb. badSince < 7)
	      return;				// a repetition of the same packet
	   int64_t lost	= lostMin;
	   if ((int64_t)cb. badSince > lostMin)
	      lost	+= 8 * (((int64_t)cb. badSince - lostMin) / 8);
	   cb. S	+= 1 + lost;
	}
	cb. lastCI	= ci;
	cb. badSince	= 0;
	if (dlen == 0)					// padding packet
	   return;
Pass &p	= cb. pass;
	if (first) {
	   if (p. active)
	      combinerEndPass (idx, s, s. packetId);	// its last packet was lost
	   p		= Pass ();
	   if (last || dlen < 4)
	      return;				// a one packet unit gains nothing
	   p. active	= true;
	   p. key	= unitKey (data, dlen);
	   p. sFirst	= cb. S;
	   p. first. assign (data, data + dlen);
	   Cand &c	= cb. cands [p. key];
	   c. used	= s. frames;
	   if (c. K <= 0) {
	      size_t e	= expectedUnitLength (data, dlen);
	      if (e > 0 && fullLen > 0)
	         c. K	= (int)((e + fullLen - 1) / fullLen);
	      if (c. K > maxUnitPackets)
	         c. K	= -1;
	   }
	   p. K		= c. K;
	   cb. passes ++;
//	keep the store small
	   if (cb. cands. size () > maxCands) {
	      auto victim	= cb. cands. end ();
	      for (auto it = cb. cands. begin (); it != cb. cands. end (); ++ it)
	         if (it -> first != p. key && (victim == cb. cands. end () ||
	             it -> second. used < victim -> second. used))
	            victim	= it;
	      if (victim != cb. cands. end ())
	         cb. cands. erase (victim);
	   }
	   for ( ; ; ) {				// and its memory bounded
	      size_t total	= 0;
	      for (auto &e : cb. cands)
	         for (auto &v : e. second. mid)
	            total	+= v. size ();
	      if (total <= maxCandBytes || cb. cands. size () < 2)
	         break;
	      auto victim	= cb. cands. end ();
	      for (auto it = cb. cands. begin (); it != cb. cands. end (); ++ it)
	         if (it -> first != p. key && (victim == cb. cands. end () ||
	             it -> second. used < victim -> second. used))
	            victim	= it;
	      if (victim == cb. cands. end ())
	         break;
	      cb. cands. erase (victim);
	   }
	   return;
	}
	if (!p. active) {
	   cb. orphans ++;
	   return;
	}
int64_t pos	= cb. S - p. sFirst;
	if (pos <= 0 || pos >= maxUnitPackets) {
	   cb. conflicts ++;
	   p		= Pass ();
	   return;
	}
	if (p. K > 0 && pos >= p. K) {		// the unit is over: its last packet
	   combinerEndPass (idx, s, s. packetId);	// was lost, this is another unit
	   p		= Pass ();
	   cb. orphans ++;
	   return;
	}
	if ((int64_t)p. pk. size () <= pos) {
	   p. pk. resize ((size_t)pos + 1);
	   p. have. resize ((size_t)pos + 1, 0);
	}
	p. pk [(size_t)pos]	= std::vector<uint8_t> (data, data + dlen);
	p. have [(size_t)pos]	= 1;
	if (last) {
	   p. lastSeen	= true;
	   p. last		= p. pk [(size_t)pos];
	   int knew	= (int)pos + 1;
	   Cand &c	= cb. cands [p. key];
	   if (c. K > 0 && c. K != knew) {	// what we expected was wrong
	      cb. conflicts ++;
	      c		= Cand ();
	   }
	   c. K		= knew;
	   p. K		= knew;
	   combinerEndPass (idx, s, s. packetId);
	   p		= Pass ();
	}
}

//	the end of one repetition of a unit: keep its packets, and if now
//	all of them are known, assemble the unit and let the CRC decide
void	DataHub::combinerEndPass (int idx, Stream &s, int pid) {
Combiner &cb	= s. cb;
Pass	&p	= cb. pass;
	if (!p. active)
	   return;
auto it	= cb. cands. find (p. key);
	if (it == cb. cands. end ())
	   return;
Cand	&c	= it -> second;
int	K	= c. K > 0 ? c. K : p. K;
	if (K < 3)
	   return;					// nothing in between to combine
	if ((int)c. mid. size () != K) {		// first use, or K was learned
	   c. mid. assign ((size_t)K, std::vector<uint8_t> ());
	   c. have. assign ((size_t)K, 0);
	   c. delivered	= false;
	}
	c. used	= s. frames;
//	keep the packets of this repetition. If one differs from what we hold,
//	the unit has changed on the air: start again with this repetition only
bool	changed	= false;
	for (int i = 1; i < K - 1 && i < (int)p. pk. size (); i ++)
	   if (p. have [(size_t)i] && c. have [(size_t)i] &&
	       c. mid [(size_t)i] != p. pk [(size_t)i])
	      changed	= true;
	if (changed) {
	   c. mid. assign ((size_t)K, std::vector<uint8_t> ());
	   c. have. assign ((size_t)K, 0);
	   c. delivered	= false;
	}
bool	thisComplete = p. lastSeen;
	for (int i = 1; i < K - 1; i ++) {
	   bool got	= i < (int)p. pk. size () && p. have [(size_t)i];
	   if (got) {
	      c. mid [(size_t)i]	= p. pk [(size_t)i];
	      c. have [(size_t)i]	= 1;
	   }
	   else
	      thisComplete	= false;
	}
	if (!p. lastSeen)				// no last packet from this repetition:
	   return;				// no group CRC to check against
bool	all	= true;
	for (int i = 1; i < K - 1; i ++)
	   if (!c. have [(size_t)i])
	      all	= false;
	if (!all)
	   return;
Assembly tmp;
	tmp. buf	= p. first;
	for (int i = 1; i < K - 1; i ++)
	   tmp. buf. insert (tmp. buf. end (), c. mid [(size_t)i]. begin (),
	                                   c. mid [(size_t)i]. end ());
	tmp. buf. insert (tmp. buf. end (), p. last. begin (), p. last. end ());
DataGroup g;
bool	crcOk	= parseDataGroup (tmp. buf. data (), tmp. buf. size (), g) &&
	                 g. crcPresent && g. crcOk;
	if (thisComplete) {
//	this repetition was complete by itself: the normal path has delivered
//	it. Remember that, so a later damaged repetition does not deliver again
	   c. delivered	= crcOk;
	   return;
	}
	if (c. delivered)
	   return;
	if (!crcOk) {					// a mixture of versions? start over
	   cb. rejected ++;
	   c. mid. assign ((size_t)K, std::vector<uint8_t> ());
	   c. have. assign ((size_t)K, 0);
	   for (int i = 1; i < K - 1 && i < (int)p. pk. size (); i ++)
	      if (p. have [(size_t)i]) {
	         c. mid [(size_t)i]	= p. pk [(size_t)i];
	         c. have [(size_t)i]	= 1;
	      }
	   return;
	}
	c. delivered	= true;
	cb. completed ++;
	s. combinedUnits ++;
	finishUnit (idx, s, tmp, pid, true);
}

}	// namespace dxdata
