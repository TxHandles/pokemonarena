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
};
} // namespace

const LegendDef& Legend(int id) { return kLegends[id < 0 ? 0 : (id >= LEGEND_COUNT ? LEGEND_COUNT - 1 : id)]; }
