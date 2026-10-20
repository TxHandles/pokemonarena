#pragma once
#include <string>
#include "raylib.h"

// Gym leader pictures: Leaders/<Name>.png next to the game, matched to a stage's trainer
// ("Leader Brock", "Lt. Surge", "Boss Giovanni"). Shown in the gym intro and as battle cut-ins.
const Texture2D* LeaderPicture(const std::string& trainer);   // nullptr if there's none
std::string LeaderShortName(const std::string& trainer);      // "Lt. Surge" -> "Surge"
const char* LeaderLastStandLine(const std::string& trainer);   // their line when down to one Pokemon
// Draw a leader standing with their feet at (cx, footY), scale x their picture. flip = face right.
void DrawLeaderPicture(const Texture2D& t, float cx, float footY, float scale, Color tint = WHITE, bool flip = false);
void UnloadLeaderPictures();
