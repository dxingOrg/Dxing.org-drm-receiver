#
/*
 *    Dxing.org DRM receiver - service information from FAC and SDC
 *
 *    Collects what the station says about itself: services (label,
 *    language, programme type), alternative frequencies (AFS),
 *    announcements and alarm, time and date. The decoder thread writes
 *    (handleEntity, setFacService), the window reads (snapshot).
 *    Pure C++ (no Qt), so it can be tested without Qt.
 *    The entity layouts follow ETSI ES 201 980 / GY/T 423-2025 clause
 *    6.4.3; what is not verified on a real signal is marked in the code.
 *
 *    Released under the GNU General Public License, version 2 or later.
 */
#ifndef	__SERVICE_INFO__
#define	__SERVICE_INFO__

#include	<stdint.h>
#include	<string>
#include	<vector>
#include	<mutex>

namespace dxdata {

struct SvcEntry {
	bool		haveFac		= false;
	uint32_t	serviceId	= 0;
	bool		isAudio		= false;
	int		language	= 0;		// FAC, 4 bits
	int		programmeType	= 0;		// FAC, 5 bits (30 = alarm, 31 = skip)
	uint32_t	lastBlock	= 0;		// FAC block that last confirmed it
	bool		haveLabel	= false;
	std::string	label;				// UTF-8
	bool		haveLang	= false;
	std::string	lang3;				// SDC entity 12
	std::string	country2;
};

struct AfsMux {				// entity 3
	bool		sync		= false;
	bool		layer		= false;
	bool		restricted	= false;
	int		serviceFlags	= 0;		// 4 bits, MSB = short id 3
	bool		hasRegSched	= false;
	int		regionId	= 0;
	int		scheduleId	= 0;
	std::vector<int> freqsKHz;
	bool	operator == (const AfsMux &) const;
};

struct Schedule {				// entity 4
	int		id		= 0;
	int		days		= 0;		// 7 bits, MSB = Monday
	int		startMin	= 0;
	int		durationMin	= 0;
	bool	operator == (const Schedule &) const;
};

struct Announcement {			// entity 6
	int		serviceFlags	= 0;
	bool		sameMux		= false;
	int		id		= 0;
	int		support		= 0;		// 10 bits
	int		switching	= 0;		// 10 bits
};

struct Region {				// entity 7 (coarse)
	int		id		= 0;
	int		lat		= 0;		// degrees, signed
	int		lon		= 0;
	int		latExt		= 0;
	int		lonExt		= 0;
	std::vector<int> ciraf;
};

struct DetailRect {				// entity 13, units 1/16 degree
	int		lat, lon, latExt, lonExt;
	bool	operator == (const DetailRect &o) const {
	   return lat == o. lat && lon == o. lon &&
	          latExt == o. latExt && lonExt == o. lonExt; }
};
struct DetailRegion {
	int		id		= 0;
	std::vector<DetailRect> rects;
};

struct OtherSvc {				// entity 11, one per entity
	bool		announce	= false;
	int		announceId	= 0;
	bool		hasRegSched	= false;
	int		regionId	= 0;
	int		scheduleId	= 0;
	bool		sameService	= false;
	int		systemId	= 0;
	bool		hasServiceId	= false;
	uint32_t	serviceId	= 0;
	std::vector<int> freqs;			// kHz (systems 0 1 2) or raw code
	bool		parsed		= true;
	std::string	raw;			// hex, when it could not be parsed
	bool	operator == (const OtherSvc &) const;
};

struct TimeInfo {
	bool		valid		= false;
	int		mjd		= 0;
	int		hour		= 0;
	int		minute		= 0;
	bool		hasOffset	= false;
	int		offsetHalfHours	= 0;		// signed
};

struct InfoSnapshot {
	SvcEntry	service [4];
	std::vector<AfsMux>	afs;
	std::vector<Schedule>	schedules;
	std::vector<Announcement> announcements;
	std::vector<Region>	regions;
	std::vector<DetailRegion> detailRegions;
	std::vector<OtherSvc>	other;
	TimeInfo	time;
	bool		alarmFac;		// FAC programme type 30 seen recently
	bool		alarmSdc;		// switching flag of an alarm set
	uint32_t	counter;
};

//	pure helpers (also used by the window)
void	mjdToDate	(int mjd, int &y, int &m, int &d);
std::string	formatDateTime (int mjd, int hour, int minute, int addMinutes);
std::string	languageName	(int code);		// FAC language, 4 bits
std::string	programmeTypeName (int code);		// FAC type, 5 bits
std::string	systemName	(int systemId);
std::string	scheduleText	(const Schedule &);
std::string	announcementTypes (int bits);		// names of the set bits

class SdcInfo {
public:
		SdcInfo		();
		~SdcInfo	();
	void	clear		();
//	bits: one bit per byte (0 or 1), MSB first, from the first bit of the
//	body; nbits = 8 * lengthofBody + 4
	void	handleEntity	(int type, int versionFlag,
	                         const uint8_t *bits, int nbits);
	void	setFacService	(int shortId, uint32_t serviceId, bool isAudio,
	                         int language, int programmeType);
	uint32_t changeCounter	();
	bool	alarmActive	();
	InfoSnapshot snapshot	();
private:
	std::mutex	lock;
	InfoSnapshot	data;
	int		listVersion [16];
	uint32_t	facBlocks;
	uint32_t	lastAlarmFac;
//	The FAC has only an 8 bit CRC: on a weak signal one block in 256 that
//	is pure noise passes it, and shows a service that does not exist (with a
//	language and programme type of its own). A real service comes round
//	again and again, so a record counts only after it was seen several
//	times in a row, and it disappears when it is not seen for a while.
	struct Pending {
	   bool		valid	= false;
	   uint32_t	serviceId = 0;
	   bool		isAudio	= false;
	   int		language = 0;
	   int		programmeType = 0;
	   int		count	= 0;
	}		pend [4];
	bool		newVersion	(int type, int versionFlag);
	void		touch		();
	void		updateAlarmSdc	();
};

}
#endif
