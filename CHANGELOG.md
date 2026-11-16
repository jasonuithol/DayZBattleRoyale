# Changelog

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
