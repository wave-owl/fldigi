// ----------------------------------------------------------------------------
// cw_spectrum.h
//
// Wideband CW spectral detector prototype for fldigi.
//
// This class performs only spectral analysis and peak detection.  It does not
// manage CW channels, decode Morse, or implement the operator's fixed-frequency
// CW receiver.
// ----------------------------------------------------------------------------

#ifndef CW_SPECTRUM_H
#define CW_SPECTRUM_H

#include <complex>
#include <vector>

#include "gfft.h"

#include "cw_signal.h"
#include "cw_tracker.h"

class cw_spectrum
{
public:
	enum {
		SAMPLE_RATE = 8000,
		FFT_SIZE    = 1024,
		HOP_SIZE    = 512,
		NUM_BINS    = FFT_SIZE / 2 + 1
	};

	cw_spectrum();
	~cw_spectrum();

	void reset();

	// Accept an arbitrary number of 8 kHz real audio samples.  trx.cxx may
	// normally supply 512 samples per call, but the interface does not depend
	// on that block size.

	void process(const float *samples, int len);
	void process(const double *samples, int len);

	const std::vector<cw_spectral_peak>& peaks() const
	{
		return spectral_peaks;
	}

	void set_search_range(double low_hz, double high_hz);
	void set_snr_threshold(double linear_power_ratio);

	double low_frequency() const  { return search_low; }
	double high_frequency() const { return search_high; }
	double snr_threshold() const  { return threshold; }

private:
	typedef double fft_type;
	typedef std::complex<fft_type> fft_complex;

	g_fft<fft_type> *fft;

	double incoming[HOP_SIZE];
	double window[HOP_SIZE];
	fft_complex fftdata[FFT_SIZE];
	double power[NUM_BINS];

	int incoming_count;
	double search_low;
	double search_high;
	double threshold;

	std::vector<cw_spectral_peak> spectral_peaks;

	cw_tracker tracker;

	void make_window();
	void analyze();
	void find_peaks();

	double local_noise(int bin) const;
	double interpolate_peak(int bin) const;
	double estimate_width(int bin) const;

	template <typename T>
		void process_samples(const T *samples, int len);

	cw_spectrum(const cw_spectrum&);
	cw_spectrum& operator=(const cw_spectrum&);

/*  instrumentation */
	unsigned long fft_count;

};

#endif // CW_SPECTRUM_H
