// ----------------------------------------------------------------------------
// timesig_formats.cxx  --  DCF77, MSF and WWVB time codes
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
// DCF77  77.5 kHz Mainflingen (DE): carrier reduced to 15% for 100 ms (0) or
//        200 ms (1); no reduction in second 59.  BCD time of the next minute
//        in CET/CEST with even parity.
// MSF    60 kHz Anthorn (UK): carrier off for 100 ms (A=0 B=0), 200 ms
//        (A=1 B=0), 300 ms (A=1 B=1), 100 + 100 ms (A=0 B=1) and 500 ms in
//        second 0.  BCD time of the next minute in GMT/BST on the A bits, odd
//        parity and flags on the B bits.
// WWVB   60 kHz Fort Collins (US): carrier reduced by 17 dB for 200 ms (0),
//        500 ms (1) or 800 ms (marker at seconds 0, 9, 19 ... 59).  BCD UTC
//        time of the current minute, day of the year, DUT1 and US DST bits.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "timesig_decoder.h"

namespace tscore {

namespace {

inline long lmod(long a, long n) { return ((a % n) + n) % n; }
inline bool data(int s) { return s >= 0 && s != SYM_MARK; }

struct Field { int sec, w; };

// BCD value of a field from 0/1 bits (-1 = unknown); -1 if a bit is unknown
// or a decimal digit is out of range
int bcd(const int *bit, const Field *f, int n)
{
	int v = 0, digit[3] = { 0, 0, 0 };
	for (int k = 0; k < n; k++) {
		int b = bit[f[k].sec];
		if (b < 0) return -1;
		if (b) {
			v += f[k].w;
			digit[f[k].w >= 100 ? 2 : f[k].w >= 10 ? 1 : 0] += f[k].w;
		}
	}
	if (digit[0] > 9 || digit[1] > 90 || digit[2] > 900) return -1;
	return v;
}

void put_bcd(int *bit, const Field *f, int n, int v)
{
	int part[3] = { v % 10, (v / 10 % 10) * 10, (v / 100) * 100 };
	// largest weight first
	bool done[16] = { false };
	for (int i = 0; i < n; i++) {
		int k = -1;
		for (int j = 0; j < n; j++)
			if (!done[j] && (k < 0 || f[j].w > f[k].w)) k = j;
		done[k] = true;
		int d = f[k].w >= 100 ? 2 : f[k].w >= 10 ? 1 : 0;
		bit[f[k].sec] = part[d] >= f[k].w;
		if (bit[f[k].sec]) part[d] -= f[k].w;
	}
}

#define NF(a) (int)(sizeof(a) / sizeof(*a))

void add_error(Frame &f, const char *e)
{
	if (!f.errors.empty()) f.errors += ", ";
	f.errors += e;
}

void add_note(Frame &f, const std::string &s)
{
	if (!f.notes.empty()) f.notes += "  ";
	f.notes += s;
}

void init_frame(Frame &f, const int *sym)
{
	memcpy(f.sym, sym, sizeof(f.sym));
	f.valid = false;
	f.errors.clear();
	f.repaired.clear();
	f.missing = 0;
	f.utc_offset = 0;
	f.zone = "UTC";
	f.utc = 0;
	f.notes.clear();
	f.by_continuity = false;
	f.unconfirmed = false;
}

void repair(Frame &f, int sec, int sym)
{
	f.sym[sec] = sym;
	f.repaired.push_back(sec);
}

void check_missing(Frame &f, int from, int to)
{
	for (int k = from; k <= to; k++) if (f.sym[k] < 0) f.missing++;
	if (f.missing) {
		char s[32];
		snprintf(s, sizeof(s), "%d bits missing", f.missing);
		add_error(f, s);
	}
}

// Monday = 1 ... Sunday = 7
int weekday(long days) { return (int)lmod(days + 3, 7) + 1; }

//======================================================================
// DCF77
//======================================================================

const Field D_MIN[] = { {21,1}, {22,2}, {23,4}, {24,8}, {25,10}, {26,20}, {27,40} };
const Field D_HR[]  = { {29,1}, {30,2}, {31,4}, {32,8}, {33,10}, {34,20} };
const Field D_DAY[] = { {36,1}, {37,2}, {38,4}, {39,8}, {40,10}, {41,20} };
const Field D_DOW[] = { {42,1}, {43,2}, {44,4} };
const Field D_MON[] = { {45,1}, {46,2}, {47,4}, {48,8}, {49,10} };
const Field D_YR[]  = { {50,1}, {51,2}, {52,4}, {53,8}, {54,10}, {55,20}, {56,40}, {57,80} };
const int D_PAR[3][2] = { {21, 28}, {29, 35}, {36, 58} };   // even parity, parity bit last

class DCF77 : public Format {
public:
	const char *name() const { return "DCF77"; }

	int classify(int dur) const
	{
		if (dur < 50 || dur > 280) return -1;
		return dur < 150 ? 0 : 1;
	}

	// minute marker: no pulse in the second before s0
	int score(const SymbolLog &log, long s0, bool &marker, bool &conflict) const
	{
		int v = symbol_at(log, s0 - 1);
		if (v >= 0) { conflict = true; return 0; }
		int sc = 0;
		if (v == SYM_MISSING) { marker = true; sc++; }
		int b0 = symbol_at(log, s0);
		if (b0 == 0) sc++; else if (b0 == 1) sc -= 3;
		int b20 = symbol_at(log, s0 + 20);
		if (b20 == 1) sc++; else if (b20 == 0) sc -= 3;
		for (int g = 0; g < 3; g++) {
			int par = 0;
			bool complete = true;
			for (int k = D_PAR[g][0]; k <= D_PAR[g][1] && complete; k++) {
				int x = symbol_at(log, s0 + k);
				if (data(x)) par ^= x; else complete = false;
			}
			if (complete) sc += par == 0 ? 2 : -3;
		}
		return sc;
	}

	Frame decode(const int *in) const
	{
		Frame f;
		init_frame(f, in);
		int *b = f.sym;
		for (int k = 0; k < 59; k++) if (!data(b[k])) b[k] = SYM_MISSING;
		b[59] = SYM_MISSING;

		// fixed bits and single missing bits of a parity group can be rebuilt
		if (b[0] < 0)  repair(f, 0, 0);
		if (b[20] < 0) repair(f, 20, 1);
		for (int g = 0; g < 3; g++) {
			int miss = -1, nmiss = 0, p = 0;
			for (int k = D_PAR[g][0]; k <= D_PAR[g][1]; k++) {
				if (b[k] >= 0) p ^= b[k];
				else { miss = k; nmiss++; }
			}
			if (nmiss == 1) repair(f, miss, p);
		}

		int minute = bcd(b, D_MIN, NF(D_MIN));
		int hour   = bcd(b, D_HR, NF(D_HR));
		int day    = bcd(b, D_DAY, NF(D_DAY));
		int dow    = bcd(b, D_DOW, NF(D_DOW));
		int month  = bcd(b, D_MON, NF(D_MON));
		int year   = bcd(b, D_YR, NF(D_YR));
		int par[3];
		for (int g = 0; g < 3; g++) {
			par[g] = 0;
			for (int k = D_PAR[g][0]; k <= D_PAR[g][1]; k++) {
				if (b[k] < 0) { par[g] = -1; break; }
				par[g] ^= b[k];
			}
		}

		if (b[17] == 1 && b[18] == 0) { f.utc_offset = 2; f.zone = "CEST"; }
		else if (b[17] == 0 && b[18] == 1) { f.utc_offset = 1; f.zone = "CET"; }
		else f.utc_offset = -99;

		if (b[0] != 0)  add_error(f, "bit 0 not 0");
		if (b[20] != 1) add_error(f, "bit 20 not 1");
		if (f.utc_offset == -99) add_error(f, "bad Z1/Z2");
		if (par[0] == 1) add_error(f, "minute parity");
		if (par[1] == 1) add_error(f, "hour parity");
		if (par[2] == 1) add_error(f, "date parity");
		if (minute > 59) add_error(f, "minute out of range");
		if (hour > 23) add_error(f, "hour out of range");
		if (day == 0 || day > 31) add_error(f, "day out of range");
		if (dow == 0 || dow > 7) add_error(f, "weekday out of range");
		if (month == 0 || month > 12) add_error(f, "month out of range");
		check_missing(f, 0, 58);
		if (f.errors.empty() && (minute < 0 || hour < 0 || day < 0 || dow < 0 || month < 0 || year < 0))
			add_error(f, "bad BCD digit");
		if (f.errors.empty()) {
			long days = days_from_civil(2000 + year, month, day);
			if (weekday(days) != dow)
				add_error(f, "weekday does not match date");
			else {
				f.utc = utc_from_civil(2000 + year, month, day, hour, minute)
					- f.utc_offset * 3600 - 60;
				f.valid = true;
			}
		}

		unsigned meteo = 0;
		bool meteo_complete = true;
		for (int k = 1; k <= 14; k++) {
			meteo <<= 1;
			if (b[k] >= 0) meteo |= b[k]; else meteo_complete = false;
		}
		char s[40];
		if (meteo_complete) {
			snprintf(s, sizeof(s), "meteo 0x%04X", meteo);
			add_note(f, s);
		}
		if (b[16] == 1) add_note(f, "CET/CEST change at end of hour");
		if (b[19] == 1) add_note(f, "leap second at end of hour");
		if (b[15] == 1) add_note(f, "call bit R set (abnormal TX operation)");
		return f;
	}

	void encode(time_t start, int *b) const
	{
		time_t next = start + 60;
		int off = eu_summer_time(next) ? 2 : 1;
		time_t local = next + off * 3600;
		long days = (long)floor(local / 86400.0);
		int y, mo, d;
		civil_from_days(days, y, mo, d);
		long sod = (long)(local - (time_t)days * 86400);

		for (int k = 0; k < 60; k++) b[k] = 0;
		b[59] = SYM_MISSING;
		b[16] = eu_summer_time(next + 3600) != eu_summer_time(next);
		b[17] = off == 2;
		b[18] = off == 1;
		b[20] = 1;
		put_bcd(b, D_MIN, NF(D_MIN), (int)(sod / 60 % 60));
		put_bcd(b, D_HR, NF(D_HR), (int)(sod / 3600));
		put_bcd(b, D_DAY, NF(D_DAY), d);
		put_bcd(b, D_DOW, NF(D_DOW), weekday(days));
		put_bcd(b, D_MON, NF(D_MON), mo);
		put_bcd(b, D_YR, NF(D_YR), y % 100);
		for (int g = 0; g < 3; g++) {
			int p = 0;
			for (int k = D_PAR[g][0]; k < D_PAR[g][1]; k++) p ^= b[k];
			b[D_PAR[g][1]] = p;
		}
	}

	bool predictable(int sec) const { return sec >= 17 && sec <= 58 && sec != 19; }

	// 0 | meteo 1-14 | 15-19 | 20 | minute | hour | day | weekday | month | year
	bool group_start(int sec) const
	{
		return sec == 1 || sec == 15 || sec == 20 || sec == 21 || sec == 29 ||
			sec == 36 || sec == 42 || sec == 45 || sec == 50;
	}

	bool silent(int sec) const { return sec == 59; }
};

//======================================================================
// MSF
//======================================================================

const Field M_YR[]  = { {17,80}, {18,40}, {19,20}, {20,10}, {21,8}, {22,4}, {23,2}, {24,1} };
const Field M_MON[] = { {25,10}, {26,8}, {27,4}, {28,2}, {29,1} };
const Field M_DAY[] = { {30,20}, {31,10}, {32,8}, {33,4}, {34,2}, {35,1} };
const Field M_DOW[] = { {36,4}, {37,2}, {38,1} };
const Field M_HR[]  = { {39,20}, {40,10}, {41,8}, {42,4}, {43,2}, {44,1} };
const Field M_MIN[] = { {45,40}, {46,20}, {47,10}, {48,8}, {49,4}, {50,2}, {51,1} };
// odd parity of A bits [from, to] in B bit `bit`
const int M_PAR[4][3] = { {17, 24, 54}, {25, 35, 55}, {36, 38, 56}, {39, 51, 57} };
const int M_PATTERN[8] = { 0, 1, 1, 1, 1, 1, 1, 0 };   // A bits 52-59

class MSF : public Format {
public:
	const char *name() const { return "MSF"; }

	// symbol = A + 2 B
	int classify(int dur) const
	{
		if (dur < 50) return -1;
		if (dur < 150) return 0;
		if (dur < 250) return 1;
		if (dur < 400) return 3;
		if (dur < 650) return SYM_MARK;
		return -1;
	}

	// 100 ms off, 100 ms on, 100 ms off: A = 0, B = 1
	int second_pulse(int sym, int gap, int dur) const
	{
		return (sym == 0 && gap >= 150 && gap <= 260 && dur >= 50 && dur <= 160) ? 2 : -1;
	}

	int settle_ms() const { return 450; }

	int score(const SymbolLog &log, long s0, bool &marker, bool & /*conflict*/) const
	{
		int v = symbol_at(log, s0);
		int sc = 0;
		if (v == SYM_MARK) { marker = true; sc += 4; }
		else if (data(v)) sc -= 4;
		for (int k = 1; k < 60; k++)
			if (symbol_at(log, s0 + k) == SYM_MARK) sc -= 3;
		for (int k = 52; k < 60; k++) {
			int x = symbol_at(log, s0 + k);
			if (data(x)) sc += (x & 1) == M_PATTERN[k - 52] ? 1 : -2;
		}
		for (int g = 0; g < 4; g++) {
			int par = 0;
			bool complete = true;
			for (int k = M_PAR[g][0]; k <= M_PAR[g][1] && complete; k++) {
				int x = symbol_at(log, s0 + k);
				if (data(x)) par ^= x & 1; else complete = false;
			}
			int pb = symbol_at(log, s0 + M_PAR[g][2]);
			if (complete && data(pb)) sc += (par ^ (pb >> 1)) == 1 ? 2 : -3;
		}
		return sc;
	}

	Frame decode(const int *in) const
	{
		Frame f;
		init_frame(f, in);
		int *s = f.sym;
		for (int k = 1; k < 60; k++) if (!data(s[k])) s[k] = SYM_MISSING;
		if (s[0] != SYM_MARK) {
			if (s[0] < 0) repair(f, 0, SYM_MARK);
			else add_error(f, "no minute marker");
		}
		if (s[52] < 0) repair(f, 52, 0);
		if (s[59] < 0) repair(f, 59, 0);
		// a single missing A bit of a parity group (B = 0 there)
		for (int g = 0; g < 4; g++) {
			int pb = s[M_PAR[g][2]];
			if (pb < 0) continue;
			int miss = -1, nmiss = 0, p = 1 ^ (pb >> 1);
			for (int k = M_PAR[g][0]; k <= M_PAR[g][1]; k++) {
				if (s[k] >= 0) p ^= s[k] & 1;
				else { miss = k; nmiss++; }
			}
			if (nmiss == 1) repair(f, miss, p);
		}

		int a[60], b[60];
		for (int k = 0; k < 60; k++) {
			a[k] = data(s[k]) ? (s[k] & 1) : -1;
			b[k] = data(s[k]) ? (s[k] >> 1) & 1 : -1;
		}
		int year   = bcd(a, M_YR, NF(M_YR));
		int month  = bcd(a, M_MON, NF(M_MON));
		int day    = bcd(a, M_DAY, NF(M_DAY));
		int dow    = bcd(a, M_DOW, NF(M_DOW));
		int hour   = bcd(a, M_HR, NF(M_HR));
		int minute = bcd(a, M_MIN, NF(M_MIN));

		for (int k = 52; k < 60; k++)
			if (a[k] >= 0 && a[k] != M_PATTERN[k - 52]) { add_error(f, "bad 52-59 pattern"); break; }
		static const char *par_name[4] = { "year parity", "date parity", "weekday parity", "time parity" };
		for (int g = 0; g < 4; g++) {
			int p = b[M_PAR[g][2]];
			for (int k = M_PAR[g][0]; k <= M_PAR[g][1] && p >= 0; k++)
				p = a[k] < 0 ? -1 : p ^ a[k];
			if (p == 0) add_error(f, par_name[g]);
		}
		if (minute > 59) add_error(f, "minute out of range");
		if (hour > 23) add_error(f, "hour out of range");
		if (day == 0 || day > 31) add_error(f, "day out of range");
		if (dow > 6) add_error(f, "weekday out of range");
		if (month == 0 || month > 12) add_error(f, "month out of range");
		check_missing(f, 1, 59);
		if (f.errors.empty() && (minute < 0 || hour < 0 || day < 0 || dow < 0 || month < 0 || year < 0))
			add_error(f, "bad BCD digit");

		f.utc_offset = b[58] == 1 ? 1 : 0;
		f.zone = f.utc_offset ? "BST" : "GMT";
		if (f.errors.empty()) {
			long days = days_from_civil(2000 + year, month, day);
			if (weekday(days) % 7 != dow)
				add_error(f, "weekday does not match date");
			else {
				f.utc = utc_from_civil(2000 + year, month, day, hour, minute)
					- f.utc_offset * 3600 - 60;
				f.valid = true;
			}
		}

		bool dut_complete = true;
		int dut = 0;
		for (int k = 1; k <= 16; k++) {
			if (b[k] < 0) { dut_complete = false; break; }
			dut += k <= 8 ? b[k] : -b[k];
		}
		char t[40];
		if (dut_complete) {
			snprintf(t, sizeof(t), "DUT1 %+.1f s", dut * 0.1);
			add_note(f, t);
		}
		if (b[53] == 1) add_note(f, "GMT/BST change within the hour");
		return f;
	}

	void encode(time_t start, int *s) const
	{
		time_t next = start + 60;
		int off = eu_summer_time(next) ? 1 : 0;
		time_t local = next + off * 3600;
		long days = (long)floor(local / 86400.0);
		int y, mo, d;
		civil_from_days(days, y, mo, d);
		long sod = (long)(local - (time_t)days * 86400);

		int a[60], b[60];
		for (int k = 0; k < 60; k++) a[k] = b[k] = 0;
		put_bcd(a, M_YR, NF(M_YR), y % 100);
		put_bcd(a, M_MON, NF(M_MON), mo);
		put_bcd(a, M_DAY, NF(M_DAY), d);
		put_bcd(a, M_DOW, NF(M_DOW), weekday(days) % 7);
		put_bcd(a, M_HR, NF(M_HR), (int)(sod / 3600));
		put_bcd(a, M_MIN, NF(M_MIN), (int)(sod / 60 % 60));
		for (int k = 52; k < 60; k++) a[k] = M_PATTERN[k - 52];
		b[53] = eu_summer_time(next + 3600) != eu_summer_time(next);
		for (int g = 0; g < 4; g++) {
			int p = 1;
			for (int k = M_PAR[g][0]; k <= M_PAR[g][1]; k++) p ^= a[k];
			b[M_PAR[g][2]] = p;
		}
		b[58] = off;
		for (int k = 0; k < 60; k++) s[k] = a[k] + 2 * b[k];
		s[0] = SYM_MARK;
	}

	bool predictable(int sec) const { return sec >= 17 && sec != 53; }

	// marker | DUT1 1-16 | year | month | day | weekday | hour | minute | 52-59
	bool group_start(int sec) const
	{
		return sec == 1 || sec == 17 || sec == 25 || sec == 30 || sec == 36 ||
			sec == 39 || sec == 45 || sec == 52;
	}
};

//======================================================================
// WWVB
//======================================================================

const Field W_MIN[] = { {1,40}, {2,20}, {3,10}, {5,8}, {6,4}, {7,2}, {8,1} };
const Field W_HR[]  = { {12,20}, {13,10}, {15,8}, {16,4}, {17,2}, {18,1} };
const Field W_DOY[] = { {22,200}, {23,100}, {25,80}, {26,40}, {27,20}, {28,10},
                        {30,8}, {31,4}, {32,2}, {33,1} };
const Field W_DUT[] = { {40,8}, {41,4}, {42,2}, {43,1} };
const Field W_YR[]  = { {45,80}, {46,40}, {47,20}, {48,10}, {50,8}, {51,4}, {52,2}, {53,1} };
const int W_MARKS[] = { 0, 9, 19, 29, 39, 49, 59 };
const int W_ZEROS[] = { 4, 10, 11, 14, 20, 21, 24, 34, 35, 44, 54 };

bool w_marker(int sec)
{
	for (int k = 0; k < NF(W_MARKS); k++) if (W_MARKS[k] == sec) return true;
	return false;
}

// first day (days from 1970) of US DST, and of standard time, in year y:
// second Sunday of March, first Sunday of November
void us_dst_days(int y, long &begin, long &end)
{
	long mar1 = days_from_civil(y, 3, 1), nov1 = days_from_civil(y, 11, 1);
	begin = mar1 + lmod(3 - mar1, 7) + 7;   // 1970-01-04 was a Sunday
	end = nov1 + lmod(3 - nov1, 7);
}

class WWVB : public Format {
public:
	const char *name() const { return "WWVB"; }

	int classify(int dur) const
	{
		if (dur < 100 || dur > 950) return -1;
		if (dur < 350) return 0;
		if (dur < 650) return 1;
		return SYM_MARK;
	}

	// the minute starts on the second of two consecutive markers (59, 0)
	int score(const SymbolLog &log, long s0, bool &marker, bool & /*conflict*/) const
	{
		int v0 = symbol_at(log, s0), vp = symbol_at(log, s0 - 1);
		int sc = 0;
		if (v0 == SYM_MARK) { marker = true; sc += 3; }
		else if (data(v0)) sc -= 4;
		if (vp == SYM_MARK) sc += 3;
		else if (data(vp)) sc -= 4;
		for (int k = 1; k < 59; k++) {
			int x = symbol_at(log, s0 + k);
			if (x < 0) continue;
			if (w_marker(k)) sc += x == SYM_MARK ? 1 : -2;
			else if (x == SYM_MARK) sc -= 2;
		}
		for (int k = 0; k < NF(W_ZEROS); k++)
			if (symbol_at(log, s0 + W_ZEROS[k]) == 1) sc--;
		return sc;
	}

	Frame decode(const int *in) const
	{
		Frame f;
		init_frame(f, in);
		int *s = f.sym;
		bool bad_mark = false, bad_zero = false;
		for (int k = 0; k < 60; k++) {
			if (w_marker(k)) {
				if (s[k] < 0) repair(f, k, SYM_MARK);
				else if (s[k] != SYM_MARK) bad_mark = true;
			} else if (s[k] == SYM_MARK) {
				bad_mark = true;
				s[k] = SYM_MISSING;
			}
		}
		for (int k = 0; k < NF(W_ZEROS); k++) {
			int z = W_ZEROS[k];
			if (s[z] < 0) repair(f, z, 0);
			else if (s[z] != 0) bad_zero = true;
		}
		int b[60];
		for (int k = 0; k < 60; k++) b[k] = data(s[k]) ? s[k] : -1;

		int minute = bcd(b, W_MIN, NF(W_MIN));
		int hour   = bcd(b, W_HR, NF(W_HR));
		int doy    = bcd(b, W_DOY, NF(W_DOY));
		int year   = bcd(b, W_YR, NF(W_YR));
		int dut    = bcd(b, W_DUT, NF(W_DUT));
		bool leap_year = year >= 0 && year % 4 == 0;

		if (bad_mark) add_error(f, "markers misplaced");
		if (bad_zero) add_error(f, "unused bit set");
		if (minute > 59) add_error(f, "minute out of range");
		if (hour > 23) add_error(f, "hour out of range");
		if (doy == 0 || doy > (leap_year ? 366 : 365)) add_error(f, "day of year out of range");
		if (year >= 0 && b[55] >= 0 && b[55] != (int)leap_year) add_error(f, "leap year bit");
		check_missing(f, 0, 59);
		if (f.errors.empty() && (minute < 0 || hour < 0 || doy < 0 || year < 0))
			add_error(f, "bad BCD digit");
		if (f.errors.empty()) {
			f.utc = (time_t)(days_from_civil(2000 + year, 1, 1) + doy - 1) * 86400
				+ hour * 3600 + minute * 60;
			f.valid = true;
		}

		char t[40];
		if (dut >= 0 && dut <= 9 && b[36] >= 0 && b[37] >= 0 && b[38] >= 0) {
			int sign = (b[36] == 1 && b[37] == 0 && b[38] == 1) ? 1 :
				(b[36] == 0 && b[37] == 1 && b[38] == 0) ? -1 : 0;
			if (sign) {
				snprintf(t, sizeof(t), "DUT1 %+.1f s", sign * dut * 0.1);
				add_note(f, t);
			}
		}
		if (b[57] >= 0 && b[58] >= 0) {
			static const char *dst[4] = { "US standard time", "US DST ends today",
				"US DST begins today", "US DST in effect" };
			add_note(f, dst[b[57] * 2 + b[58]]);
		}
		if (b[56] == 1) add_note(f, "leap second at end of month");
		return f;
	}

	void encode(time_t start, int *s) const
	{
		long days = (long)floor(start / 86400.0);
		int y, mo, d;
		civil_from_days(days, y, mo, d);
		long sod = (long)(start - (time_t)days * 86400);
		int doy = (int)(days - days_from_civil(y, 1, 1)) + 1;

		for (int k = 0; k < 60; k++) s[k] = 0;
		for (int k = 0; k < NF(W_MARKS); k++) s[W_MARKS[k]] = SYM_MARK;
		put_bcd(s, W_MIN, NF(W_MIN), (int)(sod / 60 % 60));
		put_bcd(s, W_HR, NF(W_HR), (int)(sod / 3600));
		put_bcd(s, W_DOY, NF(W_DOY), doy);
		put_bcd(s, W_YR, NF(W_YR), y % 100);
		s[36] = 1; s[38] = 1;   // DUT1 +0.0
		s[55] = y % 4 == 0;
		long begin, end;
		us_dst_days(y, begin, end);
		s[57] = days >= begin && days < end;    // DST at 24:00 UTC
		s[58] = days > begin && days <= end;    // DST at 00:00 UTC
	}

	bool predictable(int sec) const { return !(sec >= 36 && sec <= 43) && sec != 56; }

	bool group_start(int sec) const { return sec == 1 || (sec && sec % 10 == 0); }

	bool checked() const { return false; }
};

} // namespace

const Format &format_dcf77() { static DCF77 f; return f; }
const Format &format_msf()   { static MSF f;   return f; }
const Format &format_wwvb()  { static WWVB f;  return f; }

} // namespace tscore
