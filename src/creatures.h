#pragma once
#include <string>

// Folder next to the game where players add or reskin creatures.
std::string CreaturesDir();

// Reads every creatures/<folder>/creature.txt and adds or replaces species. Image files in
// each folder are recorded as that species' sprite folder (loaded later by LoadAllSprites).
// Appends a readable report to 'log'.
void LoadCustomCreatures(std::string& log);

// Writes creatures/MOVE-LIST.txt (every built-in move you can use by name) and the load log.
void WriteCreatureFiles(const std::string& log);
