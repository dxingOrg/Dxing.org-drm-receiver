#
/*
 *    Dxing.org DRM receiver - assembling data broadcast packets from the
 *    repetitions of a carousel
 *
 *    The Chinese "data broadcast packets" (GY/T 423, clause 6.7) are up to
 *    4109 bytes, i.e. 92 DRM packets, and a station repeats the whole set
 *    over and over. At a poor signal almost every repetition lacks a few
 *    DRM packets, so no repetition is complete, but together they are.
 *    Every DRM packet has its own CRC, so a received packet is reliable;
 *    what is missing is only the place of the packets between the lost
 *    ones. This class finds the place by comparing the repetitions:
 *    a run of consecutive good packets of one repetition overlaps with a
 *    run of another repetition (the same packet content), and the runs
 *    are merged. When a run from the first packet (start of a data
 *    broadcast packet) to the last one has no hole, the packet is
 *    complete, and its own CRC16 confirms it.
 *
 *    Pure C++ (no Qt). Released under the GNU General Public License,
 *    version 2 or later.
 */
#ifndef	__DATA_CAROUSEL__
#define	__DATA_CAROUSEL__

#include	<stdint.h>
#include	<stddef.h>
#include	<string>
#include	<vector>
#include	<map>
#include	<set>
#include	<unordered_map>
#include	<unordered_set>

namespace dxdata {

class DbCarousel {
public:
		DbCarousel	();
		~DbCarousel	();
	void	clear		();
//	data bytes in a full DRM packet (the SDC packet length)
	void	setCapacity	(int dataBytes);
//	one good packet of the data packet id (data = the useful bytes)
	void	slotOk		(bool first, bool last, int ci,
	                         const uint8_t *data, size_t dlen);
//	a slot whose CRC failed
	void	slotBad		();
//	end of a frame: look for finished packets in the run in progress
	void	endFrame	();
//	packets that became complete since the last call
	bool	take		(std::vector<uint8_t> &unit);
	uint32_t completed	() const { return unitsCompleted; }
	uint32_t runCount	() const { return (uint32_t)runs. size (); }
	uint32_t nodeCount	() const { return (uint32_t)contents. size (); }
	std::string stats	() const;
private:
	struct Entry {
	   int		node;			// -1: a hole (lost packet)
	   bool		first;
	   bool		last;
	};
	struct Run {
	   std::vector<Entry> v;
	   std::unordered_set<int> nodes;	// the packets in it (fast test)
	};
	int		capacity;
	std::unordered_map<std::string, int> nodeIds;
	std::vector<std::string> contents;
	std::vector<bool> mergeable;
	std::vector<Run> runs;
	Run		cur;
	bool		haveCur;
	int		lastCi;
	int		badSince;
	std::set<uint64_t> delivered;
	std::vector<std::vector<uint8_t> > ready;
	uint32_t	unitsCompleted;
	int		nodeFor		(const uint8_t *d, size_t n);
	void		closeCur	();
	void		addRun		(const Run &r);
	void		indexRun	(Run &r);
	bool		align		(const Run &a, const Run &b, int &off);
	void		mergeInto	(Run &a, const Run &b, int off);
	void		scan		(const Run &r);
	bool		unitSize	(int node, size_t &total);
};

uint16_t dbCrc16 (const uint8_t *p, size_t n);

}
#endif
