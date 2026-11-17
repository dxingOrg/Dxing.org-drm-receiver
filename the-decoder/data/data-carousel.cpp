#
/*
 *    Dxing.org DRM receiver - data broadcast packets from the repetitions
 *    of a carousel (see the header)
 *    Released under the GNU General Public License, version 2 or later.
 */
#include	"data-carousel.h"
#include	<stdio.h>
#include	<string.h>

namespace dxdata {

static const size_t	MAX_NODES	= 30000;
static const size_t	MAX_RUNS	= 600;

//	CRC-16 CCITT, start value FFFF, inverted (as in the DRM packets)
uint16_t dbCrc16 (const uint8_t *p, size_t n) {
uint16_t c = 0xFFFF;
	for (size_t i = 0; i < n; i ++) {
	   c ^= (uint16_t)(p [i] << 8);
	   for (int k = 0; k < 8; k ++)
	      c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
	}
	return (uint16_t)~c;
}

static uint64_t fnv (const std::vector<uint8_t> &v) {
uint64_t h = 1469598103934665603ULL;
	for (uint8_t b : v) {
	   h ^= b;
	   h *= 1099511628211ULL;
	}
	return h;
}

	DbCarousel::DbCarousel	() {
	clear ();
}

	DbCarousel::~DbCarousel	() {
}

void	DbCarousel::clear	() {
	capacity	= 45;
	nodeIds. clear ();
	contents. clear ();
	mergeable. clear ();
	runs. clear ();
	cur. v. clear ();
	haveCur		= false;
	lastCi		= 0;
	badSince	= 0;
	delivered. clear ();
	ready. clear ();
	unitsCompleted	= 0;
}

void	DbCarousel::setCapacity (int d) {
	if (d > 0 && d < 300)
	   capacity = d;
}

//	the key holds the useful bytes; a short (last) packet differs from a
//	full one by its length. A packet of one repeated byte (padding) has no
//	value for recognising a place: it is kept but never used to align.
int	DbCarousel::nodeFor (const uint8_t *d, size_t n) {
std::string key ((const char *)d, n);
auto it = nodeIds. find (key);
	if (it != nodeIds. end ())
	   return it -> second;
	if (contents. size () >= MAX_NODES)
	   clear ();			// runaway: start again
int id = (int)contents. size ();
bool varied = false;
	for (size_t i = 1; i < n; i ++)
	   if (d [i] != d [0]) {
	      varied = true;
	      break;
	   }
	nodeIds [key] = id;
	contents. push_back (key);
	mergeable. push_back (varied && n >= 8);
	return id;
}

void	DbCarousel::slotOk (bool first, bool last, int ci,
	                         const uint8_t *data, size_t dlen) {
	if (dlen == 0) {
	   slotBad ();
	   return;
	}
int n = nodeFor (data, dlen);
Entry e;
	e. node	= n;
	e. first = first;
	e. last	= last;
	if (haveCur) {
//	the packets between two good ones that were lost: each of them used
//	one continuity number, so the numbers tell how many (when < 8)
	   if (badSince < 8 && ci == ((lastCi + 1 + badSince) & 7)) {
Entry hole;
	      hole. node = -1; hole. first = false; hole. last = false;
	      for (int i = 0; i < badSince; i ++)
	         cur. v. push_back (hole);
	   }
	   else
	      closeCur ();
	}
	if (!haveCur) {
	   cur. v. clear ();
	   haveCur = true;
	}
	cur. v. push_back (e);
	lastCi	= ci;
	badSince = 0;
}

void	DbCarousel::slotBad	() {
	if (!haveCur)
	   return;
	badSince ++;
	if (badSince >= 8)
	   closeCur ();
}

void	DbCarousel::closeCur	() {
	if (haveCur && !cur. v. empty ()) {
Run r = cur;
	   haveCur = false;
	   cur. v. clear ();
	   badSince = 0;
	   addRun (r);
	}
	haveCur = false;
	badSince = 0;
}

//	the places of the packets in two runs agree where they share packets
void	DbCarousel::indexRun (Run &r) {
	r. nodes. clear ();
	for (const Entry &e : r. v)
	   if (e. node >= 0 && mergeable [e. node])
	      r. nodes. insert (e. node);
}

bool	DbCarousel::align (const Run &a, const Run &b, int &off) {
bool shared = false;
	for (const Entry &e : b. v)
	   if (e. node >= 0 && a. nodes. count (e. node)) {
	      shared = true;
	      break;
	   }
	if (!shared)
	   return false;			// the usual case: nothing in common
std::unordered_map<int, int> pos;
std::set<int> twice;
	for (size_t i = 0; i < a. v. size (); i ++) {
int nd = a. v [i]. node;
	   if (nd < 0 || !mergeable [nd])
	      continue;
	   if (pos. count (nd))
	      twice. insert (nd);
	   else
	      pos [nd] = (int)i;
	}
bool have = false;
int o = 0;
	for (size_t j = 0; j < b. v. size (); j ++) {
int nd = b. v [j]. node;
	   if (nd < 0 || !mergeable [nd] || twice. count (nd))
	      continue;
auto it = pos. find (nd);
	   if (it == pos. end ())
	      continue;
int d = it -> second - (int)j;
	   if (!have) {
	      o = d;
	      have = true;
	   }
	   else
	   if (d != o)
	      return false;			// the two disagree: not the same place
	}
	if (!have)
	   return false;
//	every place where both have a packet must hold the same packet
	for (size_t j = 0; j < b. v. size (); j ++) {
int i = (int)j + o;
	   if (i < 0 || i >= (int)a. v. size ())
	      continue;
	   if (b. v [j]. node >= 0 && a. v [i]. node >= 0 &&
	       b. v [j]. node != a. v [i]. node)
	      return false;
	}
	off = o;
	return true;
}

void	DbCarousel::mergeInto (Run &a, const Run &b, int off) {
int lo = (off < 0) ? off : 0;
int hi = (int)a. v. size ();
	if ((int)b. v. size () + off > hi)
	   hi = (int)b. v. size () + off;
Run r;
Entry hole;
	hole. node = -1; hole. first = false; hole. last = false;
	r. v. assign (hi - lo, hole);
	for (size_t i = 0; i < a. v. size (); i ++)
	   r. v [(int)i - lo] = a. v [i];
	for (size_t j = 0; j < b. v. size (); j ++) {
Entry &t = r. v [(int)j + off - lo];
	   if (b. v [j]. node < 0)
	      continue;
	   if (t. node < 0)
	      t = b. v [j];
	   else {
	      t. first |= b. v [j]. first;
	      t. last  |= b. v [j]. last;
	   }
	}
	indexRun (r);
	a = r;
}

void	DbCarousel::addRun (const Run &r) {
Run m = r;
	indexRun (m);
bool merged = true;
	while (merged) {
	   merged = false;
	   for (size_t k = 0; k < runs. size (); k ++) {
int off;
	      if (align (runs [k], m, off)) {
Run base = runs [k];
	         mergeInto (base, m, off);
	         m = base;
	         runs. erase (runs. begin () + k);
	         merged = true;
	         break;
	      }
	   }
	}
	runs. push_back (m);
	scan (m);
//	too many runs: forget the shortest ones that carry no start
	while (runs. size () > MAX_RUNS) {
size_t worst = 0;
size_t wl = (size_t)-1;
	   for (size_t k = 0; k < runs. size (); k ++)
	      if (runs [k]. v. size () < wl) {
	         wl = runs [k]. v. size ();
	         worst = k;
	      }
	   runs. erase (runs. begin () + worst);
	}
}

//	size of the whole data broadcast packet that starts with this node
bool	DbCarousel::unitSize (int node, size_t &total) {
const std::string &c = contents [node];
	if (c. size () < 12 || (uint8_t)c [0] != 0x49 || (uint8_t)c [1] != 0x59)
	   return false;
uint64_t v = 0;
	for (int i = 0; i < 6; i ++)
	   v = (v << 8) | (uint8_t)c [2 + i];
//	48 bits after the start marker: 4 service type, 3 resource, 1 extended,
//	10 number, 4 update, 2 type, 10 count, 2 FEC, 12 length
int svc	= (int)((v >> 44) & 0xF);
int ext	= (int)((v >> 40) & 1);
size_t len = (size_t)(v & 0xFFF);
	total = 2 + 6 + (ext ? 2 : 0) + (svc == 1 ? 2 : 0) + len + 2;
	return true;
}

void	DbCarousel::scan (const Run &r) {
	for (size_t i = 0; i < r. v. size (); i ++) {
	   if (r. v [i]. node < 0 || !r. v [i]. first)
	      continue;
size_t total;
	   if (!unitSize (r. v [i]. node, total))
	      continue;
size_t need = (total + capacity - 1) / capacity;
	   if (need == 0 || i + need > r. v. size ())
	      continue;
	   if (!r. v [i + need - 1]. last)
	      continue;
std::vector<uint8_t> u;
bool whole = true;
	   for (size_t k = 0; k < need; k ++) {
	      if (r. v [i + k]. node < 0) {
	         whole = false;
	         break;
	      }
const std::string &c = contents [r. v [i + k]. node];
	      u. insert (u. end (), c. begin (), c. end ());
	   }
	   if (!whole || u. size () != total)
	      continue;
	   if (dbCrc16 (u. data (), total - 2) !=
	       (uint16_t)((u [total - 2] << 8) | u [total - 1]))
	      continue;			// does not fit together: not accepted
uint64_t h = fnv (u);
	   if (delivered. insert (h). second) {
	      ready. push_back (u);
	      unitsCompleted ++;
	   }
	}
}

void	DbCarousel::endFrame	() {
	if (haveCur)
	   scan (cur);
}

bool	DbCarousel::take (std::vector<uint8_t> &unit) {
	if (ready. empty ())
	   return false;
	unit = ready. front ();
	ready. erase (ready. begin ());
	return true;
}

std::string DbCarousel::stats () const {
char b [160];
size_t longest = 0;
	for (auto &r : runs)
	   if (r. v. size () > longest)
	      longest = r. v. size ();
	snprintf (b, sizeof (b),
	   "   carousel assembly: %u distinct packets, %u runs (longest %u), "
	   "%u data broadcast packets completed\n", (unsigned)contents. size (),
	   (unsigned)runs. size (), (unsigned)longest, (unsigned)unitsCompleted);
	return std::string (b);
}

}
