The original pixel-art charts for each Pokemon (as downloaded), kept here so the game files stay small.
tools/spritepack.cpp turns them into the one-pixel-per-square sprites in Pokemon/<Name>/:
    spritepack art_source/Pokemon Pokemon [Name]
The game itself can still read charts dropped straight into its Pokemon folder.
