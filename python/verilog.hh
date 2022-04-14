#ifndef HGDB_VITIS_VERILOG_HH
#define HGDB_VITIS_VERILOG_HH

#include <unordered_map>
#include <string>
#include <memory>
#include <vector>

struct NamedScope {
    std::string name;

    std::unordered_map<std::string, std::unique_ptr<NamedScope>> members;
};


std::unique_ptr<NamedScope> parse_verilog(const std::vector<std::string> &files);

#endif  // HGDB_VITIS_VERILOG_HH
