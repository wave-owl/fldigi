// ----------------------------------------------------------------------------
// timesig_decoder.h  --  LF time signal decoder (signal processing core)
//
// Copyright (C) 2026
//		Domenico MrGrady Porto
//
// Developed with the assistance of Claude (Anthropic).
//
// This file is part of fldigi.
//
// Fldigi is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Fldigi is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with fldigi.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------
//
// The core has no dependency on the rest of fldigi so that it can be unit
// tested on its own.  The receiver is expected to produce an audio tone
// (CW / SSB demodulation of the carrier).  DCF77, MSF and WWVB all reduce the
// carrier at the start of every second; the length of the reduction carries
// one symbol per second and a 60 second frame carries the time code.  The
// pulse detector and the minute synchroniser are shared, the stations differ
// only in the Format (symbols, minute marker and telegram layout).

#ifndef TIMESIG_DECODER_H
#define TIMESIG_DECODER_H

#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace tscore {

enum { ENV_RATE = 1000 };          // envelope samples per second (1 ms each)

// symbols: 0, 1 (DCF77, WWVB) or A + 2 B (MSF), and the markers
enum {
	SYM_UNSEEN  = -2,              // second not observed
	SYM_MISSING = -1,              // second without a valid pulse
	SYM_MARK    = 8                // marker (MSF second 0, WWVB P0..P5)
};

// Coherent tone demodulator: mixes the audio with a local oscillator at the
// tone frequency, low pass filters I/Q (2 poles) and decimates the magnitude
// to ENV_RATE.
class ToneDemod {
public:
	ToneDemod(double samplerate, double freq, double bandwidth);
	void set_freq(double f);
	void set_bandwidth(double bw);
	double get_freq() const { return freq; }
	// appends envelope samples to out
	void process(const double *in, int len, std::vector<double> &out);
private:
	double fs, freq, dphi, alpha;
	double phase, i1, i2, q1, q2;
	double dec_acc, sum;
	int n;
};

// A decoded minute.  The frame starts at second 0 of a minute; DCF77 and MSF
// transmit the time of the following minute, WWVB the time of the current one.
struct Frame {
	int sym[60];           // index = second of the minute (after repair)
	bool valid;
	std::string errors;    // comma separated, empty if valid
	std::vector<int> repaired;
	int missing;
	int utc_offset;        // hours: station time = UTC + utc_offset
	const char *zone;      // "CET", "CEST", "GMT", "BST", "UTC"
	time_t utc;            // UTC of the start of the frame (valid frames)
	std::string notes;     // announcements, DUT1, ... (printable)
	bool by_continuity;    // completed by comparing with a previous frame
	bool unconfirmed;      // decoded but not yet confirmed (no check bits)
};

typedef std::map<long, int> SymbolLog;   // absolute second -> symbol
int symbol_at(const SymbolLog &log, long sec);   // SYM_UNSEEN if absent

class Format {
public:
	virtual ~Format() {}
	virtual const char *name() const = 0;
	// symbol of a carrier reduction of `dur` ms at the start of a second, -1 if invalid
	virtual int classify(int dur) const = 0;
	// a second reduction `gap` ms after the first one in the same second:
	// new symbol for that second, -1 if not allowed
	virtual int second_pulse(int /*sym*/, int /*gap*/, int /*dur*/) const { return -1; }
	// time after the start of a second when its symbol is final (ms)
	virtual int settle_ms() const { return 0; }
	// evidence that a minute starts at absolute second s0 (the score of the
	// telegram structure).  conflict: a pulse contradicts the marker;
	// marker: the marker itself was observed.
	virtual int score(const SymbolLog &log, long s0, bool &marker, bool &conflict) const = 0;
	virtual Frame decode(const int *sym) const = 0;
	// symbols of the frame starting at UTC `start` (tests, continuity)
	virtual void encode(time_t start, int *sym) const = 0;
	// symbols that encode() can predict
	virtual bool predictable(int sec) const = 0;
	// printing: a space before this second, and the character of a symbol
	virtual bool group_start(int sec) const = 0;
	virtual char sym_char(int sym) const;
	// a second that never carries a pulse (DCF77 second 59)
	virtual bool silent(int /*sec*/) const { return false; }
	// the telegram has check bits (parity)
	virtual bool checked() const { return true; }
};

const Format &format_dcf77();
const Format &format_msf();
const Format &format_wwvb();

// Calendar helpers (proleptic Gregorian, UTC)
long   days_from_civil(int y, int m, int d);
void   civil_from_days(long days, int &y, int &m, int &d);
time_t utc_from_civil(int y, int mo, int d, int h, int mi);
// European summer time (last Sunday of March to last Sunday of October, 01:00 UTC)
bool   eu_summer_time(time_t utc);

// Checks an incomplete frame against the one expected to start at UTC
// `start` (from a previous valid frame).  Returns true and fills `out` if
// compatible.
bool check_continuity(const Format &fmt, const int *sym, time_t start, Frame &out);

// Validates the frames over time: completes damaged frames from the previous
// valid one and, for formats without check bits, accepts a frame only when
// it agrees with the previous decode.  `end_time` is the time (s, any clock)
// of the end of the frame.
class FrameTracker {
public:
	explicit FrameTracker(const Format &f) : fmt(f) { reset(); }
	void reset() { have_prev = have_raw = false; consecutive = 0; }
	Frame process(const Frame &f, double end_time);
	int consecutive;       // valid frames in a row
private:
	const Format &fmt;
	bool have_prev, have_raw;
	time_t prev_utc, raw_utc;
	double prev_end, raw_end;
};

// Event sink for the decoder
class Listener {
public:
	virtual ~Listener() {}
	virtual void on_pulse(long /*t*/, int /*dur*/, int /*sym*/, int /*sec*/, int /*missed_before*/) {}
	virtual void on_glitch(long /*t*/, int /*dur*/) {}
	virtual void on_sync(bool /*synced*/, const char * /*reason*/) {}
	// t0 = envelope sample index of the start of the minute that follows the frame
	virtual void on_frame(const Frame & /*f*/, long /*t0*/) {}
};

// Pulse detector and minute synchroniser.  Every received pulse gets an
// "absolute second" number; a log of the last two minutes is used to score
// all 60 possible positions of the minute against the telegram structure and
// the best one wins.
class Decoder {
public:
	Decoder(const Format &fmt, Listener *l);
	void reset(const char *reason);
	void push(const double *env, int len);
	bool synced() const { return phase >= 0; }
	int  second() const { return cur_sec; }
	int  score() const { return best_score; }
	long now() const { return t; }
	double depth_db() const { return depth; }
	bool present() const { return signal_present; }
	void set_invert(bool inv) { invert = inv; }
	const Format &format() const { return fmt; }
private:
	void step(double x);
	void update_levels();
	void on_rise(long tr);
	void report_pending();
	int  score_phase(int p, bool &ok);
	void choose_phase();
	void set_phase(int p, const char *reason);

	const Format &fmt;
	Listener *listener;
	long t;
	std::vector<double> win;
	size_t wi, filled;
	double hi, lo, th, hyst, depth;
	bool signal_present, invert;
	int state, cand, cand_n;
	long cand_t, fall_t, last_fall;
	bool have_fall;
	bool rise_pend;
	long rise_t;

	SymbolLog log;
	long A, first_A, last_emitted;
	bool have_A, have_emitted;
	int phase, best_score, cur_sec;

	// pulse waiting for settle_ms() before being reported
	bool pend;
	long pend_t;
	int pend_dur, pend_missed;
};

} // namespace tscore

#endif
