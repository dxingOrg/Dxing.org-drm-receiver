#
/*
 *    Copyright (C) 2025
 *    (based on airspy-handler.h / hackrf-handler.h by
 *     Jan van Katwijk, J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Dxing.org DRM receiver
 *
 *    This software is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This software is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this software; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __AIRSPYHF_HANDLER__
#define	__AIRSPYHF_HANDLER__

#include	<QObject>
#include	<QFrame>
#include	<QSettings>
#include	<atomic>
#include	"radio-constants.h"
#include	"ringbuffer.h"
#include	"rate-converter.h"
#include	"device-handler.h"
#include	"ui_airspyhf-widget.h"
#include	"libairspyhf/airspyhf.h"

#ifdef __MINGW32__
#define GETPROCADDRESS  GetProcAddress
#else
#define GETPROCADDRESS  dlsym
#endif

typedef	int	(*pfn_airspyhf_open)		(struct airspyhf_device **device);
//	Optional (older libraries may lack them): the serial numbers of the
//	connected receivers - the same ones other programs show - and opening
//	a receiver by its serial number.
typedef	int	(*pfn_airspyhf_list_devices)	(uint64_t *serials, int count);
typedef	int	(*pfn_airspyhf_open_sn)		(struct airspyhf_device **device,
	                                         uint64_t serial_number);
typedef	int	(*pfn_airspyhf_close)		(struct airspyhf_device *device);
typedef	int	(*pfn_airspyhf_get_samplerates)	(struct airspyhf_device *device,
	                                         uint32_t *buffer,
	                                         const uint32_t len);
typedef	int	(*pfn_airspyhf_set_samplerate)	(struct airspyhf_device *device,
	                                         uint32_t samplerate);
typedef	int	(*pfn_airspyhf_set_freq)	(struct airspyhf_device *device,
	                                         const uint32_t freq_hz);
typedef	int	(*pfn_airspyhf_set_hf_agc)	(struct airspyhf_device *device,
	                                         uint8_t flag);
typedef	int	(*pfn_airspyhf_set_hf_agc_threshold)
	                                        (struct airspyhf_device *device,
	                                         uint8_t flag);
typedef	int	(*pfn_airspyhf_set_hf_att)	(struct airspyhf_device *device,
	                                         uint8_t value);
typedef	int	(*pfn_airspyhf_set_hf_lna)	(struct airspyhf_device *device,
	                                         uint8_t flag);
typedef	int	(*pfn_airspyhf_start)		(struct airspyhf_device *device,
	                                         airspyhf_sample_block_cb_fn callback,
	                                         void *ctx);
typedef	int	(*pfn_airspyhf_stop)		(struct airspyhf_device *device);
typedef	int	(*pfn_airspyhf_is_streaming)	(struct airspyhf_device *device);
typedef	int	(*pfn_airspyhf_board_partid_serialno_read)
	                                        (struct airspyhf_device *device,
	                                         airspyhf_read_partid_serialno_t *read_partid_serialno);

///////////////////////////////////////////////////////////////////////////
class	airspyhfHandler: public deviceHandler, public Ui_airspyhfWidget {
Q_OBJECT
public:
			airspyhfHandler	(RadioInterface *mr,
	                                 QSettings      *s,
	                                 RingBuffer<std::complex<float>> *r);
			~airspyhfHandler	();
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
	struct airspyhf_device	*theDevice;
private:
	QFrame		myFrame;

	bool			load_airspyhfFunctions	(void);
	pfn_airspyhf_open		airspyhf_open;
	pfn_airspyhf_list_devices	airspyhf_list_devices;
	pfn_airspyhf_open_sn		airspyhf_open_sn;
	pfn_airspyhf_close		airspyhf_close;
	pfn_airspyhf_get_samplerates	airspyhf_get_samplerates;
	pfn_airspyhf_set_samplerate	airspyhf_set_samplerate;
	pfn_airspyhf_set_freq		airspyhf_set_freq;
	pfn_airspyhf_set_hf_agc	airspyhf_set_hf_agc;
	pfn_airspyhf_set_hf_agc_threshold
	                                airspyhf_set_hf_agc_threshold;
	pfn_airspyhf_set_hf_att	airspyhf_set_hf_att;
	pfn_airspyhf_set_hf_lna	airspyhf_set_hf_lna;
	pfn_airspyhf_start		airspyhf_start;
	pfn_airspyhf_stop		airspyhf_stop;
	pfn_airspyhf_is_streaming	airspyhf_is_streaming;
	pfn_airspyhf_board_partid_serialno_read
	                                airspyhf_board_partid_serialno_read;

	QSettings	*airspyhfSettings;
	std::atomic<bool>	running;
	HINSTANCE	Handle;
private slots:
	void		setHfAgc	(int);
	void		setHfAgcThreshold (int);
	void		setHfLna	(int);
	void		setHfAtt	(int);
};
#endif
