# Changelog

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
