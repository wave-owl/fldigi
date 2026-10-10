#ifndef SSTV_EDITOR_H
#define SSTV_EDITOR_H

#include <functional>
#include <vector>

// GUI thread only. RGB is tightly packed, three bytes per pixel.
// The callback receives a composited image in the selected SSTV mode size.
void sstv_open_editor(int width, int height,
                      const std::vector<unsigned char>& initial_rgb,
                      const std::function<void(const std::vector<unsigned char>&)>& apply);

#endif
