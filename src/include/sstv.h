// ----------------------------------------------------------------------------
// sstv.h  --  SSTV modem
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

#ifndef _SSTV_H
#define _SSTV_H

#include <string>
#include <vector>

#include "modem.h"
#include "sstv_decoder.h"

#define SSTV_SAMPLERATE		12000
#define SSTV_CENTER			1900	// leader tone, marked by the waterfall cursor
#define SSTV_SCOPE_LEN		400

class sstv : public modem, private sstvcore::Listener {
private:
	sstvcore::FMDemod	*demod;
	sstvcore::Decoder	*decoder;
	sstvcore::Encoder	*encoder;
	std::vector<float>	fbuf;

	// picture queued by the GUI for the next transmission
	const sstvcore::Mode	*tx_mode;
	std::vector<unsigned char>	tx_rgb;
	int		tx_last_row;

	double	scope_buf[SSTV_SCOPE_LEN];
	long	samples;
	long	last_scope;
	long	last_status;
	long	last_show;
	double	tune_offset;	// residual offset measured on the last VIS
	bool	pending_rows;

	void	put_text(const std::string &s);
	void	update_status();
	void	show_picture(bool all);

	// sstvcore::Listener
	void	on_start(const sstvcore::Mode &m, double offset_hz);
	void	on_rows(int first_row, int last_row);
	void	on_redraw();
	void	on_done(bool complete, double clock_ppm);

public:
	sstv();
	~sstv();
	void	init();
	void	rx_init();
	void	tx_init();
	void	restart();
	int		rx_process(const double *buf, int len);
	int		tx_process();
	void	abort_picture();
	// GUI thread: transmit a picture (mode size, RGB)
	void	send_picture(const sstvcore::Mode &m, const std::vector<unsigned char> &rgb);
};

// picture window (sstv-pic.cxx), GUI thread only
void sstv_pic_start(int width, int height, const char *mode_name);
void sstv_pic_update();
void sstv_pic_done(std::string filename);
void sstv_pic_status(std::string msg);

// transmit window (sstv-pic.cxx), GUI thread only
void sstv_showTxViewer();
void sstv_txpic_progress(int row, int height);
void sstv_txpic_done(bool complete);

// picture shared between the receive thread and the GUI
void sstv_shared_set(const std::vector<unsigned char> &rgb, int width, int height, int first_row, int last_row);
bool sstv_shared_copy(std::vector<unsigned char> &out, int &width, int &height);

#endif
