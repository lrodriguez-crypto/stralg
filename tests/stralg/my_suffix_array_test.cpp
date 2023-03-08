#include <fstream>
#include <remap.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <sstream>
#include <vector>
#include <iostream>

extern "C" {
	#include <suffix_array.h>
}

#include <suffix_array_util.hpp>

static std::string read_file(std::string filename){
	std::ifstream input_text{filename};

	if(!input_text){
		std::cerr << filename + ": Error opening file" << std::endl;
		perror(filename.c_str());
		exit(-1);
	}

	std::stringstream buffer;
	buffer << input_text.rdbuf();

    return buffer.str();
}

static std::vector<std::string> read_patterns(std::string patterns_filename){
	//Read patterns
	std::vector<std::string> patterns;

	std::ifstream input_patterns{patterns_filename};
	std::string line;

	while (getline(input_patterns, line)) 
		patterns.push_back(line); 

	return patterns;
}

static std::pair<int, int> search_pattern(struct suffix_array *sa, uint8_t *pattern){
    int l_idx = lower_bound_search(sa, pattern);
    int u_idx = upper_bound_search(sa, pattern);
	return {l_idx, u_idx};
}

static void my_tests(std::string text, std::vector<std::string> patterns, std::string filename){

	std::cout << "Text: [" << text << "]" << std::endl;

    struct suffix_array *sa;
    sa = skew_sa_construction((uint8_t *)text.c_str());
    compute_lcp(sa);
	print_suffix_array(sa);
	//test_order(sa); //test_inverse(sa); //test_lcp(sa); //if required check suffix_array_rest.c

	Save::save_to_file(sa, filename, true);

	std::ofstream patterns_idxs{filename + ".idxs"};
	for(auto &pattern: patterns){
		auto idx = search_pattern(sa, (uint8_t *)pattern.c_str());
		std::cout << idx.first << "," << idx.second << " : pattern: " << pattern << std::endl;
		patterns_idxs << idx.first << "," << idx.second << ",[" << pattern << "]" << std::endl;
	}
	patterns_idxs.close();

    free_suffix_array(sa);
	printf("======================================== \n");
}

int main(int argc, char *argv[]) {
	if(argc < 3){
		std::cerr << "Error: it is required two parameters for this test" << std::endl;
		std::cerr << "use: " << argv[0] << " <input_text_file> <intput_patters_file>" << std::endl;
		exit(-1);
	}

	std::string text_filename{argv[1]};
	std::string patterns_filename{argv[2]};

	std::cout << "Input text file name: " << text_filename << std::endl;
	std::cout << "Input patterns file name: " << patterns_filename << std::endl;

	std::string text = read_file(text_filename);
	std::vector<std::string> patterns = read_patterns(patterns_filename);

	my_tests(text, patterns, text_filename);
    return EXIT_SUCCESS;
}
