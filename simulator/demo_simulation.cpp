#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <sys/mman.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <sstream>
#include <fcntl.h>
#include <algorithm>
#include <iterator>
#include <stdlib.h>
#include <time.h>
#include <set>
#include "mem.h"
#include <map>

#define HUGEPAGE_SZ 2000000
#define MAX_ENTRIES 2000000/4096
#define NUM_LINES 1000000
#define VAL_SIZE 4096
std::string DIR = "/proj/sosp21ae-PG0/user/murray22/nicbenchmarks/simulator/csv/";
size_t number_of_pages = 0;
std::string trace_file = "cluster001";
std::vector<size_t> second_check = {5};
std::vector<size_t> threshold_discard = {2};

//"cluster001";
// Basic facts: Page is 2MB, Assume each key is 

/* General Purpose Data structures*/
std::unordered_map<size_t, std::vector<std::string>> offline_hot_keys;
std::unordered_map<size_t, std::vector<std::string>> page_to_key_map;
std::unordered_map<std::string, size_t> key_to_page_map;
std::unordered_map<size_t, size_t> client_to_page_map;
std::unordered_map<size_t, std::vector<std::string>> page_to_client_map;
std::map<size_t, size_t> clients;
std::map<std::string, size_t> keys;

/* LRU Policy Data structures */
std::set<size_t> unseen_pages;
std::set<size_t> seen_pages;

/* Popular Client Policy Data structures */
std::map<size_t, size_t> client_map;
std::map<size_t, size_t> sorted_client;

/* Hot Key Policy Data strucutres */
std::map<std::string, size_t> key_map;
std::map<size_t, std::string> sorted_key;

int good_operation(std::string op) {
  return op.compare("get") == 0;
}

bool cmp(std::pair<size_t, int>& a,
         std::pair<size_t, int>& b){
    return a.second < b.second;
}

std::pair<size_t,size_t> flip_pair(const std::pair<size_t,size_t> &p) {
    return std::pair<size_t,size_t>(p.second, p.first);
}

std::pair<size_t,std::string> flip_key_pair(const std::pair<std::string,size_t> &p) {
    return std::pair<size_t,std::string>(p.second, p.first);
}

std::map<size_t,size_t> flip_map(const std::map<size_t,size_t> &src) {
    std::map<size_t,size_t> dst;
    std::transform(src.begin(), src.end(), std::inserter(dst, dst.begin()), 
                   flip_pair);
    return dst;
}

std::map<size_t,std::string> flip_key_map(const std::map<std::string,size_t> &src) {
    std::map<size_t,std::string> dst;
    std::transform(src.begin(), src.end(), std::inserter(dst, dst.begin()),
                   flip_key_pair);
    return dst;
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
  // twitter req: 0: timestamp, 1: key, 2: ksize, 3: vsize, 4: cid, 5: op, 6: ttl
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

size_t convert_to_sizet(std::string str_size_t) {
  std::stringstream sstream;
  sstream << str_size_t;
  size_t val;
  sstream >> val;
  return val;
}

std::string get_val(std::string bytes_str) {
  size_t bytes = convert_to_sizet(bytes_str);
  std::string val = "s";
  for (size_t i = 1; i < bytes; i++) {
    val.append("s");
  }
  return val;
}

void assign_pages() {
  std::ifstream trace;
  trace.open(trace_file);
  std::string line;
  size_t current_page = 0;
  size_t num_slots = MAX_ENTRIES;
  if (trace.is_open()) {
    while (getline(trace, line)) {
      std::vector<std::string> twitter_req = parse_line(line);
      if (twitter_req[5].compare("get") != 0 || key_to_page_map.find(twitter_req[1]) != key_to_page_map.end()) {
        continue;
      }
      if (current_page == 0 || num_slots == 0) {
        current_page += 1;
        std::vector<std::string> new_vec;
        new_vec.push_back(twitter_req[1]);
        page_to_key_map.insert(std::pair<size_t, std::vector<std::string>>(current_page, new_vec));
        key_to_page_map.insert(std::pair<std::string, size_t>(twitter_req[1], current_page));
        client_to_page_map.insert(std::pair<size_t, size_t>(convert_to_sizet(twitter_req[4]), current_page));
        num_slots = MAX_ENTRIES - 1;
      } else {
        page_to_key_map[current_page].push_back(twitter_req[1]);
        key_to_page_map.insert(std::pair<std::string, size_t>(twitter_req[1], current_page));
              client_to_page_map.insert(std::pair<size_t, size_t>(convert_to_sizet(twitter_req[4]), current_page));
        num_slots -= 1;
      }
    }
  }
  number_of_pages = current_page;
  trace.close();
}

size_t add_key(std::string key) {
  for (auto it = page_to_key_map.begin(); it != page_to_key_map.end(); ++it) {
    if (it->second.size() < MAX_ENTRIES) {
      it->second.push_back(key);
      key_to_page_map.insert(std::pair<std::string, size_t>(key, it->first));
      return 0;
    }
  }
  number_of_pages += 1;
  std::vector<std::string> new_vec;
  new_vec.push_back(key);
  page_to_key_map.insert(std::pair<size_t, std::vector<std::string>>(number_of_pages, new_vec));
  key_to_page_map.insert(std::pair<std::string, size_t>(key, number_of_pages));
  return 1;
}

std::vector<size_t> remove_pages(int max_evict, int policy_no) {
  std::vector<size_t> discarded_pages;
  if (number_of_pages == 0) {
    return discarded_pages;
  }
  if (policy_no == 2) {
    sorted_client = flip_map(client_map);
  } else if (policy_no == 3) {
    sorted_key = flip_key_map(key_map);
  }
  for (int i = 0; i < max_evict; i++) {
    size_t page_no;
    if (policy_no == 1) {
      std::cout << "Enter here: " << unseen_pages.size() << std::endl;
      if (unseen_pages.size() == 0) {
        return discarded_pages;
      }
      auto it = unseen_pages.begin();
      page_no = *it;
      unseen_pages.erase(it);
    } else if (policy_no == 2) {
      auto it = sorted_client.begin();
      page_no = client_to_page_map[it->second];
      sorted_client.erase(it->first);
    } else if (policy_no == 3) {
      auto it = sorted_key.begin();
      page_no = key_to_page_map[it->second];
      sorted_key.erase(it->first);
    } else {
      page_no = rand() % number_of_pages + 1;
    }
    std::vector<std::string> keys = page_to_key_map[page_no];
    for (size_t h = 0; h < keys.size(); h++) {
      key_to_page_map.erase(keys[h]);
    }
    page_to_key_map.erase(page_no);
    number_of_pages -= 1;
    discarded_pages.push_back(page_no);
  }
}

void prepare_for_trace_run(int policy_no) {
  if (policy_no == 1) {
    for(auto kv : page_to_key_map){
      unseen_pages.insert(kv.first);
    }
    std::cout << "Size of unseen pages: " << unseen_pages.size() << std::endl;
  } else if (policy_no == 2) {
    for (auto it=client_to_page_map.begin(); it!=client_to_page_map.end(); ++it) { 
      client_map.insert(std::pair<size_t, size_t>(it->first, 0));
    }
  } else if (policy_no == 3) {
    for (auto it=key_to_page_map.begin(); it!=key_to_page_map.end(); ++it) {
      key_map.insert(std::pair<std::string, size_t>(it->first, 0));
    }
  }
}

void update_policy_info(int policy_no, std::string key, std::string cli) {
  if (policy_no == 1) {
    unseen_pages.erase(key_to_page_map[key]);
    // std::cout << "Size of unseen: " << unseen_pages.size() << std::endl;
    seen_pages.insert(key_to_page_map[key]);
  } else if (policy_no == 2) {
    if (client_map.find(convert_to_sizet(cli)) != client_map.end()) {
      client_map[convert_to_sizet(cli)] += 1; 
    }
  } else if (policy_no == 3) {
   if (key_map.find(key) != key_map.end()) {
     /*key_map.insert(std::pair<std::string, size_t>(key, 1));
   } else {*/
     key_map[key] += 1; 
   } 
  }
}

void clean_up_after_trace_run(int policy_no) {
  if (policy_no == 1) {
    // unseen_pages.erase(unseen_pages.begin(), unseen_pages.end());
    // seen_pages.erase(seen_pages.begin(), seen_pages.end());
    unseen_pages = seen_pages;
  }
}

int test_policy(int policy_no) {
  std::ifstream trace;
  std::ofstream pinning;
  std::ofstream perf;
  trace.open(trace_file);
  if (policy_no == 1) { // LRU
    pinning.open(DIR + "pinning1.csv");
    perf.open(DIR + "perf1.csv");
  } else if (policy_no == 2) { // Clients
    pinning.open(DIR + "pinning2.csv");
    perf.open(DIR + "perf2.csv");
  } else if (policy_no == 3) { // Keys
    pinning.open(DIR + "pinning3.csv");
    perf.open(DIR + "perf3.csv");
  } else { // Random
    pinning.open(DIR + "pinning4.csv");
    perf.open(DIR + "perf4.csv");
  }
  std::string line;
  for (size_t i = 0; i < second_check.size(); i++) {
    for (size_t j = 0; j < threshold_discard.size(); j++) {
      pinning << "Heuristics," << second_check[i] << "," << threshold_discard[j] << "," << number_of_pages << "\n";
      perf << "Heuristics," << second_check[i] << "," << threshold_discard[j] << "," << number_of_pages << "\n";
      std::cout << "Heuristics: " << second_check[i] << " " << threshold_discard[j] << std::endl;
      prepare_for_trace_run(policy_no); //TODO
      size_t perf_counter = 0;
      size_t current_second = 0;
      size_t lineno = 0;
      if (trace.is_open()) {
        while (getline(trace, line)) {
          lineno += 1;
          std::vector<std::string> twitter_req = parse_line(line);
          if (!good_operation(twitter_req[5])) {
            // std::cout << "Do we not make it past here? " << twitter_req[5] << std::endl;
            continue;
          }
          if (key_to_page_map.find(twitter_req[1]) == key_to_page_map.end()) {
            perf_counter += add_key(twitter_req[1]);
          } 
          update_policy_info(policy_no, twitter_req[1], twitter_req[4]); //TODO
          /*Time to check!*/
          // std::cout << "Policy is checked! " << convert_to_sizet(twitter_req[0])  << current_second << std::endl;
          if (convert_to_sizet(twitter_req[0]) == current_second) {
            // std::cout << "Do we get stuck here?" << std::endl;
            std::cout << "Line: " << lineno << std::endl;
            current_second += second_check[i];
            std::cout << "Policy is checked! " << convert_to_sizet(twitter_req[0])  << current_second << std::endl;
            std::vector<size_t> discarded_pages = remove_pages(threshold_discard[j], policy_no); // TODO
            std::cout << "Pinning counter: " << discarded_pages.size() << std::endl;
            pinning << twitter_req[0];
            for (size_t k = 0; k < discarded_pages.size(); k++) {
              pinning << "," << discarded_pages[k];
            }
            pinning << '\n';
            
            perf << convert_to_sizet(twitter_req[0]) << "," << perf_counter << "," << discarded_pages.size() << "\n";
            std::cout << "Perf counter: " << twitter_req[0] << std::endl;
            clean_up_after_trace_run(policy_no);
            // prepare_for_trace_run(policy_no);
            perf_counter = 0;
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

int main(int argc, char* argv[]) { // Args: trace_file, pgsize, num_pages
  /* Test 4 different policies here. */
  srand (time(NULL));
  for (int i = 1; i <= 4; i++) {
    std::cout << "Now testing policy " << i << "!" << std::endl;
    assign_pages();
    test_policy(i);
  }
  //offline_hot_key_distribution();
  return 0;
}

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
