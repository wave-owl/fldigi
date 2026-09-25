// ----------------------------------------------------------------------------
// cw_channel.h -- experimental CW narrow-band channel extractor/detector
// Stage 7: externally calibrated NCO experiment + explicit NCO diagnostics.
// ----------------------------------------------------------------------------

#ifndef CW_CHANNEL_H
#define CW_CHANNEL_H

#include <cstdio>

#include "complex.h"
#include "filters.h"

#include "cw_observer.h"

class cw_channel
{
#define CARRIER_HISTORY 20

public:
	cw_channel();
	~cw_channel();

	void open_csv_files();
	void init(double frequency);
	void reset();

	void set_frequency(double frequency);
	double get_frequency() const { return frequency; }

	void process(const float *samples, int len);
	void process(const double *samples, int len);

private:
	static const int SAMPLE_RATE = 8000;
	static const int DECIMATE = 16;
	static const int FIR_LENGTH = 512;

	double  lowpass_hz;

	static const int DETECTOR_ATTACK = 200;
	static const int DETECTOR_DECAY = 1000;
	static const int BITFILTER_LENGTH = 4;
	static const double PHASE_ALPHA;

	// cw observer
	cw_observer observer;

	// Stage 8 carrier-evidence accumulator
	bool   carrier_active;
	unsigned carrier_samples;

	double carrier_sum_hz;
	double carrier_sum_hz2;
	double carrier_sum_coherence;

	double carrier_mean_hz;
	double carrier_sigma_hz;
	double carrier_mean_coherence;

	double carrier_evidence;

	// Stage 9 recent carrier-frequency history
	unsigned int carrier_history_size; //CARRIER_HISTORY = 7;

	double carrier_history[CARRIER_HISTORY];
	unsigned int carrier_history_count;
	unsigned int carrier_history_next;

	double carrier_recent_mean_hz;
	double carrier_recent_sigma_hz;
	double carrier_recent_median_hz;
	double carrier_recent_mad_hz;
	unsigned int carrier_recent_good_count;

	C_FIR_filter *filter;
	Cmovavg *bitfilter;
	FILE *diagnostic;
	FILE *events;

	double frequency;
	double phase;
	double phase_increment;
	double carrier_tolerance;

	unsigned long input_samples;
	unsigned long output_samples;

	double sig_avg;
	double noise_floor;
	double agc_peak;
	double metric;
	bool in_tone;

	bool timing_valid;
	bool timing_state;
	unsigned long timing_start_sample;

	bool phase_valid;
	cmplx previous_filtered;
	double phase_mean_cos;
	double phase_mean_sin;
	double phase_delta;
	double phase_hz;
	double phase_coherence;

	void configure_filter();
	void reset_detector();
	void reset_timing();
	void reset_phase();
	void update_phase(const cmplx& filtered);

	void carrier_start();
	void carrier_update();
	void carrier_history_reset();
	void carrier_history_update(double hz);
	void carrier_history_statistics();

	template <typename T>
	void process_samples(const T *samples, int len);

	void detect(
		double envelope,
		double smoothed,
		double i_value,
		double q_value);

	void open_observation_file();

	void record_observation(double confidence,
		unsigned long start_sample,
		unsigned long end_sample,
		unsigned long run_samples);

	void observe_state(
		bool new_state,
		double confidence,
		double norm_value,
		double lower,
		double upper);

	void report(
		double envelope,
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
		unsigned long run_samples);

};

#endif // CW_CHANNEL_H
