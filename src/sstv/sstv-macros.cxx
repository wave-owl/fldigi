// =====================================================================
// sstv-macros.cxx -- side-effect-free SSTV QSL text expansion
// Copyright (C) 2026, Dave Freese, W1HKJ
// GPL-3.0-or-later
// =====================================================================

#include "sstv-macros.h"

namespace sstvmacros {

std::string expand(const std::string& input, const Values& values) {
	std::string result;
	result.reserve(input.size());

	size_t position = 0;
	while (position < input.size()) {
		if (input.compare(position, 8, "<MYCALL>") == 0) {
			result += values.mycall;
			position += 8;
		} else if (input.compare(position, 6, "<CALL>") == 0) {
			result += values.call;
			position += 6;
		} else if (input.compare(position, 5, "<RST>") == 0) {
			result += values.rst;
			position += 5;
		} else {
			result += input[position];
			++position;
		}
	}

	return result;
}

} // namespace sstvmacros
