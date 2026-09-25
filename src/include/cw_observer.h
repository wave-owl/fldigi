#ifndef CW_OBSERVER_H
#define CW_OBSERVER_H

#include <cstdio>
#include <string>

/*
 * Stage 11
 *
 * A completed candidate CW mark and its measured signal attributes.
 *
 * This structure deliberately contains no Morse interpretation:
 *
 *     no dit/dah classification
 *     no WPM estimate
 *     no character interpretation
 *
 * Those belong to later stages.
 */
struct cw_observation
{
	unsigned long sequence;

	double frequency;

	double start_time;
	double end_time;
	double duration_ms;
	bool preceding_space_valid;
	double preceding_space_ms;

	double confidence;

	double carrier_mean_hz;
	double carrier_sigma_hz;
	double carrier_coherence;
	double carrier_evidence;

	double recent_mean_hz;
	double recent_sigma_hz;
	double recent_median_hz;
	double recent_mad_hz;
	unsigned recent_good_count;

	bool valid;

	cw_observation();
};


/*
 * Measurements supplied by cw_channel when a candidate mark ends.
 *
 * Keeping this separate from cw_observation means cw_channel need not
 * know anything about observation sequencing, preceding-space timing,
 * or CSV output.
 */
struct cw_signal_attributes
{
	double frequency;

	unsigned long start_sample;
	unsigned long end_sample;
	unsigned long duration_samples;

	double confidence;

	double carrier_mean_hz;
	double carrier_sigma_hz;
	double carrier_coherence;
	double carrier_evidence;

	double recent_mean_hz;
	double recent_sigma_hz;
	double recent_median_hz;
	double recent_mad_hz;
	unsigned recent_good_count;

	cw_signal_attributes();
};


class cw_observer
{
public:
	cw_observer();
	~cw_observer();

	void reset();

	void set_sample_timing(double sample_rate, unsigned decimation);

	bool open_csv(const std::string& filename);
	void close_csv();

	/*
	 * Accept one completed candidate mark.
	 *
	 * Returns true and fills obs when an observation was produced.
	 */
	bool observe(const cw_signal_attributes& attr,
		     cw_observation& obs);

private:
	double sample_rate;
	unsigned decimation;

	unsigned long sequence;

	bool have_previous_mark;
	unsigned long previous_end_sample;

	FILE *csv;

	double samples_to_seconds(unsigned long samples) const;
	double samples_to_ms(unsigned long samples) const;

	void write_csv_header();
	void write_csv(const cw_observation& obs);
};

#endif
