// SSTV editable favorites. GUI thread only for render(). GPL-3.0-or-later.
#include "sstv-favorite.h"
#include <FL/Fl.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <cstdio>
#include <cstdint>

namespace sstvfavorite {

namespace {
	const char magic[] = "FLDIGI-SSTV-FAVORITE-1";
	bool valid(int w,int h) {
		return w >0 && h> 0 && w <= 8192 && h <= 8192;
	}\

	std::string hex(const std::string& s) {
		const char* d="0123456789ABCDEF";
		std::string out;
		for (unsigned char c:s) {
			out += d[c >> 4];
			out += d[c&15];
		}
		return out;
	}

	bool unhex(const std::string& s, std::string& out) {
		if (s.size() % 2)
			return false;
		out.clear();
		for (size_t i = 0; i < s.size(); i += 2) {
			int v = 0;
			for (int j = 0; j < 2; ++j) {
				char c = s[i+j];
				int n = (c >= '0' && c <= '9') ? c - '0': 
						(c >= 'A' && c <= 'F') ? c - 'A' + 10 :
						(c >= 'a' && c <= 'f') ? c - 'a' + 10 : -1;
				if (n < 0)
					return false;
				v = v * 16 + n;
			}
			out += (char)v;
		}
		return true;
	}
}

bool save(const std::string& path, const Project& p,std::string& error) {
	if (!valid(p.width,p.height) ||
		!valid(p.bg_width,p.bg_height) ||
		p.background.size() != size_t(p.bg_width) * p.bg_height * 3 ||
		p.texts.size() >64 ) {
		error = "Invalid project dimensions or layers";
		return false;
	}
	std::string tmp = path + ".tmp";
	std::ofstream f(tmp.c_str(), std::ios::binary|std::ios::trunc);

	if (!f) {
		error = "Cannot create favorite";
		return false;
	}
	f << magic << '\n' << p.width << ' ' << p.height << '\n'
	  << p.bg_width << ' ' << p.bg_height << '\n'
	  << p.texts.size() << '\n';
	for (const Text& t:p.texts)
		f << t.x << ' ' << t.y << ' ' << t.font << ' ' <<t.size << ' '
		  << (int)t.r << ' ' << (int)t.g << ' ' << (int)t.b << ' '
		  << (t.value.empty() ? "-" : hex(t.value)) << '\n';
	f << "RGB\n";
	f.write(reinterpret_cast<const char*>(p.background.data()), p.background.size());
	f.close();
	if (!f) {
		std::remove(tmp.c_str());
		error = "Cannot write favorite";
		return false;
	}
	if (std::rename(tmp.c_str(), path.c_str()) != 0) {
		std::remove(tmp.c_str());
		error = "Cannot rename favorite";
		return false;
	}
	return true;
}

bool load(const std::string& path,Project& out,std::string& error) {
	std::ifstream f(path.c_str(),std::ios::binary);
	if (!f) {
		error = "Cannot open favorite";
		return false;
	}
	std::string line;
	std::getline(f, line);
	if (line != magic) {
		error = "Unrecognized favorite format";
		return false;
	}
	Project p;
	size_t n = 0;
	if (!(f >> p.width >> p.height >> p.bg_width >> p.bg_height >> n) ||
		!valid(p.width, p.height) ||
		!valid(p.bg_width, p.bg_height) ||
		n > 64) {
		error = "Invalid favorite header";
		return false;
	}
	for (size_t i = 0; i < n; ++i) {
		Text t;
		int r,g,b;
		std::string encoded;
		if (!(f >> t.x >> t.y >> t.font >> t.size >> r >> g >> b >> encoded) ||
			!std::isfinite(t.x) ||
			!std::isfinite(t.y) ||
			t.size < 1 ||
			t.size > 1000 ||
			t.font < 0 ||
			t.font > 255 ||
			r < 0 || r > 255 ||
			g < 0 || g > 255 ||
			b < 0 || b > 255 ||
			(encoded != "-" && !unhex(encoded,t.value))) {
			error = "Invalid text layer";
			return false;
		}
		t.r = r;
		t.g = g;
		t.b = b;
		p.texts.push_back(t);
	}
	f >> line;
	if (line != "RGB") {
		error = "Missing RGB data";
		return false;
	}
	f.get(); // newline after RGB marker
	p.background.resize( size_t(p.bg_width) * p.bg_height * 3);
	if ( !f.read(reinterpret_cast<char*>(p.background.data()),p.background.size()) ) {
		error = "Truncated background";
		return false;
	}
	out = std::move(p);
	return true;
}

std::vector<unsigned char> render(const Project& p,int w,int h) {
	if (!valid(w,h) ||
		!valid(p.bg_width, p.bg_height) ||
		p.background.size() != size_t(p.bg_width) * p.bg_height * 3)
		return {};
	std::vector<unsigned char> rgb(size_t(w) * h * 3, 0);
	double scale = std::min(double(w)/p.bg_width, double(h)/p.bg_height);
	int tw = std::max(1, (int)std::lround(p.bg_width * scale)),
		th = std::max(1, (int)std::lround(p.bg_height * scale));
	int ox = (w - tw)/2,
		oy = (h - th)/2;
	for (int y = 0; y < th; ++y)
		for (int x = 0; x < tw; ++x) {
			int sx = std::min(p.bg_width - 1, (int)((x + .5) * p.bg_width / tw));
			int sy = std::min(p.bg_height - 1, (int)((y + .5) * p.bg_height / th));
			size_t si = (size_t(sy) * p.bg_width + sx) * 3,
				   di = (size_t(oy + y) * w + ox + x) * 3;
		for (int c = 0; c < 3; ++c)
			rgb[di + c] = p.background[si + c];
		}
	Fl_Offscreen off = fl_create_offscreen(w,h);
	if (!off)
		return {};
	fl_begin_offscreen(off);
	Fl_RGB_Image image(rgb.data(), w, h, 3);
	image.draw(0, 0);
	for (const Text& t:p.texts) {
		if (t.value.empty())
			continue;
		fl_font(t.font, std::max(1, (int)std::lround(t.size * double(h) / p.height)));
		fl_color(fl_rgb_color(t.r, t.g, t.b));
		fl_draw(t.value.c_str(), (int)std::lround(t.x * w), (int)std::lround(t.y * h));
	}
	unsigned char* pixels = fl_read_image(nullptr, 0, 0, w, h, 0);
	fl_end_offscreen();
	fl_delete_offscreen(off);
	if (!pixels)
		return {};
	rgb.assign(pixels,pixels + size_t(w) * h * 3);
	delete[] pixels;
	return rgb;
}

} // namespace

