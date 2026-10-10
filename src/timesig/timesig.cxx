// ----------------------------------------------------------------------------
// timesig.cxx  --  LF time signal receive modem (DCF77, MSF, WWVB)
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
// Decodes the time code of DCF77 (77.5 kHz, Germany), MSF (60 kHz, UK) and
// WWVB (60 kHz, USA).  Tune the receiver in CW or SSB so that the carrier
// produces an audio tone, then click on the tone in the waterfall.  Every
// second the received symbol is printed in the receive text; at each new
// minute the complete telegram is decoded and printed (date, time, time
// zone, announcements, DUT1).

#include <config.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <sys/time.h>

#include "timesig.h"
#include "digiscope.h"
#include "fl_digi.h"
#include "debug.h"

using namespace tscore;

static const char *dow_name[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

static const Format &format_of(trx_mode md)
{
	switch (md) {
	case MODE_MSF:  return format_msf();
	case MODE_WWVB: return format_wwvb();
	default:        return format_dcf77();
	}
}

timesig::timesig(trx_mode md) : modem(), fmt(format_of(md)), quiet(false)
{
	mode = md;
	samplerate = TIMESIG_SAMPLERATE;
	cap &= ~CAP_TX;		// receive only
	demod = new ToneDemod(samplerate, frequency, TIMESIG_FILTER_BW);
	decoder = new Decoder(fmt, this);
	tracker = new FrameTracker(fmt);
	set_bandwidth(2 * TIMESIG_FILTER_BW);
}

timesig::~timesig()
{
	delete tracker;
	delete decoder;
	delete demod;
}

void timesig::tx_init()
{
}

void timesig::rx_init()
{
	memset(scope_ring, 0, sizeof(scope_ring));
	scope_ptr = 0;
	last_scope = 0;
	last_status = 0;
	tracker->reset();
	line_open = false;
	// active_modem is not set yet while the modem starts: print nothing here
	quiet = true;
	decoder->reset("start");
	quiet = false;
	set_scope_mode(Digiscope::SCOPE);
	put_MODEstatus(mode);
}

void timesig::init()
{
	modem::init();
	rx_init();
}

void timesig::restart()
{
	demod->set_freq(frequency);
}

double timesig::wall_time()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec + tv.tv_usec * 1e-6;
}

void timesig::put_text(const std::string &s)
{
	for (size_t i = 0; i < s.length(); i++)
		put_rx_char((unsigned char)s[i]);
}

void timesig::update_status()
{
	char msg[80];
	if (decoder->synced())
		snprintf(msg, sizeof(msg), "sync  s %02d", decoder->second());
	else
		snprintf(msg, sizeof(msg), decoder->present() ? "no sync" : "no signal");
	put_Status1(msg);
	snprintf(msg, sizeof(msg), "depth %.1f dB", decoder->depth_db());
	put_Status2(msg);
	display_metric(decoder->present() ? std::min(100.0, decoder->depth_db() * 5.0) : 0.0);
}

//======================================================================
// decoder events
//======================================================================

void timesig::on_sync(bool synced, const char *reason)
{
	if (quiet) return;
	char msg[120];
	if (line_open) { put_text("\n"); line_open = false; }
	snprintf(msg, sizeof(msg), "[%s %s: %s]\n", fmt.name(), synced ? "sync" : "no sync", reason);
	put_text(msg);
}

void timesig::on_pulse(long, int, int sym, int sec, int missed_before)
{
	if (sec < 0) return;
	std::string s;
	for (int k = sec - missed_before; k <= sec; k++) {
		int ks = (k + 60) % 60;
		if (fmt.group_start(ks)) s += ' ';
		if (fmt.silent(ks)) continue;
		s += k < sec ? '_' : fmt.sym_char(sym);
	}
	put_text(s);
	line_open = true;
}

void timesig::on_frame(const Frame &fr, long t0)
{
	// PC time of the end of the frame (start of the new minute)
	double end_wall = wall_time() - (decoder->now() - t0) / 1000.0;
	Frame f = tracker->process(fr, end_wall);

	char line[200];
	std::string out = line_open ? "\n" : "";
	line_open = false;

	if (f.valid || f.unconfirmed) {
		// the minute that has just started, in station time and in UTC
		time_t now_utc = f.utc + 60;
		time_t local = now_utc + f.utc_offset * 3600;
		struct tm lt, ut;
		gmtime_r(&local, &lt);
		gmtime_r(&now_utc, &ut);
		const char *result = f.unconfirmed ? "unconfirmed (no parity, wait for the next minute)" :
			f.by_continuity ? "OK (by continuity)" :
			f.repaired.empty() ? "OK" : "OK (repaired)";
		if (strcmp(f.zone, "UTC"))
			snprintf(line, sizeof(line),
				"%s %04d-%02d-%02d %s %02d:%02d %s (UTC%+d) = %02d:%02d UTC  %s\n",
				fmt.name(), lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, dow_name[lt.tm_wday],
				lt.tm_hour, lt.tm_min, f.zone, f.utc_offset, ut.tm_hour, ut.tm_min, result);
		else
			snprintf(line, sizeof(line),
				"%s %04d-%02d-%02d %s %02d:%02d %s (day %d)  %s\n",
				fmt.name(), lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, dow_name[lt.tm_wday],
				lt.tm_hour, lt.tm_min, f.zone, lt.tm_yday + 1, result);
		out += line;

		if (f.valid) {
			double offset = end_wall - (double)now_utc;
			snprintf(line, sizeof(line), "      %s%sPC clock %+.2f s  %d consecutive\n",
				f.notes.c_str(), f.notes.empty() ? "" : "  ", offset, tracker->consecutive);
			out += line;
		}
	} else {
		snprintf(line, sizeof(line), "%s telegram error: %s\n", fmt.name(), f.errors.c_str());
		out += line;
	}
	put_text(out);
	update_status();
}

//======================================================================
// receive
//======================================================================

int timesig::rx_process(const double *buf, int len)
{
	if (demod->get_freq() != frequency)
		demod->set_freq(frequency);

	env.clear();
	demod->process(buf, len, env);

	for (size_t i = 0; i < env.size(); i++) {
		scope_ring[scope_ptr % 2000] = env[i];
		scope_ptr++;
	}
	decoder->push(env.data(), (int)env.size());

	long now = decoder->now();
	if (now - last_scope >= 100) {
		last_scope = now;
		const int step = 2000 / TIMESIG_SCOPE_LEN;
		for (int i = 0; i < TIMESIG_SCOPE_LEN; i++) {
			double s = 0;
			for (int k = 0; k < step; k++)
				s += scope_ring[(scope_ptr + i * step + k) % 2000];
			scope_buf[i] = s / step;
		}
		set_scope(scope_buf, TIMESIG_SCOPE_LEN, true);
	}
	if (now - last_status >= 250) {
		last_status = now;
		update_status();
	}
	return 0;
}

int timesig::tx_process()
{
	return -1;
}
