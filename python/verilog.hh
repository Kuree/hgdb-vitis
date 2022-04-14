#ifndef HGDB_VITIS_VERILOG_HH
#define HGDB_VITIS_VERILOG_HH

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// for now, we're only interested in the signal names
std::unordered_map<std::string, std::unordered_set<std::string>> parse_verilog(
    const std::vector<std::string> &files, const std::string &top_name);

#endif  // HGDB_VITIS_VERILOG_HH
