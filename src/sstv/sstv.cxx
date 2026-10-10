// ----------------------------------------------------------------------------
// sstv.cxx  --  SSTV modem
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
// Receives slow scan television pictures.  The mode is identified from the
// VIS header (Martin, Scottie, Robot, PD families); the tuning error is
// measured on the VIS and the slant caused by the sound card clock is
// corrected on the line sync pulses.  Received pictures are shown in their
// own window and saved as PNG in the fldigi pictures directory.
//
// Pictures are transmitted from the "SSTV Tx" window (Send image... in the
// transmit text menu) in any of the same modes, with the VIS header.  The
// tones are the standard ones (1200 Hz sync, 1500..2300 Hz picture).

#include <config.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <pthread.h>

#include "sstv.h"
#include "digiscope.h"
#include "fl_digi.h"
#include "main.h"
#include "qrunner.h"
#include "trx.h"
#include "debug.h"

using namespace sstvcore;

//======================================================================
// picture shared with the GUI thread
//======================================================================

static pthread_mutex_t shared_mutex = PTHREAD_MUTEX_INITIALIZER;
static std::vector<unsigned char> shared_rgb;
static int shared_w = 0, shared_h = 0;

void sstv_shared_set(const std::vector<unsigned char> &rgb, int width, int height,
		int first_row, int last_row)
{
	pthread_mutex_lock(&shared_mutex);
	if (width != shared_w || height != shared_h || shared_rgb.size() != rgb.size()) {
		shared_w = width;
		shared_h = height;
		shared_rgb = rgb;
	} else if (first_row < 0) {
		shared_rgb = rgb;
	} else {
		size_t a = (size_t)first_row * width * 3, b = (size_t)(last_row + 1) * width * 3;
		if (b > rgb.size()) b = rgb.size();
		if (a < b) memcpy(&shared_rgb[a], &rgb[a], b - a);
	}
	pthread_mutex_unlock(&shared_mutex);
}

bool sstv_shared_copy(std::vector<unsigned char> &out, int &width, int &height)
{
	pthread_mutex_lock(&shared_mutex);
	bool ok = !shared_rgb.empty();
	out = shared_rgb;
	width = shared_w;
	height = shared_h;
	pthread_mutex_unlock(&shared_mutex);
	return ok;
}

//======================================================================
// modem
//======================================================================

sstv::sstv() : modem()
{
	mode = MODE_SSTV;
	samplerate = SSTV_SAMPLERATE;
	cap |= CAP_IMG;
	encoder = 0;
	tx_mode = 0;
	tx_last_row = -1;
	demod = new FMDemod(samplerate, SSTV_CENTER);
	decoder = new Decoder(samplerate, this);
	// 1200 Hz sync to 2300 Hz white around the 1900 Hz cursor
	set_bandwidth(1200);
}

sstv::~sstv()
{
	delete encoder;
	delete decoder;
	delete demod;
}

static pthread_mutex_t tx_mutex = PTHREAD_MUTEX_INITIALIZER;

void sstv::send_picture(const Mode &m, const std::vector<unsigned char> &rgb)
{
	pthread_mutex_lock(&tx_mutex);
	tx_mode = &m;
	tx_rgb = rgb;
	pthread_mutex_unlock(&tx_mutex);
	start_tx();
}

void sstv::tx_init()
{
	// the receive side starts over after the transmission
	if (decoder->busy()) decoder->finish();

	delete encoder;
	encoder = 0;
	pthread_mutex_lock(&tx_mutex);
	if (tx_mode && tx_rgb.size() == (size_t)tx_mode->width * tx_mode->height * 3)
		encoder = new Encoder(samplerate, *tx_mode, tx_rgb.data());
	const Mode *m = tx_mode;
	tx_mode = 0;
	tx_rgb.clear();
	pthread_mutex_unlock(&tx_mutex);

	tx_last_row = -1;
	char msg[120];
	if (encoder) {
		snprintf(msg, sizeof(msg), "\n[SSTV Tx %s (VIS %d) %dx%d, %.0f s]\n",
			m->name, m->vis, m->width, m->height, encoder->duration_s());
		put_Status1(m->name);
	} else
		snprintf(msg, sizeof(msg), "\n[SSTV Tx: no picture, use Send image... in the Tx menu]\n");
	put_text(msg);
}

void sstv::rx_init()
{
	// transmission interrupted from outside (Esc, mode change)
	if (encoder) {
		delete encoder;
		encoder = 0;
		put_text("[SSTV Tx aborted]\n");
		REQ(sstv_txpic_done, false);
	}
	samples = 0;
	last_scope = last_status = last_show = 0;
	tune_offset = 0;
	pending_rows = false;
	decoder->reset();
	set_scope_mode(Digiscope::SCOPE);
	put_MODEstatus(mode);
}

void sstv::init()
{
	modem::init();
	set_freq(SSTV_CENTER);
	rx_init();
}

void sstv::restart()
{
	demod->set_center(frequency);
}

void sstv::abort_picture()
{
	if (decoder->busy()) decoder->finish();
}

void sstv::put_text(const std::string &s)
{
	for (size_t i = 0; i < s.length(); i++)
		put_rx_char((unsigned char)s[i]);
}

void sstv::update_status()
{
	char msg[80];
	const Mode *m = decoder->current_mode();
	if (m)
		snprintf(msg, sizeof(msg), "%s  row %d/%d", m->name, decoder->rows_done(), m->height);
	else
		snprintf(msg, sizeof(msg), "waiting for VIS");
	put_Status1(msg);
	snprintf(msg, sizeof(msg), "off %+.0f Hz", m ? decoder->offset() : tune_offset);
	put_Status2(msg);
}

void sstv::show_picture(bool all)
{
	const Mode *m = decoder->current_mode();
	if (!m) return;
	if (all)
		sstv_shared_set(decoder->rgb(), m->width, m->height, -1, -1);
	REQ(sstv_pic_update);
}

void sstv::on_start(const Mode &m, double offset_hz)
{
	char msg[160];
	snprintf(msg, sizeof(msg), "\n[SSTV %s (VIS %d) %dx%d, tuning offset %+.0f Hz]\n",
		m.name, m.vis, m.width, m.height, offset_hz);
	put_text(msg);
	sstv_shared_set(decoder->rgb(), m.width, m.height, -1, -1);
	REQ(sstv_pic_start, m.width, m.height, m.name);
}

void sstv::on_rows(int first_row, int last_row)
{
	const Mode *m = decoder->current_mode();
	sstv_shared_set(decoder->rgb(), m->width, m->height, first_row, last_row);
	pending_rows = true;
}

void sstv::on_redraw()
{
	show_picture(true);
}

void sstv::on_done(bool complete, double clock_ppm)
{
	const Mode *m = decoder->current_mode();
	if (!m) return;
	sstv_shared_set(decoder->rgb(), m->width, m->height, -1, -1);

	// file name: sstv_YYYYMMDD_HHMMSSZ_<mode>.png in the pictures directory
	char stamp[32], name[64];
	time_t now = time(0);
	struct tm tm;
	gmtime_r(&now, &tm);
	strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%SZ", &tm);
	snprintf(name, sizeof(name), "%s", m->name);
	for (char *p = name; *p; p++) if (*p == ' ') *p = '_';
	std::string fname = PicsDir + "sstv_" + stamp + "_" + name + ".png";

	char msg[300];
	snprintf(msg, sizeof(msg), "[SSTV %s %s, %d/%d rows, clock %+.0f ppm, saved %s]\n",
		m->name, complete ? "complete" : "incomplete", decoder->rows_done(), m->height,
		clock_ppm, fname.c_str());
	put_text(msg);
	REQ(sstv_pic_done, fname);

	// automatic fine tuning for the next picture
	tune_offset = decoder->offset();
	if (fabs(tune_offset) > 15 && fabs(tune_offset) < 300)
		set_freq(frequency + tune_offset);
}

//======================================================================
// receive
//======================================================================

int sstv::rx_process(const double *buf, int len)
{
	if (fabs(frequency - SSTV_CENTER) > 400) set_freq(SSTV_CENTER);
	demod->set_center(frequency);

	fbuf.clear();
	demod->process(buf, len, fbuf);
	// refer the frequencies to the nominal tones: cursor = 1900 Hz
	const float shift = (float)(SSTV_CENTER - frequency);
	for (size_t i = 0; i < fbuf.size(); i++) fbuf[i] += shift;
	decoder->push(fbuf.data(), (int)fbuf.size());
	samples += len;

	// scope: frequency track of the last block, 1100 Hz (bottom) to 2400 Hz (top)
	if (samples - last_scope >= samplerate / 10 && !fbuf.empty()) {
		last_scope = samples;
		for (int i = 0; i < SSTV_SCOPE_LEN; i++) {
			double f = fbuf[(size_t)i * fbuf.size() / SSTV_SCOPE_LEN];
			scope_buf[i] = std::max(0.0, std::min(1.0, (f - 1100.0) / 1300.0));
		}
		set_scope(scope_buf, SSTV_SCOPE_LEN, false);
	}
	if (pending_rows && samples - last_show >= samplerate / 4) {
		last_show = samples;
		pending_rows = false;
		REQ(sstv_pic_update);
	}
	if (samples - last_status >= samplerate / 4) {
		last_status = samples;
		update_status();
	}
	return 0;
}

int sstv::tx_process()
{
	modem::tx_process();

	if (!encoder) return -1;

	bool aborted = stopflag;
	if (!aborted) {
		int n = encoder->generate(outbuf, 512);
		if (n > 0) ModulateXmtr(outbuf, n);
		int row = encoder->row_sent();
		const Mode &m = encoder->mode_info();
		if (row != tx_last_row && (row - tx_last_row >= 4 || row == m.height - 1)) {
			tx_last_row = row;
			REQ(sstv_txpic_progress, row, m.height);
			char msg[80];
			snprintf(msg, sizeof(msg), "Tx row %d/%d", row + 1, m.height);
			put_Status2(msg);
		}
		if (!encoder->done()) return 0;
	}

	stopflag = false;
	bool complete = encoder->done();
	put_text(complete ? "[SSTV Tx complete]\n" : "[SSTV Tx aborted]\n");
	REQ(sstv_txpic_done, complete);
	delete encoder;
	encoder = 0;
	return -1;
}
