// ----------------------------------------------------------------------------
// sstv_decoder.cxx  --  SSTV receive decoder (signal processing core)
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

#include "sstv_decoder.h"

namespace sstvcore {

//======================================================================
// Mode table
//======================================================================

static Mode martin(const char *name, int vis, double scan)
{
	Mode m;
	m.name = name; m.vis = vis; m.width = 320; m.height = 256;
	m.rows_per_unit = 1; m.yuv = false;
	const double sync = 4.862, gap = 0.572;
	m.sync_ms = 0; m.sync_len_ms = sync; m.first_ms = 0;
	double t = sync + gap;
	Scan g = { CH_G, 0, t, scan }; t += scan + gap;
	Scan b = { CH_B, 0, t, scan }; t += scan + gap;
	Scan r = { CH_R, 0, t, scan }; t += scan + gap;
	m.scans.push_back(g); m.scans.push_back(b); m.scans.push_back(r);
	Tone s = { 0, sync, 1200 };
	m.tones.push_back(s);
	m.unit_ms = t;
	return m;
}

// Scottie: separator, G, separator, B, sync, porch, R.  A single sync
// pulse precedes the first line.
static Mode scottie(const char *name, int vis, double scan)
{
	Mode m;
	m.name = name; m.vis = vis; m.width = 320; m.height = 256;
	m.rows_per_unit = 1; m.yuv = false;
	const double sync = 9.0, gap = 1.5;
	m.first_ms = sync;
	double t = gap;
	Scan g = { CH_G, 0, t, scan }; t += scan + gap;
	Scan b = { CH_B, 0, t, scan }; t += scan;
	m.sync_ms = t; m.sync_len_ms = sync;
	Tone s = { t, sync, 1200 };
	m.tones.push_back(s);
	t += sync + gap;
	Scan r = { CH_R, 0, t, scan }; t += scan;
	m.scans.push_back(g); m.scans.push_back(b); m.scans.push_back(r);
	m.unit_ms = t;
	return m;
}

static Mode robot36()
{
	Mode m;
	m.name = "Robot 36"; m.vis = 8; m.width = 320; m.height = 240;
	m.rows_per_unit = 2; m.yuv = true;
	m.sync_ms = 0; m.sync_len_ms = 9.0; m.first_ms = 0;
	// line 0: sync 9, porch 3, Y 88, separator 4.5, porch 1.5, R-Y 44
	// line 1: same, B-Y instead of R-Y
	Scan y0 = { CH_Y, 0, 12.0, 88.0 };
	Scan cr = { CH_CR, 0, 106.0, 44.0 };
	Scan y1 = { CH_Y, 1, 162.0, 88.0 };
	Scan cb = { CH_CB, 0, 256.0, 44.0 };
	m.scans.push_back(y0); m.scans.push_back(cr); m.scans.push_back(y1); m.scans.push_back(cb);
	// separator 1500 Hz before R-Y, 2300 Hz before B-Y, porch 1900 Hz
	const Tone t[] = { { 0, 9, 1200 }, { 100, 4.5, 1500 }, { 104.5, 1.5, 1900 },
		{ 150, 9, 1200 }, { 250, 4.5, 2300 }, { 254.5, 1.5, 1900 } };
	m.tones.assign(t, t + 6);
	m.unit_ms = 300.0;
	return m;
}

static Mode robot72()
{
	Mode m;
	m.name = "Robot 72"; m.vis = 12; m.width = 320; m.height = 240;
	m.rows_per_unit = 1; m.yuv = true;
	m.sync_ms = 0; m.sync_len_ms = 9.0; m.first_ms = 0;
	Scan y = { CH_Y, 0, 12.0, 138.0 };
	Scan cr = { CH_CR, 0, 156.0, 69.0 };
	Scan cb = { CH_CB, 0, 231.0, 69.0 };
	m.scans.push_back(y); m.scans.push_back(cr); m.scans.push_back(cb);
	const Tone t[] = { { 0, 9, 1200 }, { 150, 4.5, 1500 }, { 154.5, 1.5, 1900 },
		{ 225, 4.5, 2300 }, { 229.5, 1.5, 1500 } };
	m.tones.assign(t, t + 5);
	m.unit_ms = 300.0;
	return m;
}

// PD: sync 20 ms, porch 2.08 ms, Y (even row), R-Y, B-Y, Y (odd row)
static Mode pd(const char *name, int vis, int w, int h, double pixel_ms)
{
	Mode m;
	m.name = name; m.vis = vis; m.width = w; m.height = h;
	m.rows_per_unit = 2; m.yuv = true;
	m.sync_ms = 0; m.sync_len_ms = 20.0; m.first_ms = 0;
	double scan = w * pixel_ms, t = 20.0 + 2.08;
	Scan y0 = { CH_Y, 0, t, scan }; t += scan;
	Scan cr = { CH_CR, 0, t, scan }; t += scan;
	Scan cb = { CH_CB, 0, t, scan }; t += scan;
	Scan y1 = { CH_Y, 1, t, scan }; t += scan;
	m.scans.push_back(y0); m.scans.push_back(cr); m.scans.push_back(cb); m.scans.push_back(y1);
	Tone s = { 0, 20.0, 1200 };
	m.tones.push_back(s);
	m.unit_ms = t;
	return m;
}

const std::vector<Mode> &modes()
{
	static std::vector<Mode> table;
	if (table.empty()) {
		table.push_back(martin("Martin M1", 44, 146.432));
		table.push_back(martin("Martin M2", 40, 73.216));
		table.push_back(scottie("Scottie S1", 60, 138.240));
		table.push_back(scottie("Scottie S2", 56, 88.064));
		table.push_back(scottie("Scottie DX", 76, 345.600));
		table.push_back(robot36());
		table.push_back(robot72());
		table.push_back(pd("PD 50", 93, 320, 256, 0.286));
		table.push_back(pd("PD 90", 99, 320, 256, 0.532));
		table.push_back(pd("PD 120", 95, 640, 496, 0.190));
		table.push_back(pd("PD 160", 98, 512, 400, 0.382));
		table.push_back(pd("PD 180", 96, 640, 496, 0.286));
		table.push_back(pd("PD 240", 97, 640, 496, 0.382));
		table.push_back(pd("PD 290", 94, 800, 616, 0.286));
	}
	return table;
}

const Mode *mode_by_vis(int vis)
{
	const std::vector<Mode> &t = modes();
	for (size_t i = 0; i < t.size(); i++)
		if (t[i].vis == vis) return &t[i];
	return 0;
}

const Mode *mode_by_name(const std::string &name)
{
	const std::vector<Mode> &t = modes();
	for (size_t i = 0; i < t.size(); i++)
		if (name == t[i].name) return &t[i];
	return 0;
}

//======================================================================
// FM discriminator
//======================================================================

FMDemod::FMDemod(double samplerate, double c)
	: fs(samplerate), phase(0), bp(0), pi_prev(1), pq_prev(0)
{
	// windowed sinc low pass, 1200 Hz: passes 1100..2300 Hz with up to
	// +/- 300 Hz of mistuning around the 1900 Hz centre
	const int N = 63;
	const double fc = 1200.0 / fs;
	taps.resize(N);
	double sum = 0;
	for (int i = 0; i < N; i++) {
		double k = i - (N - 1) / 2.0;
		double s = k == 0 ? 2 * fc : sin(2 * M_PI * fc * k) / (M_PI * k);
		double w = 0.42 - 0.5 * cos(2 * M_PI * i / (N - 1)) + 0.08 * cos(4 * M_PI * i / (N - 1));
		taps[i] = s * w;
		sum += taps[i];
	}
	for (int i = 0; i < N; i++) taps[i] /= sum;
	bi.assign(N, 0.0);
	bq.assign(N, 0.0);
	set_center(c);
}

void FMDemod::set_center(double f)
{
	center = f;
	dphi = 2 * M_PI * f / fs;
}

void FMDemod::process(const double *in, int len, std::vector<float> &out)
{
	const size_t N = taps.size();
	for (int n = 0; n < len; n++) {
		bi[bp] = in[n] * cos(phase);
		bq[bp] = -in[n] * sin(phase);
		phase += dphi;
		if (phase > 2 * M_PI) phase -= 2 * M_PI;
		double si = 0, sq = 0;
		size_t k = bp;
		for (size_t t = 0; t < N; t++) {
			si += taps[t] * bi[k];
			sq += taps[t] * bq[k];
			k = k == 0 ? N - 1 : k - 1;
		}
		bp = (bp + 1) % N;
		// phase difference between successive samples
		double re = si * pi_prev + sq * pq_prev;
		double im = sq * pi_prev - si * pq_prev;
		pi_prev = si;
		pq_prev = sq;
		double f = center + atan2(im, re) * fs / (2 * M_PI);
		if (f < 900) f = 900;
		if (f > 2900) f = 2900;
		out.push_back((float)f);
	}
}

//======================================================================
// Decoder
//======================================================================

static const double VIS_BIT_MS = 30.0;
static const double TOL = 70.0;          // Hz tolerance on VIS tones

Decoder::Decoder(double samplerate, Listener *l)
	: listener(l), fs(samplerate), spm_nominal(samplerate / 1000.0),
	  base(0), now(0), mode(0)
{
	reset();
}

void Decoder::reset()
{
	mode = 0;
	vis_best_end = -1;
	vis_best_err = 1e9;
	vis_code = -1;
	vis_deadline = 0;
	vis_off = 0;
	syncs.clear();
	trim();
}

// keep only the last 2 seconds when idle
void Decoder::trim()
{
	long keep = (long)(2.0 * fs);
	if (mode || (long)track.size() <= keep * 2) return;
	long drop = (long)track.size() - keep;
	track.erase(track.begin(), track.begin() + drop);
	cum.erase(cum.begin(), cum.begin() + drop);
	base += drop;
}

double Decoder::avg(double a, double b) const
{
	long ia = (long)floor(a) - base, ib = (long)floor(b) - base;
	long n = (long)track.size();
	if (ia < 0) ia = 0;
	if (ib > n) ib = n;
	if (ib <= ia) return ia < n ? track[ia] : (n ? track[n - 1] : 0.0);
	return (cum[ib] - cum[ia]) / (ib - ia);
}

unsigned char Decoder::level(double f) const
{
	double v = (f - off - 1500.0) * (255.0 / 800.0);
	if (v < 0) v = 0;
	if (v > 255) v = 255;
	return (unsigned char)(v + 0.5);
}

void Decoder::push(const float *freq, int len)
{
	for (int i = 0; i < len; i++) {
		// cum[k] = sum of the track before index k (cum.size() == track.size() + 1)
		if (cum.empty()) cum.push_back(0.0);
		track.push_back(freq[i]);
		cum.push_back(cum.back() + freq[i]);
		now++;
		if (!mode) {
			if (now % (long)spm_nominal == 0) check_vis();
		}
	}
	if (mode) decode_units();
	else trim();
}

// The VIS header ends at `end`: leader 1900 Hz, start bit 1200 Hz,
// 7 data bits (LSB first, 1100 Hz = 1, 1300 Hz = 0), even parity, stop bit.
void Decoder::check_vis()
{
	const double b = VIS_BIT_MS * spm_nominal;
	double end = (double)now;
	double first = end - 10 * b - 200 * spm_nominal;
	if (first < base + 1) {
		vis_best_end = -1;
		return;
	}
	double margin = 0.2 * b;
	double leader = avg(end - 10 * b - 200 * spm_nominal, end - 10 * b - margin);
	double o = leader - 1900.0;
	double err = 0;
	if (fabs(o) > 300) goto fail;
	{
		// leader must be steady: check two halves
		double l1 = avg(end - 10 * b - 200 * spm_nominal, end - 10 * b - 100 * spm_nominal);
		double l2 = avg(end - 10 * b - 100 * spm_nominal, end - 10 * b - margin);
		if (fabs(l1 - l2) > 30) goto fail;
		double sb = avg(end - 10 * b + margin, end - 9 * b - margin) - o;
		double st = avg(end - b + margin, end - margin) - o;
		if (fabs(sb - 1200) > TOL || fabs(st - 1200) > TOL) goto fail;
		err += fabs(sb - 1200) + fabs(st - 1200);
		int code = 0, ones = 0;
		for (int k = 0; k < 8; k++) {
			double f = avg(end - (9 - k) * b + margin, end - (8 - k) * b - margin) - o;
			int bit;
			if (fabs(f - 1100) < TOL) { bit = 1; err += fabs(f - 1100); }
			else if (fabs(f - 1300) < TOL) { bit = 0; err += fabs(f - 1300); }
			else goto fail;
			ones += bit;
			if (k < 7) code |= bit << k;
		}
		if (ones % 2) goto fail;
		if (!mode_by_vis(code)) goto fail;
		if (vis_best_end < 0 || err < vis_best_err) {
			if (vis_best_end < 0) vis_deadline = now + (long)(15 * spm_nominal);
			vis_best_end = now;
			vis_best_err = err;
			vis_code = code;
			vis_off = o;
		}
	}
fail:
	if (vis_best_end >= 0 && now >= vis_deadline) {
		// best alignment found over the last 15 ms: start the picture there
		const Mode *m = mode_by_vis(vis_code);
		double end_at = (double)vis_best_end;
		vis_best_end = -1;
		start(m, vis_off);
		t0 = end_at;
	}
}

void Decoder::start(const Mode *m, double offset_hz)
{
	mode = m;
	off = offset_hz;
	t0 = (double)now;
	spm = spm_nominal;
	units = m->height / m->rows_per_unit;
	next_unit = 0;
	rows = 0;
	last_fit_unit = 0;
	last_sync_unit = 0;
	syncs.clear();
	img.assign((size_t)m->width * m->height * 3, 0);
	raw.assign((size_t)units * m->scans.size() * m->width, 0);
	if (listener) listener->on_start(*m, off);
}

void Decoder::finish()
{
	if (!mode) return;
	bool complete = next_unit >= units;
	fit_timing(true);
	if (listener) listener->on_done(complete, clock_ppm());
	mode = 0;
	vis_best_end = -1;
	trim();
}

double Decoder::clock_ppm() const
{
	return (spm / spm_nominal - 1.0) * 1e6;
}

void Decoder::decode_units()
{
	while (mode && next_unit < units) {
		double end = unit_start(next_unit) + (mode->unit_ms + 3) * spm;
		if (!have(end)) break;
		find_sync(next_unit);
		render_unit(next_unit);
		int u = next_unit++;
		int r0 = u * mode->rows_per_unit;
		rows = r0 + mode->rows_per_unit;
		if (listener) listener->on_rows(r0, rows - 1);

		// refine the timing every 8 units once enough sync pulses are known
		if (next_unit - last_fit_unit >= 8 && syncs.size() >= 6) {
			last_fit_unit = next_unit;
			fit_timing(false);
		}
		// picture lost: no sync pulse for 15 units
		if (u - last_sync_unit > 15 && u > 20) {
			finish();
			return;
		}
	}
	if (mode && next_unit >= units) finish();
}

// Find the sync pulse near its expected position.  Picture content never
// goes below 1500 Hz, so the sync is the only stretch of samples below
// 1350 Hz: maximise the number of such samples inside the pulse, minus those
// just before it (to lock on the leading edge).
int Decoder::low_count(long a, long b) const
{
	a -= base; b -= base;
	if (a < 0) a = 0;
	if (b > (long)track.size()) b = (long)track.size();
	const float th = (float)(1350.0 + off);
	int n = 0;
	for (long i = a; i < b; i++) n += track[i] < th;
	return n;
}

void Decoder::find_sync(int u)
{
	double expect = unit_start(u) + mode->sync_ms * spm;
	long L = (long)(mode->sync_len_ms * spm);
	double win = std::min(20.0, mode->unit_ms * 0.05) * spm;
	long pre = (long)std::min(2.0 * spm, (double)L);
	int best = -1000000;
	double best_pos = 0;
	int best_in = 0;
	for (long p = (long)(expect - win); p <= (long)(expect + win); p++) {
		if (p - pre < base) continue;
		int in = low_count(p, p + L);
		int score = in - low_count(p - pre, p);
		if (score > best) { best = score; best_pos = p; best_in = in; }
	}
	if (best_in >= 0.7 * L && best >= 0.5 * L) {
		SyncPt s = { u, best_pos - mode->sync_ms * spm };
		syncs.push_back(s);
		last_sync_unit = u;
	}
}

// line period and phase from the sync pulses (robust least squares)
void Decoder::fit_timing(bool final)
{
	if (syncs.size() < 6) return;
	std::vector<SyncPt> pts = syncs;
	double a = 0, b = 0;   // pos = a + b * unit
	for (int iter = 0; iter < 3; iter++) {
		double n = pts.size(), sx = 0, sy = 0, sxx = 0, sxy = 0;
		for (size_t i = 0; i < pts.size(); i++) {
			sx += pts[i].unit; sy += pts[i].pos;
			sxx += (double)pts[i].unit * pts[i].unit; sxy += pts[i].unit * pts[i].pos;
		}
		double den = n * sxx - sx * sx;
		if (den <= 0) return;
		b = (n * sxy - sx * sy) / den;
		a = (sy - b * sx) / n;
		std::vector<SyncPt> keep;
		double tol = 3.0 * spm_nominal;   // 3 ms
		for (size_t i = 0; i < pts.size(); i++)
			if (fabs(pts[i].pos - (a + b * pts[i].unit)) < tol) keep.push_back(pts[i]);
		if (keep.size() < 6) return;
		pts.swap(keep);
	}
	double new_spm = b / mode->unit_ms;
	// accept up to 1% clock error
	if (fabs(new_spm / spm_nominal - 1.0) > 0.01) return;
	spm = new_spm;
	t0 = a - mode->first_ms * spm;
	if (final) {
		for (int u = 0; u < next_unit; u++) render_unit(u);
		if (listener) listener->on_redraw();
	}
}

static const double EDGE_MS = 0.35;

void Decoder::render_unit(int u)
{
	const Mode &m = *mode;
	const int W = m.width;
	double us = unit_start(u);
	size_t ns = m.scans.size();
	for (size_t s = 0; s < ns; s++) {
		const Scan &sc = m.scans[s];
		double px = sc.len_ms * spm / W;
		double st = us + sc.start_ms * spm;
		// the demodulator smears each tone transition over a fraction of a
		// millisecond: the pixels next to the sync and porches take the
		// nearest clean value, otherwise the edge columns come out discoloured
		double w = std::max(px, 1.0);
		double edge = std::min(EDGE_MS, 0.05 * sc.len_ms) * spm;
		double lo = st + edge, hi = st + sc.len_ms * spm - edge - w;
		unsigned char *dst = &raw[((size_t)u * ns + s) * W];
		for (int x = 0; x < W; x++) {
			double a = std::max(lo, std::min(hi, st + x * px));
			dst[x] = level(avg(a, a + w));
		}
	}
	for (int r = 0; r < m.rows_per_unit; r++) {
		int row = u * m.rows_per_unit + r;
		if (row >= m.height) break;
		unsigned char *out = &img[(size_t)row * W * 3];
		const unsigned char *ch[6] = { 0, 0, 0, 0, 0, 0 };
		for (size_t s = 0; s < ns; s++) {
			const Scan &sc = m.scans[s];
			if (sc.ch == CH_Y && sc.row != r) continue;
			ch[sc.ch] = &raw[((size_t)u * ns + s) * W];
		}
		for (int x = 0; x < W; x++) {
			if (!m.yuv) {
				out[3 * x]     = ch[CH_R] ? ch[CH_R][x] : 0;
				out[3 * x + 1] = ch[CH_G] ? ch[CH_G][x] : 0;
				out[3 * x + 2] = ch[CH_B] ? ch[CH_B][x] : 0;
			} else {
				// ITU-R BT.601 (studio range), as used by MMSSTV and others
				double Y = ch[CH_Y] ? ch[CH_Y][x] : 0;
				double Cr = ch[CH_CR] ? ch[CH_CR][x] : 128;
				double Cb = ch[CH_CB] ? ch[CH_CB][x] : 128;
				double R = 1.164 * (Y - 16) + 1.596 * (Cr - 128);
				double G = 1.164 * (Y - 16) - 0.392 * (Cb - 128) - 0.813 * (Cr - 128);
				double B = 1.164 * (Y - 16) + 2.017 * (Cb - 128);
				out[3 * x]     = (unsigned char)std::max(0.0, std::min(255.0, R + 0.5));
				out[3 * x + 1] = (unsigned char)std::max(0.0, std::min(255.0, G + 0.5));
				out[3 * x + 2] = (unsigned char)std::max(0.0, std::min(255.0, B + 0.5));
			}
		}
	}
}

//======================================================================
// Encoder
//======================================================================

static const double RAMP_MS = 5.0;   // raised cosine at the start and the end

Encoder::Encoder(double samplerate, const Mode &m, const unsigned char *rgb, double amplitude)
	: mode(m), fs(samplerate), amp(amplitude), phase(0), n(0)
{
	// VIS: leader, break, leader, start bit, 7 data bits LSB first
	// (1100 Hz = 1, 1300 Hz = 0), even parity, stop bit
	const Tone lead = { 0, 300, 1900 }, brk = { 0, 10, 1200 }, bit = { 0, 30, 1200 };
	header.push_back(lead);
	header.push_back(brk);
	header.push_back(lead);
	header.push_back(bit);
	int ones = 0;
	for (int k = 0; k < 8; k++) {
		int b = k < 7 ? (m.vis >> k) & 1 : ones & 1;
		ones += b;
		Tone t = { 0, 30, b ? 1100.0 : 1300.0 };
		header.push_back(t);
	}
	header.push_back(bit);
	double t = 0;
	for (size_t i = 0; i < header.size(); i++) {
		header[i].start_ms = t;
		t += header[i].len_ms;
	}
	vis_ms = t;
	const int units = m.height / m.rows_per_unit;
	total_ms = vis_ms + m.first_ms + units * m.unit_ms;
	total = (long)ceil(total_ms * fs / 1000.0);

	// levels of every scan, in transmission order
	const int W = m.width;
	const size_t ns = m.scans.size();
	lev.assign((size_t)units * ns * W, 0);
	for (int u = 0; u < units; u++) {
		for (size_t s = 0; s < ns; s++) {
			const Scan &sc = m.scans[s];
			unsigned char *dst = &lev[((size_t)u * ns + s) * W];
			for (int x = 0; x < W; x++) {
				double v = 0;
				if (!m.yuv) {
					const unsigned char *p = &rgb[((size_t)(u * m.rows_per_unit) * W + x) * 3];
					v = sc.ch == CH_R ? p[0] : sc.ch == CH_G ? p[1] : p[2];
				} else if (sc.ch == CH_Y) {
					const unsigned char *p = &rgb[((size_t)(u * m.rows_per_unit + sc.row) * W + x) * 3];
					v = 16 + 0.257 * p[0] + 0.504 * p[1] + 0.098 * p[2];
				} else {
					// chroma shared by the rows of the unit: average them
					for (int r = 0; r < m.rows_per_unit; r++) {
						const unsigned char *p = &rgb[((size_t)(u * m.rows_per_unit + r) * W + x) * 3];
						v += sc.ch == CH_CR ?
							128 + 0.439 * p[0] - 0.368 * p[1] - 0.071 * p[2] :
							128 - 0.148 * p[0] - 0.291 * p[1] + 0.439 * p[2];
					}
					v /= m.rows_per_unit;
				}
				dst[x] = (unsigned char)std::max(0.0, std::min(255.0, v + 0.5));
			}
		}
	}
}

double Encoder::freq_at(double ms) const
{
	if (ms < vis_ms) {
		for (size_t i = 0; i < header.size(); i++)
			if (ms < header[i].start_ms + header[i].len_ms) return header[i].freq;
		return 1200;
	}
	double t = ms - vis_ms;
	if (t < mode.first_ms) return 1200;   // Scottie: sync before the first line
	t -= mode.first_ms;
	const int units = mode.height / mode.rows_per_unit;
	int u = (int)(t / mode.unit_ms);
	if (u >= units) return 1500;
	double w = t - u * mode.unit_ms;
	const size_t ns = mode.scans.size();
	for (size_t s = 0; s < ns; s++) {
		const Scan &sc = mode.scans[s];
		if (w >= sc.start_ms && w < sc.start_ms + sc.len_ms) {
			int x = (int)((w - sc.start_ms) / sc.len_ms * mode.width);
			if (x >= mode.width) x = mode.width - 1;
			return 1500.0 + lev[((size_t)u * ns + s) * mode.width + x] * (800.0 / 255.0);
		}
	}
	for (size_t i = 0; i < mode.tones.size(); i++) {
		const Tone &tn = mode.tones[i];
		if (w >= tn.start_ms && w < tn.start_ms + tn.len_ms) return tn.freq;
	}
	return 1500;
}

int Encoder::generate(double *out, int max)
{
	int k = 0;
	const long ramp = (long)(RAMP_MS * fs / 1000.0);
	for (; k < max && n < total; k++, n++) {
		double f = freq_at(n * 1000.0 / fs);
		phase += 2 * M_PI * f / fs;
		if (phase > 2 * M_PI) phase -= 2 * M_PI;
		double g = 1.0;
		long edge = std::min(n, total - 1 - n);
		if (edge < ramp) g = 0.5 - 0.5 * cos(M_PI * edge / ramp);
		out[k] = amp * g * sin(phase);
	}
	return k;
}

int Encoder::row_sent() const
{
	double t = n * 1000.0 / fs - vis_ms - mode.first_ms;
	if (t < 0) return -1;
	int u = (int)(t / mode.unit_ms);
	return std::min(mode.height, u * mode.rows_per_unit) - 1;
}

std::vector<unsigned char> fit_picture(const unsigned char *rgb, int w, int h, const Mode &m)
{
	const int W = m.width, H = m.height;
	std::vector<unsigned char> out((size_t)W * H * 3, 0);
	if (w <= 0 || h <= 0) return out;
	double scale = std::min((double)W / w, (double)H / h);
	int dw = std::max(1, (int)(w * scale + 0.5)), dh = std::max(1, (int)(h * scale + 0.5));
	int ox = (W - dw) / 2, oy = (H - dh) / 2;
	for (int y = 0; y < dh; y++) {
		// area average of the source pixels covered by each output pixel
		int sy0 = (int)((double)y * h / dh), sy1 = std::max(sy0 + 1, (int)((double)(y + 1) * h / dh));
		for (int x = 0; x < dw; x++) {
			int sx0 = (int)((double)x * w / dw), sx1 = std::max(sx0 + 1, (int)((double)(x + 1) * w / dw));
			double acc[3] = { 0, 0, 0 };
			int cnt = 0;
			for (int sy = sy0; sy < sy1 && sy < h; sy++)
				for (int sx = sx0; sx < sx1 && sx < w; sx++) {
					const unsigned char *p = &rgb[((size_t)sy * w + sx) * 3];
					acc[0] += p[0]; acc[1] += p[1]; acc[2] += p[2];
					cnt++;
				}
			unsigned char *d = &out[((size_t)(y + oy) * W + x + ox) * 3];
			for (int c = 0; c < 3; c++) d[c] = (unsigned char)(acc[c] / std::max(1, cnt) + 0.5);
		}
	}
	return out;
}

} // namespace sstvcore
