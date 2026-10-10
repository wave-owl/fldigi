// SSTV picture editor for fldigi. GUI thread only.
// Copyright (C) 2026. GPL-3.0-or-later.
#include "sstv-editor.h"
#include "sstv-favorite.h"
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Hold_Browser.H>
//#include <FL/Fl_Value_Spinner.H>
#include <FL/Fl_Color_Chooser.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Shared_Image.H>
#include <FL/Fl_Native_File_Chooser.H>

#include "flslider2.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <png.h>

namespace {
using sstvfavorite::Text;
using sstvfavorite::Project;
static const int font_ids[]={FL_HELVETICA,FL_HELVETICA_BOLD,FL_COURIER,FL_COURIER_BOLD,FL_TIMES,FL_TIMES_BOLD};
static int font_index(int font){for (int i=0;i<6;++i)if (font_ids[i]==font)return i;return 1;}
class Editor;
class Preview : public Fl_Widget {
public:
	Editor* editor;
	Preview(int x,int y,int w,int h,Editor* e):Fl_Widget(x,y,w,h),editor(e) {}
	void draw() override;
	int handle(int event) override;
};

class Editor {
public:
	int width, height;
	Project project;
 int selected = 0;
 Text& layer(){return project.texts[selected];}
 Fl_Browser* layers;
	std::function<void(const std::vector<unsigned char>&)> apply;
	Fl_Double_Window* win;
	Preview* preview;
	Fl_Input* input;
	Fl_Choice* fonts;
	Fl_Spinner2* size;
	Fl_Button* color;
	Fl_Button* apply_button;
	bool dragging = false;
	int dx = 0, dy = 0;

	Editor(int w,int h,const std::vector<unsigned char>& rgb,
		   std::function<void(const std::vector<unsigned char>&)> cb)
		: width(w),height(h),apply(std::move(cb)) {
		project.width = w;
		project.height = h;
		project.bg_width = w;
		project.bg_height = h;
		project.background = rgb;
		if (project.background.size()!=size_t(w)*h*3)
			project.background.assign(size_t(w)*h*3,0);
		Text t;
		t.value = "CQ de W1HKJ";
		project.texts.push_back(t);
		int ww = std::max(520, width + 24);
		int bw = (ww - 116) / 5 - 2;
		win = new Fl_Double_Window(ww,height+213,"SSTV Image Editor");
		win->begin();
		preview = new Preview((ww-width)/2,8,width,height,this);
		int y = height + 17;
		auto* load = new Fl_Button(4, y, 110, 28,"Background...");
		load->callback([](Fl_Widget*,void* p) {static_cast<Editor*>(p)->load_image();}, this);
		auto* open = new Fl_Button(load->x() + load->w() + 2, y, bw, 28,"Open...");
		open->callback([](Fl_Widget*,void* p) {static_cast<Editor*>(p)->open_project();}, this);
		auto* save = new Fl_Button(open->x() + bw + 2, y, bw, 28,"Save...");
		save->callback([](Fl_Widget*,void* p) {static_cast<Editor*>(p)->save_project();}, this);
		auto* export_png = new Fl_Button(save->x() + bw + 2, y, bw, 28,"Export...");
		export_png->callback([](Fl_Widget*, void* p) {
			static_cast<Editor*>(p)->export_png();
		}, this);
		apply_button = new Fl_Button(export_png->x() + bw + 2, y, bw, 28,"Apply");
		if (!apply) apply_button->deactivate();
		apply_button->callback([](Fl_Widget*,void* p) {
			auto* e = static_cast<Editor*>(p);
			if (!e->apply)
				return;
			e->sync();
			e->apply(e->compose());
			e->win->hide();
		}, this);
		auto* cancel = new Fl_Button(apply_button->x() + bw + 2, y, bw, 28,"Cancel");
		cancel->callback([](Fl_Widget*,void* p) {static_cast<Editor*>(p)->win->hide();}, this);
		y += 35;
		input = new Fl_Input(58,y,ww-70,26,"Text:");
		input->value(layer().value.c_str());
		input->callback([](Fl_Widget*,void* p) {static_cast<Editor*>(p)->sync();}, this);
		input->when(FL_WHEN_CHANGED);
		y += 35;
		fonts = new Fl_Choice(58,y,155,26,"Font:");
		fonts->add("Helvetica|Helvetica Bold|Courier|Courier Bold|Times|Times Bold");
		fonts->value(1);
		fonts->callback([](Fl_Widget*,void* p) {static_cast<Editor*>(p)->sync();}, this);
		size = new Fl_Spinner2(268,y,72,26,"Size:");
		size->range(6,150);size->step(1);size->value(layer().size);
		size->callback([](Fl_Widget*,void* p) {static_cast<Editor*>(p)->sync();}, this);
		color = new Fl_Button(366,y,94,26,"Text color...");
		color->callback([](Fl_Widget*,void* p) {
			auto* e = static_cast<Editor*>(p);
			double r = e->layer().r/255.,
				   g = e->layer().g/255.,
				   b = e->layer().b/255.;
				   if (fl_color_chooser("Text color",r,g,b)) {
					   e->layer().r = (unsigned char)std::lround(r*255);
					   e->layer().g = (unsigned char)std::lround(g*255);
					   e->layer().b = (unsigned char)std::lround(b*255);
					   e->preview->redraw(); }
		}, this);
		y += 36;
		layers = new Fl_Hold_Browser(58,y,ww-275,62,"Layers:");
		layers->when(FL_WHEN_CHANGED | FL_WHEN_NOT_CHANGED);
		layers->callback([](Fl_Widget*, void* p) {
			Editor* editor = static_cast<Editor*>(p);
			int index = editor->layers->value() - 1;
			if (index >= 0 &&
				index < (int)editor->project.texts.size()) {
				editor->selected = index;
				editor->show_layer(false);
			}
		}, this);
		auto* add = new Fl_Button(ww-208,y,96,27,"Add text");
		add->callback([](Fl_Widget*,void* p) {
			auto* e = static_cast<Editor*>(p);
			Text t;
			t.value = "New text";
			t.y = .25+.08*e->project.texts.size();
			e->project.texts.push_back(t);
			e->selected = (int)e->project.texts.size()-1;
			e->show_layer();
		}, this);
		auto* remove = new Fl_Button(ww-108,y,96,27,"Remove");
		remove->callback([](Fl_Widget*,void* p) {
			auto* e = static_cast<Editor*>(p);
			if (e->project.texts.size() <=1 ) {
				e->input->value("");
				e->sync();
				return;
			}
			e->project.texts.erase(e->project.texts.begin()+e->selected);
			e->selected = 0;
			e->show_layer();
		}, this);
		show_layer();
		win->end();
		win->resizable(nullptr);
		win->callback([](Fl_Widget* w,void*){w->hide();});
	}

	~Editor(){delete win;}

	void show_layer(bool rebuild_browser = true) {
		if (rebuild_browser) {
			layers->clear();
			for (size_t index = 0; index < project.texts.size(); ++index) {
				std::string name = project.texts[index].value;
				if (name.empty()) {
					name = "(empty)";
				}
				layers->add(name.c_str());
			}
		}
		layers->value(selected + 1);
		const Text& text = layer();
		input->value(text.value.c_str());
		fonts->value(font_index(text.font));
		size->value(text.size);
		preview->redraw();
	}

	void sync(){
		Text& t = layer();
		t.value = input->value();
		t.font = font_ids[std::max(0,std::min(5,fonts->value()))];
		t.size = (int)size->value();
		preview->redraw();
	}

	std::vector<unsigned char> compose() {
		return sstvfavorite::render(project,width,height);
	}

	std::string choose_file(const char* title, const char* filter,
							  bool save_file) {
		Fl_Native_File_Chooser chooser;

		char dirbuf[FL_PATH_MAX + 1] = "";
#ifdef __WIN32__
		fl_filename_expand(dirbuf, sizeof(dirbuf) -1, "$USERPROFILE/");
#else
		fl_filename_expand(dirbuf, sizeof(dirbuf) -1, "$HOME/");
#endif

		chooser.title(title);
		chooser.preset_file(dirbuf);
		chooser.type(save_file ? Fl_Native_File_Chooser::BROWSE_SAVE_FILE :
							   Fl_Native_File_Chooser::BROWSE_FILE);
		chooser.filter(filter);
		if (save_file) {
			chooser.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
		}

		int result = chooser.show();

		if (result == -1) {
			fl_alert("File selection failed: %s", chooser.errmsg());
		}
		if (result != 0 || !chooser.filename()) {
			return std::string();
		}
		return chooser.filename();
	}

	void save_project() {
		sync();
		std::string fn = choose_file("Save SSTV favorite",
									 "SSTV favorite\t*.sstvf", true);
		if (fn.empty()) {
			return;
		}
		if (fn.size()<6 || fn.substr(fn.size()-6) != ".sstvf")
			fn += ".sstvf";
		std::string err;
		if (!sstvfavorite::save(fn,project,err))
			fl_alert("Save failed: %s", err.c_str());
	}


	void export_png() {
		sync();
		std::string filename = choose_file("Export SSTV PNG",
			"PNG image\t*.png", true);
		if (filename.empty()) {
			return;
		}
		if (filename.size() < 4 ||
			filename.substr(filename.size() - 4) != ".png") {
			filename += ".png";
		}

		std::vector<unsigned char> rgb = compose();
		if (rgb.size() != size_t(width) * height * 3) {
			fl_alert("PNG export failed: invalid RGB image size");
			return;
		}

		png_image image = {};
		image.version = PNG_IMAGE_VERSION;
		image.width = width;
		image.height = height;
		image.format = PNG_FORMAT_RGB;

		if (!png_image_write_to_file(&image, filename.c_str(), 0,
			rgb.data(), 0, nullptr)) {
			fl_alert("PNG export failed: %s", image.message);
		}
		png_image_free(&image);
	}

	void open_project() {
		std::string path = choose_file("Open SSTV favorite",
									   "SSTV favorite\t*.sstvf", false);
		if (path.empty()) {
			return;
		}
		Project p;
		std::string err;
		if (!sstvfavorite::load(path, p, err)) {
			fl_alert("Open failed: %s", err.c_str());
			return;
		}
		// Preserve project reference dimensions; render into current mode size.
		project = std::move(p);
		if (project.texts.empty())
			project.texts.push_back(Text());
		selected = 0;
		dragging = false;
		show_layer();
	}

	void load_image(){
		std::string path = choose_file("SSTV background",
			"Image files\t*.{png,jpg,jpeg,bmp,gif}", false);
		if (path.empty()) {
			return;
		}
		Fl_Shared_Image* image = Fl_Shared_Image::get(path.c_str());
		if (!image) {
			fl_alert("Cannot open image");
			return;
		}
		int sw = image->w(),
			sh = image->h(),
			d = image->d();
		if (sw <= 0 || sh <= 0 || d < 1 || d > 4|| image->count() != 1) {
			image->release();
			fl_alert("Unsupported image");
			return;
		}
		const unsigned char* src = (const unsigned char*)image->data()[0];
		int stride = image->ld() ? image->ld() : sw*d;
		project.bg_width = sw;
		project.bg_height = sh;
		project.background.resize(size_t(sw)*sh*3);
		for (int y = 0; y < sh; ++y)
			for (int x = 0; x < sw; ++x) {
			const unsigned char* p = src+size_t(y)*stride+x*d;
			unsigned char* q = &project.background[(size_t(y)*sw+x)*3];
			q[0] = p[0];
			q[1] = d >= 3 ? p[1] : p[0];
			q[2] = d >= 3 ? p[2] : p[0];
		}
		image->release();
		preview->redraw();
	}

};

void Preview::draw(){
	// Compose in image coordinates, exactly as Apply does.  FLTK's
	// fl_translate() does not reliably translate Fl_RGB_Image::draw(),
	// so do not attempt to shift paint() into widget coordinates.
	fl_push_clip(x(), y(), w(), h());
	fl_color(FL_BLACK);
	fl_rectf(x(), y(), w(), h());
	std::vector<unsigned char> rgb = editor->compose();
	if (rgb.size() == size_t(editor->width) * editor->height * 3) {
		Fl_RGB_Image img(rgb.data(), editor->width, editor->height, 3);
		img.draw(x(), y());
	}
	fl_pop_clip();
	fl_color(FL_DARK3);
	fl_rect(x(), y(), w(), h());
}

int Preview::handle(int ev){
	if (ev == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE) {
		const int px = Fl::event_x() - x();
		const int py = Fl::event_y() - y();
		if (px < 0 || py < 0 || px >= w() || py >= h())
			return 0;

		// Hit-test in reverse paint order so the topmost overlay wins.
		// Text::y is the BASELINE, not the top of the glyphs.
		for (int i = (int)editor->project.texts.size() - 1; i >= 0; --i) {
			const Text& t = editor->project.texts[i];
			const double scale = double(h()) / editor->project.height;
			const int font_size = std::max(1, (int)std::lround(t.size * scale));
			fl_font(t.font, font_size);
			const int left = (int)std::lround(t.x * w());
			const int baseline = (int)std::lround(t.y * h());
			const int text_width = (int)std::ceil(fl_width(t.value.c_str()));
			const int ascent = fl_height() - fl_descent();
			// Small margin makes narrow glyphs easier to select.
			if (px >= left - 3 && px <= left + text_width + 3 &&
				py >= baseline - ascent - 3 &&
				py <= baseline + fl_descent() + 3) {
				editor->selected = i;
				editor->show_layer();
				editor->dragging = true;
				editor->dx = px - left;
				editor->dy = py - baseline;
				return 1;
			}
		}
		editor->dragging = false;
		return 1;
	}
	if (ev == FL_DRAG && editor->dragging) {
		editor->layer().x = double(Fl::event_x() - x() - editor->dx) / w();
		editor->layer().y = double(Fl::event_y() - y() - editor->dy) / h();
		redraw();
		return 1;
	}
	if (ev == FL_RELEASE) {
		editor->dragging = false;
		return 1;
	}
	return Fl_Widget::handle(ev);
}

// Keep the editor alive while shown; a subsequent invocation replaces it.
std::unique_ptr<Editor> current;
}

void sstv_open_editor(int width,int height,
					  const std::vector<unsigned char>& initial_rgb,
					  const std::function<void(const std::vector<unsigned char>&)>& apply){
	if (width <= 0 || height <= 0)
		return;
	if (current && current->win->shown()) {
		current->win->show();
		return;
	}
	current.reset(new Editor(width,height, initial_rgb,apply));
	current->win->show();
}

void sstv_open_favorite_editor(
	int width,
	int height,
	const std::string& path,
	const std::function<void(const std::vector<unsigned char>&)>& apply) {

	if (width <= 0 || height <= 0)
		return;
	std::string error;
	sstvfavorite::Project p;
	if (!sstvfavorite::load(path,p,error)) {
		fl_alert("Cannot open favorite: %s", error.c_str());
		return;
	}
	if (current && current->win->shown())
		current->win->hide();
	current.reset(new Editor(width,height, {}, apply));
	current->project = std::move(p);
	if (current->project.texts.empty())
		current->project.texts.push_back(sstvfavorite::Text());
	current->selected = 0;
	current->show_layer();
	current->win->show();
}
