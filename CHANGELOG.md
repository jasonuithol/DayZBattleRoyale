# Changelog

## 0.1 (unreleased)

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
- **The circle:**
  - Nothing is shown for the first 5 minutes; the whole map is safe.
  - Then it closes from around the whole map over seven phases, about 28 minutes.
  - It's drawn as real lines on the Expansion map, which opens zoomed out on you. Outside it the screen turns
  blue and health drains, faster each phase.
- **Loot:** five guns, each with a magazine and ammo; three tiers of helmet, vest and backpack;
  attachments as separate loot; medical items; jerry cans.
- **Cars:** complete, with 15% fuel.
- **Supply drops:** Expansion's airdrop plane, carrying top-tier loot.
- **View distance:** capped through FlyingWithoutCTD while on the server, which stops the
  freezes high up.
- **No stall at the start of a round:** the next round's loot goes out a few buildings at a
  time during the lobby.
- **End of round:** a winner is announced, then the round's loot, cars, bodies and crates are
  removed.
