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

#include <string.h>
#include <string>
#include <vector>

#include <base/logging.h>

#include "command.h"

int main(int argc, char** argv) {
  InitLogging(argv, android::base::StderrLogger);
  std::vector<std::string> args;

  if (argc == 1) {
    args.push_back("help");
  } else {
    for (int i = 1; i < argc; ++i) {
      if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
        args.insert(args.begin(), "help");
      } else {
        args.push_back(argv[i]);
      }
    }
  }

  Command* command = Command::FindCommandByName(args[0]);
  if (command == nullptr) {
    LOG(ERROR) << "malformed command line: unknown command " << args[0];
    return 1;
  }
  std::string command_name = args[0];

  LOG(DEBUG) << "command '" << command_name << "' starts running";
  bool result = command->Run(args);
  LOG(DEBUG) << "command '" << command_name << "' "
             << (result ? "finished successfully" : "failed");
  return result ? 0 : 1;
}
