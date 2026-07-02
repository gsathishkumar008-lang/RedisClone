#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include <string>
#include <vector>
#include "Database.h"

using namespace std;

string handleSet(Database &redis, vector<string> &tokens);

string handleGet(Database &redis, vector<string> &tokens);

string handleDel(Database &redis, vector<string> &tokens);

string handleExists(Database &redis, vector<string> &tokens);

string handleKeys(Database &redis, vector<string> &tokens);

string handleClear(Database &redis, vector<string> &tokens);

void initializeCommands();

string executeCommand(Database &redis,
                      vector<string> &tokens);

#endif