#include "PSIFileUtils.h"

namespace PSIFileUtils {

std::string read_file_to_string(const std::string path) {
	// Return empty string if not succesfull
	std::string contents;

	FILE *fp = fopen(path.c_str(), "rb");
	if (fp != NULL) {
		fseek(fp, 0, SEEK_END);
		contents.resize(std::ftell(fp));
		rewind(fp);
		fread(&contents[0], 1, contents.size(), fp);
		fclose(fp);
	}

	return contents;
}

std::vector<unsigned char> read_file(const std::string path) {
	std::ifstream ifs(path.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
	std::vector<unsigned char> bytes;

	if (ifs.is_open()) {
		std::ifstream::pos_type file_size = ifs.tellg();
		ifs.seekg(0, std::ios::beg);
		bytes = std::vector<unsigned char>(file_size);
   		ifs.read(reinterpret_cast<char*>(&bytes[0]), file_size);
	}

	return bytes;
}

} // PSIFileUtils
