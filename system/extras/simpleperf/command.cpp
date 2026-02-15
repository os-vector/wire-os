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

#include "command.h"

#include <algorithm>
#include <string>
#include <vector>

static std::vector<Command*>& Commands() {
  // commands is used in the constructor of Command. Defining it as a static
  // variable in a function makes sure it is initialized before use.
  static std::vector<Command*> commands;
  return commands;
}

Command* Command::FindCommandByName(const std::string& cmd_name) {
  for (auto& command : Commands()) {
    if (command->Name() == cmd_name) {
      return command;
    }
  }
  return nullptr;
}

static bool CompareCommandByName(Command* cmd1, Command* cmd2) {
  return cmd1->Name() < cmd2->Name();
}

const std::vector<Command*>& Command::GetAllCommands() {
  std::sort(Commands().begin(), Commands().end(), CompareCommandByName);
  return Commands();
}

void Command::RegisterCommand(Command* cmd) {
  Commands().push_back(cmd);
}

void Command::UnRegisterCommand(Command* cmd) {
  for (auto it = Commands().begin(); it != Commands().end(); ++it) {
    if (*it == cmd) {
      Commands().erase(it);
      break;
    }
  }
}
