# Changelog

## 0.7.1 - 2026-11-21

- **Apples in the lobby:** a dozen lying about at all times, to throw at each other (nobody
  can be hurt there).
- **A fountain to throw them into:** Chernogorsk square's fountain is in every lobby, with a
  big stone apple on its column. An apple thrown into its basin goes in, and everyone
  waiting is told who got it in and how many are in so far.
- **Fixed: reloading made magazines.** Reloading with no spare magazine made a new one and
  kept the old one too (a free 60-round STANAG, say). The new one now replaces the old one,
  whose rounds go back to your ammo.
- **Fixed: picking something up in the lobby said you were ready.** F is the game's interact
  key too; picking up an apple (or opening a door) no longer counts as ready.

## 0.7.0 - 2026-11-21

- **Much more loot, put out as you come near.** A building's loot goes out once a player is
  within 200 m of it and below 150 m (under the canopy, or on foot), not all over the map at
  the start. 4 buildings in 5 have loot (was about half): half of those are **armouries** (6
  guns, armour, medical, grenades: about 60 things), the rest have enough for 2 or 3 players.
  Loot pops in as you get close.
- **Loot on the roofs:** a flat roof has a gun, armour and medical on it a third of the time,
  on top of what's inside, and the tower blocks on Chernarus always have two of those, so
  you can pick one under the canopy and land on it.
- **Loot well outside the circle goes** as it shrinks, once it's 100 m outside; anything
  someone carries stays.
- **Rounds start straight away:** the lobby no longer waits for the loot to be put out.
- **Nasdara and Livonia need nothing extra:** their lobby places are in BattleDayZ itself;
  the BattleDayZ Nasdara and BattleDayZ Livonia server add-ons are gone.
- Settings: `lootShare`, `armouryShare`, `roofLootShare`, `lootRadius`, `lootHeight`,
  `lootItemCap` and `lootClearBeyond` replace `lootBuildings`. Testing: `testRoofs`.

## 0.6.0 - 2026-11-20

- **Manoeuvring in free fall:** with no keys you drift forward; **W** tracks (tilted forward,
  arms down the sides, 30 m/s forward), **W and Shift** dive (40 m/s forward, 32 m/s down),
  **Shift** plummets head down (50 m/s straight down), **S** brakes (tilted back, arms and legs
  spread, hardly any forward), **A** and **D** roll and drift. Every game sees the body move.
- **Opening the canopy:** the body comes back to neutral first, then the canopy opens, the
  jumper swings upright and their hands go up to grip the lines. It opens by itself at 150 m
  (was 100 m), to leave room to come out of a plummet.
- **Flying the canopy:** it glides forward; **W** lets it lead (faster), **S** holds it back,
  **Shift** sinks faster, **A** and **D** bank it into a turn (hold them to spiral down). The
  jumper swings under the canopy as it tilts.
- **The camera:** in free fall it tilts with the jumper, pulls back while they manoeuvre, and
  shakes in the wind with wisps streaming past, more the faster they go; under the canopy it
  moves out and up, looking down on them, and turns with them.
- **The animations:** the mod ships its own player animation graph, the game's with the jump's
  tilts, poses and blends added, made from the installed game's at every build.

## 0.5.7 - 2026-11-20

- **Street lamps that were dark at night are lit.** Expansion lights only the lamps the server
  has when the mission loads, about 1,050 of Chernarus's 2,700 (575 on Livonia); the server
  makes the rest later, round where players go, and they stayed dark (all of Nagornoye and
  Novodmitrovsk, say). Each is now lit as it comes.
- **Lamps with loudspeakers are lit too:** the metal street lamps and the wooden and concrete
  power pole lamps with a loudspeaker on them (659 on Chernarus) are vanilla classes Expansion
  doesn't light.
- **Lamps in the lobby in the sky are lit** at night, like the map's.
- **No more flickering:** a town's worth of lit lamps is more lights than the game draws at
  once, and the lit patches flickered on and off. Each player's game lights only the 16 lamps
  nearest them (the rest light up as they come close), and the lamps cast no shadows.
- **Nobody jumps without the kit:** a player who reaches the plane's door with less than the
  kit on them (it happened once, after a respawn; the cause isn't known yet) is given it again
  there. The server's log says how many things each player has on them at each step.

## 0.5.6 - 2026-11-19

- **Every street lamp lit at night** (`lampsLit` 1, the share lit): Expansion's own setting
  lights one in three at most, and is shared with every mission on the server.
- **Boats on Chernarus: 30** (was 12), and the islands get boat spots of their own: 4 round
  the shore of Prison Island and of Skalisty Island, among the map's 72 on the mainland.
- **Boarding:** players wait in the lobby, hearing the engine start up, until every game
  shows the round's time of day and plays the engine (60 s at most); then the plane takes off.
  The lobby shows the next round's time of day from the moment the last round ends.
- **Testing:** `testShowFinal` marks the point the last circle closes on, on the map, and
  `testShowVehicles` every car and boat; a `circleDamage` of 0 means no storm cuts at all.
- **The first circle is as wide as the map** (`firstCircle` 1, was 0.5),
  so there's time to reach it from the far side of the map. It's also the first white circle
  shown: before, the first one shown was 70% of it.
- **Loot all over the map:** up to 6,000 buildings (`lootBuildings`) picked at random
  anywhere on the map, nothing to do with the circles. Before, only buildings inside the first
  circle got loot, and that circle isn't shown at the start: with the plane's route no longer
  tied to it, a player could land far from any loot without knowing.
- **Loot stays put until the round:** the next round's loot goes out as the last one ends,
  and the game's own clean-up deleted most of it after a few hours (4 h for most things)
  while the lobby waited for players. The round's loot now lasts two days; the round removes
  it itself.

## 0.5.5 - 2026-11-18

- **Bright nights:** a night round is under a full moon in a clear sky. The date moves to the
  night of the month with the brightest moon at that hour, and clouds, rain and fog are
  cleared for an hour.

## 0.5.4 - 2026-11-18

- **The plane flies between two random points on opposite sides of the map** (north to south
  or west to east, either way), nothing to do with the circles. Before, its route went through
  a point near the first circle's middle, and when that middle was off the map (0.5.3) the
  route was 1 m long: everyone was dropped off the map at once, where they couldn't move.
- **Every circle's centre is on the map**, the first one too.

## 0.5.3 - 2026-11-18

- **Morning, noon and night rounds:** each round starts at the next hour in `roundHours`,
  taking turns: 7:00, then 12:00, then 23:00, then 7:00 again. Before, every round was at noon.
- **Circles close in on a random spot:** the point the last circle closes on is picked at the
  start of the round, any point of the map equally likely. The first circle contains it, and
  every circle after lies wholly inside the one before and contains it too, each centre
  equally likely to be anywhere that does that. Before, the first circle was always in the
  middle of the map and each next one off-centre by at most half of what would fit, picked so
  it favoured the middle: the circles drifted to the middle of the map.
- **The first circle is half the map's width across** (`firstCircle` 0.5, as a share of the
  map's width), placed round that point; `startRadius` and `circleDrift` are gone.

## 0.5.2 - 2026-11-18

- **No more crash on quitting:** every game that had seen a parachute jump, and every server,
  crashed as it shut down. The game keeps a finished script command until its own shutdown,
  and the jump's (or even one that does nothing) crashed it if its class was in the world
  module (4_World), unloaded first; in 3_Game it doesn't. The jump's command is in 3_Game now.
- **The round lets go of everything when the mission ends:** its timers, and the game objects
  it holds (it had kept them until the program's final clean-up).
- **`-brskip=` for the self-test:** leave out steps of the headless check (circles, hits,
  guns, sky, lobbies, plane, chute, drop), for tracking down what one leaves behind.

## 0.5.1 - 2026-11-18

- **Reload from your ammo, fixed:** in 0.4.1 and 0.5 the reload key never got to it (it was
  hooked onto a vanilla function nothing calls), and the top-up it did on the gun's own
  magazine never reached the player's game (the gun keeps its own count of its magazine).
  Now reload is a weapon action of its own: it fills a spare magazine of the same kind from
  your loose rounds and unopened ammo boxes (the magazine the last reload took off, or a new
  one) and swaps it in with vanilla's own reload, animation and all. With a fuller spare
  already, or no ammo for it, reload is vanilla's.
- **`kitExtras`:** more things everyone gets in the lobby's kit, for testing or events
  ("Class", or "Gun|Magazine" for a loaded gun). Empty by default.

## 0.5 - 2026-11-17

- **Watch the round when you're out:** knocked out (or joining) while a round is on, you watch
  a player still in it, the camera orbiting them (the mouse turns it). F moves on to the next
  player; past the last, back to the lobby, where F watches again. The server holds your own
  character, unhurt, 150 m above whoever you watch, so your game has them and what's round
  them. Everyone comes back to the lobby when the round ends.
- **The lobby clock waits for players:** with nobody (or not enough players) in the lobby, the
  countdown holds at its full time instead of running out and starting again, so whoever
  arrives first sees all 3 minutes (5 after a server start), not whatever was left of a cycle.

## 0.4.1 - 2026-11-17

- **Old lobbies really go:** the lobby's models are no longer sent to players. A static model
  a server sends stays with every game that comes near it again, even after the server deletes
  it, so old lobbies kept reappearing, and a new one built where an old one had been could
  leave players falling through. Now the server builds its own copy (players stand on it, it
  picks where they appear) and each player's game builds the same copy itself, from the place
  on the map and where it goes (sent in the state every second), and takes it down when the
  lobby moves or the game leaves. Only its buildings (Land_ classes, real buildings with
  doors) are still sent; deleting those reaches every game. A swap no longer sends 500 to 1,300
  objects to every player either. The lobbies take 4 places in turn, 600 m apart.
- **Falling through the lobby:** anyone caught falling through it 3 times in a minute waits
  on the ground under it instead (still unhurt), until the next round. Each catch is logged.
- **The plane's engine from take-off:** it played only once Expansion's plane reached a
  player's game, a quarter or half the way across, and then at full volume right by the
  camera. Now each player's game plays the engine itself from the moment they're on board,
  at the plane and quieter, and Expansion's loop for that plane stays quiet meanwhile.
- **Boats** on the maps with water: 12 a round on Chernarus and 5 on Livonia (`boats`, of
  `boatTypes`), at random vanilla boat spots (72 and 17; `make_spots.py` keeps them now),
  afloat, with a spark plug in and the same 15% of a tank as the cars. Nasdara has none.
- **Ready with F:** the lobby lasts 3 minutes between rounds (`lobbySeconds` 180, was 60),
  or until everyone waiting has pressed F to say they're ready. The second line shows how
  many are waiting and ready, and tells each player whether they are.
- **No stamina:** sprinting, jumping and climbing never tire (vanilla's own switch).
- **Adrenaline heals, and the PO-X antidote:** epinephrine (adrenaline) keeps its vanilla
  boost and also brings health back to 100% (every limb, blood and shock). Medical loot is
  now a bandage half the time, else morphine, epinephrine or the PO-X antidote (for gas
  poisoning), and supply drops carry an epinephrine.
- **Reload refills the magazine:** with a magazine on the gun that isn't full and ammo for
  it in the inventory, reload tops that magazine up instead of swapping in another: from
  loose rounds first, then from boxes of rounds, used unopened (what's left of a box stays as
  loose rounds). With no ammo for it, reload is vanilla's.
- **Gloves in the kit:** everyone gets work gloves along with the rest of the plain kit.

## 0.4 - 2026-11-17

- **The lobby is in the sky:** a copy of a real place on the map, floating 1 km up
  (`lobbyHeight`), on a level concrete floor. On Chernarus: Zub castle, Chernogorsk square
  and Prison Island. On Nasdara: Qalat-e-Lut (the caravanserai), the airport junkyard and
  Kirov dam. On Livonia: Branzow castle, Brena square and Lukow airfield. A different one each round (`lobbies`), the next built beside the old one,
  everyone moved across, then the old one taken down, by the server and by each player's game
  (a server can't take back the static models it sent). Players appear on open, flat ground;
  anyone who walks or falls off the edge is put back, unhurt. The server copies whatever
  stands at the place when it builds the lobby: buildings (doors and ladders work), rocks and
  anything within a building keep their heights, trees, bushes and small things in the open
  stand on the floor. Its buildings are never saved with the world.
- **Chernarus and Livonia:** two more missions, `battleroyale.chernarusplus` and
  `battleroyale.enoch`, each with its own loot and car spots and lobbies.
- **Maps kept apart from the game:** each map has a profile (`BR_Maps.c`), picked by the
  world's name. The core mod has Chernarus; the DLC maps have small mods of their own,
  **BattleDayZ Nasdara** and **BattleDayZ Livonia**, which their servers load after BattleDayZ
  (server only: players don't need them). Another map (a DLC or
  a third-party map like Deer Isle) gets its own the same way. Missions are now in
  `missions/<map>/`.
- **More vanilla car spots on Nasdara:** `make_spots.py` reads them straight from the map's
  `cfgeventspawns.xml` (333 on Nasdara, motorbike spots included, against 147; 480 on
  Chernarus, no boats, 255 on Livonia), so it works for any map with a vanilla mission folder.
- **Any map size:** the vehicle squares come in a whole number per quarter (12 x 12 on Nasdara
  and Chernarus, 8 x 8 on 12.8 km Livonia), and a vehicle's spot is kept 50 m inside this
  map's edge, not Nasdara's.
- **`tools/make_lobby.py`:** copies a place from one map into a lobby file for another
  (`lobbies/<name>.json` in the mission), for when a map should have another map's landmark.
- **The jump from the sky lobby:** the jump only counts as in the air once nothing solid is
  within 5 m below, not by height above the terrain, which the lobby is a kilometre above.
- **No guaranteed vehicles:** 0.3 gave the airport and the big military base 4 each; now no
  place gets any for certain.
- **Balanced over the whole map:** the vehicles are dealt round the map's 16 quarters (4 x 4
  km) in turn, 6 or 7 each, using 12 x 12 squares lined up with the map's edges (`carSquare`
  1365). 0.3's squares followed the first circle, which leaves out the corners and most of
  the west, and didn't line up with the quarters (one held 9 squares, another 4). None go
  within 1 km of the lobby.
- **Vehicles turn up anywhere something's built:** half the time a square's vehicle goes to a
  vanilla car spot (if it has one), otherwise it's parked by whatever structure is about,
  classed or not (a house, a hangar, a solar farm, an oil derrick, a fort, a wreck), or out in
  the open. 0.3 parked them only by buildings that have loot, and always took a vanilla spot
  when there was one.

## 0.3 - 2026-11-16

- **Vehicles everywhere:** the 100 vehicles are spread evenly over the map instead of only at
  Nasdara's vanilla car spots, which bunch up in the towns of the centre and west (the whole
  east had 7 of 147). The map is cut into 1.5 km squares (`carSquare`), each getting the same
  share: at a vanilla spot in it if there's a free one, otherwise on flat, clear, dry ground by
  a building or out in the open. The airport and the big military base get 4 each first
  (`carHotspots`: "x z radius count").
- **The landing log line names the player.**
- **No "Safe zone" marker:** the star at the next circle's centre, on the map and floating in
  the world, is gone. The circles on the map show where to go.

## 0.2 - 2026-11-16

- **Vehicles:** 100 of them, each an enduro motorbike, an Ada 4x4, an M1025 or Expansion's Land Rover
  (the `vehicles` setting), with every part fitted and 15% fuel.
- **Guns:** two per calibre, fourteen in all (9 mm, .45, .357, .308, 12 gauge, 7.62×39,
  5.56×45), every one loaded, including revolver cylinders and both shotgun barrels. The
  attachments are the ones that fit them, and the supply drop has an M16-A2 and a DMR.
- **Frag grenades** in the loot.
- **Sights:** only Mini Sights, Baraka, ACOG, Combat Sights and the hunting scope. Sights and
  suppressors ignore NATO and Soviet mounts (`config.cpp`).
- **Sights that take a battery** come with one fitted.
- **The kit** adds a military belt with a pistol holster.
- **Molotovs** in the loot, with matches. Molotov, Better Explosions and Expansion Vehicles are
  now required mods.
- **Vehicles last the round:** Nasdara's economy gave the Ada and some motorbike colours a
  3-second lifetime and deleted them half a minute after they spawned; every vehicle now gets
  a day.
- **Clean-up:** blown-up vehicles' wrecks, the parts thrown off and fires go with the round.
- **Roof landings:** the parachute lands you on whatever is under you (a roof, a car, a rock);
  it used to pull you through a roof to the ground. The landing log line has the position.

## 0.1 - 2026-11-16

First playable demo on Nasdara: solo rounds, with the simplest version of every feature.

- **Lobby:** off the edge of the map, with a countdown. The first round after a server start
  waits longer, so people can log in.
- **Spawn:** everyone gets the same kit and boards a plane that crosses the map, edge to edge.
  They watch from behind it, orbiting with the mouse, and the route is drawn on the map. F jumps.
- **The jump:** third person throughout.
  - A long free fall lying flat, steering with WASD.
  - F opens the parachute (it opens by itself 100 m up otherwise), upright under a visible
    ram-air canopy that everyone sees.
  - A smooth, steerable glide down, and no damage from the jump.
  - A pack on your back from the jump until you land.
- **The circle:**
  - It cycles: a cooldown with the next circle unknown, then the next circle is announced in
    white (the current one turns yellow), then the yellow turns red and closes on it.
  - Nothing is shown for the first 5 minutes and the whole map is safe. The first red starts
    just round the whole map.
  - The timers run 5, 4, 3, 2, 1 minutes, then 30 s, until it closes to nothing. Then the
    storm is everywhere and the last one alive wins.
  - It's drawn as real lines on the Expansion map, which opens zoomed out on you. The red one
    glides as it closes.
- **The storm (outside the circle):**
  - A vanilla gas zone's effects on the player inside it.
  - A glowing red wall along the nearby edge, with gas clouds behind it, so you can see it
    coming. It glides in as the circle closes.
  - Damage comes as real hits: a flinch, a grunt, red on screen, and sometimes a bleeding cut.
    It gets heavier each circle. No hit while you're bandaging, so you can patch up.
- **Compass:** Expansion's compass at the top of the screen, for everyone.
- **Lobby reset:** everyone arriving in the lobby is stripped to the plain kit and fully healed.
- **Loot:** five guns, each with a magazine and ammo; three tiers of helmet, vest and backpack;
  attachments as separate loot; medical items; jerry cans.
- **Cars:** complete, with 15% fuel.
- **Supply drops:** Expansion's airdrop plane, carrying top-tier loot.
- **View distance:** capped through FlyingWithoutCTD while on the server, which stops the
  freezes high up.
- **No stall at the start of a round:** the next round's loot goes out a few buildings at a
  time during the lobby.
- **End of round:** the winner and the last one out are announced, then the round's loot,
  cars, bodies and crates are removed and the next round starts.
- **Testing settings:** start in the storm on the ground (`testStorm`), a small first circle
  (`testWhiteRadius`), and AI bots to win or lose against (`testBots`).
