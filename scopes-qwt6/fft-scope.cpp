#
/*
 *    Copyright (C)  2017, 2018
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the SDR-J (JSDR).
 *
 *    SDR-J is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    SDR-J is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with SDR-J; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include	<cmath>
#include	"spectrum-scope.h"
#include	"waterfall-scope.h"
#include	"fft-scope.h"

//
	fftScope::fftScope (QwtPlot	*plotgrid,
	                    int16_t	displaySize,
	                    int32_t	scale,
	                    int32_t	SampleRate,
	                    int16_t	level,
	                    int16_t	freq) {
int16_t	i;
double	temp;
	this	-> scope		= new spectrumScope (plotgrid, 
	                                                     displaySize);
	viewMode = SPECTRUMVIEW;

	connect (scope, SIGNAL (clickedwithLeft (int)),
	         this,  SIGNAL (clickedwithLeft (int)));
	connect (scope, SIGNAL (clickedwithRight (int)),
	         this,  SIGNAL (clickedwithRight (int)));
	this	-> plotgrid		= plotgrid;
	this	-> displaySize		= displaySize;
	this	-> segmentSize		= SampleRate / freq;
	this	-> scale		= scale;
	this	-> spectrumSize		= 2 * displaySize;
	this	-> sampleRate		= SampleRate;
	this	-> MaxFrequency		= SampleRate / 2;
	this	-> freq			= freq;
	bitDepth			= 8;
	averageCounter			= 0;
	this	-> fillPointer		= 0;
	this	-> vfo			= 0;
//	the slider value is the scale, exactly as setLevel () applies it
	this	-> amplification	= level;
	this	-> autoScale		= (level <= 0);
	this	-> autoAmp		= 20;
	this	-> needle		= 0;
	this	-> needleBin		= displaySize / 2;
	this	-> displayBuffer	= new double [displaySize];
	this	-> averageBuffer	= new double [displaySize];
	memset (averageBuffer, 0, displaySize * sizeof (double));
	this	-> X_axis		= new double [displaySize];
	this	-> Window		= new float [spectrumSize];
	this	-> sampleCounter	= 0;
	this	-> spectrum_fft		= new common_fft (spectrumSize);
	this	-> spectrumBuffer	= spectrum_fft	-> getVector ();

	for (i = 0; i < spectrumSize; i ++) 
	   Window [i] = 0.42 - 0.5 * cos ((2.0 * M_PI * i) / (spectrumSize - 1)) +
	                      0.08 * cos ((4.0 * M_PI * i) / (spectrumSize - 1));


	temp	= (double)MaxFrequency / displaySize;
	for (i = 0; i < displaySize; i ++)
	   X_axis [i] =
	      ((double)vfo - (double)MaxFrequency
	           +  (double)((i) * (double) 2 * temp)) / ((double)scale);
	
	scope	-> set_bitDepth (bitDepth);
}

	fftScope::~fftScope (void) {
	delete[]	this	-> displayBuffer;
	delete[]	this	-> averageBuffer;
	delete[]	this	-> X_axis;
	delete[]	this	-> Window;
	delete		this	-> spectrum_fft;
	delete		this	-> scope;
}

//	0 = "Auto", 1 .. 100 = the part (in %) of the level range that is shown
void	fftScope::setLevel (int16_t sliderValue) {
	amplification	= sliderValue;
	autoScale	= (sliderValue <= 0);
}

void	fftScope::setZero (int64_t vfo) {
	this	-> vfo = vfo;
}

void	fftScope::setNeedle (int32_t needle) {
	this -> needle = needle;
	needleBin       = (needle * displaySize) / sampleRate + displaySize / 2;
}

void	fftScope::addElements (DSPCOMPLEX *v, int32_t n) {
int32_t	i;

	for (i = 0; i < n; i ++)
	   addElement (v [i]);
}

void	fftScope::addElement (DSPCOMPLEX x) {
int32_t	i, j;
//	
	if (fillPointer < spectrumSize) {
	   spectrumBuffer [fillPointer] = x;
	   fillPointer ++;
	}

	if (++ sampleCounter < segmentSize)
	   return;

	if (fillPointer < spectrumSize)
	   for (i = fillPointer; i < spectrumSize; i ++)
	      spectrumBuffer [i] = std::complex<float> (0, 0);

	fillPointer	= 0;
	sampleCounter	= 0;
	for (i = 0; i < spectrumSize; i ++) {
	   spectrumBuffer [i] = cmul (spectrumBuffer [i], Window [i]); 
	}

	spectrum_fft	-> do_FFT ();
	for (i = 0; i < displaySize / 2; i ++) {
	   displayBuffer [i] = 0;
	   displayBuffer [displaySize / 2 + i] = 0;
	   for (j = 0; j < 2; j ++) {
	      displayBuffer [i] +=
	           0.25 * abs (spectrumBuffer [spectrumSize / 2 + 2 * i + j]);
	      displayBuffer [displaySize / 2 + i] +=
	           0.25 * abs (spectrumBuffer [2 * i + j]);
	   }
	}

	for (i = 0; i < displaySize; i ++) {
	   if (displayBuffer [i] != displayBuffer [i])
	      displayBuffer [i] = 0;
	   averageBuffer [i] = 
	      ((double)(freq - 1)) / (freq) * averageBuffer [i] +
	                1.0f / (freq) * displayBuffer [i];
	   displayBuffer [i] = averageBuffer [i];

	}
	if (averageCounter < 5) {
	   averageCounter ++;
	   return;
	}

//	X axis labels
	double  temp    = (double)sampleRate / 2 / displaySize;
        for (i = 0; i < displaySize; i ++)
           X_axis [i] =
                 ((double)vfo - (double)(sampleRate / 2) +
                  (double)((i) * (double) 2 * temp)) / (int32_t)scale;

double	amp	= amplification;
	if (autoScale) {
//	"Auto": fit the strongest part of the spectrum. The bottom of the plot is
//	fixed at the noise floor (bitDepth * 6 dB below full scale), only the top
//	moves, so the span wanted is the strongest level above that floor plus
//	some headroom. Bins are averaged with their neighbours first, so a single
//	narrow spike does not squash the broad signals (like DRM) we look for.
	   const int W	= 8;
	   double acc	= 0;
	   double peak	= 0;
	   for (i = 0; i < displaySize; i ++) {
	      acc += displayBuffer [i];
	      if (i >= W)
	         acc -= displayBuffer [i - W];
	      if ((i >= W - 1) && (acc / W > peak))
	         peak = acc / W;
	   }
	   int bd	= ((bitDepth < 1) || (bitDepth > 32)) ? 24 : bitDepth;
	   double spanDb	= bd * 6.0206;		// 20 log10 (2 ^ bd)
	   double wanted	= (20 * log10 (peak + 1) * 1.25 + 1.0) / spanDb * 100;
	   if (wanted < 1)
	      wanted	= 1;
	   if (wanted > 100)
	      wanted	= 100;
	   if (wanted > autoAmp)		// quick to open up ...
	      autoAmp	= 0.5 * autoAmp + 0.5 * wanted;
	   else					// ... slow to close in
	      autoAmp	= 0.95 * autoAmp + 0.05 * wanted;
	   amp	= autoAmp;
	}

	scope -> display (X_axis,
	                  displayBuffer,
	                  amp,
	                  ((quint64)vfo + needle) / (quint64)scale,
	                  needleBin);
}

void	fftScope::setScope (quint64 centerFreq, int32_t offset) {
	vfo	= centerFreq;
	needle	= offset;
	needleBin	= (needle * displaySize) / sampleRate + displaySize / 2;
}

void	fftScope::switch_viewMode	(void) {
	delete scope;
	if (viewMode == SPECTRUMVIEW) {
	   viewMode = WATERFALLVIEW;
	   scope = new waterfallScope (plotgrid, displaySize, 50);
	}
	else {
	   viewMode = SPECTRUMVIEW;
	   scope = new spectrumScope (plotgrid, displaySize);
	}

	scope	-> set_bitDepth (bitDepth);
	sampleCounter	= 0;
	fillPointer	= 0;
	averageCounter	= 0;
	memset (averageBuffer, 0, displaySize * sizeof (double));
	connect (scope, SIGNAL (clickedwithLeft (int)),
	         this,  SIGNAL (clickedwithLeft (int)));
	connect (scope, SIGNAL (clickedwithRight (int)),
	         this,  SIGNAL (clickedwithRight (int)));
}

void	fftScope::set_bitDepth		(int16_t bd) {
	scope	-> set_bitDepth (bd);
	bitDepth	= bd;
}

