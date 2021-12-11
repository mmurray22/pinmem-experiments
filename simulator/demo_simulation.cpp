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
#include <stdlib.h>
#include <time.h>
#include <set>
#include "mem.h"

#define TWOMB 2000000
#define MAX_ENTRIES 2000000/8192
#define NUM_LINES 1000000
#define VAL_SIZE 8192
size_t number_of_pages = 0;
std::string trace_file = "cluster001"; //"synthetic_twemcache.csv";
//"cluster001";
// Basic facts: Page is 2MB, Assume each key is 
std::unordered_map<size_t, std::vector<std::string>> offline_hot_keys;
std::unordered_map<size_t, std::vector<std::string>> page_to_key_map;
std::unordered_map<std::string, size_t> key_to_page_map;
std::unordered_map<size_t, std::vector<std::string>> page_to_client_map;


void print_hot_keys(void);
size_t convert_to_sizet(std::string str_size_t);
int test_policy_one();
/*int test_policy_two();
int test_policy_three();*/
int test_policy_four();
size_t execute_trace(std::unordered_map<std::string, size_t> kv_store);
size_t offline_hot_key_distribution(std::string trace_file);
std::unordered_map<std::string, size_t> fill_pinned_pages(std::string trace_file, std::string* buffer);
size_t count_lines(std::string trace_file);
std::vector<std::string> parse_line(std::string line);
std::string get_val(std::string bytes_str);

void assign_pages() {
  std::ifstream trace;
  trace.open(trace_file);
  std::string line;
  size_t current_page = 0;
  size_t num_slots = MAX_ENTRIES;
  if (trace.is_open()) {
    while (getline(trace, line)) {
      std::vector<std::string> twitter_req = parse_line(line);
      if (twitter_req[5].compare("gets") != 0 || key_to_page_map.find(twitter_req[1]) != key_to_page_map.end()) {
        continue;
      }
      if (current_page == 0 || num_slots == 0) {
	current_page += 1;
	std::vector<std::string> new_vec;
	new_vec.push_back(twitter_req[1]);
        page_to_key_map.insert(std::pair<size_t, std::vector<std::string>>(current_page, new_vec));
	key_to_page_map.insert(std::pair<std::string, size_t>(twitter_req[1], current_page));
	num_slots = MAX_ENTRIES - 1;
      } else {
	page_to_key_map[current_page].push_back(twitter_req[1]);
	key_to_page_map.insert(std::pair<std::string, size_t>(twitter_req[1], current_page));
	num_slots -= 1;
      }
      std::cout << "Current page: " << current_page << " and key " << twitter_req[1] << std::endl;
    }
  }
  number_of_pages = current_page;
  trace.close();
}

int main(int argc, char* argv[]) { // Args: trace_file, pgsize, num_pages
  /*1. Intake a twitter trace file and register a set of memory for the memory to be placed into*/
  // Register memory
  assign_pages();
  /* Analyze offline the "hot" keys over time */
  //offline_hot_key_distribution(argv[1]);
  /* Place the elements of the twitter trace file into registered memory*/
  //std::unordered_map<std::string, size_t> kv_store = fill_pinned_pages(argv[1], buffer);
  
  /* Test 4 different policies here. */
  test_policy_one(); // LRU
  //test_policy_two(); // Client
  //test_policy_three(); // Hot keys
  assign_pages();
  test_policy_four(); // Random
  return 0;
}

int test_policy_one() { // LRU
  std::ifstream trace;
  std::ofstream pinning;
  std::ofstream perf;
  trace.open(trace_file);
  pinning.open("pinning.csv");
  perf.open("perf.csv");
  std::string line;

  std::vector<size_t> second_check = {10};
  std::vector<size_t> threshold_discard = {1, 2};
  for (size_t i = 0; i < second_check.size(); i++) {
    for (size_t j = 0; j < threshold_discard.size(); j++) {
      pinning << i << "," << j << "\n";
      perf << i << "," << j << "\n";
      std::set<size_t> unseen_pages;
      std::set<size_t> seen_pages;
      for(auto kv : page_to_key_map){
        unseen_pages.insert(kv.first);
      }

      size_t current_second = 5;
      if (trace.is_open()) {
        while (getline(trace, line)) {
          std::vector<std::string> twitter_req = parse_line(line);
	  if (twitter_req[5].compare("gets") != 0) {
	    continue;
	  }
	  size_t perf_counter = 0; 
	  if (key_to_page_map.find(twitter_req[1]) == key_to_page_map.end()) {
	    size_t room = 0;
            for (auto it = page_to_key_map.begin(); it != page_to_key_map.end(); ++it) {
	      if (it->second.size() < MAX_ENTRIES) {
		it->second.push_back(twitter_req[1]);
	        key_to_page_map.insert(std::pair<std::string, size_t>(twitter_req[1], it->first));
	        room = 1;
		break;
	      }
	    }  
	    if (room == 0) {
	      number_of_pages += 1;
	      perf_counter += 1;
	      std::vector<std::string> new_vec;
	      new_vec.push_back(twitter_req[1]);
	      page_to_key_map.insert(std::pair<size_t, std::vector<std::string>>(number_of_pages, new_vec));
	      key_to_page_map.insert(std::pair<std::string, size_t>(twitter_req[1], number_of_pages));
	    }
	  } else {
	     unseen_pages.erase(key_to_page_map[twitter_req[1]]);
	     seen_pages.insert(key_to_page_map[twitter_req[1]]);
	  }
	  // Important: key - 1, key_size - 2, val_size - 3, op - 5
          if (convert_to_sizet(twitter_req[0]) == current_second) {
            current_second += second_check[i];
            size_t counter = 0;
            for (std::set<size_t>::iterator it=unseen_pages.begin(); it!=unseen_pages.end(); ++it) {
	      size_t discarded_page = *it;
	      std::cout << "Discarded page " << discarded_page << " second " << twitter_req[0]  << std::endl;
	      std::vector<std::string> keys = page_to_key_map[discarded_page];
	      for (size_t h = 0; h < keys.size(); h++) {
	        key_to_page_map.erase(keys[h]);
	      }
	      page_to_key_map.erase(discarded_page);
	      counter += 1;
	      number_of_pages -= 1;
	      std::cout << convert_to_sizet(twitter_req[0]) << " " << page_to_key_map.size() << std::endl;
              pinning << convert_to_sizet(twitter_req[0]) << "," <<  discarded_page << "\n" ;	      
	      if (counter > threshold_discard[j]) {
	        break;
	      }
	    }
	    perf << convert_to_sizet(twitter_req[0]) << "," << perf_counter << "," << counter << "\n";
            unseen_pages = seen_pages;
	  }
        }
      }
    }
  }
  page_to_key_map.clear();
  key_to_page_map.clear();
  perf.close();
  pinning.close();
  trace.close();
  return 0;
}

int test_policy_four() { // LRU
  std::ifstream trace;
  std::ofstream pinning;
  std::ofstream perf;
  trace.open(trace_file);
  pinning.open("pinning_rand.csv");
  perf.open("perf_rand.csv");
  std::string line;

  std::vector<size_t> second_check = {10};
  std::vector<size_t> threshold_discard = {1};
  for (size_t i = 0; i < second_check.size(); i++) {
    for (size_t j = 0; j < threshold_discard.size(); j++) {
      pinning << i << "," << j << "\n";
      perf << i << "," << j << "\n";
      size_t current_second = 5;
      if (trace.is_open()) {
        while (getline(trace, line)) {
          std::vector<std::string> twitter_req = parse_line(line);
	  if (twitter_req[5].compare("gets") != 0) {
	    continue;
	  }
	  size_t perf_counter = 0; 
	  if (key_to_page_map.find(twitter_req[1]) == key_to_page_map.end()) {
	    size_t room = 0;
            for (auto it = page_to_key_map.begin(); it != page_to_key_map.end(); ++it) {
	      if (it->second.size() < MAX_ENTRIES) {
		it->second.push_back(twitter_req[1]);
	        key_to_page_map.insert(std::pair<std::string, size_t>(twitter_req[1], it->first));
	        room = 1;
		break;
	      }
	    }  
	    if (room == 0) {
	      number_of_pages += 1;
	      perf_counter += 1;
	      std::vector<std::string> new_vec;
	      new_vec.push_back(twitter_req[1]);
	      page_to_key_map.insert(std::pair<size_t, std::vector<std::string>>(number_of_pages, new_vec));
	      key_to_page_map.insert(std::pair<std::string, size_t>(twitter_req[1], number_of_pages));
	    }
	  }
	  // Important: key - 1, key_size - 2, val_size - 3, op - 5
          if (convert_to_sizet(twitter_req[0]) == current_second) {
            current_second += second_check[i];
            size_t counter = 0;
	    for (size_t k = 0; k < threshold_discard[j]; k++) {
	      std::cout << "Number: " << number_of_pages << std::endl;
	      if (number_of_pages == 0) {
	        break;
	      }
	      size_t discarded_page = rand() % number_of_pages + 1;
	      std::cout << "Discarded page " << discarded_page << " second " << twitter_req[0]  << std::endl;
	      std::vector<std::string> keys = page_to_key_map[discarded_page];
	      for (size_t h = 0; h < keys.size(); h++) {
	        key_to_page_map.erase(keys[h]);
	      }
	      page_to_key_map.erase(discarded_page);
	      counter += 1;
	      number_of_pages -= 1;
	      std::cout << convert_to_sizet(twitter_req[0]) << " " << page_to_key_map.size() << std::endl;
              pinning << convert_to_sizet(twitter_req[0]) << "," <<  discarded_page << "\n" ;	      
	      if (counter > threshold_discard[j]) {
	        break;
	      }
	    }
	    perf << convert_to_sizet(twitter_req[0]) << "," << perf_counter << "," << counter << "\n";
	  }
        }
      }
    }
  }
  perf.close();
  pinning.close();
  trace.close();
  return 0;
}

/*int test_policy_two() { // Client
  std::ifstream trace;
  trace.open(trace_file);
  std::string line;
  std::vector<size_t> second_check = {1, 5, 10};
  std::vector<size_t> threshold_discard = {1, 5, 10};
  for (size_t i = 0; i < second_check.size(); i++) {
    for (size_t j = 0; j < threshold_discard.size(); j++) {
      if (trace.is_open()) {
        while (getline(trace, line)) {
          std::vector<std::string> twitter_req = parse_line(line);
          // Important: key - 1, key_size - 2, val_size - 3, op - 5
          if (std::find(hot_keys.begin(), hot_keys.end(), twitter_req[1]) == hot_keys.end()) {
            hot_keys.push_back(twitter_req[1]);
            key_frequency.insert(std::pair<std::string, size_t>(twitter_req[1], 1));
          } else {
            key_frequency[twitter_req[1]] += 1;
          }

          if (convert_to_sizet(twitter_req[0]) % second_check[i] == 0) {

            for (size_t k = 0; k < threshold_discard[j]; k++) {
              size_t pg = rand() % number_of_pages + 1;
            }
          }
        }
      }
    }
  }
  trace.close();
  return 0;
}*/

/*int test_policy_three() { // Hot Key
  std::ifstream trace;
  trace.open(trace_file);
  std::string line;
  std::vector<size_t> second_check = {1, 5, 10};
  std::vector<size_t> threshold_discard = {1, 5, 10};
  for (size_t i = 0; i < second_check.size(); i++) {
    for (size_t j = 0; j < threshold_discard.size(); j++) {
      if (trace.is_open()) {
        while (getline(trace, line)) {
          std::vector<std::string> twitter_req = parse_line(line);
          // Important: key - 1, key_size - 2, val_size - 3, op - 5
          if (std::find(hot_keys.begin(), hot_keys.end(), twitter_req[1]) == hot_keys.end()) {
            hot_keys.push_back(twitter_req[1]);
            key_frequency.insert(std::pair<std::string, size_t>(twitter_req[1], 1));
          } else {
            key_frequency[twitter_req[1]] += 1;
          }

          if (convert_to_sizet(twitter_req[0]) % second_check[i] == 0) {

            for (size_t k = 0; k < threshold_discard[j]; k++) {
              size_t pg = rand() % NUM_PAGES + 1;
            }
            std::vector<std::string> old_hot_keys;
            copy(hot_keys.begin(), hot_keys.end(), back_inserter(old_hot_keys));
            offline_hot_keys.insert(std::pair<size_t, std::vector<std::string>>(curr_sec, old_hot_keys));
            curr_sec = convert_to_sizet(twitter_req[0]);
            hot_keys.clear();
          }
        }
      }
    }
  }
  trace.close();
  return 0;
}*/

/*int test_policy_four() { // Random
  srand (time(NULL));
  std::ifstream trace;
  trace.open(trace_file);
  std::string line;
  std::vector<size_t> second_check = {1, 5, 10};
  std::vector<size_t> threshold_discard = {1, 5, 10};
  for (size_t i = 0; i < second_check.size(); i++) {
    for (size_t j = 0; j < threshold_discard.size(); j++) {
      if (trace.is_open()) {
        while (getline(trace, line)) {
          std::vector<std::string> twitter_req = parse_line(line);
          // Important: key - 1, key_size - 2, val_size - 3, op - 5
          if (std::find(hot_keys.begin(), hot_keys.end(), twitter_req[1]) == hot_keys.end()) {
            hot_keys.push_back(twitter_req[1]);
            key_frequency.insert(std::pair<std::string, size_t>(twitter_req[1], 1));
          } else {
            key_frequency[twitter_req[1]] += 1;
          }

          if (convert_to_sizet(twitter_req[0]) % second_check[i] == 0) {
	    for (size_t k = 0; k < threshold_discard[j]; k++) {
	      size_t pg = rand() % number_of_pages + 1; 
	      // TODO 
	    }
          }
        }
      }
    }
  }
  trace.close();
  return 0;
}*/

/*size_t offline_hot_key_distribution(std::string trace_file) {
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
}*/

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
    //std::cout << token << std::endl;
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
