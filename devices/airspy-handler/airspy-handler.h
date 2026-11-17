#
/*
 *    Copyright (C) 2025
 *    (based on hackrf-handler.h by Jan van Katwijk, J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the drm-receiver
 *
 *    drm-receiver is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    drm-receiver is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with drm-receiver; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __AIRSPY_HANDLER__
#define	__AIRSPY_HANDLER__

#include	<QObject>
#include	<QFrame>
#include	<QSettings>
#include	<atomic>
#include	<utility>
#include	"radio-constants.h"
#include	"ringbuffer.h"
#include	"rate-converter.h"
#include	"device-handler.h"
#include	"ui_airspy-widget.h"
#include	"libairspy/airspy.h"

#ifdef __MINGW32__
#define GETPROCADDRESS  GetProcAddress
#else
#define GETPROCADDRESS  dlsym
#endif

//
//	Dll and ".so" function prototypes, resolved at runtime,
//	exactly as is done for the hackrf and rtlsdr handlers.
typedef	int	(*pfn_airspy_init)		(void);
typedef	int	(*pfn_airspy_exit)		(void);
typedef	int	(*pfn_airspy_open)		(struct airspy_device **device);
typedef	int	(*pfn_airspy_close)		(struct airspy_device *device);
typedef	int	(*pfn_airspy_get_samplerates)	(struct airspy_device *device,
	                                         uint32_t *buffer,
	                                         const uint32_t len);
typedef	int	(*pfn_airspy_set_samplerate)	(struct airspy_device *device,
	                                         uint32_t samplerate);
typedef	int	(*pfn_airspy_set_sample_type)	(struct airspy_device *device,
	                                         enum airspy_sample_type sample_type);
typedef	int	(*pfn_airspy_set_freq)		(struct airspy_device *device,
	                                         const uint32_t freq_hz);
typedef	int	(*pfn_airspy_set_lna_gain)	(struct airspy_device *device,
	                                         uint8_t value);
typedef	int	(*pfn_airspy_set_mixer_gain)	(struct airspy_device *device,
	                                         uint8_t value);
typedef	int	(*pfn_airspy_set_vga_gain)	(struct airspy_device *device,
	                                         uint8_t value);
typedef	int	(*pfn_airspy_set_lna_agc)	(struct airspy_device *device,
	                                         uint8_t value);
typedef	int	(*pfn_airspy_set_mixer_agc)	(struct airspy_device *device,
	                                         uint8_t value);
typedef	int	(*pfn_airspy_start_rx)		(struct airspy_device *device,
	                                         airspy_sample_block_cb_fn callback,
	                                         void *rx_ctx);
typedef	int	(*pfn_airspy_stop_rx)		(struct airspy_device *device);
typedef	int	(*pfn_airspy_is_streaming)	(struct airspy_device *device);
typedef	const char *(*pfn_airspy_error_name)	(enum airspy_error errcode);
typedef	int	(*pfn_airspy_board_partid_serialno_read)
	                                        (struct airspy_device *device,
	                                         airspy_read_partid_serialno_t *read_partid_serialno);

///////////////////////////////////////////////////////////////////////////
class	airspyHandler: public deviceHandler, public Ui_airspyWidget {
Q_OBJECT
public:
			airspyHandler	(RadioInterface *mr,
	                                 QSettings      *s,
	                                 RingBuffer<std::complex<float>> *r);
			~airspyHandler		();
	void		setVFOFrequency		(int32_t);
	int32_t		getVFOFrequency		();

	bool		restartReader		();
	void		stopReader		();
	void		resetBuffer		();
	int16_t		bitDepth		();
//
//	Visible for the (static) callback function
	RingBuffer<std::complex<float>>  *_I_Buffer;
	int32_t		sampleCnt;
	int32_t		inputRate;
	int32_t		outputRate;
	void		report_dataAvailable	();
	rateConverter	*theConverter;
	struct airspy_device	*theDevice;
private:
	QFrame		myFrame;

	bool			load_airspyFunctions	(void);
	pfn_airspy_init			airspy_init;
	pfn_airspy_exit			airspy_exit;
	pfn_airspy_open			airspy_open;
	pfn_airspy_close		airspy_close;
	pfn_airspy_get_samplerates	airspy_get_samplerates;
	pfn_airspy_set_samplerate	airspy_set_samplerate;
	pfn_airspy_set_sample_type	airspy_set_sample_type;
	pfn_airspy_set_freq		airspy_set_freq;
	pfn_airspy_set_lna_gain	airspy_set_lna_gain;
	pfn_airspy_set_mixer_gain	airspy_set_mixer_gain;
	pfn_airspy_set_vga_gain	airspy_set_vga_gain;
	pfn_airspy_set_lna_agc		airspy_set_lna_agc;
	pfn_airspy_set_mixer_agc	airspy_set_mixer_agc;
	pfn_airspy_start_rx		airspy_start_rx;
	pfn_airspy_stop_rx		airspy_stop_rx;
	pfn_airspy_is_streaming	airspy_is_streaming;
	pfn_airspy_error_name		airspy_error_name;
	pfn_airspy_board_partid_serialno_read
	                                airspy_board_partid_serialno_read;

	QSettings	*airspySettings;
	std::atomic<bool>	running;
	HINSTANCE	Handle;
	bool		libraryLoaded;
private slots:
	void		setLNAGain	(int);
	void		setMixerGain	(int);
	void		setVGAGain	(int);
};
#endif

