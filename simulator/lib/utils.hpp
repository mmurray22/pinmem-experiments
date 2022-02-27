#pragma once

#include <sstream>
#include <string>
#include <array>
#include <algorithm>
#include <memory>
#include <map>
#include <iostream>
#include <functional>

#define BUF_SIZE 1024


/*
 * Generic Utils class with useful functions
 */

class Utils {
	public:
  	  /*Compare the second element in two pairs. The element MUST be a numerical type.*/
          template<typename T>
	  bool cmp(std::pair<size_t, T>& a, std::pair<size_t, T>& b){
		  return a.second < b.second;
	  }

	  /*Flip pair values*/
	  template<typename T, typename U>
	  std::pair<U,T> flip_pair(const std::pair<T,U> &p) {
		  return std::pair<U,T>(p.second, p.first);
	  }

	  /*Flip map*/
	  template<typename T, typename U>
	  std::map<U,T> flip_map(const std::map<T,U> &src) { // TODO: maybe use mem_fn for transform?
		  std::map<U,T> dst;
		  std::transform(src.begin(), src.end(), std::inserter(dst, dst.begin()), [this](std::pair<T, U> p){ return this->flip_pair(p);});
                  return dst;
	  }

	  /*Count lines in a provided file*/
	  size_t count_lines(std::string trace_file) {
		  std::array<char, BUF_SIZE> buf;
		  std::string output = "";
		  std::string command_to_execute = "wc -l " + trace_file;
		  const char* char_req = command_to_execute.c_str();
		  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(char_req, "r"), pclose);
		  if (!pipe) {
			  std::cout << "popen() failed!" << std::endl; 
			  return 0;
		  }
		  while (fgets(buf.data(), buf.size(), pipe.get()) != nullptr) {
			  output += buf.data();
		  }
		  std::stringstream sstream(output);
		  size_t result;
		  sstream >> result;
		  return result;
	  }
};
