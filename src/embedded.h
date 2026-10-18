#pragma once
#include <string>

// The game's data files are baked into the exe (see tools/embed_files.py). Any that are missing
// next to the game are written back at startup, so the exe always comes with its creatures,
// story.txt and map. Files that already exist are never overwritten (your edits are safe).
// Returns a short report for the load log.
std::string RestoreEmbeddedFiles();

// Where the game keeps its data (Pokemon/, story.txt, map.png, saves/, settings.txt), ending in '/'.
// Next to the exe if that folder already holds game data (the zip layout); otherwise, e.g. when
// ArenaClash.exe was downloaded on its own, an "ArenaClash Data" folder next to it.
std::string DataDir();
