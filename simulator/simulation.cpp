#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <sys/mman.h>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <fcntl.h>
#include <algorithm>
#include <iterator>
#include "mem.h"

#define NUM_PAGES 2048
std::unordered_map<size_t, std::vector<std::string>> offline_hot_keys;

void print_hot_keys(void);
size_t convert_to_sizet(std::string str_size_t);
int test_policy_one();
int test_policy_two();
int test_policy_three();
size_t execute_trace(std::unordered_map<std::string, size_t> kv_store);
size_t offline_hot_key_distribution(std::string trace_file);
std::unordered_map<std::string, size_t> fill_pinned_pages(std::string trace_file, std::string* buffer);
size_t count_lines(std::string trace_file);
std::vector<std::string> parse_line(std::string line);
std::string get_val(std::string bytes_str);

int main(int argc, char* argv[]) { // Args: trace_file, pgsize, num_pages
  /*1. Intake a twitter trace file and register a set of memory for the memory to be placed into*/
  if (argc < 3) {
    printf("Too few arguments!\n");
    return -1;
  }
  int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_HUGETLB;
  size_t num_lines = count_lines(argv[1]);

  // Register memory
  size_t pgsize = convert_to_sizet(argv[2]);
  size_t num_pages = convert_to_sizet(argv[3]); 
  void * addr = mmap(NULL, pgsize * num_pages, PROT_READ | PROT_WRITE, flags, -1, 0);
  if (addr == MAP_FAILED) {
    printf("Failed to mmap memory\n");
  }
  std::string buffer[num_lines];
  if (mlock(buffer, num_lines * sizeof(char*)) == -1) {
    perror("mlock");
  }
  printf("Pinned the pages!\n");

  /* Analyze offline the "hot" keys over time */
  offline_hot_key_distribution(argv[1]);
  /* Place the elements of the twitter trace file into registered memory*/
  std::unordered_map<std::string, size_t> kv_store = fill_pinned_pages(argv[1], buffer);
  

  /* TODO Analyze IRL*/
  execute_trace(kv_store);

  /*5. Test 3 different policies here. */
  test_policy_one(); // Go with the hot keys
  test_policy_two(); // Random
  test_policy_three(); // Haven't seen it in the last second? Dump the page
  return 0;
}

int test_policy_one() {
  return 0;
}

int test_policy_two() {
  return 0;
}

int test_policy_three() {
  return 0;
}

size_t offline_hot_key_distribution(std::string trace_file) {
  std::ifstream trace;
  trace.open(trace_file);
  std::string line;
  std::unordered_map<std::string, size_t> kv_map;
  size_t curr_sec = 0;
  std::vector<std::string> hot_keys;
  std::unordered_map<std::string, size_t> key_frequency;
  if (trace.is_open()) {
    while (getline(trace, line)) {
      std::vector<std::string> twitter_req = parse_line(line);
      // Important: key - 1, key_size - 2, val_size - 3, op - 5
      /*TODO: Exact mechanics of obtaining hot_keys*/
      if (std::find(hot_keys.begin(), hot_keys.end(), twitter_req[1]) == hot_keys.end()) {
        hot_keys.push_back(twitter_req[1]);
	key_frequency.insert(std::pair<std::string, size_t>(twitter_req[1], 1));
      } else {
        key_frequency[twitter_req[1]] += 1; 
      }

      if (convert_to_sizet(twitter_req[0]) > curr_sec) {
	std::vector<std::string> old_hot_keys;
	copy(hot_keys.begin(), hot_keys.end(), back_inserter(old_hot_keys));
	offline_hot_keys.insert(std::pair<size_t, std::vector<std::string>>(curr_sec, old_hot_keys));
        curr_sec = convert_to_sizet(twitter_req[0]);
	hot_keys.clear();
      }
    }
  }
  print_hot_keys();
  return 0;
}

void print_hot_keys() {
  for (auto const &pair: offline_hot_keys) {
    std::cout << "{" << pair.first << ": ";
    for (size_t i = 0; i < pair.second.size()-1; i++) {
	    std::cout << pair.second[i] << ", " << std::endl;
    } 
    std::cout << pair.second[pair.second.size()-1] << "}\n";
  }
}

size_t execute_trace(std::unordered_map<std::string, size_t> kv_store) {
  return 0;
}

std::unordered_map<std::string, size_t> fill_pinned_pages(std::string trace_file, std::string* buffer) {
  std::ifstream trace;
  trace.open(trace_file);
  std::string line;
  std::unordered_map<std::string, size_t> kv_map;
  size_t i = 0;
  if (trace.is_open()) {
    while (getline(trace, line)) {
      std::vector<std::string> twitter_req = parse_line(line);
      // Important: key - 1, key_size - 2, val_size - 3, op - 5
      buffer[i] = get_val(twitter_req[3]);
      if (twitter_req[5].compare("gets") == 0) {
        kv_map.insert(std::pair<std::string, size_t>(twitter_req[1], i));
      }
      i+=1;
    }
  }
  return kv_map;
}

size_t count_lines(std::string trace_file) {
  std::ifstream trace;
  std::string line;
  size_t count = 0;
  trace.open(trace_file);
  if (trace.is_open()) {
    while (getline(trace, line)) {
      count += 1;
    }
  }
  trace.close();
  return count;
}

std::vector<std::string> parse_line(std::string line) {
  std::string delim = ",";
  size_t pos = 0;
  std::string token;
  std::vector<std::string> twitter_req;
  while ((pos = line.find(delim)) != std::string::npos) {
    token = line.substr(0, pos);
    std::cout << token << std::endl;
    twitter_req.push_back(token);
    line.erase(0, pos + delim.length());
  }
  return twitter_req;
}

std::string get_val(std::string bytes_str) {
  size_t bytes = convert_to_sizet(bytes_str);
  std::string val = "s";
  for (size_t i = 1; i < bytes; i++) {
    val.append("s");
  }
  return val;
}

size_t convert_to_sizet(std::string str_size_t) {
  std::stringstream sstream;
  sstream << str_size_t;
  size_t val;
  sstream >> val;
  return val;
}
