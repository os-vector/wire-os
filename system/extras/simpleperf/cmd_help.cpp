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

#include <stdio.h>
#include <string>
#include <vector>

#include <base/logging.h>

#include "command.h"

class HelpCommand : public Command {
 public:
  HelpCommand()
      : Command("help", "print help information for simpleperf",
                "Usage: simpleperf help [subcommand]\n"
                "    Without subcommand, print short help string for every subcommand.\n"
                "    With subcommand, print long help string for the subcommand.\n\n") {
  }

  bool Run(const std::vector<std::string>& args) override;

 private:
  void PrintShortHelp();
  void PrintLongHelpForOneCommand(const Command& cmd);
};

bool HelpCommand::Run(const std::vector<std::string>& args) {
  if (args.size() == 1) {
    PrintShortHelp();
  } else {
    Command* cmd = Command::FindCommandByName(args[1]);
    if (cmd == nullptr) {
      LOG(ERROR) << "malformed command line: can't find help string for unknown command " << args[0];
      LOG(ERROR) << "try using \"--help\"";
      return false;
    } else {
      PrintLongHelpForOneCommand(*cmd);
    }
  }
  return true;
}

void HelpCommand::PrintShortHelp() {
  printf("Usage: simpleperf [--help] subcommand [args_for_subcommand]\n\n");
  for (auto& command : Command::GetAllCommands()) {
    printf("%-20s%s\n", command->Name().c_str(), command->ShortHelpString().c_str());
  }
}

void HelpCommand::PrintLongHelpForOneCommand(const Command& command) {
  printf("%s\n", command.LongHelpString().c_str());
}

HelpCommand help_command;
