// ----------------------------------------------------------------------------
// timesig.h  --  LF time signal receive modem (DCF77, MSF, WWVB)
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

#ifndef _TIMESIG_H
#define _TIMESIG_H

#include <ctime>
#include <string>
#include <vector>

#include "modem.h"
#include "timesig_decoder.h"

#define TIMESIG_SAMPLERATE	8000
#define TIMESIG_FILTER_BW	40		// demodulator low pass, Hz
#define TIMESIG_SCOPE_LEN	400		// scope points (2 s of envelope)

class timesig : public modem, private tscore::Listener {
private:
	const tscore::Format	&fmt;
	tscore::ToneDemod	*demod;
	tscore::Decoder		*decoder;
	tscore::FrameTracker	*tracker;
	std::vector<double>	env;

	double	scope_ring[2000];
	double	scope_buf[TIMESIG_SCOPE_LEN];
	long	scope_ptr;
	long	last_scope;
	long	last_status;

	bool	line_open;
	bool	quiet;

	void	put_text(const std::string &s);
	double	wall_time();
	void	update_status();

	// tscore::Listener
	void	on_pulse(long t, int dur, int sym, int sec, int missed_before);
	void	on_sync(bool synced, const char *reason);
	void	on_frame(const tscore::Frame &f, long t0);

public:
	timesig(trx_mode md);
	~timesig();
	void	init();
	void	rx_init();
	void	tx_init();
	void	restart();
	int		rx_process(const double *buf, int len);
	int		tx_process();
};

#endif
