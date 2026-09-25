#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "cw_tracker.h"

const double cw_tracker::ASSOCIATION_HZ = 12.0;
const double cw_tracker::ACTIVE_ORIGIN_HZ = 15.0;
const double cw_tracker::FREQUENCY_ALPHA = 0.20;
const double cw_tracker::ACTIVE_CONFIDENCE = 6.0;

cw_tracker::cw_tracker()
{
	reset();
}

void cw_tracker::reset()
{
	signal_tracks.clear();
}

double cw_tracker::hit_contribution(const cw_spectral_peak& peak) const
{
	// cw_spectrum reports SNR as a linear power ratio.
	if (peak.snr <= 0.0)
		return 1.0;

	double snr_db = 10.0 * std::log10(peak.snr);
	double contribution = snr_db / 6.0;

	if (contribution < 1.0)
		contribution = 1.0;
	else if (contribution > 3.0)
		contribution = 3.0;

	return contribution;
}

void cw_tracker::measure_history(cw_track& track)
{
	unsigned int hits = 0;
	unsigned int longest = 0;
	unsigned int run = 0;
	unsigned int transitions = 0;
	bool previous = false;
	bool have_previous = false;

	for (unsigned int i = 0; i < HISTORY_BITS; ++i) {
		bool bit = ((track.hit_history >> i) & 1U) != 0;

		if (bit) {
			++hits;
			++run;
			if (run > longest)
				longest = run;
		} else {
			run = 0;
		}

		if (have_previous && bit != previous)
			++transitions;

		previous = bit;
		have_previous = true;
	}

	track.window_hits = hits;
	track.longest_run = longest;
	track.transitions = transitions;
}

void cw_tracker::record_history(cw_track& track, bool hit)
{
	track.hit_history <<= 1;
	if (hit)
		track.hit_history |= 1U;

	measure_history(track);
}

bool cw_tracker::may_associate(const cw_track& track,
			       const cw_spectral_peak& peak) const
{
	if (std::fabs(peak.frequency - track.frequency) > ASSOCIATION_HZ)
		return false;

	// Once established, do not allow a sequence of nearby unrelated peaks
	// to walk the track arbitrarily far from the frequency where it formed.
	if (track.active &&
	    std::fabs(peak.frequency - track.origin_frequency) > ACTIVE_ORIGIN_HZ)
		return false;

	return true;
}

void cw_tracker::update_track(cw_track& track,
			      const cw_spectral_peak& peak,
			      unsigned long frame)
{
	track.peak_frequency = peak.frequency;
	track.peak_snr = peak.snr;
	if (peak.snr > track.max_snr)
		track.max_snr = peak.snr;

	track.frequency =
		(1.0 - FREQUENCY_ALPHA) * track.frequency +
		FREQUENCY_ALPHA * peak.frequency;

	track.last_seen = frame;
	track.hits++;
	track.misses = 0;

	double contribution = hit_contribution(peak);

	track.confidence += contribution;
	track.hit_strength += contribution;

	record_history(track, true);

	if (!track.active &&
	    track.hits >= MIN_ACTIVE_HITS &&
	    track.window_hits >= MIN_WINDOW_HITS &&
	    track.longest_run >= MIN_HIT_RUN &&
	    track.confidence >= ACTIVE_CONFIDENCE) {
		track.active = true;

		// Establish the center of the accepted track here rather than using
		// the very first candidate peak, which may have been poorly estimated.
		track.origin_frequency = track.frequency;
/*
		std::fprintf(stderr,
			"CWTRACK ACTIVE frame=%lu f=%8.2f Hz hits=%u "
			"win=%u run=%u trans=%u conf=%5.2f "
			"avg=%5.2f\n",
			frame, track.frequency, track.hits,
			track.window_hits, track.longest_run,
			track.transitions, track.confidence,
			track.hit_strength / track.hits);
*/
	}
}

void cw_tracker::create_track(const cw_spectral_peak& peak,
			      unsigned long frame)
{
	cw_track track;

	track.frequency = peak.frequency;
	track.origin_frequency = peak.frequency;
	track.peak_frequency = peak.frequency;
	track.peak_snr = peak.snr;
	track.max_snr = peak.snr;

	track.created = frame;
	track.last_seen = frame;

	track.hits = 1;
	track.misses = 0;

	track.hit_history = 1U;
	track.window_hits = 1;
	track.longest_run = 1;
	track.transitions = 1;

	track.confidence = hit_contribution(peak);
	track.hit_strength = track.confidence;
	track.active = false;

	signal_tracks.push_back(track);
}

void cw_tracker::process(const std::vector<cw_spectral_peak>& peaks,
			 unsigned long frame)
{
	std::vector<bool> peak_used(peaks.size(), false);
	std::vector<bool> track_matched(signal_tracks.size(), false);

	// Associate each existing track with the nearest unused peak which
	// satisfies both the normal frequency gate and, for an active track,
	// the origin-frequency constraint.
	for (std::size_t ti = 0; ti < signal_tracks.size(); ++ti) {
		std::size_t best = peaks.size();
		double best_delta = ASSOCIATION_HZ;

		for (std::size_t pi = 0; pi < peaks.size(); ++pi) {
			if (peak_used[pi])
				continue;

			if (!may_associate(signal_tracks[ti], peaks[pi]))
				continue;

			double delta = std::fabs(peaks[pi].frequency -
						 signal_tracks[ti].frequency);

			if (delta <= best_delta) {
				best_delta = delta;
				best = pi;
			}
		}

		if (best != peaks.size()) {
			update_track(signal_tracks[ti], peaks[best], frame);
			peak_used[best] = true;
			track_matched[ti] = true;
		}
	}

	// A missing FFT observation is part of the temporal signature.  Record
	// the miss before aging confidence so the 32-frame history represents
	// every analysis frame, not just successful associations.
	for (std::size_t ti = 0; ti < signal_tracks.size(); ++ti) {
		if (!track_matched[ti]) {
			record_history(signal_tracks[ti], false);

			signal_tracks[ti].misses++;
			signal_tracks[ti].confidence -= 1.0;
			if (signal_tracks[ti].confidence < 0.0)
				signal_tracks[ti].confidence = 0.0;
		}
	}

	// Established signals survive ordinary CW spaces much longer than
	// unconfirmed candidates.
	for (std::size_t ti = signal_tracks.size(); ti-- > 0; ) {
		const cw_track& track = signal_tracks[ti];
		unsigned int timeout =
			track.active ? ACTIVE_TIMEOUT : CANDIDATE_TIMEOUT;

		if (track.misses > timeout ||
		    (!track.active &&
		     track.confidence <= 0.0 &&
		     track.misses > 0)) {
			if (track.active) {
/*
				std::fprintf(stderr,
					"CWTRACK LOST   frame=%lu f=%8.2f Hz hits=%u "
					"win=%u run=%u trans=%u avg=%5.2f\n",
					frame, track.frequency, track.hits,
					track.window_hits, track.longest_run,
					track.transitions,
					track.hit_strength / track.hits);
*/
			}

			signal_tracks.erase(signal_tracks.begin() + ti);
		}
	}

	// Every unused spectral observation begins a new hypothesis.  Neighbor
	// suppression remains deliberately disabled until the close-signal
	// recordings tell us how much suppression is safe.
	for (std::size_t pi = 0; pi < peaks.size(); ++pi) {
		if (!peak_used[pi])
			create_track(peaks[pi], frame);
	}
}
