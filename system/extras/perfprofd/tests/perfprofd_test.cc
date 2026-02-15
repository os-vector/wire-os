/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <cctype>
#include <string>
#include <regex>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <base/stringprintf.h>

#include "perfprofdcore.h"
#include "perfprofdutils.h"
#include "perfprofdmockutils.h"

#include "perf_profile.pb.h"
#include "google/protobuf/text_format.h"

//
// Set to argv[0] on startup
//
static const char *executable_path;

//
// test_dir is the directory containing the test executable and
// any files associated with the test (will be created by the harness).
//
// dest_dir is a subdirectory of test_dir that we'll create on the fly
// at the start of each testpoint (into which new files can be written),
// then delete at end of testpoint.
//
static std::string test_dir;
static std::string dest_dir;

// Path to perf executable on device
#define PERFPATH "/system/bin/perf"

// Temporary config file that we will emit for the daemon to read
#define CONFIGFILE "perfprofd.conf"

static std::string encoded_file_path(int seq)
{
  return android::base::StringPrintf("%s/perf.data.encoded.%d",
                                     dest_dir.c_str(), seq);
}

class PerfProfdTest : public testing::Test {
 protected:
  virtual void SetUp() {
    mock_perfprofdutils_init();
    create_dest_dir();
    yesclean();
  }

  virtual void TearDown() {
    mock_perfprofdutils_finish();
  }

  void noclean() {
    clean_ = false;
  }
  void yesclean() {
    clean_ = true;
  }

 private:
  bool clean_;

  void create_dest_dir() {
    setup_dirs();
    ASSERT_FALSE(dest_dir == "");
    if (clean_) {
      std::string cmd("rm -rf ");
      cmd += dest_dir;
      system(cmd.c_str());
    }
    std::string cmd("mkdir -p ");
    cmd += dest_dir;
    system(cmd.c_str());
  }

  void setup_dirs()
  {
    if (test_dir == "") {
      ASSERT_TRUE(executable_path != nullptr);
      std::string s(executable_path);
      auto found = s.find_last_of("/");
      test_dir = s.substr(0,found);
      dest_dir = test_dir;
      dest_dir += "/tmp";
    }
  }

};

static bool bothWhiteSpace(char lhs, char rhs)
{
  return (std::isspace(lhs) && std::isspace(rhs));
}

//
// Squeeze out repeated whitespace from expected/actual logs.
//
static std::string squeezeWhite(const std::string &str,
                                const char *tag,
                                bool dump=false)
{
  if (dump) { fprintf(stderr, "raw %s is %s\n", tag, str.c_str()); }
  std::string result(str);
  std::replace( result.begin(), result.end(), '\n', ' ');
  auto new_end = std::unique(result.begin(), result.end(), bothWhiteSpace);
  result.erase(new_end, result.end());
  while (result.begin() != result.end() && std::isspace(*result.rbegin())) {
    result.pop_back();
  }
  if (dump) { fprintf(stderr, "squeezed %s is %s\n", tag, result.c_str()); }
  return result;
}

///
/// Helper class to kick off a run of the perfprofd daemon with a specific
/// config file.
///
class PerfProfdRunner {
 public:
  PerfProfdRunner()
      : config_path_(test_dir)
  {
    config_path_ += "/" CONFIGFILE;
  }

  ~PerfProfdRunner()
  {
    remove_processed_file();
  }

  void addToConfig(const std::string &line)
  {
    config_text_ += line;
    config_text_ += "\n";
  }

  void remove_semaphore_file()
  {
    std::string semaphore(test_dir);
    semaphore += "/" SEMAPHORE_FILENAME;
    unlink(semaphore.c_str());
  }

  void create_semaphore_file()
  {
    std::string semaphore(test_dir);
    semaphore += "/" SEMAPHORE_FILENAME;
    close(open(semaphore.c_str(), O_WRONLY|O_CREAT));
  }

  void write_processed_file(int start_seq, int end_seq)
  {
    std::string processed = test_dir + "/" PROCESSED_FILENAME;
    FILE *fp = fopen(processed.c_str(), "w");
    for (int i = start_seq; i < end_seq; i++) {
      fprintf(fp, "%d\n", i);
    }
    fclose(fp);
  }

  void remove_processed_file()
  {
    std::string processed = test_dir + "/" PROCESSED_FILENAME;
    unlink(processed.c_str());
  }

  int invoke()
  {
    static const char *argv[3] = { "perfprofd", "-c", "" };
    argv[2] = config_path_.c_str();

    writeConfigFile(config_path_, config_text_);

    // execute daemon main
    return perfprofd_main(3, (char **) argv);
  }

 private:
  std::string config_path_;
  std::string config_text_;

  void writeConfigFile(const std::string &config_path,
                       const std::string &config_text)
  {
    FILE *fp = fopen(config_path.c_str(), "w");
    ASSERT_TRUE(fp != nullptr);
    fprintf(fp, "%s\n", config_text.c_str());
    fclose(fp);
  }
};

//......................................................................

static void readEncodedProfile(const char *testpoint,
                               wireless_android_play_playlog::AndroidPerfProfile &encodedProfile)
{
  struct stat statb;
  int perf_data_stat_result = stat(encoded_file_path(0).c_str(), &statb);
  ASSERT_NE(-1, perf_data_stat_result);

  // read
  std::string encoded;
  encoded.resize(statb.st_size);
  FILE *ifp = fopen(encoded_file_path(0).c_str(), "r");
  ASSERT_NE(nullptr, ifp);
  size_t items_read = fread((void*) encoded.data(), statb.st_size, 1, ifp);
  ASSERT_EQ(1, items_read);
  fclose(ifp);

  // decode
  encodedProfile.ParseFromString(encoded);
}

static std::string encodedLoadModuleToString(const wireless_android_play_playlog::LoadModule &lm)
{
  std::stringstream ss;
  ss << "name: \"" << lm.name() << "\"\n";
  if (lm.build_id() != "") {
    ss << "build_id: \"" << lm.build_id() << "\"\n";
  }
  return ss.str();
}

static std::string encodedModuleSamplesToString(const wireless_android_play_playlog::LoadModuleSamples &mod)
{
  std::stringstream ss;

  ss << "load_module_id: " << mod.load_module_id() << "\n";
  for (size_t k = 0; k < mod.address_samples_size(); k++) {
    const auto &sample = mod.address_samples(k);
    ss << "  address_samples {\n";
    for (size_t l = 0; l < mod.address_samples(k).address_size();
         l++) {
      auto address = mod.address_samples(k).address(l);
      ss << "    address: " << address << "\n";
    }
    ss << "    count: " << sample.count() << "\n";
    ss << "  }\n";
  }
  return ss.str();
}

#define RAW_RESULT(x) #x

//
// Check to see if the log messages emitted by the daemon
// match the expected result. By default we use a partial
// match, e.g. if we see the expected excerpt anywhere in the
// result, it's a match (for exact match, set exact to true)
//
static void compareLogMessages(const std::string &actual,
                               const std::string &expected,
                               const char *testpoint,
                               bool exactMatch=false)
{
   std::string sqexp = squeezeWhite(expected, "expected");
   std::string sqact = squeezeWhite(actual, "actual");
   if (exactMatch) {
     EXPECT_STREQ(sqexp.c_str(), sqact.c_str());
   } else {
     std::size_t foundpos = sqact.find(sqexp);
     bool wasFound = true;
     if (foundpos == std::string::npos) {
       std::cerr << testpoint << ": expected result not found\n";
       std::cerr << " Actual: \"" << sqact << "\"\n";
       std::cerr << " Expected: \"" << sqexp << "\"\n";
       wasFound = false;
     }
     EXPECT_TRUE(wasFound);
   }
}

TEST_F(PerfProfdTest, MissingGMS)
{
  //
  // AWP requires cooperation between the daemon and the GMS core
  // piece. If we're running on a device that has an old or damaged
  // version of GMS core, then the config directory we're interested in
  // may not be there. This test insures that the daemon does the
  // right thing in this case.
  //
  PerfProfdRunner runner;
  runner.addToConfig("only_debug_build=0");
  runner.addToConfig("trace_config_read=0");
  runner.addToConfig("config_directory=/does/not/exist");
  runner.addToConfig("main_loop_iterations=1");
  runner.addToConfig("use_fixed_seed=1");
  runner.addToConfig("collection_interval=100");

  // Kick off daemon
  int daemon_main_return_code = runner.invoke();

  // Check return code from daemon
  EXPECT_EQ(0, daemon_main_return_code);

  // Verify log contents
  const std::string expected = RAW_RESULT(
      I: sleep 90 seconds
      W: unable to open config directory /does/not/exist: (No such file or directory)
      I: profile collection skipped (missing config directory)
                                          );

  // check to make sure entire log matches
  compareLogMessages(mock_perfprofdutils_getlogged(),
                     expected, "MissingGMS");
}


TEST_F(PerfProfdTest, MissingOptInSemaphoreFile)
{
  //
  // Android device owners must opt in to "collect and report usage
  // data" in order for us to be able to collect profiles. The opt-in
  // check is performed in the GMS core component; if the check
  // passes, then it creates a semaphore file for the daemon to pick
  // up on.
  //
  PerfProfdRunner runner;
  runner.addToConfig("only_debug_build=0");
  std::string cfparam("config_directory="); cfparam += test_dir;
  runner.addToConfig(cfparam);
  std::string ddparam("destination_directory="); ddparam += dest_dir;
  runner.addToConfig(ddparam);
  runner.addToConfig("main_loop_iterations=1");
  runner.addToConfig("use_fixed_seed=1");
  runner.addToConfig("collection_interval=100");

  runner.remove_semaphore_file();

  // Kick off daemon
  int daemon_main_return_code = runner.invoke();

  // Check return code from daemon
  EXPECT_EQ(0, daemon_main_return_code);

  // Verify log contents
  const std::string expected = RAW_RESULT(
      I: profile collection skipped (missing semaphore file)
                                          );
  // check to make sure log excerpt matches
  compareLogMessages(mock_perfprofdutils_getlogged(),
                     expected, "MissingOptInSemaphoreFile");
}

TEST_F(PerfProfdTest, MissingPerfExecutable)
{
  //
  // Perfprofd uses the 'simpleperf' tool to collect profiles
  // (although this may conceivably change in the future). This test
  // checks to make sure that if 'simpleperf' is not present we bail out
  // from collecting profiles.
  //
  PerfProfdRunner runner;
  runner.addToConfig("only_debug_build=0");
  runner.addToConfig("trace_config_read=1");
  std::string cfparam("config_directory="); cfparam += test_dir;
  runner.addToConfig(cfparam);
  std::string ddparam("destination_directory="); ddparam += dest_dir;
  runner.addToConfig(ddparam);
  runner.addToConfig("main_loop_iterations=1");
  runner.addToConfig("use_fixed_seed=1");
  runner.addToConfig("collection_interval=100");
  runner.addToConfig("perf_path=/does/not/exist");

  // Create semaphore file
  runner.create_semaphore_file();

  // Kick off daemon
  int daemon_main_return_code = runner.invoke();

  // Check return code from daemon
  EXPECT_EQ(0, daemon_main_return_code);

  // expected log contents
  const std::string expected = RAW_RESULT(
      I: profile collection skipped (missing 'perf' executable)
                                          );
  // check to make sure log excerpt matches
  compareLogMessages(mock_perfprofdutils_getlogged(),
                     expected, "MissingPerfExecutable");
}

TEST_F(PerfProfdTest, BadPerfRun)
{
  //
  // Perf tools tend to be tightly coupled with a specific kernel
  // version -- if things are out of sync perf could fail or
  // crash. This test makes sure that we detect such a case and log
  // the error.
  //
  PerfProfdRunner runner;
  runner.addToConfig("only_debug_build=0");
  std::string cfparam("config_directory="); cfparam += test_dir;
  runner.addToConfig(cfparam);
  std::string ddparam("destination_directory="); ddparam += dest_dir;
  runner.addToConfig(ddparam);
  runner.addToConfig("main_loop_iterations=1");
  runner.addToConfig("use_fixed_seed=1");
  runner.addToConfig("collection_interval=100");
  runner.addToConfig("perf_path=/system/bin/false");

  // Create semaphore file
  runner.create_semaphore_file();

  // Kick off daemon
  int daemon_main_return_code = runner.invoke();

  // Check return code from daemon
  EXPECT_EQ(0, daemon_main_return_code);

  // Verify log contents
  const std::string expected = RAW_RESULT(
      I: profile collection failed (perf record returned bad exit status)
                                          );

  // check to make sure log excerpt matches
  compareLogMessages(mock_perfprofdutils_getlogged(),
                     expected, "BadPerfRun");
}

TEST_F(PerfProfdTest, ConfigFileParsing)
{
  //
  // Gracefully handly malformed items in the config file
  //
  PerfProfdRunner runner;
  runner.addToConfig("only_debug_build=0");
  runner.addToConfig("main_loop_iterations=1");
  runner.addToConfig("collection_interval=100");
  runner.addToConfig("use_fixed_seed=1");
  runner.addToConfig("destination_directory=/does/not/exist");

  // assorted bad syntax
  runner.addToConfig("collection_interval=0");
  runner.addToConfig("collection_interval=-1");
  runner.addToConfig("collection_interval=2");
  runner.addToConfig("nonexistent_key=something");
  runner.addToConfig("no_equals_stmt");

  // Kick off daemon
  int daemon_main_return_code = runner.invoke();

  // Check return code from daemon
  EXPECT_EQ(0, daemon_main_return_code);

  // Verify log contents
  const std::string expected = RAW_RESULT(
      W: line 6: specified value 0 for 'collection_interval' outside permitted range [100 4294967295] (ignored)
      W: line 7: malformed unsigned value (ignored)
      W: line 8: specified value 2 for 'collection_interval' outside permitted range [100 4294967295] (ignored)
      W: line 9: unknown option 'nonexistent_key' ignored
      W: line 10: line malformed (no '=' found)
                                          );

  // check to make sure log excerpt matches
  compareLogMessages(mock_perfprofdutils_getlogged(),
                     expected, "ConfigFileParsing");
}

TEST_F(PerfProfdTest, BasicRunWithCannedPerf)
{
  //
  // Verify the portion of the daemon that reads and encodes
  // perf.data files. Here we run the encoder on a canned perf.data
  // file and verify that the resulting protobuf contains what
  // we think it should contain.
  //
  std::string input_perf_data(test_dir);
  input_perf_data += "/canned.perf.data";

  // Kick off encoder and check return code
  PROFILE_RESULT result =
      encode_to_proto(input_perf_data, encoded_file_path(0).c_str());
  EXPECT_EQ(OK_PROFILE_COLLECTION, result);

  // Read and decode the resulting perf.data.encoded file
  wireless_android_play_playlog::AndroidPerfProfile encodedProfile;
  readEncodedProfile("BasicRunWithCannedPerf",
                     encodedProfile);

  // Expect 29 load modules
  EXPECT_EQ(29, encodedProfile.programs_size());

  // Check a couple of load modules
  { const auto &lm0 = encodedProfile.load_modules(0);
    std::string act_lm0 = encodedLoadModuleToString(lm0);
    std::string sqact0 = squeezeWhite(act_lm0, "actual for lm 0");
    const std::string expected_lm0 = RAW_RESULT(
        name: "/data/app/com.google.android.apps.plus-1/lib/arm/libcronet.so"
                                                );
    std::string sqexp0 = squeezeWhite(expected_lm0, "expected_lm0");
    EXPECT_STREQ(sqexp0.c_str(), sqact0.c_str());
  }
  { const auto &lm9 = encodedProfile.load_modules(9);
    std::string act_lm9 = encodedLoadModuleToString(lm9);
    std::string sqact9 = squeezeWhite(act_lm9, "actual for lm 9");
    const std::string expected_lm9 = RAW_RESULT(
        name: "/system/lib/libandroid_runtime.so" build_id: "8164ed7b3a8b8f5a220d027788922510"
                                                );
    std::string sqexp9 = squeezeWhite(expected_lm9, "expected_lm9");
    EXPECT_STREQ(sqexp9.c_str(), sqact9.c_str());
  }

  // Examine some of the samples now
  { const auto &p1 = encodedProfile.programs(0);
    const auto &lm1 = p1.modules(0);
    std::string act_lm1 = encodedModuleSamplesToString(lm1);
    std::string sqact1 = squeezeWhite(act_lm1, "actual for lm1");
    const std::string expected_lm1 = RAW_RESULT(
        load_module_id: 9 address_samples { address: 296100 count: 1 }
                                                );
    std::string sqexp1 = squeezeWhite(expected_lm1, "expected_lm1");
    EXPECT_STREQ(sqexp1.c_str(), sqact1.c_str());
  }
  { const auto &p1 = encodedProfile.programs(2);
    const auto &lm2 = p1.modules(0);
    std::string act_lm2 = encodedModuleSamplesToString(lm2);
    std::string sqact2 = squeezeWhite(act_lm2, "actual for lm2");
    const std::string expected_lm2 = RAW_RESULT(
        load_module_id: 2
        address_samples { address: 28030244 count: 1 }
        address_samples { address: 29657840 count: 1 }
                                                );
    std::string sqexp2 = squeezeWhite(expected_lm2, "expected_lm2");
    EXPECT_STREQ(sqexp2.c_str(), sqact2.c_str());
  }
}

TEST_F(PerfProfdTest, BasicRunWithLivePerf)
{
  //
  // Basic test to exercise the main loop of the daemon. It includes
  // a live 'perf' run
  //
  PerfProfdRunner runner;
  runner.addToConfig("only_debug_build=0");
  std::string ddparam("destination_directory="); ddparam += dest_dir;
  runner.addToConfig(ddparam);
  std::string cfparam("config_directory="); cfparam += test_dir;
  runner.addToConfig(cfparam);
  runner.addToConfig("main_loop_iterations=1");
  runner.addToConfig("use_fixed_seed=12345678");
  runner.addToConfig("max_unprocessed_profiles=100");
  runner.addToConfig("collection_interval=9999");
  runner.addToConfig("sample_duration=2");

  // Create semaphore file
  runner.create_semaphore_file();

  // Kick off daemon
  int daemon_main_return_code = runner.invoke();

  // Check return code from daemon
  EXPECT_EQ(0, daemon_main_return_code);

  // Read and decode the resulting perf.data.encoded file
  wireless_android_play_playlog::AndroidPerfProfile encodedProfile;
  readEncodedProfile("BasicRunWithLivePerf", encodedProfile);

  // Examine what we get back. Since it's a live profile, we can't
  // really do much in terms of verifying the contents.
  EXPECT_LT(0, encodedProfile.programs_size());

  // Verify log contents
  const std::string expected = RAW_RESULT(
      I: starting Android Wide Profiling daemon
      I: config file path set to /data/nativetest/perfprofd_test/perfprofd.conf
      I: random seed set to 12345678
      I: sleep 674 seconds
      I: initiating profile collection
      I: profile collection complete
      I: sleep 9325 seconds
      I: finishing Android Wide Profiling daemon
                                          );
  // check to make sure log excerpt matches
  compareLogMessages(mock_perfprofdutils_getlogged(),
                     expected, "BasicRunWithLivePerf", true);
}

TEST_F(PerfProfdTest, MultipleRunWithLivePerf)
{
  //
  // Basic test to exercise the main loop of the daemon. It includes
  // a live 'perf' run
  //
  PerfProfdRunner runner;
  runner.addToConfig("only_debug_build=0");
  std::string ddparam("destination_directory="); ddparam += dest_dir;
  runner.addToConfig(ddparam);
  std::string cfparam("config_directory="); cfparam += test_dir;
  runner.addToConfig(cfparam);
  runner.addToConfig("main_loop_iterations=3");
  runner.addToConfig("use_fixed_seed=12345678");
  runner.addToConfig("collection_interval=9999");
  runner.addToConfig("sample_duration=2");
  runner.write_processed_file(1, 2);

  // Create semaphore file
  runner.create_semaphore_file();

  // Kick off daemon
  int daemon_main_return_code = runner.invoke();

  // Check return code from daemon
  EXPECT_EQ(0, daemon_main_return_code);

  // Read and decode the resulting perf.data.encoded file
  wireless_android_play_playlog::AndroidPerfProfile encodedProfile;
  readEncodedProfile("BasicRunWithLivePerf", encodedProfile);

  // Examine what we get back. Since it's a live profile, we can't
  // really do much in terms of verifying the contents.
  EXPECT_LT(0, encodedProfile.programs_size());

  // Examine that encoded.1 file is removed while encoded.{0|2} exists.
  EXPECT_EQ(0, access(encoded_file_path(0).c_str(), F_OK));
  EXPECT_NE(0, access(encoded_file_path(1).c_str(), F_OK));
  EXPECT_EQ(0, access(encoded_file_path(2).c_str(), F_OK));

  // Verify log contents
  const std::string expected = RAW_RESULT(
      I: starting Android Wide Profiling daemon
      I: config file path set to /data/nativetest/perfprofd_test/perfprofd.conf
      I: random seed set to 12345678
      I: sleep 674 seconds
      I: initiating profile collection
      I: profile collection complete
      I: sleep 9325 seconds
      I: sleep 4974 seconds
      I: initiating profile collection
      I: profile collection complete
      I: sleep 5025 seconds
      I: sleep 501 seconds
      I: initiating profile collection
      I: profile collection complete
      I: sleep 9498 seconds
      I: finishing Android Wide Profiling daemon
                                          );
  // check to make sure log excerpt matches
  compareLogMessages(mock_perfprofdutils_getlogged(),
                     expected, "BasicRunWithLivePerf", true);
}

int main(int argc, char **argv) {
  executable_path = argv[0];
  // switch to / before starting testing (perfprofd
  // should be location-independent)
  chdir("/");
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
