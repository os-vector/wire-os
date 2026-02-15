/*
**
** Copyright 2015, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <string>
#include <sstream>
#include <map>
#include <set>
#include <cctype>

#include <base/file.h>
#include <base/stringprintf.h>
#include <cutils/properties.h>

#include "perfprofdcore.h"
#include "perfprofdutils.h"
#include "perf_data_converter.h"
#include "cpuconfig.h"

//
// Perf profiling daemon -- collects system-wide profiles using
//
//       simpleperf record -a
//
// and encodes them so that they can be uploaded by a separate service.
//

//......................................................................

//
// Output file from 'perf record'.
//
#define PERF_OUTPUT "perf.data"

//
// This enum holds the results of the "should we profile" configuration check.
//
typedef enum {

  // All systems go for profile collection.
  DO_COLLECT_PROFILE,

  // The selected configuration directory doesn't exist.
  DONT_PROFILE_MISSING_CONFIG_DIR,

  // Destination directory does not contain the semaphore file that
  // the perf profile uploading service creates when it determines
  // that the user has opted "in" for usage data collection. No
  // semaphore -> no user approval -> no profiling.
  DONT_PROFILE_MISSING_SEMAPHORE,

  // No perf executable present
  DONT_PROFILE_MISSING_PERF_EXECUTABLE,

  // We're running in the emulator, perf won't be able to do much
  DONT_PROFILE_RUNNING_IN_EMULATOR

} CKPROFILE_RESULT;

//
// Are we running in the emulator? If so, stub out profile collection
// Starts as uninitialized (-1), then set to 1 or 0 at init time.
//
static int running_in_emulator = -1;

//
// Is this a debug build ('userdebug' or 'eng')?
// Starts as uninitialized (-1), then set to 1 or 0 at init time.
//
static int is_debug_build = -1;

//
// Random number generator seed (set at startup time).
//
static unsigned short random_seed[3];

//
// Config file path. May be overridden with -c command line option
//
static const char *config_file_path =
    "/data/data/com.google.android.gms/files/perfprofd.conf";

//
// This table describes the config file syntax in terms of key/value pairs.
// Values come in two flavors: strings, or unsigned integers. In the latter
// case the reader sets allowable minimum/maximum for the setting.
//
class ConfigReader {

 public:
  ConfigReader();
  ~ConfigReader();

  // Ask for the current setting of a config item
  unsigned getUnsignedValue(const char *key) const;
  std::string getStringValue(const char *key) const;

  // read the specified config file, applying any settings it contains
  void readFile(bool initial);

 private:
  void addUnsignedEntry(const char *key,
                        unsigned default_value,
                        unsigned min_value,
                        unsigned max_value);
  void addStringEntry(const char *key, const char *default_value);
  void addDefaultEntries();
  void parseLine(const char *key, const char *value, unsigned linecount);

  typedef struct { unsigned minv, maxv; } values;
  std::map<std::string, values> u_info;
  std::map<std::string, unsigned> u_entries;
  std::map<std::string, std::string> s_entries;
  bool trace_config_read;
};

ConfigReader::ConfigReader()
    : trace_config_read(false)
{
  addDefaultEntries();
}

ConfigReader::~ConfigReader()
{
}

//
// Populate the reader with the set of allowable entries
//
void ConfigReader::addDefaultEntries()
{
  // Average number of seconds between perf profile collections (if
  // set to 100, then over time we want to see a perf profile
  // collected every 100 seconds). The actual time within the interval
  // for the collection is chosen randomly.
  addUnsignedEntry("collection_interval", 14400, 100, UINT32_MAX);

  // Use the specified fixed seed for random number generation (unit
  // testing)
  addUnsignedEntry("use_fixed_seed", 0, 0, UINT32_MAX);

  // For testing purposes, number of times to iterate through main
  // loop.  Value of zero indicates that we should loop forever.
  addUnsignedEntry("main_loop_iterations", 0, 0, UINT32_MAX);

  // Destination directory (where to write profiles). This location
  // chosen since it is accessible to the uploader service.
  addStringEntry("destination_directory", "/data/misc/perfprofd");

  // Config directory (where to read configs).
  addStringEntry("config_directory", "/data/data/com.google.android.gms/files");

  // Full path to 'perf' executable.
  addStringEntry("perf_path", "/system/xbin/simpleperf");

  // Desired sampling period (passed to perf -c option). Small
  // sampling periods can perturb the collected profiles, so enforce
  // min/max.
  addUnsignedEntry("sampling_period", 500000, 5000, UINT32_MAX);

  // Length of time to collect samples (number of seconds for 'perf
  // record -a' run).
  addUnsignedEntry("sample_duration", 3, 2, 600);

  // If this parameter is non-zero it will cause perfprofd to
  // exit immediately if the build type is not userdebug or eng.
  // Currently defaults to 1 (true).
  addUnsignedEntry("only_debug_build", 1, 0, 1);

  // If the "mpdecision" service is running at the point we are ready
  // to kick off a profiling run, then temporarily disable the service
  // and hard-wire all cores on prior to the collection run, provided
  // that the duration of the recording is less than or equal to the value of
  // 'hardwire_cpus_max_duration'.
  addUnsignedEntry("hardwire_cpus", 1, 0, 1);
  addUnsignedEntry("hardwire_cpus_max_duration", 5, 1, UINT32_MAX);

  // Maximum number of unprocessed profiles we can accumulate in the
  // destination directory. Once we reach this limit, we continue
  // to collect, but we just overwrite the most recent profile.
  addUnsignedEntry("max_unprocessed_profiles", 10, 1, UINT32_MAX);

  // If set to 1, pass the -g option when invoking 'perf' (requests
  // stack traces as opposed to flat profile).
  addUnsignedEntry("stack_profile", 0, 0, 1);

  // For unit testing only: if set to 1, emit info messages on config
  // file parsing.
  addUnsignedEntry("trace_config_read", 0, 0, 1);
}

void ConfigReader::addUnsignedEntry(const char *key,
                                    unsigned default_value,
                                    unsigned min_value,
                                    unsigned max_value)
{
  std::string ks(key);
  if (u_entries.find(ks) != u_entries.end() ||
      s_entries.find(ks) != s_entries.end()) {
    W_ALOGE("internal error -- duplicate entry for key %s", key);
    exit(9);
  }
  values vals;
  vals.minv = min_value;
  vals.maxv = max_value;
  u_info[ks] = vals;
  u_entries[ks] = default_value;
}

void ConfigReader::addStringEntry(const char *key, const char *default_value)
{
  std::string ks(key);
  if (u_entries.find(ks) != u_entries.end() ||
      s_entries.find(ks) != s_entries.end()) {
    W_ALOGE("internal error -- duplicate entry for key %s", key);
    exit(9);
  }
  if (default_value == nullptr) {
    W_ALOGE("internal error -- bad default value for key %s", key);
    exit(9);
  }
  s_entries[ks] = std::string(default_value);
}

unsigned ConfigReader::getUnsignedValue(const char *key) const
{
  std::string ks(key);
  auto it = u_entries.find(ks);
  assert(it != u_entries.end());
  return it->second;
}

std::string ConfigReader::getStringValue(const char *key) const
{
  std::string ks(key);
  auto it = s_entries.find(ks);
  assert(it != s_entries.end());
  return it->second;
}

//
// Parse a key=value pair read from the config file. This will issue
// warnings or errors to the system logs if the line can't be
// interpreted properly.
//
void ConfigReader::parseLine(const char *key,
                             const char *value,
                             unsigned linecount)
{
  assert(key);
  assert(value);

  auto uit = u_entries.find(key);
  if (uit != u_entries.end()) {
    unsigned uvalue = 0;
    if (isdigit(value[0]) == 0 || sscanf(value, "%u", &uvalue) != 1) {
      W_ALOGW("line %d: malformed unsigned value (ignored)", linecount);
    } else {
      values vals;
      auto iit = u_info.find(key);
      assert(iit != u_info.end());
      vals = iit->second;
      if (uvalue < vals.minv || uvalue > vals.maxv) {
        W_ALOGW("line %d: specified value %u for '%s' "
                "outside permitted range [%u %u] (ignored)",
                linecount, uvalue, key, vals.minv, vals.maxv);
      } else {
        if (trace_config_read) {
          W_ALOGI("option %s set to %u", key, uvalue);
        }
        uit->second = uvalue;
      }
    }
    trace_config_read = (getUnsignedValue("trace_config_read") != 0);
    return;
  }

  auto sit = s_entries.find(key);
  if (sit != s_entries.end()) {
    if (trace_config_read) {
      W_ALOGI("option %s set to %s", key, value);
    }
    sit->second = std::string(value);
    return;
  }

  W_ALOGW("line %d: unknown option '%s' ignored", linecount, key);
}

static bool isblank(const std::string &line)
{
  for (std::string::const_iterator it = line.begin(); it != line.end(); ++it)
  {
    if (isspace(*it) == 0) {
      return false;
    }
  }
  return true;
}

void ConfigReader::readFile(bool initial)
{
  FILE *fp = fopen(config_file_path, "r");
  if (!fp) {
    if (initial) {
      W_ALOGE("unable to open configuration file %s", config_file_path);
    }
    return;
  }

  char *linebuf = NULL;
  size_t line_length = 0;
  for (unsigned linecount = 1;
       getline(&linebuf, &line_length, fp) != -1;
       ++linecount) {
    char *eq = 0;
    char *key, *value;

    // comment line?
    if (linebuf[0] == '#') {
      continue;
    }

    // blank line?
    if (isblank(linebuf)) {
      continue;
    }

    // look for X=Y assignment
    eq = strchr(linebuf, '=');
    if (!eq) {
      W_ALOGW("line %d: line malformed (no '=' found)", linecount);
      continue;
    }

    *eq = '\0';
    key = linebuf;
    value = eq+1;
    char *ln = strrchr(value, '\n');
    if (ln) { *ln = '\0'; }

    parseLine(key, value, linecount);
  }
  free(linebuf);
  fclose(fp);
}

//
// Parse command line args. Currently you can supply "-c P" to set
// the path of the config file to P.
//
static void parse_args(int argc, char** argv)
{
  int ac;

  for (ac = 1; ac < argc; ++ac) {
    if (!strcmp(argv[ac], "-c")) {
      if (ac >= argc-1) {
        W_ALOGE("malformed command line: -c option requires argument)");
        continue;
      }
      config_file_path = strdup(argv[ac+1]);
      W_ALOGI("config file path set to %s", config_file_path);
      ++ac;
    } else {
      W_ALOGE("malformed command line: unknown option or arg %s)", argv[ac]);
      continue;
    }
  }
}

//
// Convert a CKPROFILE_RESULT to a string
//
const char *ckprofile_result_to_string(CKPROFILE_RESULT result)
{
  switch (result) {
    case DO_COLLECT_PROFILE:
      return "DO_COLLECT_PROFILE";
    case DONT_PROFILE_MISSING_CONFIG_DIR:
      return "missing config directory";
    case DONT_PROFILE_MISSING_SEMAPHORE:
      return "missing semaphore file";
    case DONT_PROFILE_MISSING_PERF_EXECUTABLE:
      return "missing 'perf' executable";
    case DONT_PROFILE_RUNNING_IN_EMULATOR:
      return "running in emulator";
    default: return "unknown";
  }
  return "notreached";
}

//
// Convert a PROFILE_RESULT to a string
//
const char *profile_result_to_string(PROFILE_RESULT result)
{
  switch(result) {
    case OK_PROFILE_COLLECTION:
      return "profile collection succeeded";
    case ERR_FORK_FAILED:
      return "fork() system call failed";
    case ERR_PERF_RECORD_FAILED:
      return "perf record returned bad exit status";
    case ERR_PERF_ENCODE_FAILED:
      return "failure encoding perf.data to protobuf";
    case ERR_OPEN_ENCODED_FILE_FAILED:
      return "failed to open encoded perf file";
    case ERR_WRITE_ENCODED_FILE_FAILED:
      return "write to encoded perf file failed";
    default: return "unknown";
  }
  return "notreached";
}

//
// Check to see whether we should perform a profile collection
//
static CKPROFILE_RESULT check_profiling_enabled(ConfigReader &config)
{
  //
  // Profile collection in the emulator doesn't make sense
  //
  assert(running_in_emulator != -1);
  if (running_in_emulator) {
    return DONT_PROFILE_RUNNING_IN_EMULATOR;
  }

  //
  // Check for existence of semaphore file in config directory
  //
  if (access(config.getStringValue("config_directory").c_str(), F_OK) == -1) {
    W_ALOGW("unable to open config directory %s: (%s)",
            config.getStringValue("config_directory").c_str(), strerror(errno));
    return DONT_PROFILE_MISSING_CONFIG_DIR;
  }


  // Check for existence of semaphore file
  std::string semaphore_filepath = config.getStringValue("config_directory")
                                   + "/" + SEMAPHORE_FILENAME;
  if (access(semaphore_filepath.c_str(), F_OK) == -1) {
    return DONT_PROFILE_MISSING_SEMAPHORE;
  }

  // Check for existence of simpleperf/perf executable
  std::string pp = config.getStringValue("perf_path");
  if (access(pp.c_str(), R_OK|X_OK) == -1) {
    W_ALOGW("unable to access/execute %s", pp.c_str());
    return DONT_PROFILE_MISSING_PERF_EXECUTABLE;
  }

  //
  // We are good to go
  //
  return DO_COLLECT_PROFILE;
}

static void annotate_encoded_perf_profile(wireless_android_play_playlog::AndroidPerfProfile *profile)
{
  //
  // Load average as reported by the kernel
  //
  std::string load;
  double fload = 0.0;
  if (android::base::ReadFileToString("/proc/loadavg", &load) &&
      sscanf(load.c_str(), "%lf", &fload) == 1) {
    int iload = static_cast<int>(fload * 100.0);
    profile->set_sys_load_average(iload);
  } else {
    W_ALOGE("Failed to read or scan /proc/loadavg (%s)", strerror(errno));
  }

  //
  // Examine the contents of wake_unlock to determine whether the
  // device display is on or off. NB: is this really the only way to
  // determine this info?
  //
  std::string disp;
  if (android::base::ReadFileToString("/sys/power/wake_unlock", &disp)) {
    bool ison = (strstr(disp.c_str(), "PowerManagerService.Display") == 0);
    profile->set_display_on(ison);
  } else {
    W_ALOGE("Failed to read /sys/power/wake_unlock (%s)", strerror(errno));
  }
}

inline char* string_as_array(std::string* str) {
  return str->empty() ? NULL : &*str->begin();
}

PROFILE_RESULT encode_to_proto(const std::string &data_file_path,
                               const char *encoded_file_path)
{
  //
  // Open and read perf.data file
  //
  const wireless_android_play_playlog::AndroidPerfProfile &encodedProfile =
      wireless_android_logging_awp::RawPerfDataToAndroidPerfProfile(data_file_path);

  //
  // Issue error if no samples
  //
  if (encodedProfile.programs().size() == 0) {
    return ERR_PERF_ENCODE_FAILED;
  }

  // All of the info in 'encodedProfile' is derived from the perf.data file;
  // here we tack display status and system load.
  wireless_android_play_playlog::AndroidPerfProfile &prof =
      const_cast<wireless_android_play_playlog::AndroidPerfProfile&>
      (encodedProfile);
  annotate_encoded_perf_profile(&prof);

  //
  // Serialize protobuf to array
  //
  int size = encodedProfile.ByteSize();
  std::string data;
  data.resize(size);
  ::google::protobuf::uint8* dtarget =
        reinterpret_cast<::google::protobuf::uint8*>(string_as_array(&data));
  encodedProfile.SerializeWithCachedSizesToArray(dtarget);

  //
  // Open file and write encoded data to it
  //
  FILE *fp = fopen(encoded_file_path, "w");
  if (!fp) {
    return ERR_OPEN_ENCODED_FILE_FAILED;
  }
  size_t fsiz = size;
  if (fwrite(dtarget, fsiz, 1, fp) != 1) {
    fclose(fp);
    return ERR_WRITE_ENCODED_FILE_FAILED;
  }
  fclose(fp);
  chmod(encoded_file_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

  return OK_PROFILE_COLLECTION;
}

//
// Invoke "perf record". Return value is OK_PROFILE_COLLECTION for
// success, or some other error code if something went wrong.
//
static PROFILE_RESULT invoke_perf(const std::string &perf_path,
                                  unsigned sampling_period,
                                  const char *stack_profile_opt,
                                  unsigned duration,
                                  const std::string &data_file_path,
                                  const std::string &perf_stderr_path)
{
  pid_t pid = fork();

  if (pid == -1) {
    return ERR_FORK_FAILED;
  }

  if (pid == 0) {
    // child

    // Open file to receive stderr/stdout from perf
    FILE *efp = fopen(perf_stderr_path.c_str(), "w");
    if (efp) {
      dup2(fileno(efp), STDERR_FILENO);
      dup2(fileno(efp), STDOUT_FILENO);
    } else {
      W_ALOGW("unable to open %s for writing", perf_stderr_path.c_str());
    }

    // marshall arguments
    constexpr unsigned max_args = 12;
    const char *argv[max_args];
    unsigned slot = 0;
    argv[slot++] = perf_path.c_str();
    argv[slot++] = "record";

    // -o perf.data
    argv[slot++] = "-o";
    argv[slot++] = data_file_path.c_str();

    // -c N
    argv[slot++] = "-c";
    std::string p_str = android::base::StringPrintf("%u", sampling_period);
    argv[slot++] = p_str.c_str();

    // -g if desired
    if (stack_profile_opt)
      argv[slot++] = stack_profile_opt;

    // system wide profiling
    argv[slot++] = "-a";

    // sleep <duration>
    argv[slot++] = "/system/bin/sleep";
    std::string d_str = android::base::StringPrintf("%u", duration);
    argv[slot++] = d_str.c_str();

    // terminator
    argv[slot++] = nullptr;
    assert(slot < max_args);

    // record the final command line in the error output file for
    // posterity/debugging purposes
    fprintf(stderr, "perf invocation (pid=%d):\n", getpid());
    for (unsigned i = 0; argv[i] != nullptr; ++i) {
      fprintf(stderr, "%s%s", i ? " " : "", argv[i]);
    }
    fprintf(stderr, "\n");

    // exec
    execvp(argv[0], (char * const *)argv);
    fprintf(stderr, "exec failed: %s\n", strerror(errno));
    exit(1);

  } else {
    // parent
    int st = 0;
    pid_t reaped = TEMP_FAILURE_RETRY(waitpid(pid, &st, 0));

    if (reaped == -1) {
      W_ALOGW("waitpid failed: %s", strerror(errno));
    } else if (WIFSIGNALED(st)) {
      W_ALOGW("perf killed by signal %d", WTERMSIG(st));
    } else if (WEXITSTATUS(st) != 0) {
      W_ALOGW("perf bad exit status %d", WEXITSTATUS(st));
    } else {
      return OK_PROFILE_COLLECTION;
    }
  }

  return ERR_PERF_RECORD_FAILED;
}

//
// Remove all files in the destination directory during initialization
//
static void cleanup_destination_dir(const ConfigReader &config)
{
  std::string dest_dir = config.getStringValue("destination_directory");
  DIR* dir = opendir(dest_dir.c_str());
  if (dir != NULL) {
    struct dirent* e;
    while ((e = readdir(dir)) != 0) {
      if (e->d_name[0] != '.') {
        std::string file_path = dest_dir + "/" + e->d_name;
        remove(file_path.c_str());
      }
    }
    closedir(dir);
  } else {
    W_ALOGW("unable to open destination dir %s for cleanup",
            dest_dir.c_str());
  }
}

//
// Post-processes after profile is collected and converted to protobuf.
// * GMS core stores processed file sequence numbers in
//   /data/data/com.google.android.gms/files/perfprofd_processed.txt
// * Update /data/misc/perfprofd/perfprofd_produced.txt to remove the sequence
//   numbers that have been processed and append the current seq number
// Returns true if the current_seq should increment.
//
static bool post_process(const ConfigReader &config, int current_seq)
{
  std::string dest_dir = config.getStringValue("destination_directory");
  std::string processed_file_path =
      config.getStringValue("config_directory") + "/" + PROCESSED_FILENAME;
  std::string produced_file_path = dest_dir + "/" + PRODUCED_FILENAME;


  std::set<int> processed;
  FILE *fp = fopen(processed_file_path.c_str(), "r");
  if (fp != NULL) {
    int seq;
    while(fscanf(fp, "%d\n", &seq) > 0) {
      if (remove(android::base::StringPrintf(
          "%s/perf.data.encoded.%d", dest_dir.c_str(),seq).c_str()) == 0) {
        processed.insert(seq);
      }
    }
    fclose(fp);
  }

  std::set<int> produced;
  fp = fopen(produced_file_path.c_str(), "r");
  if (fp != NULL) {
    int seq;
    while(fscanf(fp, "%d\n", &seq) > 0) {
      if (processed.find(seq) == processed.end()) {
        produced.insert(seq);
      }
    }
    fclose(fp);
  }

  unsigned maxLive = config.getUnsignedValue("max_unprocessed_profiles");
  if (produced.size() >= maxLive) {
    return false;
  }

  produced.insert(current_seq);
  fp = fopen(produced_file_path.c_str(), "w");
  if (fp == NULL) {
    W_ALOGW("Cannot write %s", produced_file_path.c_str());
    return false;
  }
  for (std::set<int>::const_iterator iter = produced.begin();
       iter != produced.end(); ++iter) {
    fprintf(fp, "%d\n", *iter);
  }
  fclose(fp);
  chmod(produced_file_path.c_str(),
        S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
  return true;
}

//
// Collect a perf profile. Steps for this operation are:
// - kick off 'perf record'
// - read perf.data, convert to protocol buf
//
static PROFILE_RESULT collect_profile(const ConfigReader &config, int seq)
{
  //
  // Form perf.data file name, perf error output file name
  //
  std::string destdir = config.getStringValue("destination_directory");
  std::string data_file_path(destdir);
  data_file_path += "/";
  data_file_path += PERF_OUTPUT;
  std::string perf_stderr_path(destdir);
  perf_stderr_path += "/perferr.txt";

  //
  // Remove any existing perf.data file -- if we don't do this, perf
  // will rename the old file and we'll have extra cruft lying around.
  //
  struct stat statb;
  if (stat(data_file_path.c_str(), &statb) == 0) { // if file exists...
    if (unlink(data_file_path.c_str())) {          // then try to remove
      W_ALOGW("unable to unlink previous perf.data file");
    }
  }

  //
  // The "mpdecision" daemon can cause problems for profile
  // collection: if it decides to online a CPU partway through the
  // 'perf record' run, the activity on that CPU will be invisible to
  // perf, and if it offlines a CPU during the recording this can
  // sometimes leave the PMU in an unusable state (dmesg errors of the
  // form "perfevents: unable to request IRQXXX for ...").  To avoid
  // these issues, if "mpdecision" is running the helper below will
  // stop the service and then online all available CPUs. The object
  // destructor (invoked when this routine terminates) will then
  // restart the service again when needed.
  //
  unsigned duration = config.getUnsignedValue("sample_duration");
  unsigned hardwire = config.getUnsignedValue("hardwire_cpus");
  unsigned max_duration = config.getUnsignedValue("hardwire_cpus_max_duration");
  bool take_action = (hardwire && duration <= max_duration);
  HardwireCpuHelper helper(take_action);

  //
  // Invoke perf
  //
  const char *stack_profile_opt =
      (config.getUnsignedValue("stack_profile") != 0 ? "-g" : nullptr);
  std::string perf_path = config.getStringValue("perf_path");
  unsigned period = config.getUnsignedValue("sampling_period");

  PROFILE_RESULT ret = invoke_perf(perf_path.c_str(),
                                  period,
                                  stack_profile_opt,
                                  duration,
                                  data_file_path,
                                  perf_stderr_path);
  if (ret != OK_PROFILE_COLLECTION) {
    return ret;
  }

  //
  // Read the resulting perf.data file, encode into protocol buffer, then write
  // the result to the file perf.data.encoded
  //
  std::string path = android::base::StringPrintf(
      "%s.encoded.%d", data_file_path.c_str(), seq);
  return encode_to_proto(data_file_path, path.c_str());
}

//
// SIGHUP handler. Sending SIGHUP to the daemon can be used to break it
// out of a sleep() call so as to trigger a new collection (debugging)
//
static void sig_hup(int /* signum */)
{
}

//
// Assuming that we want to collect a profile every N seconds,
// randomly partition N into two sub-intervals.
//
static void determine_before_after(unsigned &sleep_before_collect,
                                   unsigned &sleep_after_collect,
                                   unsigned collection_interval)
{
  double frac = erand48(random_seed);
  sleep_before_collect = (unsigned) (((double)collection_interval) * frac);
  assert(sleep_before_collect <= collection_interval);
  sleep_after_collect = collection_interval - sleep_before_collect;
}

//
// Set random number generator seed
//
static void set_seed(ConfigReader &config)
{
  unsigned seed = 0;
  unsigned use_fixed_seed = config.getUnsignedValue("use_fixed_seed");
  if (use_fixed_seed) {
    //
    // Use fixed user-specified seed
    //
    seed = use_fixed_seed;
  } else {
    //
    // Randomized seed
    //
    seed = arc4random();
  }
  W_ALOGI("random seed set to %u", seed);
  // Distribute the 32-bit seed into the three 16-bit array
  // elements. The specific values being written do not especially
  // matter as long as we are setting them to something based on the seed.
  random_seed[0] = seed & 0xffff;
  random_seed[1] = (seed >> 16);
  random_seed[2] = (random_seed[0] ^ random_seed[1]);
}

//
// Initialization
//
static void init(ConfigReader &config)
{
  config.readFile(true);
  set_seed(config);
  cleanup_destination_dir(config);

  char propBuf[PROPERTY_VALUE_MAX];
  propBuf[0] = '\0';
  property_get("ro.kernel.qemu", propBuf, "");
  running_in_emulator = (propBuf[0] == '1');
  property_get("ro.debuggable", propBuf, "");
  is_debug_build = (propBuf[0] == '1');

  signal(SIGHUP, sig_hup);
}

//
// Main routine:
// 1. parse cmd line args
// 2. read config file
// 3. loop: {
//       sleep for a while
//       perform a profile collection
//    }
//
int perfprofd_main(int argc, char** argv)
{
  ConfigReader config;

  W_ALOGI("starting Android Wide Profiling daemon");

  parse_args(argc, argv);
  init(config);

  // Early exit if we're not supposed to run on this build flavor
  if (is_debug_build != 1 &&
      config.getUnsignedValue("only_debug_build") == 1) {
    W_ALOGI("early exit due to inappropriate build type");
    return 0;
  }

  unsigned iterations = 0;
  int seq = 0;
  while(config.getUnsignedValue("main_loop_iterations") == 0 ||
        iterations < config.getUnsignedValue("main_loop_iterations")) {

    // Figure out where in the collection interval we're going to actually
    // run perf
    unsigned sleep_before_collect = 0;
    unsigned sleep_after_collect = 0;
    determine_before_after(sleep_before_collect, sleep_after_collect,
                           config.getUnsignedValue("collection_interval"));
    perfprofd_sleep(sleep_before_collect);

    // Reread config file -- the uploader may have rewritten it as a result
    // of a gservices change
    config.readFile(false);

    // Check for profiling enabled...
    CKPROFILE_RESULT ckresult = check_profiling_enabled(config);
    if (ckresult != DO_COLLECT_PROFILE) {
      W_ALOGI("profile collection skipped (%s)",
              ckprofile_result_to_string(ckresult));
    } else {
      // Kick off the profiling run...
      W_ALOGI("initiating profile collection");
      PROFILE_RESULT result = collect_profile(config, seq);
      if (result != OK_PROFILE_COLLECTION) {
        W_ALOGI("profile collection failed (%s)",
                profile_result_to_string(result));
      } else {
        if (post_process(config, seq)) {
          seq++;
        }
        W_ALOGI("profile collection complete");
      }
    }
    perfprofd_sleep(sleep_after_collect);
    iterations += 1;
  }

  W_ALOGI("finishing Android Wide Profiling daemon");
  return 0;
}
