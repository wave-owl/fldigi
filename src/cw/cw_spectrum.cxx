// ----------------------------------------------------------------------------
// cw_spectrum.cxx
//
// Wideband CW spectral detector prototype for fldigi.
//
// 8000 samples/sec
// 1024 point FFT
// 512 sample hop (50 percent overlap)
// 7.8125 Hz/bin
// 64 ms between spectral observations
// ----------------------------------------------------------------------------

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include <cstdio>

#include "cw_spectrum.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

const double DEFAULT_LOW_HZ       = 200.0;
const double DEFAULT_HIGH_HZ      = 3500.0;
const double DEFAULT_SNR_RATIO    = 4.0;   // 6.02 dB power ratio
const double MIN_POWER            = 1.0e-30;

// Exclude the candidate and its immediate skirts from the noise estimate.
// At 7.8125 Hz/bin this uses noise samples approximately 31 to 94 Hz from
// the candidate on each side.
const int NOISE_INNER_BIN = 4;
const int NOISE_OUTER_BIN = 12;

inline double bin_hz()
{
	return (double)cw_spectrum::SAMPLE_RATE / cw_spectrum::FFT_SIZE;
}

} // namespace

cw_spectrum::cw_spectrum()
	: fft(0),
	  incoming_count(0),
	  search_low(DEFAULT_LOW_HZ),
	  search_high(DEFAULT_HIGH_HZ),
	  threshold(DEFAULT_SNR_RATIO)
{
	fft = new g_fft<fft_type>(FFT_SIZE);
	make_window();
	reset();
}

cw_spectrum::~cw_spectrum()
{
	delete fft;
	fft = 0;
}

void cw_spectrum::reset()
{
	std::memset(incoming, 0, sizeof(incoming));
	std::memset(power, 0, sizeof(power));

	for (int i = 0; i < FFT_SIZE; i++)
		fftdata[i] = fft_complex(0.0, 0.0);

	incoming_count = 0;

	fft_count = 0;

	spectral_peaks.clear();
	tracker.reset();
}

void cw_spectrum::set_search_range(double low_hz, double high_hz)
{
	if (low_hz < 0.0)
		low_hz = 0.0;
	if (high_hz > SAMPLE_RATE / 2.0)
		high_hz = SAMPLE_RATE / 2.0;

	if (low_hz > high_hz)
		std::swap(low_hz, high_hz);

	search_low = low_hz;
	search_high = high_hz;
}

void cw_spectrum::set_snr_threshold(double linear_power_ratio)
{
	if (linear_power_ratio < 1.0)
		linear_power_ratio = 1.0;
	threshold = linear_power_ratio;
}

void cw_spectrum::make_window()
{
	// 512 real samples are independently windowed.
	// The result is subsequently zero padded to a 1024 point FFT.
	for (int i = 0; i < HOP_SIZE; ++i) {
		window[i] = 0.5 - 0.5 *
			std::cos(2.0 * M_PI * i / (HOP_SIZE - 1));
	}
}

template <typename T>
void cw_spectrum::process_samples(const T *samples, int len)
{
	if (!samples || len <= 0)
		return;

	while (len > 0) {
		int n = HOP_SIZE - incoming_count;

		if (n > len)
			n = len;

		for (int i = 0; i < n; ++i)
			incoming[incoming_count + i] =
				static_cast<double>(samples[i]);

		incoming_count += n;
		samples += n;
		len -= n;

		if (incoming_count == HOP_SIZE) {
			analyze();
			incoming_count = 0;
		}
	}

}

void cw_spectrum::process(const float *samples, int len)
{
	process_samples(samples, len);
}

void cw_spectrum::process(const double *samples, int len)
{
	process_samples(samples, len);
}

void cw_spectrum::analyze()
{
	// Independent 512-sample observation.
	for (int i = 0; i < HOP_SIZE; ++i)
		fftdata[i] =
			fft_complex(incoming[i] * window[i], 0.0);

	// Zero pad to 1024 points.
	for (int i = HOP_SIZE; i < FFT_SIZE; ++i)
		fftdata[i] = fft_complex(0.0, 0.0);

	fft->ComplexFFT(fftdata);

	for (int i = 0; i < NUM_BINS; ++i)
		power[i] = std::norm(fftdata[i]);

	find_peaks();

	tracker.process(spectral_peaks, fft_count);

	fft_count++;
}

void cw_spectrum::find_peaks()
{
	spectral_peaks.clear();

	int first_bin = (int)std::ceil(search_low / bin_hz());
	int last_bin  = (int)std::floor(search_high / bin_hz());

	// local_noise() needs NOISE_OUTER_BIN bins on both sides.
	if (first_bin < NOISE_OUTER_BIN)
		first_bin = NOISE_OUTER_BIN;
	if (last_bin > NUM_BINS - 1 - NOISE_OUTER_BIN)
		last_bin = NUM_BINS - 1 - NOISE_OUTER_BIN;

	for (int bin = first_bin; bin <= last_bin; ++bin) {
		// Strict on the left and non-strict on the right gives a deterministic
		// choice if two adjacent bins happen to have identical power.
		if (!(power[bin] > power[bin - 1] &&
		      power[bin] >= power[bin + 1]))
			continue;

		double noise = local_noise(bin);
		if (noise < MIN_POWER)
			noise = MIN_POWER;

		double ratio = power[bin] / noise;
		if (ratio < threshold)
			continue;

		double delta = interpolate_peak(bin);

		cw_spectral_peak peak;
		peak.frequency = (bin + delta) * bin_hz();
		peak.power = power[bin];
		peak.noise = noise;
		peak.snr = ratio;
		peak.width = estimate_width(bin);

		spectral_peaks.push_back(peak);
	}
}

double cw_spectrum::local_noise(int bin) const
{
	std::vector<double> samples;
	samples.reserve(2 * (NOISE_OUTER_BIN - NOISE_INNER_BIN + 1));

	for (int i = NOISE_INNER_BIN; i <= NOISE_OUTER_BIN; ++i) {
		samples.push_back(power[bin - i]);
		samples.push_back(power[bin + i]);
	}

	if (samples.empty())
		return MIN_POWER;

	std::sort(samples.begin(), samples.end());

	const size_t n = samples.size();
	if (n & 1)
		return samples[n / 2];

	return 0.5 * (samples[n / 2 - 1] + samples[n / 2]);
}

double cw_spectrum::interpolate_peak(int bin) const
{
	const double a = power[bin - 1];
	const double b = power[bin];
	const double c = power[bin + 1];
	const double denominator = a - 2.0 * b + c;

	if (std::fabs(denominator) < MIN_POWER)
		return 0.0;

	double delta = 0.5 * (a - c) / denominator;

	if (delta < -0.5)
		delta = -0.5;
	else if (delta > 0.5)
		delta = 0.5;

	return delta;
}

double cw_spectrum::estimate_width(int bin) const
{
	// Crude -3 dB width estimate.  This is diagnostic information for now;
	// it may later help distinguish narrow CW carriers from broader signals.
	const double half_power = power[bin] * 0.5;

	int left = bin;
	while (left > 0 && power[left] >= half_power)
		--left;

	int right = bin;
	while (right < NUM_BINS - 1 && power[right] >= half_power)
		++right;

	return (right - left) * bin_hz();
}
