// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// File utility functions.

#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <vector>

namespace PSIFileUtils {
	std::string read_file_to_string(const std::string path);
	std::vector<unsigned char> read_file(const std::string path);
}
