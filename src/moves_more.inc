// Every other move from the main-series games (Gen 2-9 and Let's Go), so any Pokemon's real
// learnset can be used. Z-Moves and Max Moves are left out. Included from Build() in moves.cpp.
// Moves whose effect has no real-time equivalent copy the nearest existing move (like()).

    // ---------------- Normal
    add(B("Morning Sun", T::Normal, K::Heal, 0, 14).heal(0.5f));
    add(B("Natural Gift", T::Normal, K::Projectile, 60, 1.6f).phys());
    add(B("Trump Card", T::Normal, K::Projectile, 60, 1.6f).spec());
    add(B("Wring Out", T::Normal, K::Melee, 60, 1.6f).spec());
    add(B("Captivate", T::Normal, K::Burst, 0, 6).radius(70).windup(0.15f).low(St::Atk, 1));
    add(B("Judgment", T::Normal, K::Projectile, 100, 4.0f).radius(6));
    add(B("Chip Away", T::Normal, K::Melee, 70, 2.0f));
    add(B("Tail Slap", T::Normal, K::Melee, 25, 1.6f).hits(4));
    add(B("Head Charge", T::Normal, K::Dash, 120, 5.3f).recoil(0.25f));
    add(B("Techno Blast", T::Normal, K::Beam, 120, 5.3f).windup(0.5f));
    add(B("Relic Song", T::Normal, K::Wave, 75, 2.2f).st(S::Sleep, 0.1f));
    add(B("Hold Back", T::Normal, K::Melee, 40, 1.0f).fswipe());
    add(B("Noble Roar", T::Normal, K::Burst, 0, 6).radius(70).windup(0.15f).low(St::Atk, 1));
    add(B("Confide", T::Normal, K::Burst, 0, 6).radius(70).windup(0.15f).low(St::Atk, 1));
    add(B("Tearful Look", T::Normal, K::Burst, 0, 6).radius(70).windup(0.15f).low(St::Atk, 1));
    add(B("Revelation Dance", T::Normal, K::Projectile, 90, 2.9f));
    add(B("Multi-Attack", T::Normal, K::Dash, 120, 5.3f));
    add(B("Veevee Volley", T::Normal, K::Dash, 90, 2.9f));
    add(B("Stuff Cheeks", T::Normal, K::Buff, 0, 10).buff(St::Def, 1.7f));
    add(B("Terrain Pulse", T::Normal, K::Projectile, 50, 1.2f));
    add(B("Population Bomb", T::Normal, K::Melee, 20, 1.5f).hits(5));
    add(B("Tera Blast", T::Normal, K::Projectile, 80, 2.4f).spec());
    add(B("Tidy Up", T::Normal, K::Buff, 0, 10).buff(St::Atk, 1.5f).buff2(St::Spd));
    add(B("Fillet Away", T::Normal, K::Buff, 0, 10).buff(St::Atk, 1.7f).buff2(St::Spd));
    add(B("Hyper Drill", T::Normal, K::Dash, 100, 4.0f));
    add(B("Blood Moon", T::Normal, K::Beam, 140, 6.9f).windup(0.5f).radius(7));
    add(B("Tera Starstorm", T::Normal, K::Beam, 120, 5.3f).windup(0.5f));
    add(B("Nature Power", T::Normal, K::Projectile, 80, 2.4f).spec());
    add(B("Camouflage", T::Normal, K::Buff, 0, 10).buff(St::Def, 1.5f));
    add(B("Power Shift", T::Normal, K::Buff, 0, 10).buff(St::Atk, 1.5f));
    add(B("Revival Blessing", T::Normal, K::Heal, 0, 14).heal(0.5f).uses(1));
    add(B("Teatime", T::Normal, K::Heal, 0, 14).heal(0.25f));
    add(B("Court Change", T::Normal, K::Burst, 0, 9).radius(70).haze());
    add(B("Skill Swap", T::Psychic, K::Chain, 0, 9).range(150).abfx(1));
    add(B("Entrainment", T::Normal, K::Chain, 0, 9).range(150).abfx(2));
    add(B("Simple Beam", T::Normal, K::Chain, 0, 9).range(150).abfx(3));
    add(B("Doodle", T::Normal, K::Chain, 0, 9).range(150).abfx(4));
    add(B("Psycho Shift", T::Psychic, K::Chain, 0, 7).range(150).abfx(5));
    add(B("Quash", T::Dark, K::Burst, 0, 6).radius(70).windup(0.15f).low(St::Spd, 1));

    // ---------------- Fire
    add(B("V-create", T::Fire, K::Dash, 180, 9.0f).self(St::Def));
    add(B("Fusion Flare", T::Fire, K::Projectile, 100, 4.0f).radius(6));
    add(B("Burning Jealousy", T::Fire, K::Spread, 70, 2.0f).st(S::Burn, 0.3f));
    add(B("Raging Fury", T::Fire, K::Dash, 120, 5.3f));
    add(B("Armor Cannon", T::Fire, K::Beam, 120, 5.3f).windup(0.5f).self(St::Def));
    add(B("Bitter Blade", T::Fire, K::Melee, 90, 2.9f).drain(0.5f));
    add(B("Blazing Torque", T::Fire, K::Dash, 80, 2.4f).st(S::Burn, 0.3f));
    add(B("Temper Flare", T::Fire, K::Melee, 75, 2.2f));
    add(B("Sizzly Slide", T::Fire, K::Dash, 60, 1.6f).st(S::Burn, 1.0f));
    add(B("Shell Trap", T::Fire, K::Burst, 150, 7.8f).radius(50).windup(0.6f));
    add(B("Mind Blown", T::Fire, K::Burst, 150, 7.8f).radius(60).windup(0.5f).recoil(0.5f));
    add(B("Burning Bulwark", T::Fire, K::Shield, 0, 6));

    // ---------------- Water
    add(B("Origin Pulse", T::Water, K::Wave, 110, 4.6f));
    add(B("Bouncy Bubble", T::Water, K::Projectile, 60, 1.6f).drain(0.5f));
    add(B("Splishy Splash", T::Water, K::Wave, 90, 2.9f).st(S::Paralyze, 0.3f));
    add(B("Fishious Rend", T::Water, K::Melee, 85, 2.7f));
    add(B("Surging Strikes", T::Water, K::Melee, 25, 1.8f).hits(3).crit());
    add(B("Triple Dive", T::Water, K::Melee, 30, 1.6f).hits(3));
    add(B("Aqua Step", T::Water, K::Dash, 80, 2.4f));
    add(B("Hydro Steam", T::Water, K::Projectile, 80, 2.4f));
    add(B("Aqua Cutter", T::Water, K::Projectile, 70, 2.0f).phys().crit());

    // ---------------- Electric
    add(B("Bolt Strike", T::Electric, K::Dash, 130, 6.1f).st(S::Paralyze, 0.2f));
    add(B("Zippy Zap", T::Electric, K::Dash, 80, 1.8f).quick().crit());
    add(B("Pika Papow", T::Electric, K::Chain, 90, 2.9f));
    add(B("Buzzy Buzz", T::Electric, K::Projectile, 60, 1.6f).st(S::Paralyze, 1.0f));
    add(B("Bolt Beak", T::Electric, K::Melee, 85, 2.7f));
    add(B("Wildbolt Storm", T::Electric, K::Wave, 100, 4.0f).st(S::Paralyze, 0.2f));
    add(B("Electro Drift", T::Electric, K::Dash, 100, 4.0f).spec());
    add(B("Double Shock", T::Electric, K::Dash, 120, 5.3f));
    add(B("Electro Shot", T::Electric, K::Beam, 130, 6.1f).windup(0.8f));
    add(B("Thunderclap", T::Electric, K::Chain, 70, 1.5f).windup(0.03f));
    add(B("Electrify", T::Electric, K::Projectile, 0, 6).speed(170).homing(2).st(S::Paralyze, 0.5f));

    // ---------------- Grass
    add(B("Cotton Guard", T::Grass, K::Buff, 0, 10).buff(St::Def, 1.8f));
    add(B("Strength Sap", T::Grass, K::Heal, 0, 12).heal(0.3f));
    add(B("Solar Blade", T::Grass, K::Melee, 125, 5.7f).windup(0.8f));
    add(B("Leafage", T::Grass, K::Projectile, 40, 1.0f).phys());
    add(B("Trop Kick", T::Grass, K::Melee, 70, 2.0f).low(St::Atk, 1.0f));
    add(B("Sappy Seed", T::Grass, K::Projectile, 100, 4.0f).phys().st(S::Seed, 1.0f));
    add(B("Snap Trap", T::Grass, K::Trap, 35, 4.0f));
    add(B("Branch Poke", T::Grass, K::Melee, 40, 1.0f));
    add(B("Grav Apple", T::Grass, K::Lob, 80, 2.4f).low(St::Def, 1.0f));
    add(B("Jungle Healing", T::Grass, K::Heal, 0, 14).heal(0.25f).cure());
    add(B("Chloroblast", T::Grass, K::Beam, 150, 7.8f).windup(0.6f).recoil(0.5f));
    add(B("Spicy Extract", T::Grass, K::Burst, 0, 6).radius(70).windup(0.15f).low(St::Def, 1));
    add(B("Flower Trick", T::Grass, K::Projectile, 70, 2.0f).phys().crit());
    add(B("Matcha Gotcha", T::Grass, K::Spread, 80, 2.4f).drain(0.5f).st(S::Burn, 0.2f));
    add(B("Syrup Bomb", T::Grass, K::Lob, 60, 1.6f).low(St::Spd, 1.0f));
    add(B("Ivy Cudgel", T::Grass, K::Melee, 100, 4.0f).crit());
    add(B("Rototiller", T::Ground, K::Buff, 0, 10).buff(St::Atk, 1.5f));
    add(B("Flower Shield", T::Fairy, K::Buff, 0, 10).buff(St::Def, 1.5f));

    // ---------------- Ice
    add(B("Freezy Frost", T::Ice, K::Projectile, 100, 4.0f).haze());
    add(B("Glacial Lance", T::Ice, K::Rain, 120, 5.3f).phys());

    // ---------------- Fighting
    add(B("Flying Press", T::Fighting, K::Leap, 100, 4.0f));
    add(B("No Retreat", T::Fighting, K::Buff, 0, 12).buff(St::Atk, 1.5f).buff2(St::Def).uses(1));
    add(B("Meteor Assault", T::Fighting, K::Dash, 150, 7.8f).recharge(1.5f));
    add(B("Coaching", T::Fighting, K::Buff, 0, 10).buff(St::Atk, 1.5f).buff2(St::Def));
    add(B("Thunderous Kick", T::Fighting, K::Melee, 90, 2.9f).low(St::Def, 1.0f));
    add(B("Triple Arrows", T::Fighting, K::Projectile, 90, 2.9f).phys().crit().low(St::Def, 0.5f));
    add(B("Victory Dance", T::Fighting, K::Buff, 0, 10).buff(St::Atk, 1.5f).buff2(St::Def));
    add(B("Octolock", T::Fighting, K::Vortex, 10, 6).dur(3).pull(60).low(St::Def, 0.5f));
    add(B("Upper Hand", T::Fighting, K::Melee, 65, 1.3f).windup(0.03f).st(S::Flinch, 1.0f));
    add(B("Combat Torque", T::Fighting, K::Dash, 100, 4.0f).st(S::Paralyze, 0.3f));

    // ---------------- Psychic
    add(B("Heart Stamp", T::Psychic, K::Melee, 60, 1.6f).st(S::Flinch, 0.3f));
    add(B("Prismatic Laser", T::Psychic, K::Beam, 160, 8.7f).windup(0.6f).recharge(1.5f).radius(8));
    add(B("Photon Geyser", T::Psychic, K::Beam, 100, 4.0f).windup(0.4f));
    add(B("Glitzy Glow", T::Psychic, K::Projectile, 80, 2.4f));
    add(B("Eerie Spell", T::Psychic, K::Wave, 80, 2.4f));
    add(B("Mystical Power", T::Psychic, K::Projectile, 70, 2.0f));
    add(B("Psyblade", T::Psychic, K::Melee, 80, 2.4f));
    add(B("Twin Beam", T::Psychic, K::Projectile, 40, 1.4f).hits(2));
    add(B("Psychic Noise", T::Psychic, K::Projectile, 75, 2.2f));
    add(B("Freezing Glare", T::Psychic, K::Projectile, 90, 2.9f).st(S::Freeze, 0.1f));
    add(B("Power Trick", T::Psychic, K::Buff, 0, 10).buff(St::Def, 1.5f));
    add(B("Purify", T::Poison, K::Heal, 0, 14).heal(0.5f).cure());
    add(B("Lunar Blessing", T::Psychic, K::Heal, 0, 14).heal(0.25f).cure());
    add(B("Take Heart", T::Psychic, K::Buff, 0, 10).buff(St::Atk, 1.5f).buff2(St::Def));
    add(B("Magic Powder", T::Psychic, K::Projectile, 0, 6).speed(170).homing(2).low(St::Def, 1));

    // ---------------- Dark
    add(B("Hyperspace Fury", T::Dark, K::Blink, 100, 4.0f).self(St::Def));
    add(B("Baddy Bad", T::Dark, K::Projectile, 80, 2.4f));
    add(B("Lash Out", T::Dark, K::Melee, 75, 2.2f));
    add(B("Ceaseless Edge", T::Dark, K::Melee, 65, 1.8f).crit());
    add(B("Ruination", T::Dark, K::Projectile, 1, 6).fixed(-2));
    add(B("Comeuppance", T::Dark, K::Melee, 80, 2.4f));
    add(B("Wicked Torque", T::Dark, K::Dash, 80, 2.4f).st(S::Sleep, 0.1f));
    add(B("Parting Shot", T::Dark, K::Burst, 0, 6).radius(70).windup(0.15f).low(St::Atk, 1));
    add(B("Power Trip", T::Dark, K::Melee, 40, 1.0f));

    // ---------------- Poison
    add(B("Venom Drench", T::Poison, K::Burst, 0, 6).radius(70).windup(0.15f).low(St::Atk, 1));
    add(B("Noxious Torque", T::Poison, K::Dash, 100, 4.0f).st(S::Poison, 0.3f));
    add(B("Malignant Chain", T::Poison, K::Projectile, 100, 4.0f).st(S::Toxic, 0.5f));
    add(B("Corrosive Gas", T::Poison, K::Cone, 0, 7).low(St::Def, 1));

    // ---------------- Steel, Rock, Ground, Dragon, Fairy, Ghost, Bug, Flying
    add(B("Gear Up", T::Steel, K::Buff, 0, 10).buff(St::Atk, 1.5f));
    add(B("Shelter", T::Steel, K::Buff, 0, 10).buff(St::Def, 1.7f));
    add(B("Hard Press", T::Steel, K::Melee, 80, 2.4f));
    add(B("Gigaton Hammer", T::Steel, K::Melee, 160, 7.0f).windup(0.3f));
    add(B("Tar Shot", T::Rock, K::Burst, 0, 6).radius(70).windup(0.15f).low(St::Spd, 1));
    add(B("Mighty Cleave", T::Rock, K::Melee, 95, 3.2f));
    add(B("Thousand Waves", T::Ground, K::Wave, 90, 2.9f).phys());
    add(B("Sandsear Storm", T::Ground, K::Wave, 100, 4.0f).st(S::Burn, 0.2f));
    add(B("Eternabeam", T::Dragon, K::Beam, 160, 8.7f).windup(0.7f).recharge(1.5f).radius(9));
    add(B("Fickle Beam", T::Dragon, K::Beam, 80, 2.4f).windup(0.35f));
    add(B("Clangorous Soul", T::Dragon, K::Buff, 0, 12).buff(St::Atk, 1.5f).buff2(St::Def));
    add(B("Nature's Madness", T::Fairy, K::Projectile, 1, 6).fixed(-2));
    add(B("Sparkly Swirl", T::Fairy, K::Beam, 120, 5.3f).windup(0.5f));
    add(B("Magical Torque", T::Fairy, K::Dash, 100, 4.0f).st(S::Confuse, 0.3f));
    add(B("Spectral Thief", T::Ghost, K::Melee, 90, 2.9f));
    add(B("Silk Trap", T::Bug, K::Shield, 0, 6));
    add(B("Obstruct", T::Dark, K::Shield, 0, 6));
    add(B("Powder", T::Bug, K::Projectile, 0, 6).speed(170).homing(2).low(St::Atk, 1));

    // ---------------- Moves with no real-time equivalent: nearest existing move
    like("Sketch", "Mimic", T::Normal);
    like("Assist", "Metronome", T::Normal);
    like("Me First", "Copycat", T::Normal);
    like("Instruct", "Copycat", T::Psychic);
    like("Snatch", "Copycat", T::Dark);
    like("Foresight", "Lock-On", T::Normal);
    like("Odor Sleuth", "Lock-On", T::Normal);
    like("Miracle Eye", "Lock-On", T::Psychic);
    like("Telekinesis", "Lock-On", T::Psychic);
    like("Heal Bell", "Aromatherapy", T::Normal);
    like("Lucky Chant", "Safeguard", T::Normal);
    like("Heart Swap", "Power Swap", T::Psychic);
    like("Power Split", "Power Swap", T::Psychic);
    like("Speed Swap", "Power Swap", T::Psychic);
    like("Topsy-Turvy", "Power Swap", T::Dark);
    like("Guard Split", "Guard Swap", T::Psychic);
    like("Magic Room", "Wonder Room", T::Psychic);
    like("Heal Block", "Embargo", T::Psychic);
    like("Spotlight", "Follow Me", T::Normal);
    like("Shed Tail", "Substitute", T::Normal);
    like("Chilly Reception", "Snowscape", T::Ice);
    like("Ion Deluge", "Electric Terrain", T::Electric);
    like("Misty Terrain", "Grassy Terrain", T::Fairy);
    like("Psychic Terrain", "Wonder Room", T::Psychic);
    like("Dragon Cheer", "Focus Energy", T::Dragon);
    like("Celebrate", "Splash", T::Normal);
    like("Hold Hands", "Splash", T::Normal);
    like("Happy Hour", "Splash", T::Normal);
    like("Bestow", "Splash", T::Normal);
    like("Trick-or-Treat", "Leer", T::Ghost);
    like("Forest's Curse", "Leer", T::Grass);
