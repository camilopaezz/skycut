#pragma once
#include <string>

// Accepts supported final cutter PLT and Corel HPGL exports.
// Returns true only when one supported enclosing frame was converted.
// Otherwise output is an exact copy of input.
bool PltConvertGuides(const std::string &input, std::string &output);

// Corel absolute, semicolon-delimited HPGL export.
bool CorelConvertGuides(const std::string &input, std::string &output);
