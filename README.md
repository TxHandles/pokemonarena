# Pokemon Battle Masters (Arena Clash)

A 2D real-time Pokemon arena battler in C++ with raylib: 18 types, ~170 moves with Pokemon-style
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

## Title menu

Story Mode, **Co-op** (Local Co-op / Online Co-op), **Quick Battle** (1 vs CPU, 2 Players, 2v2 Battle,
Watch CPU vs CPU), Settings and Quit. Back / Esc / B leaves a submenu.

## Modes

- **Story Mode** – the only mode that earns XP. Make a profile, pick a starter and work through
  32 stages from `story.txt`: three trainers before each of 8 gyms (Brock, Misty, Lt. Surge, Erika,
  Koga, Sabrina, Blaine, Giovanni). Gyms give badges. Wins unlock new base-form creatures and coins.
  After the eighth badge: the rival on Route 22, Victory Road, then the Pokemon League.
- **Starters and the rival** – nine starters to choose from: Bulbasaur, Charmander, Squirtle, Chikorita,
  Cyndaquil, Totodile, Treecko, Torchic and Mudkip (solo, local co-op and online co-op). The rival picks once
  and keeps his choice for every rival battle, the Championship included: alone, one of the starters that beat
  yours, at random (take Charmander and he gets Squirtle, Totodile or Mudkip); in co-op, two of the seven
  starters neither of you took, at random, and he brings both. The pick is saved with the profile / co-op save (online, the
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
- **Gym leaders** – pictures of the 16 Kanto and Johto gym leaders, plus Hoenn's eight (Tate and Liza walk out
  together) and Champion Wallace (Leaders/, cut from the user's sheet)
  appear in the gym intro (the leader walks out under a spotlight next to their line), on the VS splash
  as the battle starts, and in a cut-in with a last-stand line when they're down to their last Pokemon
  (rematches too). Drop a PNG named after a trainer in Leaders/ to add one (src/leaders.*).
- **Gen 2 (the rest of Johto, #161-#251)** – Sentret to Celebi, 91 Pokemon, with sprites taken from
  the nayakoko bead charts (portrait plus the 8 walking frames), Pokedex stats, entries and abilities,
  Scarlet/Violet level-up moves and TMs (Brilliant Diamond/Shining Pearl and Sword/Shield for ones not in
  SV), shiny colours and random natures like everyone else. Raikou, Entei, Suicune, Lugia, Ho-Oh and Celebi
  are legendaries. They live in Johto's wild areas. New evolution
  kinds: day/night friendship (`evolve_friendship = Espeon, 160, day`, Umbreon at night) and Tyrogue's
  `evolve_by_stats` (Hitmonlee, Hitmonchan or Hitmontop at 20, picked by its nature's Attack vs Defense).
  Kanto Pokemon gained their Gen 2 evolutions: Crobat and Blissey (friendship), Bellossom (new **Sun
  Stone**, also Sunkern), and by trade or Linking Cord Politoed, Slowking, Steelix, Scizor, Kingdra and
  Porygon2. The babies (Pichu, Cleffa, Igglybuff, Togepi) evolve by friendship.
- **The Pokemon League** – the Elite Four (Lorelei, Bruno, Agatha, Lance) and the Champion (the rival)
  with their FireRed/LeafGreen teams, each in their own arena. They're fought as one run: lose to any
  of them and the challenge starts over from Lorelei (the Healing Center stays open between rooms).
  Beating the Champion puts your party in the **Hall of Fame** (saved in the profile, every Championship
  win adds an entry) and opens the Battle Tower. League members can be rematched from the Map like gyms.
- **Johto** – a second region on its own map (`johto.png`), opened by beating the Kanto Champion: the hub's
  first item becomes **Travel to Johto!** (and, once Kanto is done, Left/Right on it switches between the story
  and the Battle Tower). Professor Elm gives a second starter at level 60 with the same rules as Kanto: alone,
  pick any of the nine and Silver takes one of the ones that beat it; in co-op the host picks, then player 2,
  then Silver takes two of the remaining seven at random (online, each player picks on their own turn and the
  guest gets Silver's pick from the host). 38 battles follow HeartGold/SoulSilver from Cherrygrove to Lance
  (levels 60–88): the eight Johto gyms (Falkner, Bugsy, Whitney, Morty, Chuck, Jasmine, Pryce, Clair), Silver's
  five fights, the Team Rocket executives, the Sudowoodo and the shiny Red Gyarados (wild bosses that **join
  your team** when beaten, `join = yes`), the Kimono Girls, the Elite Four Will, Koga, Bruno and Karen, Champion
  Lance (the Johto Hall of Fame), and after him Red on Mt. Silver at level 95. Johto's legendaries are on
  the Map's Legendary tab (open it while the Map shows Johto): Raikou, Entei and Suicune after the Fog Badge, Ho-Oh
  and Lugia after the Kimono Girls, Celebi after Lance. `wild.txt` has 23 Johto training areas. In
  `story.txt` a `[region]` block (name, map_image, start_level, towns) starts each new region.
  Red on Mt. Silver is a **post-game challenge** now (`postgame = yes`): the story moves on to Hoenn after
  Lance, and Red is fought from the Map (old saves that already beat him keep the win).
- **Hoenn** – the third region, on the Emerald town map (`hoenn.png`), opened by beating Lance (in co-op,
  the shared save). Professor Birch gives a third starter at your team's average −3 (at least Lv 85), and
  **May** is the rival (a counter alone, two of the seven left in co-op, online too). 38 battles follow
  Emerald from Route 103 to Champion Wallace: the eight Hoenn gyms (Roxanne, Brawly, Wattson, Flannery,
  Norman, Winona, Tate & Liza as a double battle, Juan), May five times (her partners follow Emerald: the
  two of Wingull, Lotad and Slugma that don't share her starter's type, evolving as the story goes on), Wally
  twice, Team Aqua and Team Magma up to Maxie and Archie, Steven fighting beside you at the Space Center,
  the invisible Kecleon and the Electrode in disguise (bosses that join you), the Elite Four Sidney, Phoebe,
  Glacia and Drake, and Wallace (the Hoenn Hall of Fame). Steven waits at Meteor Falls as the post-game
  challenge. Hoenn levels follow your team (`level = +N`) with a **floor** (`floor = 85`: the team average
  never counts lower), so a low-level tag-along can't make a gym easy, and nothing goes over 100.
  The story is told on **story cards** (`card` / `card_after` in story.txt: the move to Littleroot, Archie at
  the museum, the Meteorite, Groudon and Kyogre waking, the weather war, Rayquaza, Steven's challenge) and
  narration lines on the battle intros (`story`). **Gifts** (`gift = Castform`): Castform after the Weather
  Institute, Beldum after Wallace. Ten legendaries on the Map's Legendary tab: Regirock, Regice and
  Registeel and Rayquaza after the Rain Badge, Latias, Latios, Groudon and Kyogre after Wallace, Jirachi after
  you beat Steven, Deoxys once Rayquaza is caught. `wild.txt` has 20 Hoenn training areas.
- **Sinnoh** – the fourth region, on the Platinum town map (`sinnoh.png`). Professor Rowan's gift is
  Turtwig, Chimchar or Piplup; Barry (your neighbour) takes the one that beats yours, and Dawn fights beside
  you in a few doubles (`assistant`: the starter neither of you took). 50 battles follow Platinum from
  Route 201 to Champion Cynthia: the eight gyms (Roark, Gardenia, Fantina, Maylene, Crasher Wake, Byron,
  Candice, Volkner), Team Galactic's commanders, the three lakes, Spear Pillar, Cyrus in the Distortion
  World, the Elite Four (Aaron, Bertha, Flint, Lucian) and Cynthia; then the Battle Zone post-game
  (the Fight Area, Stark Mountain, Marley, the Survival Area) and Cynthia's rematch team on the Map.
  Five new arenas: **Hearthome Gym** (lights out, shape-marked doors that teleport you, a chandelier that
  lights the hall), **Pastoria Gym** (floodgates raise and lower the water; Electric shorts the switch),
  **Sunyshore Gym** (gears carry and fling you; the solar panel's sunbeam), **Cynthia's Room** (the
  spotlight, ancient columns, the lake trio's mural) and the **Distortion World** (floating islands and
  shifting gravity). Fourteen legendaries on the Map's Legendary tab: Uxie, Mesprit and Azelf after
  Cyrus, Giratina in the Distortion World, Dialga and Palkia at Spear Pillar (new lairs: slowed time /
  torn space), Heatran, Regigigas (once the three Regis are caught), Cresselia, Darkrai on Newmoon Island
  (Dark Void clouds), Shaymin, Arceus in the Hall of Origin (once Dialga, Palkia and Giratina are caught),
  Manaphy and Phione. `wild.txt` has 21 Sinnoh training areas.
- **Start anywhere** – a new story (or co-op journey) begins on a region screen: pick Kanto, Johto,
  Hoenn or Sinnoh and get that region's own three starters at level 5 (Kanto: Bulbasaur, Charmander, Squirtle;
  Johto: Chikorita, Cyndaquil, Totodile; Hoenn: Treecko, Torchic, Mudkip; Sinnoh: Turtwig, Chimchar, Piplup), with that region's rival
  taking the one that beats yours. When a region's Champion falls, the hub's first item becomes
  **Choose your next region!** and you pick any region you haven't played; its professor gives you its
  starter and its rival picks theirs. **Difficulty follows the order, not the region**: whatever you play
  first uses the Map 1 curve (Kanto's levels: a Lv5 starter, Brock ~13 ... the Champion ~63), the second
  Map 2 (Johto's: a Lv60 starter, levels 60-88), the third Map 3 (Hoenn's: a Lv85+ starter, floors 80-90), the fourth Map 4
  (Sinnoh's: a Lv90 starter, floors 88-96).
  Trainers, the starter gift, wild Pokemon and legendaries all follow the region's place in your order.
  Legendaries only appear once you've reached their region (and their badges count only that region's).
  In co-op, player 1 (online: the host) chooses. Old saves keep the usual order and pick at the next
  region. The order is saved as `region_order` in the profile / co-op save; `dex = 252-386` and
  `levels = 80-98` in a `[region]` block set its starters (by Pokedex number) and level band.
- **Battle Tower** – after the Championship, the hub's first item. Endless floors of trainers: their
  levels climb with the floor (your team's average −1, +1 every 4 floors), the CPU plays Hard from floor
  5, both sides bring 3 (4 from floor 10, 5 from 20, 6 from 30), every 5th floor is fought in a gym arena
  and every 10th is a Tower Boss: a gym leader, Elite Four member or the Champion with their own team in
  their own arena. Everyone fights at full health and the Tower never changes your Pokemon's HP; XP and
  coins as usual (bosses triple). A loss ends the run; the best floor is kept. Solo, local and online co-op.
- **Legendary Pokemon** – the Map's **Legendary Pokemon** tab (Tab / X on the hub's Map, solo and co-op; the Map item says when some are waiting) lists five one-off
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
- **Hub icons** – Settings is the gear in the top-left corner of the Story and co-op hubs; in co-op the
  bar-graph icon next to it opens the co-op leaderboard. Up from the first menu item reaches them.
- **Bag** – the hub's Bag item (solo, local and online co-op) shows everything you carry in five pockets:
  Poke Balls, Held Items, Evolution Stones, TMs and Key Items (Badge Case, Coin Case, the Old Sea Map),
  with each item's description and a list of the items your Pokemon are holding.
- **HP carry-over and the Healing Center** – the Pokemon Center heals your **party** only (the Box
  doesn't count), one Pokemon or the whole party at once. HP stays between Story battles. Healing costs about
  3 coins per level for a full heal; reviving costs 6 per level + 10. Coins for a win: trainer
  30 + 6×Lv, gym 150 + 15×Lv, Elite Four 300 + 20×Lv, Champion 600 + 25×Lv, Tower 40 + 7×Lv + 4×floor,
  training 15 + 3×Lv (or `coins =` in story.txt). A free emergency heal
  kicks in if the whole team is down and you can't pay.
- **Map** – the hub draws `map.png` (set by `map_image` in story.txt) with each stage at its `map = x, y`
  spot, badges on the gyms and the lead creature at the next stage. Without an image it draws a simple map.
- **Training** – bring up to 6, face up to 3 (`training_max_foes`) but never more wild Pokemon than
  you bring (bring 1, face 1). On the team screen X / Tab opens **Where to train**: every area open so
  far with the Pokemon that live there (common / uncommon / rare, level range); ones not yet in your
  Pokedex are silhouettes. Every 5th training battle
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
  A Pokemon in the Box can be released into the wild (its held item goes back to the Bag).
  On the Story team screen (**Choose your team**), **R** (gamepad Y) opens the Box: pick a Box Pokemon and
  it swaps into the party slot under the cursor, so you can bring the right types. Green up / red down
  arrows on every card show whether it's strong against the next battle's foes or weak to them.
- **Double battles in Story** – some trainers fight two at a time: the Route 8 twins and the Celadon Park
  twins in Kanto, the Dragon's Den twins in Johto, and Ariana with a Rocket Grunt, where Lance fights
  beside you. Alone, your 2nd, 4th and 6th picks fight beside you, run by the CPU (they still earn XP);
  in co-op it's the usual 2v2. story.txt: `double = yes`, or `partner = Lance` + `partner_team = ...`.
- **2v2 Battle** – four creatures on the field: you + CPU vs 2 CPUs, P1 + P2 vs 2 CPUs, or P1 + CPU vs
  P2 + CPU. Each player brings 1–3. Attacks go at the foe you face; a player whose team is out can
  press Swap to take over a benched creature of their partner's.
- **Co-op Story** – two profiles share a co-op save (`saves/A+B.coop`: stage, badges, record).
  XP, coins, items and creatures stay in each profile. Every battle is 2v2: each player brings up
  to 3 of their party, CPU teams get +2 creatures (at least 3, never more than 6), training has
  up to 4 wild foes (fewer than the two of you bring together) and the rare creature is +6 levels with a partner. Everyone who fought gets full
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
  through evolution and shown in Party & Box. All the abilities the first 386 use are in
  (src/abilities.*, src/battle_ab.cpp), translated to real-time battles: Intimidate, Levitate, Static,
  Swift Swim, Sturdy, Flash Fire, Volt Absorb, Synchronize, Neutralizing Gas, Imposter and so on.
  Gen 2 added Huge Power, Sand Stream, Drizzle, Shadow Tag, Speed Boost, Moody, Contrary, Magic Bounce,
  Sap Sipper, Pickpocket, Prankster, Suction Cups, Telepathy, Plus, Super Luck, Magma Armor, Light Metal
  and Honey Gather.
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
  punching; both players swap codes). If the connection drops, the reconnect banner's Tab opens a box
  to type your friend's new code (when they restarted the game) without leaving. B on a gamepad backs
  out of the Host / Join pages. Testing aid: ARENA_NET_LAG_MS=80 delays every message.
- **Stalemates** – if no one loses HP for 30 s (or a battle passes 200 s), sudden death starts.

## Walking around (overworld, test build)

"Walk around (test)" in the Story / co-op menu puts you on the map: your room in Pallet Town, Route 1
and Viridian City so far (the maps are FireRed/LeafGreen rips in World/, with their walls, tall grass,
ledges, doors, signs and people in World/world.txt). Arrows walk, Shift runs, Enter talks / reads, Esc
goes back to the menu (your place is saved).

- Story trainers stand on the map and challenge you when they spot you (Blue in Oak's Lab, the
  Youngster on Route 1, Blue at Viridian's west exit when the story is there).
- Tall grass: wild Pokemon from that route (one at a time).
- The Pokemon Center Nurse heals, the Mart clerk sells; your lead Pokemon follows you; black out and
  you're back home with Mom.
- Local co-op: split screen (Red and Leaf). Each player's wild battle, Healing Center and Mart run in
  their own half on their own keys while the other keeps walking. Story battles pull both players in.
- Online co-op: each walks on their own screen and sees the other. Wild battles are your own; stand
  next to your partner while they battle and press Enter to watch it live. A story trainer that spots
  one of you calls the other over and starts the 2-player battle.
- Team up: face your partner and press Enter (online they say yes / no). Teamed up, tall grass is a
  2v2 wild battle for both of you.
- Exp. All (Mart, 5000 coins): in co-op, your partner's party gets half the XP of your solo wild battles.

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

**Gym centerpieces (src/battle_center.cpp)** – each of the eight gyms has one big interactive set piece in the
middle:
- Brock: the **Great Boulder**. A heavy hit (or a Rock/Ground/Fighting/Steel hit) knocks it loose and it rolls,
  smashing pillars and crushing anyone in its path – you included – until it shatters; a new one rises later.
- Misty: the **Whirlpool Drain** opens every so often and drags everyone toward its damaging eye (Water types
  resist it). Ice freezes it shut; Electric during the whirlpool shocks everyone caught in it.
- Surge: the **Generator** charges from hits (Electric most) and slowly by itself, crackles at anyone leaning on
  it, then overloads in a paralysing shockwave that supercharges Electric types. A Ground hit drains it.
- Erika: the **Giant Flower** opens and puffs sleep pollen; Grass types heal in it. Fire burns it down for a while;
  knocking it makes it open early.
- Koga: the **Sludge Vat** – hit it when full and it bursts in the direction you hit it, poisoning your foes and
  leaving toxic puddles that only hurt them.
- Sabrina: the **Psychic Orb** fights for whoever hit it last, firing at their foes; Dark moves knock it out.
- Blaine: the **Volcano** rumbles, marks where the lava bombs will land, then erupts. Water calms it, Fire sets it
  off early.
- Giovanni: the **Turntable** carries anyone standing on it round and round and flings them off when it reverses.
  Ground moves (Earthquake...) jam it.

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
| Skyway Gym (Falkner) | A wooden deck in high wind. Every 6–9 s the windsocks swing round (1.6 s warning), then a 3 s gust shoves everyone on the ground (half as much for Steel and Rock types; not flyers or Flying types). Flying types ×1.25 speed. **Weather vane**: hit it and for 15 s the gusts blow from your side at your foes only, and give your team a tailwind. |
| Hive Gym (Bugsy) | Five spider webs: non-Bug types ×0.55 speed on them, Bug types ×1.25 speed and ×1.15 ATK. Fire burns a web away for good. Trees. **Hive**: when it's full, a hit sends a homing swarm of six Bug shots (power 22) at the hitter's foes; Fire smokes it out for 12 s. |
| Moomoo Gym (Whitney) | Hay bales for cover (break them; fire burns them fast). Normal types ×1.15 ATK. **Milk tank**: when it's full, a hit spills three pools of Moomoo Milk by the hitter for 8 s: 2.4% max HP/s to anyone standing in them, Normal types double. |
| Lighthouse Gym (Jasmine) | Steel pillars. **Lighthouse**: its beam sweeps the floor; in the light Steel types get ×1.25 ATK and everyone else is dazzled (×0.75 speed). A hit reverses it and spins it faster for a while. |
| Shadow Chamber (Karen) | Lights out but for three drifting pools of light. Out of the light Dark types get ×1.2 ATK and ×1.15 speed (their eyes glow); in the light everyone else gets ×1.1 ATK. |
| Champion's Hall (Lance, Johto) | The Champion's spotlight plus four dragon statues breathing fire (like the Dragon's Den). |
| Balance Gym (Norman) | Four rooms: Speed (×1.25 speed), Defense (×1.25 DEF), Recovery (1% max HP/s) and Strength (×1.2 ATK) for whoever stands in them; Normal types also get half of the two rooms next door. **Balance scale**: a hit tips it your way for 10 s, and your foes' rooms work against them (slower, softer, weaker, draining). |
| Twin Moons Gym (Tate & Liza) | A double battle on a floor split into a sun half and a moon half. In the sun Fire and Psychic moves ×1.15; in the moon Psychic moves ×1.15 and everyone else ×0.9 speed. Psychic types ×1.15 ATK anywhere; Solrock heals in the sun, Lunatone in the moon. Every 12 s an eclipse swaps the halves (1.5 s warning). **Sun–moon dial**: a hit brings the eclipse now; a Dark hit jams it for 10 s. |
| Sootopolis Gym (Juan) | Cracked ice over a crater lake: stand on a tile 2 s and it cracks, 2 s more and it breaks (3% max HP and you're in the water unless you're a Water type). In the water Water types swim (×1.35 speed, ×1.15 ATK) and everyone else wades (×0.6). Tiles freeze again after 15 s, or at once under an Ice move. **Rain font**: a hit brings 12 s of rain for your side (Water ×1.25, Fire ×0.75); Grass or Electric stops it. |
| Waterfall Hall (Wallace) | The Champion's spotlight (Water types get ×1.3 in it) and six shallow pools (Water types ×1.17 speed, others ×0.8). Every 15–20 s the waterfall on the back wall surges (1.6 s warning): a wave rolls across the floor pushing everyone, then a rain shower for 8 s. **Crystal**: a hit sends the surge rolling away from you. |
| Meteor Falls (Steven) | Meteorites for cover; Steel and Rock types next to one ×1.15 DEF. Every 6 s two meteor fragments fall on markers near anyone (Rock 55, both sides). **Great meteorite**: a heavy hit cracks it open and your side's special moves get ×1.2 for 10 s; it reforms after 25 s. |

| Seafoam Depths (Articuno) | Ice rules (rinks, ice pillars) and endless snow. Every 5 s two hailstones fall on markers near the challengers (Ice 50, 10% freeze). |
| Power Plant (Zapdos) | Electric coils and rain. Every 4.5 s two lightning bolts strike near the challengers (Electric 55, 30% paralysis). |
| Moltres's Nest | Victory Road's lava pools under a blazing sun. Every 5 s two burning rocks drop near the challengers (Fire 50, 30% burn). |
| Cerulean Cave (Mewtwo) | Deep pools slow you, crystal pillars, Mewtwo hurls a psychic boulder every 3.5 s (Psychic 60) and sends a psychic pulse rippling out every 9–12 s. |
| Faraway Island (Mew) | Jungle: trees and tall grass. Mew teleports to a random patch of grass every 8–11 s, and in the grass it's ×1.1 faster and heals 0.15% HP/s. Latias and Latios use it as Southern Island. |
| Desert Ruins (Regirock) | A sandstorm that never ends (0.5% HP/s to challengers that aren't Rock, Ground or Steel). Every 5 s two boulders fall near the challengers (Rock 50). Regice uses Seafoam Depths. |
| Ancient Tomb (Registeel) | Dark stone and the lighthouse beam: in the light Steel types hit harder and everyone else slows. Every 5 s two steel shards fall (Steel 50). |
| Marine Cave (Kyogre) | Deep water (Cascade rules, rock ledges) under heavy rain. Every 4.5 s two water spouts hit near the challengers (Water 55, a big push). Groudon's Terra Cave is Moltres's Nest under its sun. |
| Sky Pillar (Rayquaza) | Crumbling tiles: stand still 2 s and a tile cracks, 2 s more and you fall through (5% max HP, back up beside the hole); holes settle after 12 s. Skyway gusts, and two dragon meteors every 5 s (Dragon 55). |
| Birth Island (Deoxys) | Bare rock and a red triangle. Deoxys teleports across the island every 8–11 s and psychic shards fall near the challengers (Psychic 55). Jirachi is met at Meteor Falls, where the meteors only hit the challengers. |

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

Combat links (src/battle_combat.cpp):
- **Sweep** – a beam that swings across an arc while the user stands rooted (Hyper Beam, Flamethrower,
  Hydro Pump, Solar Beam, Roar of Time...). Each foe is hit once; jump over it, dodge through it or duck
  behind a rock. Recharge moves start their recharge once the sweep ends.
- **Tether** – a line latched onto one foe. Drain tethers (Absorb, Mega/Giga Drain, Dream Eater – sleeping
  targets only, Parabolic Charge, Oblivion Wing) heal while they tick; binding tethers (Wrap, Bind, Clamp,
  Thunder Cage, Infestation) hold the foe on a leash; Mean Look pins it so it can't switch out. Running out of
  reach, an obstacle in the line, a dodge or a switch breaks it.
- **Grapple** – a vine/tongue/chain shot that yanks the foe toward the user on a hit (Vine Whip, Power Whip,
  Lick, Anchor Shot and Spirit Shackle – which also pin – and Smack Down, which knocks jumpers out of the air).
- **Roll** – curl up and roll, steering a little, ricocheting off walls, rocks and foes like a pinball; each bounce
  makes it faster and stronger (Rollout, Ice Ball, Steamroller, Steel Roller, Gyro Ball, Rapid Spin, Flame Wheel).
  Rapid Spin and Steel Roller sweep away hazards; Flame Wheel leaves a trail of fire. A Protect stops it dead.
- **Throw** – grab a foe in reach and hurl it (Seismic Toss, Vital Throw, Storm Throw, Circle Throw,
  Submission, Sky Drop – lift and drop). Slamming into an obstacle (x1.35), the arena wall (x1.3) or a hazard
  (x1.2) does extra damage. Ghosts can't be grabbed by Fighting throws; legendaries barely move.

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

Shots leave a tapered motion streak, hits flash with a shock ring, stat moves show chevrons in the stat's
colour (Attack red, Defence blue, Speed green) rising through a column of light, healing shows green crosses,
and everything casts a soft contact shadow.

**Battle animation (src/battle_anim.cpp)** – a visual-only layer on top of every move (it never touches the
battle itself, so online games stay in step). The user shows body language for the move's kind: it crouches
and trembles while winding up as energy gathers into it, recoils and stretches when it fires a shot or beam,
lunges with after-images and speed lines into melee moves, and hops and swells for bursts, waves, weather and
stat moves. Every move gets a casting circle on the ground in its type's colour and a flourish themed on its
type (flames, spray, crackling arcs, spiralling leaves, frost, dust, feathers, psychic motes, wisps, smoke,
glints, sparkles...) sized by its power; shots get a muzzle flash. The attacker's element splashes off
whatever it hits, and the target flinches. Hits that take a big bite of HP get an impact frame (a flash, a
ring and radial speed lines) and the camera punches in toward them, more for a knockout. The strongest moves
(120+ power, 100+ beams, Explosion) get a cinematic: the field dims, letterbox bars close in and energy rushes
into the user during the wind-up. Camera moves follow the Screen shake setting.

Beams flow rather than sit as bars: Flamethrower is a rolling stream of fire (white-hot at the mouth,
orange to red as it curls upward, embers and smoke), Overheat a broader blazing stream with heat pulses and
haze, Blue Flare a fast, tight sapphire stream with white sparks and rising blue wisps, Hydro Pump and other
Water beams a gushing torrent with white water, streaks and spray, Dragon beams two streams twisting around a
white-hot core. Breath and spray moves (Inferno, Frost Breath, Belch, Sand Attack...) billow out as a cone of
their element; sound cones roll out as arcs. Protect shows a shimmering hex barrier, Dig a moving mound of
earth, Fly a shadow sweeping the ground, teleports flash where they leave and land, and falling attacks
(Rock Slide, Blizzard, Draco Meteor) cast a growing shadow with a turning target mark.

**Presentation** – Pokemon are thrown out in a Poke Ball that arcs in and pops open in a flash, and are
recalled as red light into the ball. They breathe while standing, step with a little spring, reflect in
water, puddles and ice, and the winners hop about under falling confetti. HP bars drain with a red trail
showing the damage just taken and pulse when low; damage numbers pop out (super-effective hits bigger).
READY / FIGHT / WIN banners sweep in with a shine, the opening camera sweeps across the field, and the
final knockout plays out in slow motion (offline). Outdoor scenery comes alive: clouds drift, the sea
glints, embers rise from volcanoes, leaves or petals blow across the field by season, butterflies in summer,
cave drips and crystal glints, blinking lab lights and flickering candles. Gym crowds have proper fans
cheering with signs and flags. Screens open with a Poke Ball iris; panels everywhere have rounded,
shaded edges. Catching shows the Pokemon pulled in as red light, the ball dropping and bouncing, three
shakes with a blinking button and a star burst on the click. Evolution has a swirling backdrop that speeds
up, a white burst and sparkles around the new form.
**Appearances and matchups** – a shiny foe or a legendary gets a banner as it enters: "A Shiny Gyarados Has
Appeared!" with its sprite in a glowing medallion (sparkles for shinies, golden lightning for legendaries).
Super-effective hits throw out the attacking type's emblem with a camera punch; "No effect" leaves a grey
puff and a crossed-out ring. Trainer battles open with a VS splash: the two trainers' colours slam
together with their names and a Poke Ball per Pokemon.
**Battle HUD** – slanted see-through name plates; move buttons tinted by type, with a clock sweep and
seconds left while a move recharges, and a glint when one is ready.
**Results** – the title sweeps in, rows slide in one by one, XP bars fill (and refill with LEVEL UP! when a
level is gained), coins count up, the Pokemon that dealt the most damage gets an MVP card, and a new
gym badge drops into an opening badge case. The Hall of Fame flashes a camera for each Pokemon.
**Menus** – animated backgrounds (drifting Poke Ball shapes and light bands); items slide in one after
another when a screen opens and pop when picked. The hub's sky follows the time of day and season
(stars and a moon at night; petals, summer motes, falling leaves or snow; fireflies on summer nights), its
menu is a column of tiles with icons, and the lead Pokemon hops. Party & Box: sliding tabs, rows tinted by
type, a seasonal Box wallpaper, the portrait on a type-coloured backdrop that hops when picked, and a
hexagon chart of the stats.
**Font** – all text uses Pixelify Sans (SIL Open Font License, see fonts/OFL.txt), baked into the exe from
src/font_data.cpp (made by tools/embed_font.py). Every text call goes through GDrawText / GMeasureText in
src/ui.cpp; size-10 text is drawn at 12 px so layouts keep raylib's metrics.
Settings: **Effects intensity** (Full / Reduced: no camera moves, slow motion or big flashes / Minimal: also
fewer particles and no cinematics) and **Screen filter** (Off / Soft glow / Retro CRT).

## Arena floors and weather (src/arena.cpp, src/battle_vfx.cpp)

Battle floors are baked once per battle at full canvas resolution (1920x1080, on a few threads) from
procedural surfaces sampled in world units: grass blades with flowers or fallen leaves by season, dirt
paths with pebbles and ruts, beveled stone, tile and metal plates, wood planks with grain and nails,
cracked rock slabs, ice with frost and cracks, basalt with glowing lava seams, sand ripples and a foamy
shore, tatami, carpet and marble. A baked light pass adds shade under the walls, a soft vignette and
drifting cloud light outdoors. Weather is drawn over the field in layers: three depths of slanting rain
with splashes and far-off lightning, drifting snow in three sizes, rolling sandstorm banks with streaking
grains, and harsh sun with light shafts, motes and heat shimmer. Night darkens the edges and brings out stars.

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

**Quick Select (solo Story Mode)** – hold Swap (RT) for about half a second: the battle pauses and a
wheel of your team appears. Point at a Pokemon with the right stick (or left stick / WASD) and press A to
switch, B to cancel. On the keyboard, 1–6 switch straight to that party slot. If the swap is still
cooling down, the chosen Pokemon comes in as soon as it's ready. A quick tap still swaps to the next one.

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
| `src/battle_center.cpp` | the gym centerpieces (behaviour and drawing) |
| `src/battle_combat.cpp` | sweeping beams, tethers, grapples and throws, and their drawing |
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
