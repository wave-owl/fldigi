#ifndef SSTV_MACROS_H
#define SSTV_MACROS_H

#include <string>

namespace sstvmacros {

// A snapshot of the three existing fldigi macro values.
// Capture on the GUI thread immediately before composing a QSL image.
struct Values {
	std::string mycall;
	std::string call;
	std::string rst;
};

// Only these three value macros are expanded. All other text, including
// unknown <...> expressions, is preserved literally. No side effects.
std::string expand(const std::string& input, const Values& values);

}

#endif // SSTV_MACROS_H
