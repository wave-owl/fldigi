#ifndef CW_TRACKER_H
#define CW_TRACKER_H

#include <stdint.h>
#include <vector>

#include "cw_signal.h"

struct cw_track
{
	double frequency;          // smoothed current frequency
	double origin_frequency;   // frequency at creation; limits track walking
	double peak_frequency;     // most recent matched peak
	double peak_snr;           // most recent matched peak, linear power ratio
	double max_snr;            // strongest observed SNR, linear power ratio

	unsigned long created;
	unsigned long last_seen;

	unsigned int hits;         // lifetime matched observations
	unsigned int misses;       // consecutive unmatched frames

	uint32_t hit_history;      // newest observation in bit 0
	unsigned int window_hits;
	unsigned int longest_run;
	unsigned int transitions;

	double confidence;
	double hit_strength;     // accumulated hit contribution, no miss penalty
	bool active;
};

class cw_tracker
{
public:
	cw_tracker();

	void reset();
	void process(const std::vector<cw_spectral_peak>& peaks,
		     unsigned long frame);

	const std::vector<cw_track>& tracks() const
	{
		return signal_tracks;
	}

private:
	static const double ASSOCIATION_HZ;
	static const double ACTIVE_ORIGIN_HZ;
	static const double FREQUENCY_ALPHA;
	static const double ACTIVE_CONFIDENCE;

	static const unsigned int MIN_ACTIVE_HITS = 3;
	static const unsigned int MIN_WINDOW_HITS = 3;
	static const unsigned int MIN_HIT_RUN = 2;
	static const unsigned int CANDIDATE_TIMEOUT = 8;
	static const unsigned int ACTIVE_TIMEOUT = 32;
	static const unsigned int HISTORY_BITS = 32;

	std::vector<cw_track> signal_tracks;

	void update_track(cw_track& track,
			  const cw_spectral_peak& peak,
			  unsigned long frame);
	void create_track(const cw_spectral_peak& peak,
			  unsigned long frame);
	void record_history(cw_track& track, bool hit);
	void measure_history(cw_track& track);
	bool may_associate(const cw_track& track,
			   const cw_spectral_peak& peak) const;
	double hit_contribution(const cw_spectral_peak& peak) const;
};

#endif // CW_TRACKER_H
