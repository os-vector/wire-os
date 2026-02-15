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

#ifndef SIMPLE_PERF_COMMAND_H_
#define SIMPLE_PERF_COMMAND_H_

#include <string>
#include <vector>

#include <base/macros.h>

class Command {
 public:
  Command(const std::string& name, const std::string& short_help_string,
          const std::string& long_help_string)
      : name_(name), short_help_string_(short_help_string), long_help_string_(long_help_string) {
    RegisterCommand(this);
  }

  virtual ~Command() {
    UnRegisterCommand(this);
  }

  const std::string& Name() const {
    return name_;
  }

  const std::string& ShortHelpString() const {
    return short_help_string_;
  }

  const std::string LongHelpString() const {
    return long_help_string_;
  }

  virtual bool Run(const std::vector<std::string>& args) = 0;

  static Command* FindCommandByName(const std::string& cmd_name);
  static const std::vector<Command*>& GetAllCommands();

 private:
  const std::string name_;
  const std::string short_help_string_;
  const std::string long_help_string_;

  static void RegisterCommand(Command* cmd);
  static void UnRegisterCommand(Command* cmd);

  DISALLOW_COPY_AND_ASSIGN(Command);
};

#endif  // SIMPLE_PERF_COMMAND_H_
