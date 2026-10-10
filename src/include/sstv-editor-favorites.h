#ifndef SSTV_EDITOR_H
#define SSTV_EDITOR_H
#include <functional>
#include <string>
#include <vector>
void sstv_open_editor(int width,int height,const std::vector<unsigned char>& initial_rgb,
 const std::function<void(const std::vector<unsigned char>&)>& apply);
void sstv_open_favorite_editor(int width,int height,const std::string& path,
 const std::function<void(const std::vector<unsigned char>&)>& apply);
#endif
