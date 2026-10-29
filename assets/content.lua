-- =============================================================================
--  towerfall — game content
--
--  The C++ core owns the simulation. This file owns the CONTENT: the levels,
--  the tower balance table and the enemy roster. Change a number here and the
--  game changes — no recompile.
--
--  Returned table:
--    levels  = { { name, difficulty, rows } }
--    towers  = { { id, name, description, mode, levels = { {...} } } }
--    enemies = { { id, name, archetype, hp, speed, ... } }
-- =============================================================================

return {

  -- ---------------------------------------------------------------------------
  --  LEVELS
  --
  --  Terrain characters:
  --    '#'  rock      — blocks movement, cannot build
  --    '~'  water     — blocks movement, cannot build
  --    ','  sand      — buildable, slightly slower to cross
  --    '.'  grass     — buildable
  --    'S'  entry     — where enemies come in
  --    'E'  exit      — what the enemies are trying to reach
  --    anything else is treated as path
  --
  --  IMPORTANT: path tiles are NOT protected. You may build on them, and you
  --  may wall them off — you just may not wall the route off completely.
  -- ---------------------------------------------------------------------------
  levels = {

    {
      name = "Meadow Run",
      difficulty = "easy",
      rows = {
        "########################",
        "#......................#",
        "#..####..........####..#",
        "#..####..........####..#",
        "#......................#",
        "#......................#",
        "S..............E.......#",
        "#......................#",
        "#......................#",
        "#..####..........####..#",
        "#..####..........####..#",
        "#......................#",
        "#......................#",
        "########################",
      },
    },

    {
      name = "Stone Pass",
      difficulty = "normal",
      rows = {
        "########################",
        "#......................#",
        "#....~~~~......~~~~....#",
        "#....~~~~......~~~~....#",
        "#......................#",
        "#..##..............##..#",
        "S..##....######....##..E",
        "#..##....######....##..#",
        "#......................#",
        "#......................#",
        "#....~~~~......~~~~....#",
        "#....~~~~......~~~~....#",
        "#......................#",
        "########################",
      },
    },

    {
      name = "Serpent Hollow",
      difficulty = "hard",
      rows = {
        "##########################",
        "#........................#",
        "#..####..........####....#",
        "#..####..........####....#",
        "#........................#",
        "#.....##........##.......#",
        "S.....##........##.......E",
        "#.....##........##.......#",
        "#........................#",
        "#........................#",
        "#....####........####....#",
        "#....####........####....#",
        "#........................#",
        "#........................#",
        "#........................#",
        "##########################",
      },
    },
  },

  -- ---------------------------------------------------------------------------
  --  TOWERS
  --
  --  Each tower has 1..3 levels. Level 1 is the build cost; upgrading to
  --  level N costs that level's `cost`.
  --
  --    mode          single | splash | chain | pierce | aura
  --    range         in tiles
  --    fireRate      shots per second
  --    splashRadius  in tiles
  -- ---------------------------------------------------------------------------
  towers = {

    {
      id = "arrow", name = "Archer Post",
      description = "Rapid single-target arrows. Cheap, reliable, always useful.",
      upgradeNote = "Sharper arrows, longer reach.",
      projectileSpeed = 16.0, mode = "single",
      levels = {
        { cost = 60,  damage = 9,  range = 3.2, fireRate = 1.8 },
        { cost = 45,  damage = 15, range = 3.6, fireRate = 2.1 },
        { cost = 90,  damage = 26, range = 4.0, fireRate = 2.5 },
      },
    },

    {
      id = "cannon", name = "Mortar Pit",
      description = "Lobs shells that splash. Slow to reload, devastating to packs.",
      upgradeNote = "Bigger shell, wider blast.",
      projectileSpeed = 9.0, mode = "splash",
      levels = {
        { cost = 110, damage = 26, range = 2.8, fireRate = 0.65, splashRadius = 1.2 },
        { cost = 95,  damage = 46, range = 3.1, fireRate = 0.75, splashRadius = 1.5 },
        { cost = 180, damage = 78, range = 3.5, fireRate = 0.85, splashRadius = 1.9 },
      },
    },

    {
      id = "frost", name = "Frost Spire",
      description = "Chills enemies, slowing them so the rest of your towers get more shots.",
      upgradeNote = "Colder: slower and longer-lasting chill.",
      projectileSpeed = 13.0, mode = "single",
      levels = {
        { cost = 90,  damage = 6,  range = 2.9, fireRate = 1.1,  slowFactor = 0.40, slowDuration = 2.0 },
        { cost = 70,  damage = 9,  range = 3.2, fireRate = 1.2,  slowFactor = 0.50, slowDuration = 2.5 },
        { cost = 140, damage = 14, range = 3.6, fireRate = 1.35, slowFactor = 0.62, slowDuration = 3.2 },
      },
    },

    {
      id = "tesla", name = "Tesla Coil",
      description = "Lightning arcs to nearby enemies after hitting the first.",
      upgradeNote = "More jumps, more damage per arc.",
      projectileSpeed = 30.0, mode = "chain",
      levels = {
        { cost = 130, damage = 20, range = 2.6, fireRate = 1.0,  chainTargets = 2 },
        { cost = 110, damage = 33, range = 2.8, fireRate = 1.15, chainTargets = 3 },
        { cost = 210, damage = 52, range = 3.1, fireRate = 1.3,  chainTargets = 5 },
      },
    },

    {
      id = "sniper", name = "Longshot",
      description = "Enormous range and damage, but it reloads slowly. Line it up on bosses.",
      upgradeNote = "More range and a heavier round.",
      projectileSpeed = 40.0, mode = "single",
      levels = {
        { cost = 150, damage = 95,  range = 7.5, fireRate = 0.42 },
        { cost = 130, damage = 165, range = 8.2, fireRate = 0.48 },
        { cost = 260, damage = 280, range = 9.0, fireRate = 0.55 },
      },
    },

    {
      id = "poison", name = "Venom Spout",
      description = "Poisoned bolts that keep hurting after they land.",
      upgradeNote = "Toxicity and duration both up.",
      projectileSpeed = 11.0, mode = "single",
      levels = {
        { cost = 120, damage = 12, range = 3.0, fireRate = 0.9,  statusDuration = 4.0, statusDps = 6 },
        { cost = 100, damage = 18, range = 3.3, fireRate = 1.0,  statusDuration = 5.5, statusDps = 10 },
        { cost = 190, damage = 28, range = 3.7, fireRate = 1.15, statusDuration = 7.0, statusDps = 16 },
      },
    },

    {
      id = "flame", name = "Ember Vent",
      description = "A short, fierce burst of fire that scorches everything in front of it.",
      upgradeNote = "Wider reach and hotter burn.",
      projectileSpeed = 14.0, mode = "single",
      levels = {
        { cost = 100, damage = 8,  range = 2.0, fireRate = 2.6, statusDuration = 1.6, statusDps = 4 },
        { cost = 85,  damage = 13, range = 2.2, fireRate = 2.9, statusDuration = 2.0, statusDps = 7 },
        { cost = 160, damage = 21, range = 2.5, fireRate = 3.3, statusDuration = 2.6, statusDps = 11 },
      },
    },

    {
      id = "prism", name = "Prism Spire",
      description = "Splits one shot into several beams, hitting a whole group at once.",
      upgradeNote = "More beams per shot.",
      projectileSpeed = 22.0, mode = "pierce",
      levels = {
        { cost = 140, damage = 11, range = 3.1, fireRate = 1.3,  pierceCount = 3 },
        { cost = 120, damage = 17, range = 3.4, fireRate = 1.45, pierceCount = 5 },
        { cost = 230, damage = 27, range = 3.8, fireRate = 1.6,  pierceCount = 8 },
      },
    },

    {
      id = "mortar", name = "Sky Mortar",
      description = "Lobs a shell over everything and brings it down on the far side.",
      upgradeNote = "Longer reach, bigger shell.",
      projectileSpeed = 7.5, mode = "splash",
      levels = {
        { cost = 160, damage = 55,  range = 6.0, fireRate = 0.40, splashRadius = 1.8 },
        { cost = 140, damage = 92,  range = 6.6, fireRate = 0.46, splashRadius = 2.1 },
        { cost = 270, damage = 155, range = 7.2, fireRate = 0.52, splashRadius = 2.6 },
      },
    },

    {
      id = "ballista", name = "Siege Ballista",
      description = "Slow, colossal bolts. Put it where a boss has to walk.",
      upgradeNote = "Bigger bolt, more reach.",
      projectileSpeed = 15.0, mode = "single",
      levels = {
        { cost = 145, damage = 72,  range = 5.2, fireRate = 0.50 },
        { cost = 125, damage = 118, range = 5.6, fireRate = 0.56 },
        { cost = 250, damage = 195, range = 6.1, fireRate = 0.62 },
      },
    },

    {
      id = "tarpit", name = "Tar Pit",
      description = "A pool that drags enemies down and keeps chewing on them.",
      upgradeNote = "Wider pool, stickier, more corrosive.",
      projectileSpeed = 0.0, mode = "aura",
      levels = {
        { cost = 125, damage = 0, range = 1.9, fireRate = 0, auraDps = 9,  slowFactor = 0.45, slowDuration = 0.6 },
        { cost = 105, damage = 0, range = 2.2, fireRate = 0, auraDps = 15, slowFactor = 0.55, slowDuration = 0.8 },
        { cost = 200, damage = 0, range = 2.6, fireRate = 0, auraDps = 24, slowFactor = 0.68, slowDuration = 1.0 },
      },
    },

    {
      id = "beacon", name = "War Beacon",
      description = "No damage of its own, but every tower beside it hits harder.",
      upgradeNote = "Wider radius of influence.",
      projectileSpeed = 0.0, mode = "aura",
      levels = {
        { cost = 140, damage = 0, range = 3.0, fireRate = 0 },
        { cost = 120, damage = 0, range = 3.4, fireRate = 0 },
        { cost = 240, damage = 0, range = 3.9, fireRate = 0 },
      },
    },
  },

  -- ---------------------------------------------------------------------------
  --  ENEMIES
  --
  --  archetype:
  --    walker   — follows the path to the exit
  --    flyer    — flies straight to the exit, ignoring towers entirely
  --    attacker — ignores the path and goes for YOUR TOWERS
  -- ---------------------------------------------------------------------------
  enemies = {

    -- walkers
    { id = "grunt",   name = "Grunt",   archetype = "walker", hp = 30,  speed = 1.7,  bounty = 6,  scale = 1.0,  color = "red",       shape = "blob" },
    { id = "runner",  name = "Runner",  archetype = "walker", hp = 18,  speed = 3.4,  bounty = 7,  scale = 0.85, color = "orange",    shape = "sprite" },
    { id = "brute",   name = "Brute",   archetype = "walker", hp = 110, speed = 1.1,  bounty = 16, armor = 2,  scale = 1.25, color = "brown",  shape = "blob" },
    { id = "wraith",  name = "Wraith",  archetype = "walker", hp = 45,  speed = 2.5,  bounty = 14, scale = 1.0, color = "violet", shape = "ghost", slowResistant = true },
    { id = "crawler", name = "Crawler", archetype = "walker", hp = 190, speed = 0.85, bounty = 22, armor = 6,  scale = 1.35, color = "grey", shape = "bug", slowResistant = true },

    -- flyers: ignore the path AND the towers
    { id = "flyer",   name = "Wasp",    archetype = "flyer",  hp = 26,  speed = 2.6,  bounty = 10, scale = 0.9,  color = "yellow",   shape = "wing" },

    -- attackers: hunt your towers
    { id = "sapper",  name = "Sapper",  archetype = "attacker", hp = 55,  speed = 1.9,  bounty = 16, scale = 1.05, damage = 7, attackRange = 0.9, attackRate = 0.7, color = "green", shape = "hex" },
    { id = "bruiser", name = "Bruiser", archetype = "attacker", hp = 150, speed = 1.35, bounty = 24, armor = 3, scale = 1.3, damage = 26, attackRange = 1.0, attackRate = 0.8, color = "darkgreen", shape = "hex" },

    -- bosses
    { id = "boss",       name = "Colossus", archetype = "walker",   hp = 1400, speed = 0.95, bounty = 220, armor = 12, scale = 2.4, isBoss = true, slowResistant = true, color = "purple",  shape = "monster" },
    { id = "boss_siege", name = "Ravager",  archetype = "attacker", hp = 1900, speed = 1.05, bounty = 280, armor = 15, scale = 2.6, isBoss = true, slowResistant = true, color = "crimson", shape = "monster", damage = 42, attackRange = 1.4, attackRate = 0.55 },
  },
}
