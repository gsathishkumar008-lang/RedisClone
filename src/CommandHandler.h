#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include <string>
#include <vector>
#include "Database.h"

enum class RespParseStatus { Complete, Incomplete, Error };

void initializeCommands();
RespParseStatus extractRESPCommand(std::string& buffer, std::string& command, std::string& error);
std::string executeCommand(Database& redis, const std::string& input);

#endif
