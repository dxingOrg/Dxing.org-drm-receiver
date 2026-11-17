#
/*
 *    Dxing.org DRM receiver - service diagnostics (see service-diag.h)
 *    Released under the GNU General Public License, version 2 or later.
 */
#include	"service-diag.h"
#include	<stdio.h>
#include	<string.h>

namespace dxdata {

static const size_t	MAX_LINES	= 400;

	ServiceDiag::ServiceDiag	() {
	clear ();
}

	ServiceDiag::~ServiceDiag	() {
}

void	ServiceDiag::clear	() {
std::lock_guard<std::mutex> g (lock);
	resyncs		= 0;
	sdcBlocks	= 0;
	facBlocks	= 0;
	seq		= 0;
	entityCount. clear ();
	seen. clear ();
	for (int i = 0; i < 16; i ++)
	   typeLines [i] = 0;
	lines. clear ();
	linesFull	= false;
	for (int i = 0; i < 4; i ++) {
	   facSvc [i]	= FacSvc ();
	   slotTab [i]	= SlotInfo ();
	   audioCount [i]	= 0;
	   labelCount [i]	= 0;
	   labelDropped [i]	= 0;
	}
	numofStreams	= 0;
	audioChannel	= 0;
	chosenService	= 0;
	for (int i = 0; i < 4; i ++)
	   svcTab [i]	= SvcInfo ();
}

//	called with the lock held. A line is written once per sync cycle
//	(same key = same line), so the repeating SDC does not flood the log
void	ServiceDiag::addLine	(const std::string &key, const std::string &text) {
	if (!seen. insert (key). second)
	   return;
	if (lines. size () >= MAX_LINES) {
	   linesFull	= true;
	   return;
	}
char	b [64];
	snprintf (b, sizeof (b), "[sync %d, sdc %d, fac %d] ",
	                         resyncs, sdcBlocks, facBlocks);
	lines. push_back (std::string (b) + text);
	seq ++;
}

void	ServiceDiag::onResync	() {
std::lock_guard<std::mutex> g (lock);
	resyncs ++;
	seen. clear ();
	for (int i = 0; i < 16; i ++)
	   typeLines [i] = 0;
	sdcBlocks	= 0;
	facBlocks	= 0;
	if (lines. size () < MAX_LINES)
	   lines. push_back ("---- sync lost / restart ----");
}

void	ServiceDiag::onSdcBlock	(int afs) {
std::lock_guard<std::mutex> g (lock);
	sdcBlocks ++;
	(void)afs;
}

void	ServiceDiag::onSdcEntity	(int type, int versionFlag,
	                                 int lengthofBody, const uint8_t *bits) {
std::lock_guard<std::mutex> g (lock);
	entityCount [type] ++;
//	the entities that say something about services and streams
	if (!(type == 0 || type == 1 || type == 3 || type == 4 || type == 5 ||
	      type == 6 || type == 7 || type == 8 || type == 9 || type == 11 ||
	      type == 12 || type == 13 || type == 14))
	   return;
//	time (8) changes every minute: at most a few lines of each type per
//	sync cycle, so that the log is not flooded
	if (typeLines [type & 15] >= 12)
	   return;
int	nbits	= 8 * lengthofBody + 4;
	if (nbits < 0 || nbits > 8 * 64)
	   return;
std::string hex;
char	b [16];
	for (int i = 0; i < nbits; i += 4) {
	   int v = 0;
	   for (int j = 0; j < 4; j ++)
	      v = (v << 1) | ((i + j < nbits) ? (bits [i + j] & 1) : 0);
	   snprintf (b, sizeof (b), "%X", v);
	   hex += b;
	   if (i % 8 == 4)
	      hex += ' ';
	}
std::string key = "E" + std::to_string (type) + "v" +
	                  std::to_string (versionFlag) + hex;
	if (seen. find (key) == seen. end ())
	   typeLines [type & 15] ++;
std::string text = "SDC entity " + std::to_string (type) +
	                   " version " + std::to_string (versionFlag) +
	                   " length " + std::to_string (lengthofBody) +
	                   " bits(hex) " + hex;
	addLine (key, text);
}

void	ServiceDiag::onFacService	(int shortId, int serviceId, bool isAudio,
	                                 int language, int descriptor) {
std::lock_guard<std::mutex> g (lock);
	facBlocks ++;
	if (shortId < 0 || shortId > 3)
	   return;
FacSvc &f	= facSvc [shortId];
	f. count ++;
	f. serviceId	= serviceId;
	f. isAudio	= isAudio;
	f. language	= language;
	f. descriptor	= descriptor;
char	b [160];
	snprintf (b, sizeof (b),
	          "FAC service: short id %d, service id 0x%06X, %s, language %d, "
	          "descriptor %d", shortId, serviceId,
	          isAudio ? "audio" : "data", language, descriptor);
	addLine (std::string ("F") + b, b);
}

void	ServiceDiag::onLabel	(int shortId, const std::string &label,
	                         const std::string &verdict) {
std::lock_guard<std::mutex> g (lock);
	if (shortId >= 0 && shortId < 4) {
	   labelCount [shortId] ++;
	   if (verdict. compare (0, 7, "dropped") == 0)
	      labelDropped [shortId] ++;
	}
std::string text = "LABEL for short id " + std::to_string (shortId) +
	                   " \"" + label + "\": " + verdict;
	addLine ("L" + text, text);
}

void	ServiceDiag::setSlots	(int n, const SlotInfo *s, int channel,
	                         int chosen, const SvcInfo *sv) {
std::lock_guard<std::mutex> g (lock);
	numofStreams	= n;
	audioChannel	= channel;
	chosenService	= chosen;
	for (int i = 0; i < 4; i ++)
	   svcTab [i]	= sv [i];
	for (int i = 0; i < 4; i ++)
	   slotTab [i]	= s [i];
//	the table changing is itself worth a line
char	b [320];
std::string key = "T" + std::to_string (n);
std::string text = "STREAM TABLE: " + std::to_string (n) + " streams;";
	for (int i = 0; i < n && i < 4; i ++) {
	   snprintf (b, sizeof (b), " [%d: %s sid=%d stm=%d len=%d/%d]",
	             i, s [i]. soort == 0 ? "A" : "D",
	             s [i]. shortId, s [i]. streamId,
	             s [i]. lengthHigh, s [i]. lengthLow);
	   text += b;
	   key  += b;
	}
	addLine (key, text);
}

void	ServiceDiag::noteAudio	(int streamIndex) {
std::lock_guard<std::mutex> g (lock);
	if (streamIndex >= 0 && streamIndex < 4)
	   audioCount [streamIndex] ++;
}

std::string ServiceDiag::report	() {
std::lock_guard<std::mutex> g (lock);
std::string	r;
char	b [400];
	r += "==== Service diagnostics ====\n";
	snprintf (b, sizeof (b),
	          "Sync restarts: %d.  SDC blocks / FAC service blocks since the "
	          "last restart: %d / %d\n", resyncs, sdcBlocks, facBlocks);
	r += b;
	r += "SDC entities seen (type x count):";
	for (auto &e : entityCount) {
	   snprintf (b, sizeof (b), " %dx%d", e. first, e. second);
	   r += b;
	}
	r += "\n\nStream table (numofStreams = " + std::to_string (numofStreams) +
	     "), index = position in the multiplex:\n";
	for (int i = 0; i < 4; i ++) {
	   SlotInfo &s = slotTab [i];
	   snprintf (b, sizeof (b),
	      "  slot %d: %s %s  shortId=%d streamId=%d serviceId=0x%06X "
	      "isAudio=%d  lenA=%d lenB=%d  coding=%d mode=%d rate=%d  "
	      "name=\"%s\"\n",
	      i, s. inUse ? "used  " : "unused",
	      s. soort == 0 ? "AUDIO" : "DATA ",
	      s. shortId, s. streamId, (unsigned)s. serviceId,
	      s. isAudio ? 1 : 0, s. lengthHigh, s. lengthLow,
	      s. audioCoding, s. audioMode, s. samplingRate, s. name. c_str ());
	   r += b;
	}
	r += "\nServices (short id -> streams, label):\n";
	for (int i = 0; i < 4; i ++) {
	   SvcInfo &v = svcTab [i];
	   if (v. audioStream < 0 && v. dataStream < 0 && v. label. empty ())
	      continue;
	   snprintf (b, sizeof (b),
	      "  service %d%s: audio in stream %d, data in stream %d, "
	      "label \"%s\"\n", i, i == chosenService ? " (selected)" : "",
	      v. audioStream, v. dataStream, v. label. c_str ());
	   r += b;
	}
	r += "\nFAC services (short id -> service id):\n";
	for (int i = 0; i < 4; i ++) {
	   FacSvc &f = facSvc [i];
	   if (f. count == 0)
	      continue;
	   snprintf (b, sizeof (b),
	      "  short id %d: service id 0x%06X, %s, language %d, descriptor %d "
	      "(seen %d times)\n", i, (unsigned)f. serviceId,
	      f. isAudio ? "audio" : "data", f. language, f. descriptor,
	      f. count);
	   r += b;
	}
	r += "\nLabels received / dropped, per short id:";
	for (int i = 0; i < 4; i ++) {
	   snprintf (b, sizeof (b), "  %d: %d / %d", i,
	                            labelCount [i], labelDropped [i]);
	   r += b;
	}
	snprintf (b, sizeof (b),
	          "\n\nAudio decoder: selected service %d (button %d); the "
	          "program decodes stream index %d.\nTimes each stream index was "
	          "given to the audio decoder: 0:%d 1:%d 2:%d 3:%d\n",
	          chosenService, chosenService + 1, audioChannel, audioCount [0],
	          audioCount [1], audioCount [2], audioCount [3]);
	r += b;
	r += "\nEvent log (first time each thing is seen after a sync restart):\n";
	for (auto &l : lines)
	   r += "  " + l + "\n";
	if (linesFull)
	   r += "  (log full - use Clear to start again)\n";
	return r;
}

}
