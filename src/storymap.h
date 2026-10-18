#pragma once
#include <string>
#include "profile.h"
#include "raylib.h"

// The Story Mode map: the map image from story.txt (or a drawn stand-in), the route the
// player has taken, gyms with their badges, and the lead creature standing at the next stage.
void LoadStoryMap();      // after the window is open
void UnloadStoryMap();
bool StoryMapHasImage();

// Draws the map fitted into the box. focusStage >= 0 adds a cursor ring there (map screen).
void DrawStoryMap(const Profile& p, int x, int y, int w, int h, float t, bool townLabels, int focusStage = -1);

Color BadgeColor(const std::string& badge);
void DrawBadge(const std::string& badge, int x, int y, bool have, float r = 6.5f);
