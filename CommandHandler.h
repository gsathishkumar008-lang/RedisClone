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

string handlePing(Database &redis, vector<string> &tokens);

string handleExpire(Database &redis, vector<string> &tokens);

string handlePersist(Database &redis, vector<string> &tokens);

string handleTTL(Database &redis, vector<string> &tokens);

string handleLPush(Database &redis, vector<string> &tokens);

string handleRPop(Database &redis, vector<string> &tokens);

void initializeCommands();

vector<string> parseRESP(string input);

string executeCommand(Database &redis, string input);

#endif