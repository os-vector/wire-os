
#include "perf_data_converter.h"
#include "quipper/perf_parser.h"
#include <map>

using std::map;

namespace wireless_android_logging_awp {

struct RangeTarget {
  RangeTarget(uint64 start, uint64 end, uint64 to)
      : start(start), end(end), to(to) {}

  bool operator<(const RangeTarget &r) const {
    if (start != r.start) {
      return start < r.start;
    } else if (end != r.end) {
      return end < r.end;
    } else {
      return to < r.to;
    }
  }
  uint64 start;
  uint64 end;
  uint64 to;
};

struct BinaryProfile {
  map<uint64, uint64> address_count_map;
  map<RangeTarget, uint64> range_count_map;
};

wireless_android_play_playlog::AndroidPerfProfile
RawPerfDataToAndroidPerfProfile(const string &perf_file) {
  wireless_android_play_playlog::AndroidPerfProfile ret;
  quipper::PerfParser parser;
  if (!parser.ReadFile(perf_file) || !parser.ParseRawEvents()) {
    return ret;
  }

  typedef map<string, BinaryProfile> ModuleProfileMap;
  typedef map<string, ModuleProfileMap> ProgramProfileMap;
  ProgramProfileMap name_profile_map;
  uint64 total_samples = 0;
  for (const auto &event : parser.parsed_events()) {
    if (!event.raw_event ||
        event.raw_event->header.type != PERF_RECORD_SAMPLE) {
      continue;
    }
    string dso_name = event.dso_and_offset.dso_name();
    string program_name;
    if (dso_name == "[kernel.kallsyms]_text") {
      program_name = "kernel";
      dso_name = "[kernel.kallsyms]";
    } else if (event.command() == "") {
      program_name = "unknown_program";
    } else {
      program_name = event.command();
    }
    name_profile_map[program_name][dso_name].address_count_map[
        event.dso_and_offset.offset()]++;
    total_samples++;
    for (size_t i = 1; i < event.branch_stack.size(); i++) {
      if (dso_name == event.branch_stack[i - 1].to.dso_name()) {
        uint64 start = event.branch_stack[i].to.offset();
        uint64 end = event.branch_stack[i - 1].from.offset();
        uint64 to = event.branch_stack[i - 1].to.offset();
        // The interval between two taken branches should not be too large.
        if (end < start || end - start > (1 << 20)) {
          LOG(WARNING) << "Bogus LBR data: " << start << "->" << end;
          continue;
        }
        name_profile_map[program_name][dso_name].range_count_map[
            RangeTarget(start, end, to)]++;
      }
    }
  }

  map<string, int> name_id_map;
  for (const auto &program_profile : name_profile_map) {
    for (const auto &module_profile : program_profile.second) {
      name_id_map[module_profile.first] = 0;
    }
  }
  int current_index = 0;
  for (auto iter = name_id_map.begin(); iter != name_id_map.end(); ++iter) {
    iter->second = current_index++;
  }

  map<string, string> name_buildid_map;
  parser.GetFilenamesToBuildIDs(&name_buildid_map);
  ret.set_total_samples(total_samples);
  for (const auto &name_id : name_id_map) {
    auto load_module = ret.add_load_modules();
    load_module->set_name(name_id.first);
    auto nbmi = name_buildid_map.find(name_id.first);
    if (nbmi != name_buildid_map.end()) {
      const std::string &build_id = nbmi->second;
      if (build_id.size() == 40 && build_id.substr(32) == "00000000") {
        load_module->set_build_id(build_id.substr(0, 32));
      } else {
        load_module->set_build_id(build_id);
      }
    }
  }
  for (const auto &program_profile : name_profile_map) {
    auto program = ret.add_programs();
    program->set_name(program_profile.first);
    for (const auto &module_profile : program_profile.second) {
      int32 module_id = name_id_map[module_profile.first];
      auto module = program->add_modules();
      module->set_load_module_id(module_id);
      for (const auto &addr_count : module_profile.second.address_count_map) {
        auto address_samples = module->add_address_samples();
        address_samples->add_address(addr_count.first);
        address_samples->set_count(addr_count.second);
      }
      for (const auto &range_count : module_profile.second.range_count_map) {
        auto range_samples = module->add_range_samples();
        range_samples->set_start(range_count.first.start);
        range_samples->set_end(range_count.first.end);
        range_samples->set_to(range_count.first.to);
        range_samples->set_count(range_count.second);
      }
    }
  }
  return ret;
}

}  // namespace wireless_android_logging_awp
