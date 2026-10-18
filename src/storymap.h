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
// The badge: its picture from Badges/ when there is one (dark until won), else a colored hexagon.
void DrawBadge(const std::string& badge, int x, int y, bool have, float r = 6.5f);
bool HasBadgePicture(const std::string& badge);
// An Elite Four member (a diamond in their room's colour) or the Champion (a gold crown).
void DrawLeagueMark(const struct StageDef& st, int x, int y, bool beaten, float r = 6.5f);
// Where a map position (story.txt map coordinates) lands when the map is drawn into this box.
Vector2 StoryMapPoint(int x, int y, int w, int h, float mx, float my);
