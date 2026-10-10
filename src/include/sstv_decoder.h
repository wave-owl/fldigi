// ----------------------------------------------------------------------------
// sstv_decoder.h  --  SSTV receive decoder (signal processing core)
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
// tested on its own.  SSTV is frequency modulated: 1200 Hz sync, 1500 Hz
// black, 2300 Hz white.  Each picture starts with a VIS header that
// identifies the mode.  The picture is rendered as it arrives; the line
// period is then measured on the sync pulses (sound card clock error) and
// the picture is redrawn without slant.

#ifndef SSTV_DECODER_H
#define SSTV_DECODER_H

#include <string>
#include <vector>

namespace sstvcore {

enum Channel { CH_R, CH_G, CH_B, CH_Y, CH_CR, CH_CB };

struct Scan {
	Channel ch;
	int row;          // row inside the unit
	double start_ms;  // from the start of the unit
	double len_ms;
};

// A fixed tone inside the unit (sync pulses, separators).  The rest of the
// unit outside the scans is the 1500 Hz porch.
struct Tone {
	double start_ms;
	double len_ms;
	double freq;
};

// A "unit" is the repeating part of the signal: one picture line for most
// modes, two lines for PD and Robot 36 (which share the chroma).
struct Mode {
	const char *name;
	int vis;
	int width, height;
	int rows_per_unit;
	double unit_ms;
	double sync_ms, sync_len_ms;   // sync pulse position inside the unit
	double first_ms;               // extra time before the first unit
	bool yuv;
	std::vector<Scan> scans;
	std::vector<Tone> tones;       // for the transmitter
};

const std::vector<Mode> &modes();
const Mode *mode_by_vis(int vis);
const Mode *mode_by_name(const std::string &name);

// FM discriminator: complex mixing to the centre frequency, FIR low pass,
// phase difference.  Outputs one instantaneous frequency (Hz) per sample.
class FMDemod {
public:
	FMDemod(double samplerate, double center = 1900);
	void set_center(double f);
	void process(const double *in, int len, std::vector<float> &out);
private:
	double fs, center, phase, dphi;
	std::vector<double> taps, bi, bq;
	size_t bp;
	double pi_prev, pq_prev;
};

class Listener {
public:
	virtual ~Listener() {}
	virtual void on_start(const Mode & /*m*/, double /*offset_hz*/) {}
	virtual void on_rows(int /*first_row*/, int /*last_row*/) {}   // rows updated
	virtual void on_redraw() {}                                      // whole picture updated
	virtual void on_done(bool /*complete*/, double /*clock_ppm*/) {}
};

class Decoder {
public:
	Decoder(double samplerate, Listener *l);
	// instantaneous frequency samples, already referred to the nominal
	// SSTV tones (1900 Hz = leader)
	void push(const float *freq, int len);
	void reset();
	void start(const Mode *m, double offset_hz);   // start without VIS
	void finish();                                  // end the current picture

	bool busy() const { return mode != 0; }
	const Mode *current_mode() const { return mode; }
	const std::vector<unsigned char> &rgb() const { return img; }
	int rows_done() const { return rows; }
	double offset() const { return off; }
	double clock_ppm() const;
	int sync_count() const { return (int)syncs.size(); }

private:
	struct SyncPt { int unit; double pos; };

	double avg(double a, double b) const;   // mean frequency over [a, b) samples
	bool   have(double s) const { return s < base + (double)track.size(); }
	void   check_vis();
	void   decode_units();
	void   render_unit(int u);
	void   find_sync(int u);
	int    low_count(long a, long b) const;
	void   fit_timing(bool final);
	double unit_start(int u) const { return t0 + (mode->first_ms + u * mode->unit_ms) * spm; }
	unsigned char level(double f) const;
	void   trim();

	Listener *listener;
	double fs, spm_nominal;
	std::vector<float> track;     // frequency track
	std::vector<double> cum;      // running sums: cum[k] = sum of the track before k
	long base;                    // absolute sample index of track[0]
	long now;                     // absolute index of the next sample

	// VIS search
	long vis_best_end;
	double vis_best_err, vis_off;
	int vis_code;
	long vis_deadline;

	// picture
	const Mode *mode;
	double off, t0, spm;
	int units, next_unit, rows;
	int last_fit_unit;
	std::vector<unsigned char> img;
	std::vector<unsigned char> raw;   // per unit, per scan: decoded levels
	std::vector<SyncPt> syncs;
	int last_sync_unit;
};

// Transmitter: VIS header and picture as a phase continuous FM signal.
// The picture must be mode.width x mode.height, RGB (3 bytes per pixel).
class Encoder {
public:
	Encoder(double samplerate, const Mode &m, const unsigned char *rgb, double amplitude = 1.0);
	// writes up to `max` samples, returns the number written (0 at the end)
	int generate(double *out, int max);
	double duration_s() const { return total_ms / 1000.0; }
	int row_sent() const;          // last picture row fully sent, -1 before
	bool done() const { return n >= total; }
	const Mode &mode_info() const { return mode; }
private:
	double freq_at(double ms) const;
	const Mode &mode;
	double fs, amp, phase;
	long n, total;
	double vis_ms, total_ms;
	std::vector<Tone> header;
	std::vector<unsigned char> lev;   // per unit, per scan: levels (0..255)
};

// RGB picture of any size scaled to the mode size (letterboxed in black,
// aspect ratio kept)
std::vector<unsigned char> fit_picture(const unsigned char *rgb, int w, int h, const Mode &m);

} // namespace sstvcore

#endif
