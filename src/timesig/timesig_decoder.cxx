// ----------------------------------------------------------------------------
// timesig_decoder.cxx  --  LF time signal decoder (signal processing core)
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

#include <algorithm>
#include <cmath>
#include <cstring>

#include "timesig_decoder.h"

namespace tscore {

static const double TWO_PI = 2.0 * M_PI;
static const long LOG_SPAN = 125;     // seconds kept in the phase log
static const long RISE_CONFIRM = 40;  // ms

static inline long lmod(long a, long n) { return ((a % n) + n) % n; }

//======================================================================
// ToneDemod
//======================================================================

ToneDemod::ToneDemod(double samplerate, double f, double bw)
	: fs(samplerate), phase(0), i1(0), i2(0), q1(0), q2(0), dec_acc(0), sum(0), n(0)
{
	set_freq(f);
	set_bandwidth(bw);
}

void ToneDemod::set_freq(double f)
{
	freq = f;
	dphi = TWO_PI * f / fs;
}

void ToneDemod::set_bandwidth(double bw)
{
	alpha = 1.0 - exp(-TWO_PI * bw / fs);
}

void ToneDemod::process(const double *in, int len, std::vector<double> &out)
{
	const double step = ENV_RATE / fs;
	for (int k = 0; k < len; k++) {
		double x = in[k];
		i1 += alpha * (x * cos(phase) - i1);
		q1 += alpha * (x * sin(phase) - q1);
		i2 += alpha * (i1 - i2);
		q2 += alpha * (q1 - q2);
		phase += dphi;
		if (phase > TWO_PI) phase -= TWO_PI;
		sum += 2.0 * sqrt(i2 * i2 + q2 * q2);
		n++;
		dec_acc += step;
		if (dec_acc >= 1.0) {
			dec_acc -= 1.0;
			out.push_back(sum / n);
			sum = 0;
			n = 0;
		}
	}
}

//======================================================================
// Calendar
//======================================================================

long days_from_civil(int y, int m, int d)
{
	y -= m <= 2;
	const long era = (y >= 0 ? y : y - 399) / 400;
	const long yoe = y - era * 400;
	const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + doe - 719468;
}

void civil_from_days(long z, int &y, int &m, int &d)
{
	z += 719468;
	const long era = (z >= 0 ? z : z - 146096) / 146097;
	const long doe = z - era * 146097;
	const long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	const long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	const long mp = (5 * doy + 2) / 153;
	d = (int)(doy - (153 * mp + 2) / 5 + 1);
	m = (int)(mp < 10 ? mp + 3 : mp - 9);
	y = (int)(yoe + era * 400 + (m <= 2));
}

time_t utc_from_civil(int y, int mo, int d, int h, int mi)
{
	return (time_t)days_from_civil(y, mo, d) * 86400 + h * 3600 + mi * 60;
}

// last Sunday of a month, 01:00 UTC
static time_t eu_switch(int year, int month)
{
	long d = days_from_civil(year, month, 31);
	long dow = lmod(d + 4, 7);   // 0 = Sunday
	return (time_t)(d - dow) * 86400 + 3600;
}

bool eu_summer_time(time_t utc)
{
	int y, m, d;
	civil_from_days((long)floor(utc / 86400.0), y, m, d);
	return utc >= eu_switch(y, 3) && utc < eu_switch(y, 10);
}

int symbol_at(const SymbolLog &log, long sec)
{
	SymbolLog::const_iterator it = log.find(sec);
	return it == log.end() ? SYM_UNSEEN : it->second;
}

char Format::sym_char(int sym) const
{
	if (sym == SYM_MARK) return 'M';
	if (sym < 0) return '_';
	return (char)('0' + sym);
}

bool check_continuity(const Format &fmt, const int *sym, time_t start, Frame &out)
{
	int expected[60];
	fmt.encode(start, expected);
	int nknown = 0;
	for (int k = 0; k < 60; k++) {
		if (!fmt.predictable(k) || sym[k] < 0) continue;
		if (sym[k] != expected[k]) return false;
		nknown++;
	}
	if (nknown < 30) return false;
	int merged[60];
	for (int k = 0; k < 60; k++) merged[k] = sym[k] >= 0 ? sym[k] : expected[k];
	out = fmt.decode(merged);
	if (!out.valid || out.utc != start) return false;
	out.by_continuity = true;
	return true;
}

//======================================================================
// FrameTracker
//======================================================================

static int minutes_between(double a, double b)
{
	return (int)floor((b - a) / 60.0 + 0.5);
}

Frame FrameTracker::process(const Frame &in, double end_time)
{
	Frame f = in;
	if (f.valid && !fmt.checked()) {
		// a single wrong bit would go unnoticed: require agreement with the
		// previous decode or with the last valid frame
		bool agree =
			(have_raw && f.utc == raw_utc + minutes_between(raw_end, end_time) * 60) ||
			(have_prev && f.utc == prev_utc + minutes_between(prev_end, end_time) * 60);
		have_raw = true;
		raw_utc = f.utc;
		raw_end = end_time;
		if (!agree) {
			f.valid = false;
			f.unconfirmed = true;
		}
	}
	if (!f.valid && have_prev) {
		int minutes = minutes_between(prev_end, end_time);
		Frame c;
		if (minutes >= 1 && minutes <= 30 &&
		    check_continuity(fmt, f.sym, prev_utc + minutes * 60, c))
			f = c;
	}
	if (f.valid) {
		if (have_prev && f.utc == prev_utc + minutes_between(prev_end, end_time) * 60)
			consecutive++;
		else
			consecutive = 1;
		have_prev = true;
		prev_utc = f.utc;
		prev_end = end_time;
	} else
		consecutive = 0;
	return f;
}

//======================================================================
// Decoder
//======================================================================

Decoder::Decoder(const Format &f, Listener *l)
	: fmt(f), listener(l), t(0), win(3000, 0.0), wi(0), filled(0),
	  hi(0), lo(0), th(0), hyst(0), depth(0), signal_present(false), invert(false),
	  state(1), cand(1), cand_n(0), cand_t(0), fall_t(0), last_fall(0), have_fall(false),
	  rise_pend(false), rise_t(0),
	  A(0), first_A(0), last_emitted(0), have_A(false), have_emitted(false),
	  phase(-1), best_score(0), cur_sec(-1),
	  pend(false), pend_t(0), pend_dur(0), pend_missed(0)
{
}

void Decoder::reset(const char *reason)
{
	log.clear();
	have_A = false;
	have_emitted = false;
	have_fall = false;
	rise_pend = false;
	pend = false;
	last_fall = 0;
	cur_sec = -1;
	best_score = 0;
	// report only a lost lock, not every reset while searching
	bool was_synced = phase >= 0;
	phase = -1;
	if (listener && was_synced) listener->on_sync(false, reason);
}

void Decoder::set_phase(int p, const char *reason)
{
	if (p == phase) return;
	phase = p;
	if (listener) listener->on_sync(p >= 0, reason);
}

void Decoder::push(const double *env, int len)
{
	for (int k = 0; k < len; k++) step(invert ? -env[k] : env[k]);
}

void Decoder::update_levels()
{
	std::vector<double> s(win.begin(), win.begin() + filled);
	std::sort(s.begin(), s.end());
	lo = s[(size_t)(filled * 0.04)];
	hi = s[(size_t)(filled * 0.75)];
	th = lo + 0.45 * (hi - lo);
	hyst = 0.1 * (hi - lo);
	depth = (lo > 1e-12 && hi > 0) ? 20.0 * log10(hi / lo) : 60.0;
	signal_present = (hi - lo) > 1e-6 && depth > 7.0;
}

void Decoder::step(double x)
{
	long now = t++;
	win[wi] = x;
	wi = (wi + 1) % win.size();
	if (filled < win.size()) filled++;
	if (now % 100 == 0 && filled > 200) update_levels();
	if (pend && now - pend_t >= fmt.settle_ms()) report_pending();
	if (have_A && now - last_fall > 30000) reset("no pulses for 30 s");
	if (filled < 1500 || !signal_present) return;

	// hysteresis + 12 ms debounce
	int want = state == 1 ? (x < th - hyst ? 0 : 1) : (x > th + hyst ? 1 : 0);
	if (want != state) {
		if (cand != want) { cand = want; cand_t = now; cand_n = 0; }
		if (++cand_n >= 12) {
			state = want;
			if (want == 1) { rise_pend = true; rise_t = cand_t; }
			else if (rise_pend) rise_pend = false;   // noise burst inside a pulse
			else { fall_t = cand_t; have_fall = true; }
		}
	} else {
		cand = state;
		cand_n = 0;
	}
	// the carrier must stay up for a while to end a pulse
	if (rise_pend && now - rise_t >= RISE_CONFIRM) {
		rise_pend = false;
		on_rise(rise_t);
	}
}

void Decoder::report_pending()
{
	if (!pend) return;
	pend = false;
	if (listener) listener->on_pulse(pend_t, pend_dur, symbol_at(log, A), cur_sec, pend_missed);
}

void Decoder::on_rise(long tr)
{
	if (!have_fall) return;
	have_fall = false;
	long fall = fall_t;
	int dur = (int)(tr - fall);
	long gap = have_A ? fall - last_fall : -1;

	if (have_A && gap < 850) {
		// a second reduction in the same second (MSF "01"), or a spurious pulse
		int cur = symbol_at(log, A);
		int s = cur >= 0 ? fmt.second_pulse(cur, (int)gap, dur) : -1;
		if (s >= 0)
			log[A] = s;
		else if (listener)
			listener->on_glitch(fall, dur);
		return;
	}
	int sym = fmt.classify(dur);
	if (sym < 0) {
		if (listener) listener->on_glitch(fall, dur);
		return;
	}
	report_pending();

	long n = have_A ? (gap + 500) / 1000 : 0;
	if (have_A && labs(gap - n * 1000) > 100) reset("irregular timing");

	int missed = 0;
	if (!have_A) {
		have_A = true;
		A = first_A = 0;
	} else {
		for (long k = 1; k < n; k++) log[A + k] = SYM_MISSING;
		missed = (int)(n - 1);
		A += n;
	}
	last_fall = fall;
	log[A] = sym;
	while (!log.empty() && log.begin()->first < A - LOG_SPAN) log.erase(log.begin());

	choose_phase();
	cur_sec = phase < 0 ? -1 : (int)lmod(A - phase, 60);

	// a new minute has started: close the previous frame
	if (phase >= 0) {
		long s0 = A - cur_sec - 60;
		if ((!have_emitted || s0 != last_emitted) && s0 >= first_A) {
			have_emitted = true;
			last_emitted = s0;
			int sym60[60];
			for (int k = 0; k < 60; k++) {
				int v = symbol_at(log, s0 + k);
				sym60[k] = v >= 0 ? v : SYM_MISSING;
			}
			Frame f = fmt.decode(sym60);
			if (listener) listener->on_frame(f, fall - (long)cur_sec * 1000);
		}
	}

	pend = true;
	pend_t = fall;
	pend_dur = dur;
	pend_missed = missed;
	if (fmt.settle_ms() == 0) report_pending();
}

// score of a candidate minute phase; ok = false if a pulse contradicts it
// or if its marker was never seen
int Decoder::score_phase(int p, bool &ok)
{
	long lo_a = std::max(first_A, A - LOG_SPAN);
	int score = 0;
	bool seen = false;
	ok = true;
	for (long s0 = lo_a - lmod(lo_a - p, 60); s0 <= A + 1; s0 += 60) {
		bool marker = false, conflict = false;
		score += fmt.score(log, s0, marker, conflict);
		if (conflict) { ok = false; return 0; }
		if (marker) seen = true;
	}
	if (!seen) ok = false;
	return score;
}

void Decoder::choose_phase()
{
	int best = -1, best_sc = -1000000, cur = -1000000;
	bool cur_ok = false;
	for (int p = 0; p < 60; p++) {
		bool ok;
		int sc = score_phase(p, ok);
		if (!ok) continue;
		if (p == phase) { cur = sc; cur_ok = true; }
		if (sc > best_sc) { best_sc = sc; best = p; }
	}
	if (best < 0 || best_sc <= 0) {
		if (phase >= 0) set_phase(-1, "telegram structure not recognised");
		return;
	}
	best_score = best_sc;
	// hysteresis: move to another phase only if clearly better
	if (phase >= 0 && cur_ok && cur > 0 && best_sc < cur + 3) {
		best_score = cur;
		return;
	}
	set_phase(best, phase < 0 ? "minute marker found" : "realigned on a new minute marker");
}

} // namespace tscore
