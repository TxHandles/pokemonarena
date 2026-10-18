#pragma once
#include <cstdint>
#include <string>
#include <vector>
class Battle;
struct CombatantSpec;

// A whole battle as bytes and back (same game version on both ends).
std::string SaveBattle(const Battle& b);
bool LoadBattle(const std::string& data, Battle& b);
// Checksum of what decides the battle; equal on both computers while they're in step.
uint64_t BattleHash(const Battle& b);
std::string SaveCombatants(const std::vector<CombatantSpec>& team);
bool LoadCombatants(const std::string& data, std::vector<CombatantSpec>& team);
