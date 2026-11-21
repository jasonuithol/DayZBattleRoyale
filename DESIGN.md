# DayZ Battle Royale: design

Battle royale rounds on a DayZ server, with Nasdara as the first map. Players fight inside a
shrinking "circle of death". On Nasdara, the area outside the circle is a sandstorm.

## The round

The server runs the same loop over and over:

1. **Lobby.** Eliminated players respawn in a small room outside the map, where a clock counts
   down to the next round. They can't be hurt there, and their inventory is cleared. A new
   session also starts in the lobby, but the first countdown is 5 minutes so everyone has
   time to log in. After that, each round starts as soon as the previous one ends. Players
   who join mid-round also wait in the lobby.
2. **Spawn.** The map is cleaned up from the last round (loot, bodies, cars, supply drops).
   Fresh loot and cars are put out, and every player in the lobby is sent to a random spawn
   point. *Later:* 100 players jump out of a plane and choose where to land.
3. **Game.** The circle shrinks in phases. Each phase has a wait, then a shrink to a new,
   smaller circle somewhere inside the current one. Supply drops come down during the game.
   A player who dies is eliminated and respawns in the lobby.
4. **End.** When one player (or team) is left, they are declared the winner and everyone
   goes back to the lobby.

## Loot (fast paced, no base building)

- **Weapons.** A short list of weapon types, to keep it simple. A gun always spawns with a
  magazine in it and about three boxes of its ammo next to it.
- **Armour.** Helmets, vests and backpacks each come in three tiers.
- **Attachments** are separate, bonus loot: scopes, suppressors, extended magazines.
- **Also:** medical items and jerry cans. Nothing for base building.
- The Central Economy is switched off for items. The mod spawns its own loot table at
  building loot positions, building by building as players come near (4 in 5 looted, half of
  those armouries, extra loot on flat roofs), and removes all of it when the round ends.

## Cars

Cars are complete and drivable, but they start with only a little petrol. Jerry cans spawn
as loot. Cars are removed and spawned again every round.

## Supply drops

A crate with better loot comes down at random points inside the current circle, with a
smoke or flare marker so players can find it. Two ways to build it:

- Reuse Shipping Crate's work (plane and parachute), which needs Expansion.
- A plain vanilla crate falling with a smoke signal.

## The circle on Nasdara: the sandstorm

*Superseded (2026-11-16): the storm is a gas zone after all. See the decisions at the end.
This section is kept for what it found about the vanilla sandstorm.*

### What the vanilla sandstorm is

Vanilla scripts checked 2026-11-15:

- **Shape.** The native storm (`SandstormController`, 3_Game/Sandstorm.c) is a straight
  front. It sweeps across the whole map in the wind direction, from a start position to an
  end position (`GetDirection`, `GetPointOnEdge`, `IsPositionAtEnd`). It is not a circle,
  and script can't change its shape: it can only call `Start` and `Stop` and read values.
  The weather starts it at random (`Nasdara.c`, `cfggameplay.json` `SandstormData.sandstormFrequency`).
- **What players see, script driven and client side.** The screen effect is
  `PPERequester_SandstormEffect`: haze, colour, glow and god rays. Script sets its intensity
  every tick in `ScriptedSandstormController.OnUpdate`. Night-vision goggles and scopes also
  switch to sandstorm variants.
- **What it does to players, server side.** The `MDF_SANDSTORM_EXPOSURE_STATIC` and `_DYNAMIC`
  modifiers cause silicosis, eye irritation, squinting and coughing.
- **A zone version already exists.** `SandstormZoneSmall_TESTONLY : ContaminatedArea_Static`
  (SandstormExposure.c) is a zone that turns on the static exposure modifier when a player
  walks in. Bohemia have already tried a zone-shaped sandstorm.

### Plan

- Turn off weather sandstorms (`sandstormFrequency: 0`), so a real storm front can't cross
  the map in the middle of a round.
- **Client.** Each player's own distance outside the circle drives the sandstorm screen
  effect. It fades in over the first ~30 m past the edge, so the edge feels like a wall you
  walk or drive into. Add wind and dust sound.
- **Server.** Players outside the circle get the exposure modifier (coughing and squinting)
  plus real damage that grows each phase. Silicosis on its own works far too slowly for a
  battle royale.
- **Shelter.** Unlike the vanilla storm, buildings, cars and underground spaces give no
  shelter. Otherwise players could sit out the circle indoors.
- **Seeing the edge.** The screen effect only shows when you're already in the storm, so you
  can't see the wall coming from inside the circle. That needs:
  - the circle (current and next) drawn on the map, and a countdown on the HUD
  - possibly dust particles placed on the client along the part of the edge nearest each
    player, so the wall can be seen from a distance

### To test early

- Does the native storm also draw particles or sky effects, or play sounds, that a
  script-only storm won't get?
- Can the vanilla dust sound be reused?

On other maps, the same circle could be gas instead, using the vanilla contaminated-area
screen effect and sickness.

## Shape of the code

- **Server mod.** The round state machine, the circle, loot, cars, supply drops, the lobby
  and the clean-up.
- **Client mod.** Players need this. It has the sandstorm screen effect, the circle on the
  map, and the HUD (phase timer, players left, lobby countdown, winner).
- **Mission.** Built from `dayzOffline.nasdara`, with the Central Economy switched off for
  items and persistence wiped every round. Build, sign, deploy and test with the tooling in
  `~/Projects/DayZMissions`.

## Decisions (2026-11-15)

- **Simplest version of every feature first**, to get a playable demo as soon as possible.
  `README.md` describes what that demo does.
- **Solo only** for now.
- **Use an existing mod wherever one already does the job.** Expansion supplies the supply
  drops, the map and markers, and the notifications.
- **Outside the circle** is the blue sunglasses tint plus steady damage for now. The sandstorm
  comes later.
- **The lobby** is open, flat ground off the edge of the map, with a leash and no damage.
  Where it is doesn't matter.
- **Test locally first.** Where it runs after that (probably the VPS) is a later question.

## Decisions (2026-11-16)

- **Spawn by plane, done.** One plane per round; players watch from behind it, F to jump, F to
  open the parachute.
- **Plane view:** the view from the plane is a player camera mode (the borrowed unconscious-camera
  slot), not a scripted 'staticcamera', which left the view stuck once switched off.
- **The parachute:**
  - Steered by the client's own keys, sent to the server.
  - Gravity off during the jump, with the height following a target. The server's height
    wins, and the jumper's game follows it.
  - The canopy is our own model (orange and white, ram-air).
- **The circle:**
  - Hidden for the first 5 minutes (the whole map is safe, no damage).
  - Then it closes from around the whole map to the first white circle (5.6 km radius), with
    gentle shrinks after that (70% → 40%).
  - The next circle drifts off-centre by at most half the room it has.
- **The plane:**
  - The route runs edge to edge.
  - Free fall at 22.5 m/s, half a real skydiver's, for more time to pick a spot.
- **View distance:** capped all session through FlyingWithoutCTD.

## Decisions (2026-11-16, later)

- **The circle cycle.** Three steps, repeating:
  - a cooldown, with the next circle unknown
  - the next circle announced in white, with the current one turning yellow
  - after the notification time, the yellow turns red and closes on the white one
  Cooldown and notification shrink from 5 minutes by a minute each circle, then stay at 30 s
  until the circle closes to nothing. There's only ever one white circle on the map.
- **The storm is a gas zone, not a sandstorm.** The vanilla sandstorm screen effect was too
  tame, even pushed hard with an overlay, vignette and blur. The player gets a vanilla gas
  zone's own effects instead (`PlayerBase.SetContaminatedEffectEx`: gas round them, the tint,
  the sound, muffled hearing), switched on by the client from its distance to the circle.
  There's no zone and no trigger. It looks the same on every map, so the "other maps could be
  gas" idea is now the default.
- **Seeing the storm coming:** our own model, a see-through glowing red wall with a bright
  line along the ground, placed client-side in 10 m panels along the stretch of edge nearest
  the player. The gas zone's big clouds sit behind it. The client works out where the circle
  is every frame, from the last state and how long the red has left to close, so the wall
  glides.
- **Storm damage is real hits:** a zombie scratch to the torso every 3 s, scaled to the
  circle's damage per second, so players flinch, grunt and see red. One in four also opens a
  bleeding cut. No hit (only the same health taken) while the player is doing an action, so
  the flinch can't stop them bandaging.
- **The lobby resets everyone** on arrival: plain kit, full health.
- **Testing:** settings to start in the storm, shrink the first circle, and add AI bots that
  count as players. With them, the end of a round can be tested solo in a few minutes.
