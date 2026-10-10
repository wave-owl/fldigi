#ifndef SSTV_FAVORITE_H
#define SSTV_FAVORITE_H
#include <string>
#include <vector>
#include <functional>
namespace sstvfavorite {
struct Text {
 std::string value;
 double x = .04, y = .18; // normalized baseline coordinates
 int font = 1, size = 30; // FLTK font, size at reference height
 unsigned char r = 255, g = 255, b = 255;
};
struct Project {
 int width = 320, height = 256;
 int bg_width = 0, bg_height = 0;
 std::vector<unsigned char> background; // original RGB, not flattened
 std::vector<Text> texts;
};
bool save(const std::string& path, const Project&, std::string& error);
bool load(const std::string& path, Project&, std::string& error);
std::vector<unsigned char> render(const Project&, int width, int height);
}
#endif
