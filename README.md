# Arena Clash

A 2D real-time creature arena battler in C++ with raylib: 18 types, ~170 moves with Pokemon-style
names and effects, levels, evolution, catching, eight gym stadiums with their own mechanics, a Story
Mode with save profiles, and fully remappable controls. Teams are 1–6 creatures.

## Build

**Windows, Visual Studio 2022** (Desktop development with C++, plus Git): File → Open → Folder,
pick this folder, choose `ArenaClash.exe`, press F5. raylib downloads automatically.

**Command line:** `cmake -B build && cmake --build build --config Release`

Data lives next to the exe when that folder already has game data (the zip layout); a lone
`ArenaClash.exe` makes an `ArenaClash Data` folder next to itself instead.

The game's data files (`Pokemon/`, `story.txt`, `map.png`) are also baked
into the exe (`src/embedded_files.inc`, made by `python tools/embed_files.py`). On startup the game
writes back any of them that are missing, so the exe alone is a complete game and your own edits are
never overwritten. Re-run the script after changing those files if you want the exe to carry the
changes. The build also copies `Pokemon/` and `story.txt` next to the exe. Saves go in `saves/` and settings in
`settings.txt`, both next to the exe.

## Modes

- **Story Mode** – the only mode that earns XP. Make a profile, pick a starter and work through
  32 stages from `story.txt`: three trainers before each of 8 gyms (Brock, Misty, Lt. Surge, Erika,
  Koga, Sabrina, Blaine, Giovanni). Gyms give badges. Wins unlock new base-form creatures and coins.
  After the eighth badge: the rival on Route 22, Victory Road, then the Pokemon League.
- **Starters and the rival** – six starters to choose from: Bulbasaur, Charmander, Squirtle, Chikorita,
  Cyndaquil and Totodile (solo, local co-op and online co-op). The rival picks once and keeps his choice
  for every rival battle, the Championship included: alone, one of the two starters that beat yours, at
  random (take Charmander and he gets Squirtle or Totodile); in co-op, two of the four starters neither of
  you took, at random, and he brings both. The pick is saved with the profile / co-op save (online, the
  host picks and the guest gets it with the shared progress).
- **Shiny Pokemon** – about 1 in 512 new Pokemon is shiny: wild ones (training, rare encounters),
  legendaries, gifts, fossils and starters. A shiny keeps its shiny colours for good (saved with it,
  traded with it), sparkles when it's sent out and has a gold star in battle, the party, the Box,
  the catch screen and trades. Every Pokemon's shiny colours match the official shiny sprite:
  `tools/shinygen.py` compares the official normal and shiny sprites (PokeAPI's sprites repo,
  front and back) to see how each colour changes, finds each of our sprite colours on the official
  picture (by colour and by where it sits on the body) and writes `Pokemon/<Name>/shiny.txt`, which
  the game uses to recolour the sprites when it loads them. For a new Pokemon:
  `python tools/shinygen.py <sprites>/sprites/pokemon <Name> --sheet check.png`.
- **Gen 2 (first wave)** – Chikorita, Bayleef, Meganium, Cyndaquil, Quilava, Typhlosion, Totodile,
  Croconaw and Feraligatr, built like the original 151: Pokedex stats, entries, abilities, Scarlet/Violet
  level-up moves and TMs, evolutions (16/32, 14/36, 18/30). They aren't found in the wild.
- **The Pokemon League** – the Elite Four (Lorelei, Bruno, Agatha, Lance) and the Champion (the rival)
  with their FireRed/LeafGreen teams, each in their own arena. They're fought as one run: lose to any
  of them and the challenge starts over from Lorelei (the Healing Center stays open between rooms).
  Beating the Champion puts your party in the **Hall of Fame** (saved in the profile, every Championship
  win adds an entry) and opens the Battle Tower. League members can be rematched from the Map like gyms.
- **Battle Tower** – after the Championship, the hub's first item. Endless floors of trainers: their
  levels climb with the floor (your team's average −1, +1 every 4 floors), the CPU plays Hard from floor
  5, both sides bring 3 (4 from floor 10, 5 from 20, 6 from 30), every 5th floor is fought in a gym arena
  and every 10th is a Tower Boss: a gym leader, Elite Four member or the Champion with their own team in
  their own arena. Everyone fights at full health and the Tower never changes your Pokemon's HP; XP and
  coins as usual (bosses triple). A loss ends the run; the best floor is kept. Solo, local and online co-op.
- **Legendary Pokemon** – the hub's "Legendary Pokemon" item (solo and co-op hubs) lists five one-off
  boss encounters where they live in the games, with a map marker, their story and how they unlock:
  Articuno in the Seafoam Islands, Zapdos in the Power Plant and Moltres on Victory Road (all 8 badges),
  Mewtwo in Cerulean Cave (Champion only), and Mew on Faraway Island (once Mewtwo is caught, a sailor in
  Vermilion hands over the Old Sea Map). Each is a single boss a few levels above your team (birds: your
  average +5, at least Lv 50; Mewtwo +3, at least Lv 70; Mew +2) with a huge HP pool that grows with every
  Pokemon you bring (birds about ×6 solo with 6, ×10 in co-op; Mewtwo ×3.7 / ×5.9; Mew ×3.5 / ×5.5),
  CPU Hard, and it stands its ground instead of kiting. It fights in its own lair, with hazards that only
  hit the challengers. In co-op it's 2 on 1. A legendary
  is never knocked out: at 25% HP it's weakened and the catch starts. Solo you get **3 balls**; in co-op
  **6, taking turns** (the MVP – whoever dealt more damage – throws balls 1, 3 and 5, the partner 2, 4
  and 6; no balls = that turn passes). If it's still free after the last ball it flees and the encounter
  can be tried again from the list. Whoever catches it keeps it and the encounter is closed for good
  (saved as `legends` in the profile / co-op save; online both games share it). Lower HP = better odds;
  Mewtwo and Mew are harder to catch, and a Master Ball always works. Legendaries no longer show up as
  rare training encounters.
- **HP carry-over and the Healing Center** – HP stays between Story battles. Healing costs about
  3 coins per level for a full heal; reviving costs 6 per level + 10. Coins for a win: trainer
  30 + 6×Lv, gym 150 + 15×Lv, Elite Four 300 + 20×Lv, Champion 600 + 25×Lv, Tower 40 + 7×Lv + 4×floor,
  training 15 + 3×Lv (or `coins =` in story.txt). A free emergency heal
  kicks in if the whole team is down and you can't pay.
- **Map** – the hub draws `map.png` (set by `map_image` in story.txt) with each stage at its `map = x, y`
  spot, badges on the gyms and the lead creature at the next stage. Without an image it draws a simple map.
- **Training** – bring up to 6, face up to 3 (`training_max_foes`). Every 5th training battle
  (`rare_every`) is a lone rare creature (species with `rare = yes`) at your level + 3 to catch.
- **Shop and held items** (`src/items.cpp`) – Poke/Great/Ultra/Master Balls and 36 held items, unlocked
  by badge count: 18 type boosters (+20%), Swift Wing, Quick Claw, Lucky Egg, Amulet Coin, Leftovers,
  Muscle Band, Scope Lens, Shell Bell, Rocky Helmet, Expert Belt, King's Rock, Focus Band, Choice
  Scarf, Eviolite, Assault Vest, Life Orb, Choice Band. One item per creature; items also apply when
  a profile's creatures are used in Quick Battle / 2 Players.
- **Catching** – in catchable battles (trainers and training; not gyms) a foe at ≤25% HP is weakened:
  dazed for 1.2 s (still hittable), then it retreats and counts as beaten. After the win you pick one
  survivor and throw a ball. Chance = 35% + 55% × how far under 25% HP it is, adjusted by level gap,
  ×0.8 for evolved forms, clamped 5–95%. Caught creatures keep their level, moves and HP.
- **Quick Battle** – you vs the CPU. Use the free-play roster (everyone Lv50) or a profile's creatures.
- **2 Players** – one keyboard and/or two gamepads; each player picks free play or a profile. No XP.
- **Watch CPU vs CPU** – for testing matchups.
- **Party & Box** – a profile carries up to 6 creatures in its party; the rest wait in the Box.
  Only the party can be brought into battle. Catches and rewards go to the Box when the party is full.
  Wild battles (training, rare encounters) always have at least 2 foes.
- **2v2 Battle** – four creatures on the field: you + CPU vs 2 CPUs, P1 + P2 vs 2 CPUs, or P1 + CPU vs
  P2 + CPU. Each player brings 1–3. Attacks go at the foe you face; a player whose team is out can
  press Swap to take over a benched creature of their partner's.
- **Co-op Story** – two profiles share a co-op save (`saves/A+B.coop`: stage, badges, record).
  XP, coins, items and creatures stay in each profile. Every battle is 2v2: each player brings up
  to 3 of their party, CPU teams get +2 creatures (at least 3, never more than 6), training has
  2–4 wild foes and the rare creature is +6 levels with a partner. Everyone who fought gets full
  XP. After a catchable win there's a catch draft: whoever dealt more damage picks first, each
  player throws their own balls at one creature, and the partner can't take the same one.
- **Friendship** – every creature has a friendship meter (shown as hearts in Party & Box, 0–255,
  starts at 70). After each battle, party creatures change: a win +5 for those that fought (+2 for
  the rest), fainting −5, a wiped-out team −15 for the fainted (−8 for the rest). Some Pokemon evolve
  after a win once friendship reaches their `evolve_friendship` value.
- **Evolution stones** – Fire / Water / Thunder / Leaf / Moon Stone in the shop's third tab; use
  them from Party & Box (`evolve_item` in pokemon.txt). Trade evolutions happen by level instead
  (Alakazam 36, Gengar 38, Machamp 40, Golem 40). CPU Pokemon past level 32 have used their stone.
- **Gen 1 roster** – all 151 base forms are in the Pokemon folder, with Pokedex stats and
  Scarlet/Violet level-up moves (Brilliant Diamond/Shining Pearl for ones not in SV). Pokemon without
  sprites yet borrow built-in art of their type. With the real roster present, the built-in test
  creatures no longer appear as opponents, rewards or free-play picks. Legendaries are rare
  encounters only once your team averages level 40.
- **Pokedex** – Story hub and co-op hub. Every species you've seen (fought against) and caught, with the
  real Pokedex entry, category, height, weight, abilities, base stats and evolution family (dexdata.inc,
  generated from the PokeAPI/pokemondb data). Seen/caught lists are saved in the profile.
- **Abilities** – every Pokemon has one of its species' real abilities (hidden ability 1 in 20), kept
  through evolution and shown in Party & Box. All 114 abilities the original 151 use are in
  (src/abilities.*, src/battle_ab.cpp), translated to real-time battles: Intimidate, Levitate, Static,
  Swift Swim, Sturdy, Flash Fire, Volt Absorb, Synchronize, Neutralizing Gas, Imposter and so on.
- **Trading** – co-op hub > Trade Pokemon. Local co-op: both players pick on one screen; online: each
  player picks on their own computer and the host carries out the swap. Kadabra, Machoke, Graveler and
  Haunter evolve when traded (CPU trainers have them evolved from level 38). Traded Pokemon remember
  their original trainer and start their friendship over.
- **Online Co-op** – Co-op Story with a friend on another computer (src/net.*, src/screens_online.cpp,
  src/net_state.cpp; ENet in src/enet, MIT license). Each player has their own screens and profile;
  a battle starts when both pick the same one in the hub. Battles run on both computers in step
  (fixed 60 ticks/s, the battle's own random generator, src/rng.h) with rollback: your input applies
  after 2 ticks, the partner's is predicted and the battle is re-run from the last confirmed tick
  when their real input arrives. A checksum every 60 ticks catches drift; the host then sends its
  whole battle (SaveBattle/LoadBattle). Connecting: direct address (LAN / Radmin VPN / ZeroTier /
  forwarded UDP port 47777) or connection codes (STUN-discovered public address + UDP hole
  punching; both players swap codes). Testing aid: ARENA_NET_LAG_MS=80 delays every message.
- **Stalemates** – if no one loses HP for 30 s (or a battle passes 200 s), sudden death starts.

## Levels and evolution

- Species use real Pokedex base stats (`sp_attack`/`sp_defense` optional, averaged in); battle
  math scales them by `STAT_SCALE` (1.55, `src/data.h`). Built-in test creatures were converted.
- Starters are the Pokemon tagged `starter = yes`.
- `python tools/install_pokemon.py <folder>...` copies Pokemon folders in (WEBP/JPG to PNG) and
  re-bakes the exe data.
- Stats scale with level: ×0.55 at Lv5, ×1.0 at Lv50, ×1.5 at Lv100. Damage also scales with the
  attacker's level, so equal-level fights last the same at any level.
- XP: total to reach level L is L³. A knockout gives `yield × foe level / 7` (×1.5 vs trainers) to the
  creature that landed it; winning gives every creature that fought half the team's KO value again.
- Level-ups happen instantly, mid-fight included. Reaching the evolution level evolves the creature on
  the spot (it glows for 1.8 s, can't be hit, and the opponent's next creature waits).
- New moves fill an empty slot straight away; otherwise the game asks which move to forget after the
  battle.

## Gym and League arenas (src/field.*, src/battle_field.cpp, src/arena.cpp)

| Arena | Twist |
|---|---|
| Stone Gym (Brock) | 7 stone pillars block shots and beams. Heavy creatures (flag `heavy`, or size ≥ 1.15) break them with melee/dash/burst and the pillar flies off as a power-70 Rock boulder. Pillars grow back after 20 s. |
| Cascade Gym (Misty) | Water everywhere except a few platforms. In water: Water types ×1.35 speed, others ×0.6; hovering, Flying and `winged` creatures are unaffected. Electric hits that land in water shock everyone in it (7% max HP × type effectiveness, 0.7 s stun except Electric types). |
| Power Plant Gym (Lt. Surge) | 4 tesla coils. Within 48 units: 4% max HP per second (scaled by Electric effectiveness). Electric types within 60 get ×1.3 ATK and ×1.25 speed instead, and the CPU camps the coils. |
| Greenhouse Gym (Erika) | Trees block shots. Heavy hits knock them down (the falling trunk hits for Grass 50). Fire sets them burning for 3 s before they fall. Grass types in tall grass: ×1.3 ATK and 0.8% HP/s regen. |
| Swamp Gym (Koga) | Poison pools (2% HP/s + poison chance) and a band of toxic spikes around the edges (5% HP/s and a push inward). Poison types and flyers are immune. |
| Trick Room Gym (Sabrina) | 4 portals; creatures and projectiles that touch one come out of another. They move every 12 s. |
| Volcano Gym (Blaine) | Lava pools (4% HP/s, burn chance, slower movement) spit Fire globs (power 45) every 3–5 s with a landing marker. Rocks give cover. |
| Hideout Gym (Giovanni) | Steel pillars (indestructible). Every 10 s two wall cannons show a red aim line for 1.5 s, then fire a power-60 shot that hits both sides. |

| Frozen Chamber (Lorelei) | It always snows. Frozen rinks send everyone but Ice types sliding; Ice types skate on them (×1.3 speed, ×1.1 ATK). Fire melts a rink for 7 s. Ice pillars melt under any Fire hit and freeze again after 12 s. |
| Brawler's Ring (Bruno) | A boxing ring: knocked into the ropes, you bounce back in harder (3% max HP). Fighting types in the centre circle get ×1.3 ATK. Four training boulders. |
| Haunted Hall (Agatha) | Lights out. Three will-o'-wisps rise from the gravestones and drift after the nearest creature that isn't a Ghost: touching one is a Ghost hit (power 32, 25% burn). Ghost types move ×1.15 faster. |
| Dragon's Den (Lance) | Every 7–9 s a dragon statue's eyes glow for 1.6 s, then it breathes a fan of three Fire shots (power 50, burn chance) at someone. Dragon types get ×1.2 ATK. |
| Champion's Stadium | A spotlight glides around the floor; whoever stands in it gets ×1.25 ATK and ×1.15 speed. Fireworks, pyrotechnics and the biggest crowd in Kanto. |

| Seafoam Depths (Articuno) | Ice rules (rinks, ice pillars) and endless snow. Every 5 s two hailstones fall on markers near the challengers (Ice 50, 10% freeze). |
| Power Plant (Zapdos) | Electric coils and rain. Every 4.5 s two lightning bolts strike near the challengers (Electric 55, 30% paralysis). |
| Moltres's Nest | Victory Road's lava pools under a blazing sun. Every 5 s two burning rocks drop near the challengers (Fire 50, 30% burn). |
| Cerulean Cave (Mewtwo) | Deep pools slow you, crystal pillars, Mewtwo hurls a psychic boulder every 3.5 s (Psychic 60) and sends a psychic pulse rippling out every 9–12 s. |
| Faraway Island (Mew) | Jungle: trees and tall grass. Mew teleports to a random patch of grass every 8–11 s, and in the grass it's ×1.1 faster and heals 0.15% HP/s. |

Lair hazards only hit the challengers and stop once the legendary is weakened. A legendary boss is also a
slightly bigger target (hit radius ×1.25). Catch odds per ball: (8% + up to 30% the lower its HP was when it
gave up) × the legendary's catch rate (Mewtwo ×0.55, Mew ×0.75), then the ball's bonus as usual.

Arena hazards hit either side, and their damage scales with the battle's average level. The CPU avoids
harmful zones and seeks helpful ones. Every arena can be picked for Quick Battle / 2 Players in
Settings (or Random).

## Move system (src/moves.cpp)

Kinds: Shot, Spread, Melee, Dash, Burst, Beam, Lob, Zone, Buff, Heal, **Strike** (instant hit that
leaps to the target – Thunder Shock's chain lightning), **Wave** (ground shockwave – Earthquake, Surf),
**Rain** (falling objects – Rock Slide, Blizzard, Thunder), **Blink** (teleport strike – Shadow Sneak),
**Shield** (Protect), **Burrow** (Dig, Fly, Dive).

Effects: burn, poison, bad poison, paralysis, freeze, sleep, confusion, flinch, Leech Seed, stat drops,
drain, recoil, homing, multi-hit, high crit, Hex-style bonus, recharge, and burning/toxic patches left
where a shot lands (Ember, Sludge, Fire Blast). Every move's effect is listed in `Pokemon/MOVE-LIST.txt`,
written by the game at startup.

## Move effects (src/vfx.*, src/battle_vfx.cpp)

Every move has a "look": its projectile shape, colours, trail particles, impact style, beam decoration,
melee animation (fist, kick, bite, claw, chop, jab, whip, swoosh) and whether it explodes. Looks come from
the move's type and kind, then keyword rules (Punch, Fang, Claw...) and named overrides (Thunderbolt,
Fire Blast, Solar Beam, Explosion...), so moves added in creature files get a sensible look automatically.
Textures are generated at start-up (no image files). Effects are spawned inside the battle simulation, so
both players see the same thing online; drawing uses a solid body plus additive light so effects read on
bright and dark arenas alike.

## Pokemon folder

`Pokemon/<Name>/pokemon.txt` plus sprite images, one folder per Pokemon, made from
`Pokemon/_TEMPLATE`. Included: Bulbasaur (with sprites), Charmander, Squirtle and Pikachu (built-in
placeholder art via `placeholder_art =` until their sprites are added). Folder Pokemon are listed in
Pokedex order (`dex =`) after the built-in test creatures. See `Pokemon/HOW-TO-ADD-POKEMON.txt`.
Grid-chart sprite sheets are converted automatically (`src/chart.cpp`). Old saves holding the
removed Thistlet/Thornbloom starter get Bulbasaur instead.

## Settings and controls

Settings menu: fullscreen, FPS counter, master/effects volume, auto-aim, screen shake, damage numbers,
team size (1–6), CPU skill, Quick Battle arena, and a Controls page to rebind every action for the 1-player keyboard, both halves
of the shared keyboard, and two gamepads (keys used twice show in red).

| Default | Move | Moves 1–4 | Swap next | Swap back | Dodge | Jump | Pause |
|---|---|---|---|---|---|---|---|
| 1 player | WASD / arrows | J K L I (alt F G H T) | U (alt Q) | Y (alt Z) | Space (alt L-Shift) | E (alt O) | Esc / P |
| 2P – Player 1 | WASD | F G H T | Q | Z | E (alt L-Shift) | R | Esc / P |
| 2P – Player 2 | Arrows | Numpad 1–4 (alt , . / ;) | Numpad 0 / R-Shift | Numpad . (alt ]) | Numpad 5 (alt L) | Numpad 6 (alt ') | Numpad Enter |
| Gamepad | Left stick / d-pad | A B X Y | RT / R2 | LT / L2 | LB / L1 | RB / R1 | Start |

Swap next / back step forward or backward through your party (skipping fainted ones).

Dodge: a quick dash (in the direction you're moving, or backwards when standing still) that attacks pass
straight through; 6-second cooldown, shown on a tab above your moves. Jump: no cooldown and no
invulnerability, but you clear water, puddles, pools, ice and obstacles while airborne (Misty's islands).
You can steer a little in the air but can't use moves until you land.

## CPU skill

`kAI` at the top of `src/battle.cpp`: reaction time, dodge chance, speed, aim error, target leading,
cooldown multiplier, pauses after attacking, and circling. Measured against a simulated player that
reacts fast and aims perfectly: Easy wins ~9%, Normal ~31%, Hard ~57%.

## Code map

| File | What's in it |
|---|---|
| `src/types.*` | 18 types, colors, effectiveness chart |
| `src/moves.*` | move kinds, status effects, the move library, move descriptions |
| `src/data.*` | built-in species, learnsets, evolutions, XP and level math |
| `src/battle.*` | the fight: movement, every move kind, statuses, XP, level-ups, evolution, weakening for catches, CPU AI, HUD |
| `src/field.*`, `src/battle_field.cpp` | gym arena layouts and their mechanics (obstacles, zones, portals, cannons, electrified water) |
| `src/profile.*` | save profiles and growth outside battle |
| `src/story.*` | Story Mode ladder (story.txt), CPU teams, rewards, rare picks |
| `src/storymap.*` | the Story Mode map |
| `src/items.*` | shop items and held-item effects |
| `src/screens_*.cpp`, `src/game.h` | all menus and screens |
| `src/settings.*`, `src/input.*` | settings file, bindings, keyboard/gamepad input |
| `src/audio.*` | synthesized sound effects |
| `src/creatures.*`, `src/chart.*`, `src/sprites.*` | creature folders, chart conversion, sprites |
| `src/creature_art.h` | built-in pixel art |
| `src/arena.*`, `src/ui.*` | arena floors (stadium + each gym), drawing helpers |
