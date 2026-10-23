extern int gAutoMenu;
// Development-only scripted walkthrough (compiled with -DARENA_AUTOTEST). Saves screenshots.
static bool AutoTestStep(Game& g, float dt) {
    static int step = 0;
    static float t = 0;
    static int shots = 0;
    static int battleShots = 0;
    t += dt;
    auto shot = [&](const char* name) { TakeScreenshot(TextFormat("%02d_%s.png", shots++, name)); };
    auto next = [&]() { step++; t = 0; };
    // Walk through whatever comes after a battle; returns true when back at the hub.
    auto afterBattle = [&](const char* tag) -> bool {
        if (g.screen == Screen::BattleStats) {
            if (t > 0.5f) { shot(TextFormat("%s_stats", tag)); g.screen = Screen::StoryResults; t = 0; }
        } else if (g.screen == Screen::StoryResults) {
            if (t > 0.5f) { shot(TextFormat("%s_results", tag)); gAutoPlayP1 = false; AdvanceStoryQueues(g); t = 0; }
        } else if (g.screen == Screen::Catch) {
            if (g.catchPhase == 0 && t > 0.4f) {
                shot(TextFormat("%s_catch", tag));
                BagTake(g.profile, "Great Ball");
                Game::CatchCand c = g.catchCands[0];
                g.catchCands = {c};
                g.catchSel = 0;
                g.catchOk = true;
                g.catchShakes = 3;
                g.catchFled = false;
                g.catchPhase = 1;
                g.catchT = 0;
                t = 0;
            } else if (g.catchPhase == 2 && t > 3.6f) {
                shot(TextFormat("%s_caught", tag));
                g.catchCands.clear();
                AdvanceStoryQueues(g);
                t = 0;
            }
        } else if (g.screen == Screen::Gift || g.screen == Screen::Fossil || g.screen == Screen::Revive) {
            if (t > 0.5f) { shot(TextFormat("%s_special%d", tag, (int)g.screen)); gAutoMenu = 16; t = 0; }
        } else if (g.screen == Screen::Evolve) {
            if (g.evoT > 3.6f) { g.evoQueue.erase(g.evoQueue.begin()); AdvanceStoryQueues(g); t = 0; }
        } else if (g.screen == Screen::LearnMove) {
            if (t > 0.3f) { auto l = g.learnQueue.front(); g.learnQueue.erase(g.learnQueue.begin()); (void)l; AdvanceStoryQueues(g); t = 0; }
        } else if (g.screen == Screen::Reward) {
            if (t > 0.3f) { g.rewardsPending = 0; AdvanceStoryQueues(g); t = 0; }
        } else if (g.screen == Screen::StoryHub) {
            return t > 0.4f;
        }
        return false;
    };

    switch (step) {
        case 0: if (t > 0.4f && g.screen == Screen::Title) { shot("title"); g.profile = Profile(); g.profile.name = "Autotest"; g.starterSel = 0; g.screen = Screen::Starter; t = 0; }
            else if (t > 0.4f && g.screen == Screen::Starter) {
            shot("starter");
            DeleteProfile("Autotest");
            g.profile = Profile();
            g.profile.name = "Autotest";
            OwnedMon a = NewMon(SpeciesIndex("Cindercub"), 15);
            a.hp = 0.6f;
            a.item = "Charcoal";
            g.profile.mons.push_back(a);
            g.profile.mons.push_back(NewMon(SpeciesIndex("Joltelly"), 15));
            OwnedMon c = NewMon(SpeciesIndex("Pebblor"), 15);
            c.item = "Hard Stone";
            g.profile.mons.push_back(c);
            g.profile.badges = {"Boulder", "Cascade"};
            g.profile.storyStage = 9;
            g.profile.coins = 900;
            g.profile.bag["Poke Ball"] = 3;
            g.profile.bag["Great Ball"] = 2;
            g.profile.bag["Leftovers"] = 1;
            g.profile.trainingWins = 3;
            SaveProfile(g.profile);
            EnterStoryHub(g);
            next();
        } break;
        case 1: if (t > 0.5f) { shot("hub"); g.mapSel = 3; g.screen = Screen::Map; next(); } break;
        case 2: if (t > 0.4f) { shot("map"); g.shopTab = 0; g.shopSel = 1; g.screen = Screen::Shop; next(); } break;
        case 3: if (t > 0.3f) { shot("shop_balls"); g.shopTab = 1; g.shopSel = 19; next(); } break;
        case 4: if (t > 0.3f) { shot("shop_items"); g.myCreSel = 1; g.itemPick = true; g.itemSel = 1; g.screen = Screen::MyCreatures; next(); } break;
        case 5: if (t > 0.3f) {
            shot("hold_item");
            g.itemPick = false;
            g.profile.mons[1].item = "Leftovers";
            BagTake(g.profile, "Leftovers");
            next();
        } break;
        case 6: if (t > 0.3f) {
            shot("mycreatures_item");
            g.stageIdx = -1;
            g.mode = Mode::Training;
            g.rareBattle = false;
            BeginTeamSelect(g);
            g.picks[0] = {0, 1, 2};
            next();
        } break;
        case 7: if (t > 0.3f) { shot("select_training"); gAutoPlayP1 = true; StartBattleFromSelect(g); battleShots = 0; next(); } break;
        case 8: {
            if (g.screen == Screen::Battle && battleShots < 2 && t > 4.0f + battleShots * 5) shot(TextFormat("training%d", battleShots++));
            if (g.screen != Screen::Battle) { t = 0; next(); }
            if (t > 200) return true;
        } break;
        case 9: if (afterBattle("training")) { shot("hub_after_training"); next(); } if (t > 40) return true; break;
        case 10: {  // next training battle should be the rare one
            g.hubSel = 1;
            g.profile.trainingWins = 4;
            g.stageIdx = -1;
            g.mode = Mode::Training;
            g.rareBattle = true;
            if (g.rareSpecies < 0) g.rareSpecies = SpeciesIndex("Drakelet");
            for (auto& m : g.profile.mons) m.hp = 1;
            BeginTeamSelect(g);
            g.picks[0].clear();
            for (int i = 0; i < std::min(6, (int)g.roster[0].size()); i++) g.picks[0].push_back(i);
            next();
        } break;
        case 11: if (t > 0.3f) { shot("select_rare"); gAutoPlayP1 = true; StartBattleFromSelect(g); battleShots = 0; next(); } break;
        case 12: {
            if (g.screen == Screen::Battle && battleShots < 2 && t > 3.0f + battleShots * 5) shot(TextFormat("rare%d", battleShots++));
            if (g.screen != Screen::Battle) { t = 0; next(); }
            if (t > 200) return true;
        } break;
        case 13: if (afterBattle("rare")) { shot("hub_after_rare"); return true; } if (t > 40) return true; break;
    }
    return false;
}
