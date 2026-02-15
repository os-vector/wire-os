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

// Semaphore file that indicates that the user is opting in
#define SEMAPHORE_FILENAME "perf_profile_collection_enabled.txt"

// File containing a list of sequence numbers corresponding to profiles
// that have been processed/uploaded. Written by the GmsCore uploader,
// within the GmsCore files directory.
#define PROCESSED_FILENAME "perfprofd_processed.txt"

// File containing a list of sequence numbers corresponding to profiles
// that have been created by the perfprofd but not yet uploaded. Written
// by perfprofd within the destination directory; consumed by GmsCore.
#define PRODUCED_FILENAME "perfprofd_produced.txt"

// Main routine for perfprofd daemon
extern int perfprofd_main(int argc, char **argv);

//
// This enumeration holds the results of what happened when on an
// attempted perf profiling run.
//
typedef enum {

  // Success
  OK_PROFILE_COLLECTION,

  // Fork system call failed (lo mem?)
  ERR_FORK_FAILED,

  // Perf ran but crashed or returned a bad exit status
  ERR_PERF_RECORD_FAILED,

  // The perf.data encoding process failed somehow
  ERR_PERF_ENCODE_FAILED,

  // We tried to open the output file perf.data.encoded but the open failed
  ERR_OPEN_ENCODED_FILE_FAILED,

  // Error while writing perf.data.encoded
  ERR_WRITE_ENCODED_FILE_FAILED
} PROFILE_RESULT;

//
// Given a full path to a perf.data file specified by "data_file_path",
// read/summarize/encode the contents into a new file specified
// by "encoded_file_path". Return status indicates whether the operation
// was successful (either OK_PROFILE_COLLECTION or an error of some sort).
//
PROFILE_RESULT encode_to_proto(const std::string &data_file_path,
                               const char *encoded_file_path);
