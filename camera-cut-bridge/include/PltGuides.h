#pragma once
#include <string>

// Returns true only when one supported enclosing frame was converted.
// Otherwise output is an exact copy of input.
bool PltConvertGuides(const std::string &input, std::string &output);
