#include <iostream>
#include <fstream>
#include "suffix_array_util.hpp"


Save::Save(){
	std::cout << "Testing SAVE"  << std::endl;
}

void Save::save_to_file(struct suffix_array *sa, std::string filename, bool include_substrings){
	std::cout << "[" << sa->string << "] saving suffix arrays to file: [" << filename  << ".sa]" << ((include_substrings)? "":" NO") << " include substrings" << std::endl;

	std::ofstream output_sa{filename + ".sa"};
    for (uint32_t i = 0; i < sa->length; ++i)
		if(include_substrings)
			output_sa << i << "," << sa->array[i] << "," << (sa->string + sa->array[i]) << std::endl;
		else
			output_sa << sa->array[i] << std::endl;
	output_sa.close();

	std::ofstream output_isa{filename + ".isa"};
    for (uint32_t i = 0; i < sa->length; ++i)
		if(include_substrings)
			output_isa << i << "," << sa->inverse[i] << "," << (sa->string + i) << std::endl;
		else
			output_isa << sa->inverse[i] << std::endl;
	output_isa.close();

	std::ofstream output_lcp{filename + ".lcp"};
    for (uint32_t i = 0; i < sa->length; ++i)
		if(include_substrings)
			output_lcp << i << "," << sa->lcp[i] << "," << (sa->string + sa->array[i]) << std::endl;
		else
			output_lcp << sa->lcp[i] << std::endl;
	output_lcp.close();
}
