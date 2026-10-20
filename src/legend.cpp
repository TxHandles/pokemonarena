#include "legend.h"

namespace {
const LegendDef kLegends[LEGEND_COUNT] = {
    {"Articuno", "Seafoam Islands", ArenaKind::Seafoam, 50, 5, 1.2f, 1.0f, 452, 806, LegendUnlock::Badges8,
     "A legendary bird said to appear to travelers lost in icy mountains. It nests in the frozen caves at the bottom of the Seafoam Islands, where the currents meet.",
     "A freezing wind howls through the cave... Articuno descends!"},
    {"Zapdos", "Power Plant", ArenaKind::PowerPlant, 50, 5, 1.2f, 1.0f, 905, 322, LegendUnlock::Badges8,
     "A legendary bird that gains power when it's struck by lightning. It made its nest in the abandoned Power Plant on Route 10.",
     "The lights flicker and thunder shakes the Power Plant... Zapdos appears!"},
    {"Moltres", "Victory Road", ArenaKind::Ember, 50, 5, 1.2f, 1.0f, 158, 318, LegendUnlock::Badges8,
     "A legendary bird whose wings blaze with fire. In Red and Blue it waits deep inside Victory Road, where only trainers with all eight badges may enter.",
     "The cave walls glow red-hot... Moltres spreads its burning wings!"},
    {"Mewtwo", "Cerulean Cave", ArenaKind::Unknown, 70, 3, 0.65f, 0.55f, 700, 228, LegendUnlock::Champion,
     "Created from Mew's DNA by years of gene-splicing experiments, it escaped the Cinnabar lab. It hides in the cave north-west of Cerulean City, open only to the Champion.",
     "The air grows heavy with psychic power... Mewtwo is waiting."},
    {"Mew", "Faraway Island", ArenaKind::Faraway, 30, 2, 0.6f, 0.75f, 1040, 760, LegendUnlock::AfterMewtwo,
     "The Cinnabar Mansion journals say Mew holds the DNA of every Pokemon. Once Mewtwo has been caught, a sailor in Vermilion City hands you an Old Sea Map to Faraway Island, where Mew plays in the tall grass.",
     "Something giggles in the tall grass... Mew wants to play!"},
    {"Raikou", "Route 36 (roaming)", ArenaKind::PowerPlant, 72, 4, 0.75f, 0.7f, 445, 330, LegendUnlock::AfterStage,
     "One of three beasts that rose from the ashes of the Burned Tower. It races across Johto with the speed of lightning, and thunder rolls wherever it runs.",
     "Thunder cracks across the sky... Raikou stops in its tracks!", 1, "Ecruteak Gym", "Roams Johto after the Fog Badge"},
    {"Entei", "Route 38 (roaming)", ArenaKind::Ember, 72, 4, 0.75f, 0.7f, 365, 256, LegendUnlock::AfterStage,
     "One of the three beasts reborn in the Burned Tower's fire. Volcanoes are said to erupt whenever it barks.",
     "The ground trembles and the grass smolders... Entei roars!", 1, "Ecruteak Gym", "Roams Johto after the Fog Badge"},
    {"Suicune", "Route 42 (roaming)", ArenaKind::Water, 72, 4, 0.75f, 0.7f, 640, 226, LegendUnlock::AfterStage,
     "The third beast of the Burned Tower, said to be the north wind given form. It purifies dirty water as it races across the land.",
     "A cold north wind sweeps the water clear... Suicune appears!", 1, "Ecruteak Gym", "Roams Johto after the Fog Badge"},
    {"Ho-Oh", "Tin Tower", ArenaKind::Ember, 78, 4, 0.65f, 0.55f, 540, 196, LegendUnlock::AfterStage,
     "The rainbow-winged guardian of the skies. It left Tin Tower after the Burned Tower fire, and only returns for a trainer the Kimono Girls deem worthy.",
     "A rainbow arcs over the tower's roof... Ho-Oh descends in a blaze of light!", 1, "Ecruteak Dance Theater", "The Kimono Girls open the way"},
    {"Lugia", "Whirl Islands", ArenaKind::Unknown, 78, 4, 0.65f, 0.55f, 220, 559, LegendUnlock::AfterStage,
     "The guardian of the seas sleeps in a trench beneath the Whirl Islands. A flap of its wings is said to start a forty-day storm.",
     "The whirlpools stop spinning... Lugia rises from the depths!", 1, "Ecruteak Dance Theater", "The Kimono Girls open the way"},
    {"Celebi", "Ilex Forest Shrine", ArenaKind::Faraway, 70, 2, 0.6f, 0.75f, 356, 792, LegendUnlock::AfterStage,
     "The voice of the forest, which travels through time. Kurt's GS Ball wakes the old shrine in Ilex Forest once the Johto Champion has been crowned.",
     "Time seems to ripple around the shrine... Celebi appears!", 1, "Champion's Room", "Kurt needs the GS Ball - after Lance"},
};
} // namespace

const LegendDef& Legend(int id) { return kLegends[id < 0 ? 0 : (id >= LEGEND_COUNT ? LEGEND_COUNT - 1 : id)]; }
