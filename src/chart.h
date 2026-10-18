#pragma once
#include <string>
#include "raylib.h"

// Turns any image file into a clean, tightly-cropped sprite:
//  - Plain sprites with transparency are just cropped.
//  - Small sprites on a solid background have that background (the top-left color) made transparent.
//  - Large "pixel art chart" images (a numbered grid of colored squares, like bead/cross-stitch
//    patterns, with or without numbers in the squares and a color legend beside it) are read
//    square by square and turned into a 1-pixel-per-square sprite.
// Returns an image with width 0 on failure. 'note' describes what was done.
Image ExtractSprite(const Image& src, std::string& note, bool& wasChart, bool numberedHint = false);
