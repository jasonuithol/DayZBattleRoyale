# DayZ Battle Royale

Solo battle royale rounds on DayZ, starting with Nasdara. This is the simplest version of
every feature, built to get a playable demo running. `DESIGN.md` has the bigger picture and
what comes later.

## How a round goes

1. **Lobby.** Everyone who isn't in a round waits in the lobby, on a patch of flat open ground
   off the edge of the map, near (220, 1780). There nobody can be hurt, and anyone who walks
   more than 30 m off is brought back. A clock at the top of the screen counts down to the
   next round: longer after a server start (`firstLobbySeconds`, 5 minutes by default) so
   people can log in, then 1 minute between rounds (`lobbySeconds`). For testing,
   `mission/battleroyale.json` currently has 30 s and 15 s. New characters are created here,
   and so is anyone who dies. Meanwhile the next round is got ready: an 8 km circle (radius), which covers all of
   Nasdara, with loot in 6,000 buildings inside it (1 to 4 piles each, by size, about 25,000
   items) and 40 cars at the vanilla car spawn points. It goes out a few buildings at a time
   over about 25 s, so the server never stalls.
2. **The plane.** When the clock runs out, everyone in the lobby is stripped, healed and given
   the same plain kit (T-shirt, jeans, trainers and a bandage), and put on the plane. The time
   is set to noon. Expansion's plane crosses the whole map on a straight route, edge to edge,
   through a point near the middle, 400 m above the highest ground under it. That takes 2 to 3
   minutes (`planeSpeed`, `planeHeight`).
   - **The view:** players watch from behind the plane and can orbit it with the mouse. The
     character waits in the lobby meanwhile. The route and the plane are also drawn on the map
     until the plane has flown off it.
   - **F** jumps. Anyone still on board at the end of the route is dropped there.
3. **The jump.** Third person all the way down, whatever the player's view setting.
   - **Free fall:** 22.5 m/s down (half a real skydiver's, for more time to pick a spot),
     lying flat, steering with WASD at 30 m/s, relative to where you look.
   - **F again** opens the parachute; it opens by itself 100 m above the ground otherwise.
     Under it you're upright, coming down at 6 m/s and steering at 9 m/s. Everyone sees the
     canopy above you.
   - **Landing:** about 1.5 m up, the game's own fall takes over and lands you. Nobody can be
     hurt from the jump until 3 seconds after landing.
4. **Game.**
   - **The first 5 minutes:** no circle is shown and the whole map is safe. The screen counts
     down to when the circle appears.
   - **Then:** the white circle appears and the red one closes on it from around the whole map.
   - **After that:** each phase the circle waits, then shrinks onto a smaller one inside it, over
     seven phases (about 28 minutes from take-off) until the last circle closes completely.
   - **The map (M):**
     - It opens zoomed out, centred on you, or on the plane while you're on board.
     - The current circle is drawn in red and the next one in white, as real lines over
       Expansion's map.
     - The next circle's centre has a "Safe zone" marker, which you can also see in the world.
   - **Outside the circle:** the screen turns blue (the blue sport-glasses tint, stronger) and
     you lose health every second. The loss grows each phase: 1, 1, 2, 4, 6, 8, then 10 per second,
     out of 100.
   - **Supply drops:** the first comes 2 minutes after the circle appears, then one every 4 minutes.
     Expansion's plane drops a crate inside the next circle.
     It flies in fast (200 m/s), so it arrives about a minute after it's sent.
     It holds top-tier armour, an M4 with an ACOG and suppressor, an SVD with a PSO, ammo and
     morphine. The crate shows on the map.
   - A player who dies or leaves is out. Notifications announce who is out and how many are left.
5. **End.** The last player standing wins, and everyone is told. With only one player in the
   round, it ends when that player dies. After 15 s everything the round spawned is deleted:
   loot, cars, bodies and supply crates. Everyone goes back to the lobby.

## Loot

There is no base building, food or zombies. The Central Economy spawns nothing (see
`mission/db/economy.xml`). Each loot spot gets one of these:

| Chance | What |
|---|---|
| 25% | A gun with a magazine in it and 3 boxes of its ammo: AKM, M4A1, MP5K, SVD or Saiga |
| 30% | A helmet, vest or backpack (10% each), in one of three tiers (60/30/10%): Ssh68 / Ballistic / Gorka helmet; Police / Press / Plate carrier vest; Child / Taloon / Alice bag |
| 15% | An attachment: ACOG, reflex, Kobra or PSO-1 sight, M4 or AK suppressor, 60-round STANAG or 75-round AK drum |
| 18% | A bandage, or morphine (one time in three) |
| 12% | A jerry can, half full. Cars start with 15% of a tank |

## Layout

- `BattleRoyale/`: the mod, loaded on the server and on clients.
  - `3_Game`: the state the server sends each player every second, the jump and steering
    messages, where the plane is, and the blue tint.
  - `4_World`:
    - the jump and the parachute (`BR_Chute.c`): a script command, run identically on the
      server and every client
    - the view from the plane (`BR_PlaneCamera.c`), which borrows the unconscious camera's slot
    - a hook that lets the clean-up find supply crates (`BR_Crates.c`)
    - cars spawned with only part of a tank (`BR_CarScript.c`)
  - `data/`: the parachute model, its textures and material. All of these are generated:
    don't edit them by hand.
  - `5_Mission`: the round itself (`BR_Round.c`), the plane (`BR_Flight.c`), loot, cars and
    supply drops (`BR_Loot.c`), the settings (`BR_Settings.c`), the client HUD and F key
    (`BR_MissionGameplay.c`), and the lines on the map (`BR_Map.c`).
- `mission/`: the `battleroyale.nasdara` mission.
  - `battleroyale.json`: the settings. Every field and its default is in `BR_Settings.c`.
  - `br_spots.json`: the loot and car spots.
  - `db/economy.xml`: switches the Central Economy off.
  - `expansion/missions/`: deliberately empty, which stops Expansion's own random airdrops.
- `tools/make_spots.py`: rebuilds `br_spots.json` from vanilla `mapgroupproto.xml`,
  `mapgrouppos.xml`, and the car spots in DayZMissions' `roles.nasdara`.
- The parachute, built the same way as the Molotov mod's model:
  - `blender/build_parachute.py` builds the model, `models/parachute.blend` and the preview
    renders in `models/`.
  - `tools/make_parachute_textures.py` makes the textures (sources in `textures/`).
  - `tools/parachute_layout.py` holds the shape, shared by both.
  - These need Blender 4.2 with the Arma 3 Object Builder add-on, Pillow, GE-Proton and
    DayZ Tools (ImageToPAA, Binarize).
- `tools/build.sh`: stages the mod in `build/BattleRoyale`, with the model binarized.

**Mods it needs:**
- From Expansion (the planes, supply drops, the map on M, server markers, notifications):
  CF, Dabs Framework, DayZ-Expansion-Licensed, -Core, -Navigation and -Missions.
- FlyingWithoutCTD: it keeps the view distance capped while on the server, since high up
  the game has far more to stream in and freezes.

The full list is in `mission/mods.txt`.

## Running it

It uses the DayZMissions tooling. `~/Projects/DayZMissions/mods/BattleRoyale` links to the
staged build, `build/BattleRoyale`, and `missions/battleroyale.nasdara` links to `mission/`.
Run `tools/build.sh` after any change to the mod, scripts included, before starting a
server.

```bash
tools/build.sh                     # in this repo: stage the mod
cd ~/Projects/DayZMissions
tools/test.sh battleroyale         # headless check, prints [BRTest] lines, about a minute
tools/run_server.sh battleroyale   # local server
tools/run_client.sh battleroyale   # client with the same mods, connects to 127.0.0.1
```

What the headless test checks:
- every loot class exists
- loot spawns in buildings, and every gun has a magazine in it
- cars spawn with 15% fuel
- the kit, on a character who still has a loaded gun in hand and a full bag
- where the next circle goes
- the plane's route, height and flight time, and that Expansion's plane is created
- the parachute: a test character jumps from 50 m and lands unhurt
- a supply-drop plane is sent, and a supply crate is cleaned up
- the clean-up leaves nothing behind

It can't check anything that needs a player: the HUD, the tint, F, the view from the plane,
steering, the canopy, the lines on the map, teleporting and damage.

## Known limits

- **Other players' steering:** only the jumper's own game and the server know their steering
  keys, so other players may see a steering jumper corrected now and then. Not yet tried with
  more than one player.
- **No hands-up pose under the parachute:** gestures don't play while the jump controls the
  character.
- **Diagnostic logging is still on:** the parachute logs every 2 s on every side, and the
  client logs a heartbeat every 2 s during a round.
