// ----------------------------------------------------------------------------
// cw_channel.cxx -- experimental CW narrow-band channel extractor/detector
//
// Stage 7 CONTROLLED EXPERIMENT
//
// Same DSP and detector as Stage 5.  Two experimental changes only:
//
//   1. Initial NCO is 1467 Hz, independently estimated from fldigi's
//      4x vertical waterfall.
//
//   2. The actual NCO frequency is written into both diagnostic CSV files.
//
// No AFC or automatic frequency correction is performed.
// ----------------------------------------------------------------------------

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "cw_channel.h"

#include "cw_test_dialog.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const double cw_channel::PHASE_ALPHA = 0.125;

namespace {

void sort_values(double *v, unsigned int n)
{
	for (unsigned int i = 1; i < n; ++i) {
		const double x = v[i];
		unsigned int j = i;

		while (j > 0 && v[j - 1] > x) {
			v[j] = v[j - 1];
			--j;
		}

		v[j] = x;
	}
}

inline double channel_decayavg(double old_value, double new_value, int n)
{
	if (n <= 1)
		return new_value;

	return old_value + (new_value - old_value) / static_cast<double>(n);
}

inline double clamp_double(double v, double lo, double hi)
{
	return std::max(lo, std::min(v, hi));
}

} // namespace

void cw_channel::open_csv_files()
{
	if (diagnostic) std::fclose(diagnostic);
	if (events) std::fclose(events);

	diagnostic = std::fopen(
		std::string("cw_channel.").append(cw_test_param.csv_fname).append(".csv").c_str(),
		"w");
	if (diagnostic) {
		std::fprintf(diagnostic,
			"time,nco_hz,envelope,smoothed,i,q,dphase,phase_hz,"
			"coherence,noise,signal,peak,norm,lower,upper,"
			"confidence,state,transition,run_samples,run_ms,metric,"
			"carrier_n,carrier_ms,carrier_mean_hz,carrier_sigma_hz,"
			"carrier_mean_coherence,carrier_evidence,"
			"recent_n,recent_next,recent_mean_hz,recent_sigma_hz,recent_median_hz,"
			"recent_mad_hz,recent_good_count,"
			"recent_hz_0,recent_hz_1,recent_hz_2,recent_hz_3,"
			"recent_hz_4,recent_hz_5,recent_hz_6\n");
	}

	events = std::fopen(
		std::string("cw_events.").append(cw_test_param.csv_fname).append(".csv").c_str(),
		"w");
	if (events) {
		std::fprintf(events,
			"time,nco_hz,event,ended_state,run_samples,run_ms,"
			"confidence,norm,lower,upper,dphase,phase_hz,coherence,"
			"carrier_n,carrier_ms,carrier_mean_hz,carrier_sigma_hz,"
			"carrier_mean_coherence,carrier_evidence,"
			"recent_n,recent_next,recent_mean_hz,recent_sigma_hz,recent_median_hz,"
			"recent_mad_hz,recent_good_count,"
			"recent_hz_0,recent_hz_1,recent_hz_2,recent_hz_3,"
			"recent_hz_4,recent_hz_5,recent_hz_6\n");
	}
}

cw_channel::cw_channel() :
	filter(0),
	bitfilter(0),
	diagnostic(0),
	events(0),
	phase(0.0),
	phase_increment(0.0),
	input_samples(0),
	output_samples(0),
	sig_avg(0.0),
	noise_floor(0.0),
	agc_peak(0.0),
	metric(0.0),
	in_tone(false),
	timing_valid(false),
	timing_state(false),
	timing_start_sample(0),
	phase_valid(false),
	previous_filtered(0.0, 0.0),
	phase_mean_cos(0.0),
	phase_mean_sin(0.0),
	phase_delta(0.0),
	phase_hz(0.0),
	phase_coherence(0.0)
{
	filter = new C_FIR_filter;
	bitfilter = new Cmovavg(BITFILTER_LENGTH);

	frequency = cw_test_param.frequency;
	lowpass_hz = cw_test_param.lowpass;
	carrier_history_size = cw_test_param.history;
	carrier_tolerance = cw_test_param.tolerance;

	open_csv_files();

	init(frequency);
}

cw_channel::~cw_channel()
{
	if (events)
		std::fclose(events);

	if (diagnostic)
		std::fclose(diagnostic);

	delete bitfilter;
	delete filter;
}

void cw_channel::configure_filter()
{
	std::fprintf(stderr,
		"CW configure_filter: lowpass_hz=%.1f normalized=%.9f\n",
		lowpass_hz,
		lowpass_hz / static_cast<double>(SAMPLE_RATE));

	filter->init_lowpass(
		FIR_LENGTH,
		DECIMATE,
		 lowpass_hz / static_cast<double>(SAMPLE_RATE));
}

void cw_channel::reset_timing()
{
	timing_valid = false;
	timing_state = false;
	timing_start_sample = output_samples;
}

void cw_channel::reset_phase()
{
	phase_valid = false;
	previous_filtered = cmplx(0.0, 0.0);
	phase_mean_cos = 0.0;
	phase_mean_sin = 0.0;
	phase_delta = 0.0;
	phase_hz = 0.0;
	phase_coherence = 0.0;

	carrier_active = false;
	carrier_samples = 0;

	carrier_sum_hz = 0.0;
	carrier_sum_hz2 = 0.0;
	carrier_sum_coherence = 0.0;

	carrier_mean_hz = 0.0;
	carrier_sigma_hz = 0.0;
	carrier_mean_coherence = 0.0;

	carrier_evidence = 0.0;
}

void cw_channel::reset_detector()
{
	sig_avg = 0.0;
	noise_floor = 0.0;
	agc_peak = 0.0;
	metric = 0.0;
	in_tone = false;

	delete bitfilter;
	bitfilter = new Cmovavg(BITFILTER_LENGTH);

	reset_timing();
	reset_phase();

	carrier_history_reset();
}

void cw_channel::carrier_history_reset()
{
	carrier_history_size = cw_test_param.history;

	carrier_history_count = 0;
	carrier_history_next = 0;

	for (unsigned int i = 0; i < carrier_history_size; ++i)
		carrier_history[i] = 0.0;

	carrier_recent_mean_hz = 0.0;
	carrier_recent_sigma_hz = 0.0;
	carrier_recent_median_hz = 0.0;
	carrier_recent_mad_hz = 0.0;
	carrier_recent_good_count = 0;
}

void cw_channel::init(double freq)
{
	frequency = freq;
	phase = 0.0;
	phase_increment =
		2.0 * M_PI * frequency / static_cast<double>(SAMPLE_RATE);

	input_samples = 0;
	output_samples = 0;

	configure_filter();
	reset_detector();
	carrier_history_reset();

	observer.set_sample_timing(SAMPLE_RATE, DECIMATE);
	observer.reset();
}

void cw_channel::reset()
{
	lowpass_hz = cw_test_param.lowpass;
	carrier_history_size = cw_test_param.history;
	carrier_tolerance = cw_test_param.tolerance;

	init(cw_test_param.frequency);

	std::fprintf(stderr,
			"CW TEST RESET: freq=%.1f lowpass=%.1f history=%u tolerance=%.1f\n",
			frequency,
			lowpass_hz,
			carrier_history_size,
			carrier_tolerance);

	open_csv_files();
	open_observation_file();
}

void cw_channel::set_frequency(double freq)
{
	if (freq == frequency)
		return;

	frequency = freq;
	phase_increment =
		2.0 * M_PI * frequency / static_cast<double>(SAMPLE_RATE);

	// Deliberately no AFC in Stage 7.  If a caller changes the NCO,
	// nco_hz in the CSV makes that change visible in the experiment.
}

void cw_channel::update_phase(const cmplx& filtered)
{
	const double mag = abs(filtered);

	if (mag <= 1e-15) {
		phase_valid = false;
		phase_delta = 0.0;
		phase_hz = 0.0;
		phase_coherence = 0.0;
		previous_filtered = filtered;
		return;
	}

	if (!phase_valid || abs(previous_filtered) <= 1e-15) {
		previous_filtered = filtered;
		phase_valid = true;
		phase_delta = 0.0;
		phase_hz = 0.0;
		phase_mean_cos = 0.0;
		phase_mean_sin = 0.0;
		phase_coherence = 0.0;
		return;
	}

	const double i0 = previous_filtered.real();
	const double q0 = previous_filtered.imag();
	const double i1 = filtered.real();
	const double q1 = filtered.imag();

	const double dot   = i1 * i0 + q1 * q0;
	const double cross = q1 * i0 - i1 * q0;

	phase_delta = std::atan2(cross, dot);

	const double detector_rate =
		static_cast<double>(SAMPLE_RATE) / DECIMATE;

	phase_hz =
		phase_delta * detector_rate / (2.0 * M_PI);

	const double c = std::cos(phase_delta);
	const double s = std::sin(phase_delta);

	phase_mean_cos += PHASE_ALPHA * (c - phase_mean_cos);
	phase_mean_sin += PHASE_ALPHA * (s - phase_mean_sin);

	phase_coherence = std::sqrt(
		phase_mean_cos * phase_mean_cos +
		phase_mean_sin * phase_mean_sin);

	if (phase_coherence > 1.0)
		phase_coherence = 1.0;

	previous_filtered = filtered;
}

void cw_channel::carrier_start()
{
	carrier_active = true;
	carrier_samples = 0;

	carrier_sum_hz = 0.0;
	carrier_sum_hz2 = 0.0;
	carrier_sum_coherence = 0.0;

	carrier_mean_hz = 0.0;
	carrier_sigma_hz = 0.0;
	carrier_mean_coherence = 0.0;

	carrier_evidence = 0.0;

	carrier_history_reset();
}

void cw_channel::carrier_history_update(double hz)
{
	carrier_history[carrier_history_next] = hz;

	carrier_history_next =
		(carrier_history_next + 1) % carrier_history_size;

	if (carrier_history_count < carrier_history_size)
		++carrier_history_count;

	carrier_history_statistics();
}

void cw_channel::carrier_history_statistics()
{
	const unsigned int n = carrier_history_count;

	if (n == 0) {
		carrier_recent_mean_hz = 0.0;
		carrier_recent_sigma_hz = 0.0;
		carrier_recent_median_hz = 0.0;
		carrier_recent_mad_hz = 0.0;
		carrier_recent_good_count = 0;
		return;
	}

	double values[CARRIER_HISTORY];

	/*
	 * We don't care about chronological ordering for these
	 * statistics, so copy the valid history entries directly.
	 */
	for (unsigned int i = 0; i < n; ++i)
		values[i] = carrier_history[i];

	double sum = 0.0;
	double sum2 = 0.0;

	for (unsigned int i = 0; i < n; ++i) {
		sum += values[i];
		sum2 += values[i] * values[i];
	}

	const double dn = static_cast<double>(n);

	carrier_recent_mean_hz = sum / dn;

	double variance =
		sum2 / dn -
		carrier_recent_mean_hz * carrier_recent_mean_hz;

	if (variance < 0.0)
		variance = 0.0;

	carrier_recent_sigma_hz = std::sqrt(variance);

	/*
	 * Median frequency.
	 */
	double sorted[CARRIER_HISTORY];

	for (unsigned int i = 0; i < n; ++i)
		sorted[i] = values[i];

	sort_values(sorted, n);

	if (n & 1)
		carrier_recent_median_hz = sorted[n / 2];
	else
		carrier_recent_median_hz =
			0.5 * (sorted[n / 2 - 1] + sorted[n / 2]);

	/*
	 * Median absolute deviation (MAD).
	 */
	double deviation[CARRIER_HISTORY];

	for (unsigned int i = 0; i < n; ++i)
		deviation[i] =
			std::fabs(values[i] - carrier_recent_median_hz);

	sort_values(deviation, n);

	if (n & 1)
		carrier_recent_mad_hz = deviation[n / 2];
	else
		carrier_recent_mad_hz =
			0.5 * (deviation[n / 2 - 1] +
			       deviation[n / 2]);

	/*
	 * Experimental agreement count:
	 * observations within +/-10 Hz of the median.
	 */
	carrier_recent_good_count = 0;

	for (unsigned int i = 0; i < n; ++i) {
		if (std::fabs(values[i] -
			      carrier_recent_median_hz) <= carrier_tolerance)
			++carrier_recent_good_count;
	}
}

void cw_channel::carrier_update()
{
	if (!carrier_active || !phase_valid)
		return;

	++carrier_samples;

	carrier_sum_hz += phase_hz;
	carrier_sum_hz2 += phase_hz * phase_hz;
	carrier_sum_coherence += phase_coherence;

	const double n = static_cast<double>(carrier_samples);

	carrier_mean_hz = carrier_sum_hz / n;

	double variance =
		carrier_sum_hz2 / n -
		carrier_mean_hz * carrier_mean_hz;

	// Protect against a tiny negative value caused by roundoff.
	if (variance < 0.0)
		variance = 0.0;

	carrier_sigma_hz = std::sqrt(variance);

	carrier_mean_coherence =
		carrier_sum_coherence / n;

	/*
	 * Stage 8 experimental evidence value.
	 *
	 * Deliberately NOT used by the detector.
	 *
	 * A coherent, frequency-stable signal approaches 1.
	 * An unstable signal approaches 0.
	 *
	 * The 10 Hz scale is intentionally provisional.
	 */
	carrier_evidence =
		carrier_mean_coherence /
		(1.0 + carrier_sigma_hz / 10.0);

	carrier_history_update(phase_hz);
}

bool data_ok = false;

template <typename T>
void cw_channel::process_samples(const T *samples, int len)
{
	if (!samples || len <= 0)
		return;

	data_ok = false;
	for (int i = 0; i < len; i++)
		if (static_cast<double>(samples[i]) != 0.0) {
			data_ok = true;
			break;
		}

	for (int i = 0; i < len; ++i) {
		const double sample = static_cast<double>(samples[i]);

		// Retain the existing +j mixer convention for this controlled run.
		cmplx mixed(sample * std::cos(phase),
           -sample * std::sin(phase));

		phase += phase_increment;
		if (phase >= 2.0 * M_PI)
			phase -= 2.0 * M_PI;

		cmplx filtered;
		if (filter->run(mixed, filtered)) {
			update_phase(filtered);

			const double envelope = abs(filtered);
			const double smoothed = bitfilter->run(envelope);

			++output_samples;

			detect(envelope,
			       smoothed,
			       filtered.real(),
			       filtered.imag());
		}

		++input_samples;
	}
}

void cw_channel::process(const float *samples, int len)
{
	process_samples(samples, len);
}

void cw_channel::process(const double *samples, int len)
{
	process_samples(samples, len);
}

void cw_channel::detect(double envelope,
			double value,
			double i_value,
			double q_value)
{
	// Known startup behavior deliberately retained.
	if (agc_peak == 0.0) {
		if (value <= 0.0) {
			report(envelope, value, i_value, q_value,
				   0.0, 0.0, 0.0, 0.0, 0.0,
				   0.0, 0, 0);
			return;
		}

		sig_avg = 0.5 * value;
		noise_floor = value;
		agc_peak = value;
	}

	sig_avg = channel_decayavg(
		sig_avg,
		0.5 * value,
		(value > sig_avg) ? DETECTOR_ATTACK : DETECTOR_DECAY);

	if (value < sig_avg) {
		noise_floor = channel_decayavg(
			noise_floor,
			value,
			(value < noise_floor) ? DETECTOR_ATTACK : DETECTOR_DECAY);
	}

	if (value > sig_avg) {
		agc_peak = channel_decayavg(
			agc_peak,
			value,
			(value > agc_peak) ? DETECTOR_ATTACK : DETECTOR_DECAY);
	}

	double norm_value = 0.0;
	double norm_noise = 0.0;
	double norm_signal = 0.0;

	if (agc_peak > 0.0) {
		norm_value = value / agc_peak;
		norm_noise = noise_floor / agc_peak;
		norm_signal = sig_avg / agc_peak;
	}

	const double upper = 1.05 * norm_signal;
	const double lower = 0.95 * norm_signal;

	metric *= 0.8;
	if ((noise_floor > 1e-4) &&
	    (noise_floor < sig_avg) &&
	    (norm_noise > 0.0) &&
	    (norm_signal > 0.0)) {
		const double snr_db =
			20.0 * std::log10(norm_signal / norm_noise);

		metric +=
			0.2 * clamp_double(2.5 * snr_db, 0.0, 100.0);
	}

	double confidence = 0.0;
	const double center = 0.5 * (upper + lower);
	const double half_band = 0.5 * (upper - lower);

	if (half_band > 1e-12)
		confidence = (norm_value - center) / half_band;

	const bool old_state = in_tone;

	if (!in_tone) {
		if (norm_value > upper) {
			in_tone = true;
			carrier_start();
		}
	} else {
		if (norm_value < lower) {
			in_tone = false;
			carrier_active = false;
		}
	}

	const int transition =
		(in_tone == old_state) ? 0 : (in_tone ? 1 : -1);

	if (in_tone)
		carrier_update();

	observe_state(in_tone, confidence, norm_value, lower, upper);

	unsigned long run_samples = 0;
	if (timing_valid)
		run_samples = output_samples - timing_start_sample + 1;

	report(envelope, value, i_value, q_value,
	       norm_noise, norm_signal, norm_value,
	       lower, upper, confidence, transition, run_samples);
}

void cw_channel::open_observation_file()
{
	observer.close_csv();

	if (!observer.open_csv("cw_observations.csv"))
		std::fprintf(stderr,
			"CW Stage 11: cannot open cw_observations.csv\n");
}

void cw_channel::record_observation(double confidence,
				    unsigned long start_sample,
				    unsigned long end_sample,
				    unsigned long run_samples)
{
	if (run_samples == 0)
		return;

	cw_signal_attributes attr;

	attr.frequency = frequency;

	attr.start_sample = start_sample;
	attr.end_sample = end_sample;
	attr.duration_samples = run_samples;

	attr.confidence = confidence;

	attr.carrier_mean_hz =
		carrier_mean_hz;

	attr.carrier_sigma_hz =
		carrier_sigma_hz;

	attr.carrier_coherence =
		carrier_mean_coherence;

	attr.carrier_evidence =
		carrier_evidence;

	attr.recent_mean_hz =
		carrier_recent_mean_hz;

	attr.recent_sigma_hz =
		carrier_recent_sigma_hz;

	attr.recent_median_hz =
		carrier_recent_median_hz;

	attr.recent_mad_hz =
		carrier_recent_mad_hz;

	attr.recent_good_count =
		carrier_recent_good_count;

	cw_observation obs;
	observer.observe(attr, obs);
}

void cw_channel::observe_state(bool new_state,
			       double confidence,
			       double norm_value,
			       double lower,
			       double upper)
{
	if (!timing_valid) {
		timing_valid = true;
		timing_state = new_state;
		timing_start_sample = output_samples;
		return;
	}

	if (new_state == timing_state)
		return;

	const unsigned long run_samples =
		output_samples - timing_start_sample;

	const double run_ms =
		1000.0 * run_samples * DECIMATE /
		static_cast<double>(SAMPLE_RATE);

	const double t =
		input_samples / static_cast<double>(SAMPLE_RATE);

	if (events) {
		const double carrier_ms =
			1000.0 * carrier_samples * DECIMATE /
			static_cast<double>(SAMPLE_RATE);

		std::fprintf(events,
			"%.6f,%.6f,%s,%d,%lu,%.3f,%.9g,%.9g,%.9g,%.9g,"
			"%.9g,%.9g,%.9g,"
			"%u,%.3f,%.9g,%.9g,%.9g,%.9g,"
			"%u,%u,%.9g,%.9g,%.9g,%.9g,%u,"
			"%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",
			t,
			frequency,
			new_state ? "KEYDOWN" : "KEYUP",
			timing_state ? 1 : 0,
			run_samples,
			run_ms,
			confidence,
			norm_value,
			lower,
			upper,
			phase_delta,
			phase_hz,
			phase_coherence,
			carrier_samples,
			carrier_ms,
			carrier_mean_hz,
			carrier_sigma_hz,
			carrier_mean_coherence,
			carrier_evidence,
			carrier_history_count,
			carrier_history_next,
			carrier_recent_mean_hz,
			carrier_recent_sigma_hz,
			carrier_recent_median_hz,
			carrier_recent_mad_hz,
			carrier_recent_good_count,
			carrier_history[0],
			carrier_history[1],
			carrier_history[2],
			carrier_history[3],
			carrier_history[4],
			carrier_history[5],
			carrier_history[6]
		);
	}

	/*
	 * Stage 11:
	 * A transition from KEYDOWN to KEYUP completes a candidate mark.
	 * Capture its signal attributes before advancing the timing state.
	 */
	if (timing_state && !new_state)
		record_observation(
			confidence,
				timing_start_sample,
				output_samples,
				run_samples);

	timing_state = new_state;
	timing_start_sample = output_samples;
}

void cw_channel::report(double envelope,
			double smoothed,
			double i_value,
			double q_value,
			double norm_noise,
			double norm_signal,
			double norm_value,
			double lower,
			double upper,
			double confidence,
			int transition,
			unsigned long run_samples)
{
	if (! data_ok ) return;

	if (!diagnostic)
		return;

	const double t =
		input_samples / static_cast<double>(SAMPLE_RATE);

	const double run_ms =
		1000.0 * run_samples * DECIMATE /
		static_cast<double>(SAMPLE_RATE);

	const double carrier_ms =
		1000.0 * carrier_samples * DECIMATE /
		static_cast<double>(SAMPLE_RATE);

	std::fprintf(diagnostic,
		"%.6f,%.6f,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,"
		"%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,"
		"%d,%d,%lu,%.3f,%.6g,"
		"%u,%.3f,%.9g,%.9g,%.9g,%.9g,"
		"%u,%u,%.9g,%.9g,%.9g,%.9g,%u,"
		"%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",
		t,
		frequency,
		envelope,
		smoothed,
		i_value,
		q_value,
		phase_delta,
		phase_hz,
		phase_coherence,
		norm_noise,
		norm_signal,
		agc_peak,
		norm_value,
		lower,
		upper,
		confidence,
		in_tone ? 1 : 0,
		transition,
		run_samples,
		run_ms,
		metric,
		carrier_samples,
		carrier_ms,
		carrier_mean_hz,
		carrier_sigma_hz,
		carrier_mean_coherence,
		carrier_evidence,
		carrier_history_count,
		carrier_history_next,
		carrier_recent_mean_hz,
		carrier_recent_sigma_hz,
		carrier_recent_median_hz,
		carrier_recent_mad_hz,
		carrier_recent_good_count,
		carrier_history[0],
		carrier_history[1],
		carrier_history[2],
		carrier_history[3],
		carrier_history[4],
		carrier_history[5],
		carrier_history[6]
	);

}
