#
/*
 *    Dxing.org DRM receiver - service information (see service-info.h)
 *    Released under the GNU General Public License, version 2 or later.
 */
#include	"service-info.h"
#include	<stdio.h>
#include	<string.h>

namespace dxdata {

//	reads bits (one per byte, MSB first); reading past the end gives 0
//	bits and sets "bad"
struct BitReader {
	const uint8_t	*b;
	int		n;
	int		pos;
	bool		bad;
	BitReader	(const uint8_t *bits, int nbits):
	   b (bits), n (nbits), pos (0), bad (false) {}
	int	left	() const { return n - pos; }
	uint32_t get	(int k) {
	   uint32_t v = 0;
	   for (int i = 0; i < k; i ++) {
	      if (pos >= n) { bad = true; v <<= 1; }
	      else v = (v << 1) | (b [pos] & 1);
	      pos ++;
	   }
	   return v;
	}
	int	sgn	(int k) {
	   int32_t v = (int32_t)get (k);
	   if (v & (1 << (k - 1)))
	      v -= (1 << k);
	   return v;
	}
};

static std::string printable (const std::string &s) {
std::string r;
	for (unsigned char c : s)
	   r += (c >= 32 && c < 127) ? (char)c : '?';
	return r;
}

bool	AfsMux::operator == (const AfsMux &o) const {
	return sync == o. sync && layer == o. layer &&
	       restricted == o. restricted && serviceFlags == o. serviceFlags &&
	       hasRegSched == o. hasRegSched && regionId == o. regionId &&
	       scheduleId == o. scheduleId && freqsKHz == o. freqsKHz;
}

bool	Schedule::operator == (const Schedule &o) const {
	return id == o. id && days == o. days && startMin == o. startMin &&
	       durationMin == o. durationMin;
}

bool	OtherSvc::operator == (const OtherSvc &o) const {
	return announce == o. announce && announceId == o. announceId &&
	       hasRegSched == o. hasRegSched && regionId == o. regionId &&
	       scheduleId == o. scheduleId && sameService == o. sameService &&
	       systemId == o. systemId && hasServiceId == o. hasServiceId &&
	       serviceId == o. serviceId && freqs == o. freqs &&
	       parsed == o. parsed && raw == o. raw;
}

//
//	helpers
//
void	mjdToDate	(int mjd, int &y, int &m, int &d) {
//	Fliegel - Van Flandern; the Julian day number of MJD 51544 is 2451545
long l = (long)mjd + 2400001L + 68569L;
long n = 4 * l / 146097;
	l = l - (146097 * n + 3) / 4;
long i = 4000 * (l + 1) / 1461001;
	l = l - 1461 * i / 4 + 31;
long j = 80 * l / 2447;
	d = (int)(l - 2447 * j / 80);
	l = j / 11;
	m = (int)(j + 2 - 12 * l);
	y = (int)(100 * (n - 49) + i + l);
}

std::string formatDateTime (int mjd, int hour, int minute, int addMinutes) {
int total = hour * 60 + minute + addMinutes;
int dayShift = 0;
	while (total < 0) { total += 1440; dayShift --; }
	while (total >= 1440) { total -= 1440; dayShift ++; }
int y, m, d;
	mjdToDate (mjd + dayShift, y, m, d);
char b [48];
	snprintf (b, sizeof (b), "%04d-%02d-%02d %02d:%02d", y, m, d,
	                         total / 60, total % 60);
	return std::string (b);
}

std::string languageName (int code) {
static const char *t [16] = {"not specified", "Arabic", "Bengali",
	   "Chinese (Mandarin)", "Dutch", "English", "French", "German", "Hindi",
	   "Japanese", "Javanese", "Korean", "Portuguese", "Russian", "Spanish",
	   "other"};
	return (code >= 0 && code < 16) ? t [code] : "?";
}

std::string programmeTypeName (int code) {
static const char *t [32] = {"not specified", "News", "Current affairs",
	   "Information", "Sport", "Education", "Drama", "Culture", "Science",
	   "Varied", "Pop music", "Rock music", "Easy listening",
	   "Light classical", "Serious classical", "Other music", "Weather",
	   "Finance / business", "Children's", "Social affairs", "Religion",
	   "Phone-in", "Travel", "Leisure", "Jazz", "Country", "National music",
	   "Oldies", "Folk", "Documentary", "ALARM announcement active",
	   "reserved (receivers skip this multiplex)"};
	return (code >= 0 && code < 32) ? t [code] : "?";
}

std::string systemName (int id) {
	switch (id) {
	   case 0:	return "DRM";
	   case 1:	return "AMSS";
	   case 2:	return "AM";
	   case 6: case 7: case 8:
			return "FM";
	   case 12:	return "DAB/CDR (12)";
	   default:	return "system " + std::to_string (id);
	}
}

std::string scheduleText (const Schedule &s) {
static const char *dn [7] = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
std::string r;
	for (int i = 0; i < 7; i ++)
	   if (s. days & (0x40 >> i)) {
	      if (!r. empty ()) r += ' ';
	      r += dn [i];
	   }
	if (r. empty ()) r = "no day";
char b [64];
	snprintf (b, sizeof (b), " from %02d:%02d UTC, %d min", s. startMin / 60,
	                         s. startMin % 60, s. durationMin);
	return r + b;
}

//	The order of the 10 announcement bits is an ASSUMPTION (from the
//	Chinese text of GY/T 423: travel, news, weather, warning/alarm are
//	bits 0 .. 3 counted from the least significant bit). It has not been
//	checked on a real signal; the window also shows the raw value.
std::string announcementTypes (int bits) {
static const char *n [4] = {"travel", "news", "weather",
	                            "WARNING/ALARM"};
std::string r;
	for (int i = 0; i < 10; i ++)
	   if (bits & (1 << i)) {
	      if (!r. empty ()) r += ", ";
	      r += (i < 4) ? std::string (n [i]) : "bit " + std::to_string (i);
	   }
	return r. empty () ? std::string ("-") : r;
}

//
//	the class
//
	SdcInfo::SdcInfo	() {
	clear ();
}

	SdcInfo::~SdcInfo	() {
}

void	SdcInfo::clear	() {
std::lock_guard<std::mutex> g (lock);
uint32_t c = data. counter + 1;
	data = InfoSnapshot ();
	data. alarmFac	= false;
	data. alarmSdc	= false;
	data. counter	= c;
	for (int i = 0; i < 16; i ++)
	   listVersion [i] = -1;
	facBlocks	= 0;
	lastAlarmFac	= 0;
	for (int i = 0; i < 4; i ++)
	   pend [i] = Pending ();
}

void	SdcInfo::touch	() {
	data. counter ++;
}

bool	SdcInfo::newVersion	(int type, int versionFlag) {
	if (listVersion [type & 15] == versionFlag)
	   return false;
	listVersion [type & 15] = versionFlag;
	return true;
}

void	SdcInfo::updateAlarmSdc	() {
bool a = false;
	for (auto &x : data. announcements)
	   if (x. switching & 0x08)		// assumption, see announcementTypes
	      a = true;
	if (a != data. alarmSdc) {
	   data. alarmSdc = a;
	   touch ();
	}
}

uint32_t SdcInfo::changeCounter	() {
std::lock_guard<std::mutex> g (lock);
	return data. counter;
}

bool	SdcInfo::alarmActive	() {
std::lock_guard<std::mutex> g (lock);
	return data. alarmSdc || (lastAlarmFac != 0 &&
	                          facBlocks - lastAlarmFac < 12);
}

InfoSnapshot SdcInfo::snapshot	() {
std::lock_guard<std::mutex> g (lock);
InfoSnapshot s = data;
	s. alarmFac = (lastAlarmFac != 0 && facBlocks - lastAlarmFac < 12);
	return s;
}

void	SdcInfo::setFacService	(int shortId, uint32_t serviceId, bool isAudio,
	                         int language, int programmeType) {
std::lock_guard<std::mutex> g (lock);
	if (shortId < 0 || shortId > 3)
	   return;
const uint32_t CONFIRM	= 3;		// identical records in a row
const uint32_t FORGET	= 80;		// FAC blocks (about 30 seconds)
bool alarmBefore = lastAlarmFac != 0 && facBlocks - lastAlarmFac < 12;
	facBlocks ++;
Pending &p = pend [shortId];
	if (p. valid && p. serviceId == serviceId && p. isAudio == isAudio &&
	    p. language == language && p. programmeType == programmeType)
	   p. count ++;
	else {
	   p. valid		= true;
	   p. serviceId	= serviceId;
	   p. isAudio	= isAudio;
	   p. language	= language;
	   p. programmeType	= programmeType;
	   p. count		= 1;
	}
SvcEntry &e = data. service [shortId];
	if (p. count >= (int)CONFIRM) {
	   if (!e. haveFac || e. serviceId != serviceId || e. isAudio != isAudio ||
	       e. language != language || e. programmeType != programmeType) {
	      e. haveFac		= true;
	      e. serviceId		= serviceId;
	      e. isAudio		= isAudio;
	      e. language		= language;
	      e. programmeType	= programmeType;
	      touch ();
	   }
	   e. lastBlock	= facBlocks;
	   if (programmeType == 30)
	      lastAlarmFac = facBlocks;	// only a confirmed record may alarm
	}
//	a service that is not seen any more (noise, or another station)
	for (int i = 0; i < 4; i ++)
	   if (data. service [i]. haveFac &&
	       facBlocks - data. service [i]. lastBlock > FORGET) {
	      data. service [i]. haveFac = false;
	      pend [i] = Pending ();
	      touch ();
	   }
bool alarmNow = lastAlarmFac != 0 && facBlocks - lastAlarmFac < 12;
	if (alarmBefore != alarmNow)
	   touch ();
}

void	SdcInfo::handleEntity	(int type, int versionFlag,
	                                 const uint8_t *bits, int nbits) {
std::lock_guard<std::mutex> g (lock);
BitReader b (bits, nbits);
	switch (type) {
	   case 1: {			// label
	      int sid = b. get (2);
	      b. get (2);
std::string s;
	      for (int i = 4; i + 8 <= nbits && s. size () < 64; i += 8) {
	         int c = b. get (8);
	         if (c == 0)
	            break;
	         s += (char)c;
	      }
SvcEntry &e = data. service [sid];
	      if (!e. haveLabel || e. label != s) {
	         e. haveLabel = true;
	         e. label = s;
	         touch ();
	      }
	      return;
	   }

	   case 12: {			// language and country
	      if (nbits < 44)
	         return;
int sid = b. get (2);
	      b. get (2);
std::string l, c;
	      for (int i = 0; i < 3; i ++) l += (char)b. get (8);
	      for (int i = 0; i < 2; i ++) c += (char)b. get (8);
	      l = printable (l);
	      c = printable (c);
SvcEntry &e = data. service [sid];
	      if (!e. haveLang || e. lang3 != l || e. country2 != c) {
	         e. haveLang = true;
	         e. lang3 = l;
	         e. country2 = c;
	         touch ();
	      }
	      return;
	   }

	   case 3: {			// AFS: multiplex frequencies
AfsMux m;
	      m. sync		= b. get (1);
	      m. layer		= b. get (1);
	      m. restricted	= b. get (1);
	      m. hasRegSched	= b. get (1);
	      if (m. restricted) {
	         m. serviceFlags = b. get (4);
	         b. get (4);
	      }
	      if (m. hasRegSched) {
	         m. regionId	= b. get (4);
	         m. scheduleId	= b. get (4);
	      }
	      while (b. left () >= 16) {
	         b. get (1);
	         m. freqsKHz. push_back (b. get (15));
	      }
	      if (b. bad)
	         return;
	      if (newVersion (3, versionFlag)) {
	         data. afs. clear ();
	         touch ();
	      }
	      for (auto &x : data. afs)
	         if (x == m)
	            return;
	      data. afs. push_back (m);
	      touch ();
	      return;
	   }

	   case 4: {			// AFS: schedule
Schedule s;
	      s. id		= b. get (4);
	      s. days		= b. get (7);
	      s. startMin	= b. get (11);
	      s. durationMin	= b. get (14);
	      if (b. bad)
	         return;
	      if (newVersion (4, versionFlag)) {
	         data. schedules. clear ();
	         touch ();
	      }
	      for (auto &x : data. schedules)
	         if (x == s)
	            return;
	      data. schedules. push_back (s);
	      touch ();
	      return;
	   }

	   case 6: {			// announcement support and switching
Announcement a;
	      a. serviceFlags	= b. get (4);
	      a. sameMux	= b. get (1);
	      a. id		= b. get (2);
	      b. get (1);
	      a. support	= b. get (10);
	      a. switching	= b. get (10);
	      if (b. bad)
	         return;
	      if (newVersion (6, versionFlag)) {
	         data. announcements. clear ();
	         touch ();
	      }
//	the switching flags change WITHOUT a new version flag: update in place
	      for (auto &x : data. announcements)
	         if (x. serviceFlags == a. serviceFlags &&
	             x. sameMux == a. sameMux && x. id == a. id) {
	            if (x. support != a. support || x. switching != a. switching) {
	               x. support = a. support;
	               x. switching = a. switching;
	               touch ();
	               updateAlarmSdc ();
	            }
	            return;
	         }
	      data. announcements. push_back (a);
	      touch ();
	      updateAlarmSdc ();
	      return;
	   }

	   case 7: {			// region (coarse)
Region r;
	      r. id	= b. get (4);
	      r. lat	= b. sgn (8);
	      r. lon	= b. sgn (9);
	      r. latExt	= b. get (7);
	      r. lonExt	= b. get (8);
	      while (b. left () >= 8)
	         r. ciraf. push_back (b. get (8));
	      if (b. bad)
	         return;
	      if (newVersion (7, versionFlag)) {
	         data. regions. clear ();
	         touch ();
	      }
	      for (auto &x : data. regions)
	         if (x. id == r. id) {
	            if (x. lat != r. lat || x. lon != r. lon ||
	                x. latExt != r. latExt || x. lonExt != r. lonExt ||
	                x. ciraf != r. ciraf) {
	               x = r;
	               touch ();
	            }
	            return;
	         }
	      data. regions. push_back (r);
	      touch ();
	      return;
	   }

	   case 8: {			// time and date, UTC
	      if (nbits < 28)
	         return;
TimeInfo t;
	      t. valid	= true;
	      t. mjd	= b. get (17);
	      t. hour	= b. get (5);
	      t. minute	= b. get (6);
	      if (nbits >= 36) {
	         b. get (2);
bool neg = b. get (1);
int off = b. get (5);
	         t. hasOffset	= true;
	         t. offsetHalfHours = neg ? -off : off;
	      }
	      if (t. hour > 23 || t. minute > 59)
	         return;
	      if (!data. time. valid || data. time. mjd != t. mjd ||
	          data. time. hour != t. hour || data. time. minute != t. minute ||
	          data. time. hasOffset != t. hasOffset ||
	          data. time. offsetHalfHours != t. offsetHalfHours) {
	         data. time = t;
	         touch ();
	      }
	      return;
	   }

	   case 11: {			// AFS: other services
OtherSvc o;
	      o. announce	= b. get (1);
	      o. announceId	= b. get (2);
	      o. hasRegSched	= b. get (1);
	      o. sameService	= b. get (1);
	      b. get (2);
	      o. systemId	= b. get (5);
	      if (o. hasRegSched) {
	         o. regionId	= b. get (4);
	         o. scheduleId	= b. get (4);
	      }
int idBits = (o. systemId == 0 || o. systemId == 1 ||
	                    o. systemId == 6 || o. systemId == 12) ? 24 :
	                    (o. systemId == 7 ? 16 : 0);
int unit = (o. systemId <= 2) ? 16 : 8;
int rest = b. left ();
//	the text does not say whether "same service" removes the service id:
//	try with the id first, then without; if neither fits keep the raw bits
bool withId = true;
	      if (rest < 0 || (rest - idBits) < 0 || (rest - idBits) % unit != 0) {
	         if (idBits > 0 && o. sameService && rest >= 0 && rest % unit == 0)
	            withId = false;
	         else if (idBits == 0 && rest >= 0 && rest % unit == 0)
	            withId = false;
	         else
	            o. parsed = false;
	      }
	      if (o. parsed) {
	         if (withId && idBits > 0) {
	            o. hasServiceId = true;
	            o. serviceId = b. get (idBits);
	         }
	         while (b. left () >= unit) {
	            if (unit == 16) {
	               b. get (1);
	               o. freqs. push_back (b. get (15));
	            } else
	               o. freqs. push_back (b. get (8));
	         }
	      } else {
	         char h [8];
	         for (int i = 0; i < nbits; i += 4) {
	            int v = 0;
	            for (int j = 0; j < 4; j ++)
	               v = (v << 1) | ((i + j < nbits) ? (bits [i + j] & 1) : 0);
	            snprintf (h, sizeof (h), "%X", v);
	            o. raw += h;
	         }
	      }
	      if (newVersion (11, versionFlag)) {
	         data. other. clear ();
	         touch ();
	      }
	      for (auto &x : data. other)
	         if (x == o)
	            return;
	      data. other. push_back (o);
	      touch ();
	      return;
	   }

	   case 13: {			// detailed region
	      if (nbits < 4)
	         return;
int id = b. get (4);
std::vector<DetailRect> v;
	      while (b. left () >= 48) {
DetailRect r;
	         b. get (1);
	         r. lat	= b. sgn (12);
	         r. lon	= b. sgn (13);
	         r. latExt	= b. get (11);
	         r. lonExt	= b. get (11);
	         v. push_back (r);
	      }
	      if (newVersion (13, versionFlag)) {
	         data. detailRegions. clear ();
	         touch ();
	      }
	      for (auto &x : data. detailRegions)
	         if (x. id == id) {
	            for (auto &r : v) {
bool have = false;
	               for (auto &y : x. rects)
	                  if (y == r)
	                     have = true;
	               if (!have) {
	                  x. rects. push_back (r);
	                  touch ();
	               }
	            }
	            return;
	         }
DetailRegion d;
	      d. id	= id;
	      d. rects	= v;
	      data. detailRegions. push_back (d);
	      touch ();
	      return;
	   }

	   default:
	      return;
	}
}

}
