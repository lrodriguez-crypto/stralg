#pragma once

extern "C" {
	#include <suffix_array.h>
}

class Save{

	public :

	Save();
	static void save_to_file(struct suffix_array *, std::string filename, bool include_substring = true);
};
