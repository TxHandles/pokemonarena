#pragma once
#include <string>
#include <vector>

// Pokemon Contests (Emerald): the five conditions a Pokemon gets from Pokeblocks, and the contest itself -
// Primary Judging on its condition, then five turns of appeals with its moves; first place wins a ribbon.
struct Game;
struct OwnedMon;
enum class Screen;

constexpr int CONTEST_CATS = 5;           // Cool, Beauty, Cute, Smart, Tough
constexpr int CONTEST_RANKS = 4;          // Normal, Super, Hyper, Master
const char* ContestCatName(int c);
const char* ContestRankName(int r);
int MoveContestCat(const std::string& move);   // a move's category in Contests
std::string RibbonName(int cat, int rank);     // "Cool Ribbon", "Cool Ribbon Super", ...
int ContestRankOpen(const OwnedMon& m, int cat);   // the highest rank it may enter (it won the one below)

struct ContestEntrant {
    std::string trainer, name;
    int species = -1;
    std::vector<std::string> moves;
    int cond[6] = {0, 0, 0, 0, 0, 0};   // Cool, Beauty, Cute, Smart, Tough, sheen
    bool player = false;
    int mon = -1;                       // the player's: its index in the party
    int r1 = 0, appeal = 0, turnPts = 0;
    std::string last;                   // the move it used last turn
    int combo = -1;                     // (unused)
    bool guard = false, nervous = false, done = false;   // avoids being startled this turn / may fail / no more appeals
};
struct ContestState {
    int who = 0, cat = 0, rank = 0, phase = 0, sel = 0;   // phase: 0 category, 1 rank, 2 Pokemon, 3 primary judging, 4 appeals, 5 results
    int back = 0;                       // the screen to go back to (a Screen)
    ContestEntrant e[4];
    std::vector<int> order;             // this turn's order
    int turn = 0, step = 0, excite = 0, moveSel = 0;
    float t = 0;
    bool choosing = false;
    std::vector<std::string> log;
    int place[4] = {0, 1, 2, 3};
    std::string prize;
};

void OpenContest(Game& g, int who);       // the Contest Hall's receptionist
void UpdateContest(Game& g);
void DrawContest(const Game& g);
std::string FeedPokeblock(OwnedMon& m, int berryIndex);   // a Pokeblock: conditions up (nature's taste), sheen up; the message
