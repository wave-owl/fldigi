#ifndef SSTV_QSL_H
#define SSTV_QSL_H

#include "sstv-favorite.h"
#include "sstv-macros.h"

namespace sstvqsl {

// Return a separate project with expanded text; never alter the template.
sstvfavorite::Project expand_project(
	const sstvfavorite::Project& source,
	const sstvmacros::Values& values);

// Must be called from the FLTK GUI thread (sstvfavorite::render uses FLTK).
std::vector<unsigned char> render(
	const sstvfavorite::Project& source,
	int width, int height,
	const sstvmacros::Values& values);

} // namespace sstvqsl

#endif // SSTV_QSL_H
