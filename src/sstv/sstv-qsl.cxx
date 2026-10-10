// =====================================================================
// sstv-qsl.cxx -- SSTV favorite rendering with value-only macros
// Copyright (C) 2026, Dave Freese, W1HKJ
// GPL-3.0-or-later
// =====================================================================

#include "sstv-qsl.h"

namespace sstvqsl {

sstvfavorite::Project expand_project(
	const sstvfavorite::Project& source,
	const sstvmacros::Values& values) {

	sstvfavorite::Project expanded = source;
	for (sstvfavorite::Text& layer : expanded.texts) {
		layer.value = sstvmacros::expand(layer.value, values);
	}
	return expanded;
}

std::vector<unsigned char> render(
	const sstvfavorite::Project& source,
	int width, int height,
	const sstvmacros::Values& values) {

	const sstvfavorite::Project expanded = expand_project(source, values);
	return sstvfavorite::render(expanded, width, height);
}

} // namespace sstvqsl
