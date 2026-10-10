// ----------------------------------------------------------------------------
// sstv-pic.cxx  --  SSTV receive and transmit picture windows
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

#include <config.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Shared_Image.H>

#include "sstv.h"
#include "picture.h"
#include "trx.h"
#include "fileselect.h"
#include "gettext.h"
#include "main.h"
#include "fl_digi.h"
#include "debug.h"
#include "confdialog.h"
#include "macros.h"

#include "sstv-editor.h"
#include "sstv-favorite.h"
#include "sstv-qsl.h"
#include "strutil.h"

static Fl_Double_Window	*sstv_win = 0;

static picture			*sstv_pic = 0;

static Fl_Box			*sstv_label = 0;

static Fl_Button		*btn_save = 0;
static Fl_Button		*btn_stop = 0;
static Fl_Button		*btn_close = 0;

static std::string		sstv_title;

static const int BAR_H = 28;

static void cb_save(Fl_Widget *, void *)
{
	const char *fn = FSEL::saveas(_("Save SSTV picture"), "PNG\t*.png",
		(PicsDir + "sstv.png").c_str());
	if (fn && *fn) sstv_pic->save_png(fn);
}

static void cb_stop(Fl_Widget *, void *)
{
	if (active_modem && active_modem->get_mode() == MODE_SSTV)
		static_cast<sstv *>(active_modem)->abort_picture();
}

static void cb_close(Fl_Widget *, void *)
{
	sstv_win->hide();
}

static void create_window(int W, int H)
{
	sstv_win = new Fl_Double_Window(W + 4, H + BAR_H + 4, _("SSTV Rx"));
	sstv_win->xclass(PACKAGE_NAME);
	sstv_win->begin();
	sstv_pic = new picture(2, 2, W, H);
	sstv_pic->noslant();
	int y = H + 4;
	sstv_label = new Fl_Box(2, y, W - 190, BAR_H - 4, "");
	sstv_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
	btn_stop = new Fl_Button(W - 184, y, 60, BAR_H - 4, _("Stop"));
	btn_stop->tooltip(_("End the picture being received"));
	btn_stop->callback(cb_stop, 0);
	btn_save = new Fl_Button(W - 122, y, 60, BAR_H - 4, _("Save"));
	btn_save->callback(cb_save, 0);
	btn_close = new Fl_Button(W - 60, y, 60, BAR_H - 4, _("Close"));
	btn_close->callback(cb_close, 0);
	sstv_win->end();
}

static void layout(int W, int H)
{
	if (!sstv_win) create_window(W, H);
	sstv_win->size(W + 4, H + BAR_H + 4);
	sstv_pic->resize(2, 2, W, H);
	int y = H + 4;
	sstv_label->resize(2, y, W - 190, BAR_H - 4);
	btn_stop->resize(W - 184, y, 60, BAR_H - 4);
	btn_save->resize(W - 122, y, 60, BAR_H - 4);
	btn_close->resize(W - 60, y, 60, BAR_H - 4);
	sstv_win->init_sizes();
}

void sstv_pic_status(std::string msg)
{
	if (!sstv_label) return;
	sstv_label->copy_label(msg.c_str());
	sstv_label->redraw();
}

void sstv_pic_start(int width, int height, const char *mode_name)
{
	layout(width, height);
	sstv_pic->clear();
	sstv_title = mode_name;
	sstv_pic_status(sstv_title + _("  receiving..."));
	btn_stop->activate();
	sstv_win->show();
}

void sstv_pic_update()
{
	if (!sstv_win) return;
	std::vector<unsigned char> rgb;
	int w, h;
	if (!sstv_shared_copy(rgb, w, h)) return;
	if (w != sstv_pic->pix_width() || h != sstv_pic->pix_height()) layout(w, h);
	sstv_pic->video(rgb.data(), (int)rgb.size());
}

void sstv_pic_done(std::string filename)
{
	if (!sstv_win) return;
	sstv_pic_update();
	btn_stop->deactivate();
	if (sstv_pic->save_png(filename.c_str()) == 0)
		sstv_pic_status(sstv_title + _("  saved"));
	else {
		LOG_ERROR("Cannot save %s", filename.c_str());
		sstv_pic_status(sstv_title + _("  not saved!"));
	}
}

//======================================================================
// transmit window
//======================================================================

static Fl_Double_Window	*tx_win = 0;
static picture			*tx_pic = 0;
static Fl_Choice		*tx_choice = 0;
static Fl_Button		*btn_edit = 0;
static Fl_Button		*btn_load = 0;
static Fl_Button		*btn_send = 0;
static Fl_Button		*btn_abort = 0;
static Fl_Button		*btn_txclose = 0;
static Fl_Box			*tx_label = 0;

static std::vector<unsigned char> src_rgb;	// loaded picture, original size
static int				src_w = 0, src_h = 0;
static std::string		src_name;
static std::vector<unsigned char> tx_rgb;	// picture scaled to the mode size
static const sstvcore::Mode *tx_mode = 0;
static bool				tx_busy = false;

static const int TX_MIN_W = 400;

static void tx_set_status(const std::string &msg)
{
	tx_label->copy_label(msg.c_str());
	tx_label->redraw();
}

static void tx_show_ready()
{
	if (src_rgb.empty()) {
		tx_set_status(_("Load a picture"));
		return;
	}
	int sec = (int)((tx_mode->first_ms + tx_mode->unit_ms * tx_mode->height / tx_mode->rows_per_unit) / 1000.0 + 0.91 + 0.5);
	char msg[200];
	snprintf(msg, sizeof(msg), "%s  %dx%d -> %dx%d  %d:%02d", src_name.c_str(),
		src_w, src_h, tx_mode->width, tx_mode->height, sec / 60, sec % 60);
	tx_set_status(msg);
}

static void tx_layout()
{
	const int W = tx_mode->width, H = tx_mode->height;
	const int WW = std::max(W, TX_MIN_W);
	tx_win->size(WW + 4, H + 2 * BAR_H + 4);
	tx_pic->resize(2 + (WW - W) / 2, 2, W, H);
	int y = H + 4;
	tx_choice->resize(2, y, WW - 2 - 250, BAR_H - 4);
	btn_edit->resize(WW - 250, y, 60, BAR_H - 4);
	btn_load->resize(WW - 188, y, 60, BAR_H - 4);
	btn_send->resize(WW - 126, y, 60, BAR_H - 4);
	btn_abort->resize(WW - 64, y, 60, BAR_H - 4);
	btn_txclose->resize(WW - 64, y + BAR_H, 60, BAR_H - 4);
	tx_label->resize(2, y + BAR_H, WW - 66, BAR_H - 4);
	tx_win->init_sizes();
	tx_win->redraw();
}

// scales the loaded picture to the mode and shows it
static void tx_refit()
{
	if (src_rgb.empty())
		tx_rgb.assign((size_t)tx_mode->width * tx_mode->height * 3, 0);
	else
		tx_rgb = sstvcore::fit_picture(src_rgb.data(), src_w, src_h, *tx_mode);
	tx_layout();
	tx_pic->clear();
	tx_pic->video(tx_rgb.data(), (int)tx_rgb.size());
	if (src_rgb.empty()) btn_send->deactivate(); else btn_send->activate();
	tx_show_ready();
}

static bool tx_load(const char *fn)
{
	Fl_Shared_Image *img = Fl_Shared_Image::get(fn);
	if (!img) return false;
	int w = img->w(), h = img->h(), d = img->d();
	if (img->count() != 1 || w <= 0 || h <= 0 || d < 1 || d > 4) {
		img->release();
		return false;
	}
	const unsigned char *data = (const unsigned char *)img->data()[0];
	int ld = img->ld() ? img->ld() : w * d;
	src_rgb.resize((size_t)w * h * 3);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			const unsigned char *p = data + (size_t)y * ld + (size_t)x * d;
			unsigned char *q = &src_rgb[((size_t)y * w + x) * 3];
			if (d >= 3) { q[0] = p[0]; q[1] = p[1]; q[2] = p[2]; }
			else q[0] = q[1] = q[2] = p[0];
		}
	img->release();
	src_w = w;
	src_h = h;
	const char *b = strrchr(fn, '/');
	src_name = b ? b + 1 : fn;
	return true;
}

// Expand only value macros.  Never invoke fldigi's command macro engine.
// Snapshot the current logging controls before composing the picture.
static sstvmacros::Values tx_macro_values()
{
	sstvmacros::Values values;
	values.mycall = inpMyCallsign->value();
	values.call = inpCall->value();
	values.rst = cut_string(inpRstOut->value());
	return values;
}

static bool tx_load_favorite(const char *filename)
{
	sstvfavorite::Project project;
	std::string error;
	if (!sstvfavorite::load(filename, project, error)) {
		tx_set_status(std::string(_("Cannot load SSTV favorite: ")) + error);
		return false;
	}

	const sstvmacros::Values values = tx_macro_values();
	std::vector<unsigned char> rgb = sstvqsl::render(
		project, tx_mode->width, tx_mode->height, values);
	if (rgb.size() != (size_t)tx_mode->width * tx_mode->height * 3) {
		tx_set_status(_("Cannot render SSTV favorite"));
		return false;
	}

	src_rgb.swap(rgb);
	src_w = tx_mode->width;
	src_h = tx_mode->height;
	const char *basename = strrchr(filename, '/');
	src_name = basename ? basename + 1 : filename;
	return true;
}

static void cb_tx_load(Fl_Widget *, void *)
{
	const char *fn = FSEL::select(_("Load image file"),
		"SSTV favorite\t*.sstvf\n"
		"Portable Network Graphics\t*.png\n"
		"Independent JPEG Group\t*.{jpg,jif,jpeg,jpe}\n"
		"Graphics Interchange Format\t*.gif\n"
		"Bitmap\t*.bmp", PicsDir.c_str());
	if (!fn || !*fn) return;
	const std::string path(fn);
	const bool favorite = path.size() >= 6 &&
		path.compare(path.size() - 6, 6, ".sstvf") == 0;
	if (favorite) {
		if (!tx_load_favorite(fn)) {
			return;
		}
	} else if (!tx_load(fn)) {
		tx_set_status(_("Cannot read the picture"));
		return;
	}
	tx_refit();
}

static void cb_tx_mode(Fl_Widget *, void *)
{
	const sstvcore::Mode *m = sstvcore::mode_by_name(tx_choice->text());
	if (!m || m == tx_mode) return;
	tx_mode = m;
	tx_refit();
}

static void cb_tx_send(Fl_Widget *, void *)
{
	if (tx_busy || src_rgb.empty()) return;
	if (!active_modem || active_modem->get_mode() != MODE_SSTV) {
		tx_set_status(_("Select the SSTV mode first"));
		return;
	}
	tx_busy = true;
	tx_choice->deactivate();
	btn_load->deactivate();
	btn_send->deactivate();
	btn_abort->activate();
	tx_set_status(std::string(tx_mode->name) + _("  sending..."));
	sstv_txpic_progress(-1, tx_mode->height);
	static_cast<sstv *>(active_modem)->send_picture(*tx_mode, tx_rgb);
}

static void cb_tx_abort(Fl_Widget *, void *)
{
	if (!tx_busy) return;
	if (trx_state == STATE_TX && active_modem && active_modem->get_mode() == MODE_SSTV)
		active_modem->set_stopflag(true);
	else
		sstv_txpic_done(false);   // transmission already ended elsewhere (mode change)
}

static void cb_edit(Fl_Widget *, void *)
{
	sstv_open_editor(
		tx_mode->width,
		tx_mode->height,
		tx_rgb,
		[](const std::vector<unsigned char>& rgb) {
			tx_rgb = rgb;
			tx_pic->video(tx_rgb.data(), (int)tx_rgb.size());
		}
	);
}

static void cb_tx_close(Fl_Widget *, void *)
{
	if (tx_busy) return;
	tx_win->hide();
}

static void create_tx_window()
{
	tx_mode = sstvcore::mode_by_name("Martin M1");
	const int W = tx_mode->width, H = tx_mode->height;
	tx_win = new Fl_Double_Window(TX_MIN_W + 4, H + 2 * BAR_H + 4, _("SSTV Tx"));
	tx_win->xclass(PACKAGE_NAME);
	tx_win->callback(cb_tx_close, 0);
	tx_win->begin();
	tx_pic = new picture(2, 2, W, H);
	tx_pic->noslant();
	int y = H + 4;
	tx_choice = new Fl_Choice(2, y, 130, BAR_H - 4);
	const std::vector<sstvcore::Mode> &all = sstvcore::modes();
	for (size_t i = 0; i < all.size(); i++) {
		tx_choice->add(all[i].name);
		if (&all[i] == tx_mode) tx_choice->value((int)i);
	}
	tx_choice->tooltip(_("Transmit mode"));
	tx_choice->callback(cb_tx_mode, 0);

	btn_edit = new Fl_Button(0, y, 60, BAR_H - 4, _("Edit..."));
	btn_edit->callback(cb_edit, 0);

	btn_load = new Fl_Button(0, y, 60, BAR_H - 4, _("Load..."));
	btn_load->callback(cb_tx_load, 0);

	btn_send = new Fl_Button(0, y, 60, BAR_H - 4, _("Send"));
	btn_send->callback(cb_tx_send, 0);

	btn_abort = new Fl_Button(0, y, 60, BAR_H - 4, _("Abort"));
	btn_abort->callback(cb_tx_abort, 0);
	btn_abort->deactivate();

	tx_label = new Fl_Box(2, y + BAR_H, TX_MIN_W - 64, BAR_H - 4, "");
	tx_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

	btn_txclose = new Fl_Button(0, y + BAR_H, 60, BAR_H - 4, _("Close"));
	btn_txclose->callback(cb_tx_close, 0);

	tx_win->end();
	tx_refit();
}

void sstv_showTxViewer()
{
	if (!tx_win) create_tx_window();
	tx_win->show();
}

// rows not sent yet are shown dimmed
void sstv_txpic_progress(int row, int height)
{
	if (!tx_win || !tx_busy || tx_rgb.empty()) return;
	std::vector<unsigned char> v = tx_rgb;
	size_t first = (size_t)(row + 1) * tx_mode->width * 3;
	for (size_t i = first; i < v.size(); i++) v[i] = v[i] / 3;
	tx_pic->video(v.data(), (int)v.size());
	char msg[120];
	snprintf(msg, sizeof(msg), "%s  %s %d/%d", tx_mode->name, _("sending row"), row + 1, height);
	tx_set_status(msg);
}

void sstv_txpic_done(bool complete)
{
	if (!tx_win) return;
	tx_busy = false;
	tx_choice->activate();
	btn_load->activate();
	btn_send->activate();
	btn_abort->deactivate();
	tx_pic->video(tx_rgb.data(), (int)tx_rgb.size());
	tx_set_status(std::string(tx_mode->name) + (complete ? _("  sent") : _("  aborted")));
}
