# BattleDayZ

Solo battle royale rounds on DayZ, starting with Nasdara. This is the simplest version of
every feature, built to get a playable demo running. `DESIGN.md` has the bigger picture and
what comes later.

## How a round goes

1. **Lobby.** Everyone who isn't in a round waits in the lobby, on a patch of flat open ground
   off the edge of the map, near (220, 1780). There nobody can be hurt, and anyone who walks
   more than 30 m off is brought back. A clock at the top of the screen counts down to the
   next round: longer after a server start (`firstLobbySeconds`, 5 minutes by default) so
   people can log in, then 1 minute between rounds (`lobbySeconds`). New characters are created here,
   and so is anyone who dies. Everyone arriving in the lobby (new, respawned, or back from a
   round) is reset: the plain kit below, fully healed (every limb, broken legs, bleeding,
   diseases, food and water). Meanwhile the next round is got ready: an 8 km circle (radius), which covers all of
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
   - **The pack:** a black assault bag sits on your back from the jump until you land.
   - **Landing:** about 1.5 m up, the game's own fall takes over and lands you. Nobody can be
     hurt from the jump until 3 seconds after landing.
4. **Game.** The circle goes round the same three steps, circle after circle:
   - **Cooldown:** nobody knows where the next circle is. At the start of a round no circle is
     shown and the whole map is safe; after that, the circle you're in is white.
   - **Announced:** the next circle is drawn in white and announced, and the one you're in
     turns yellow.
   - **Closing:** after the notification time the yellow circle turns red and closes evenly
     onto the white one, over the next cooldown. The first time, it starts from a red circle
     just round the whole map.

   Cooldown and notification are each 5 minutes for the first circle, then 4, 3, 2, 1 and then
   30 seconds for every circle after (`circleSeconds`). Each white circle is 70%, 65%, 60%,
   55%, 50%, 40%, then 50% of the one before, starting at 5.6 km. One smaller than 25 m becomes
   nothing: once the red has closed to nothing, about 35 minutes into the round, the storm is
   everywhere and the last one alive wins.
   - **The map (M):**
     - It opens zoomed out, centred on you, or on the plane while you're on board.
     - The circles are real lines over Expansion's map. There's only ever one white circle.
       The red one glides as it closes.
     - The next circle's centre has a "Safe zone" marker, which you can also see in the world.
   - **The compass:** Expansion's compass across the top of the screen, for everyone.
   - **The storm (outside the circle):**
     - **Inside it:** you get what a vanilla gas zone does to a player standing in it: gas
       round you, the yellow tint and grain, its sound and muffled hearing.
     - **Seeing it coming:** within 800 m of the edge you see it. Along the 1.2 km of edge
       nearest you there's a glowing red wall, 40 m tall, with a bright line at its foot along
       the ground. Gas clouds roll behind it. It glides in as the circle closes.
     - **Damage:** every 3 seconds a hit, like a zombie's scratch: you flinch, grunt and see
       red. It's scaled to take 1, 1, 2, 4, 6, 8, then 10 health a second (`circleDamage`, out
       of 100). One hit in four also opens a bleeding cut on an arm or leg.
     - **Patching up:** while you're busy with an action, such as bandaging, the storm takes the
       same health without a hit, so the flinch can't cancel it. Nobody is hurt while
       parachuting.
   - **Supply drops:** the first comes 2 minutes after the circle appears, then one every 4 minutes.
     Expansion's plane drops a crate inside the next circle.
     It flies in fast (200 m/s), so it arrives about a minute after it's sent.
     It holds top-tier armour, an M4 with an ACOG and suppressor, an SVD with a PSO, ammo and
     morphine. The crate shows on the map.
   - A player who dies or leaves is out. Notifications announce who is out and how many are left.
5. **End.** When one player is left, everyone is told who won and who was the last one out.
   With only one player in the round, it ends when that player dies. After 15 s everything
   the round spawned is deleted: loot, cars, bodies and supply crates. Everyone goes back to
   the lobby, and the next round starts once its loot is out.

## Testing settings

Three settings in `battleroyale.json`, all off by default, shorten a test:

- `testStorm`: no plane. Everyone starts on the ground 150 m outside the first white circle,
  with the red 400 m out and closing on it over at most 60 s. Damage is on straight away.
- `testWhiteRadius`: with `testStorm`, the first white circle's radius instead of 5.6 km. With
  50 and `circleSeconds` of `[30]`, a round reaches the end game in about 2.5 minutes.
- `testBots`: this many AI survivors join every round, kitted like everyone else, in the
  middle of the first white circle. They count as players, so a round with a bot is won or
  lost like a real one. They stand still and don't bandage.

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

- `BattleDayZ/`: the mod, loaded on the server and on clients.
  - `3_Game`: the state the server sends each player every second (and where the circle is
    between updates), the jump and steering messages, and where the plane is.
  - `4_World`:
    - the jump and the parachute (`BR_Chute.c`): a script command, run identically on the
      server and every client
    - the view from the plane (`BR_PlaneCamera.c`), which borrows the unconscious camera's slot
    - a hook that lets the clean-up find supply crates (`BR_Crates.c`)
    - cars spawned with only part of a tank (`BR_CarScript.c`)
  - `data/`: the parachute and storm wall models, their textures and materials. All of these
    are generated: don't edit them by hand (except the `.rvmat` files).
  - `5_Mission`: the round itself (`BR_Round.c`), the plane (`BR_Flight.c`), loot, cars and
    supply drops (`BR_Loot.c`), the settings (`BR_Settings.c`), the client HUD, the storm's
    gas and wall, and the F key (`BR_MissionGameplay.c`), and the lines on the map (`BR_Map.c`).
- `mission/`: the `battleroyale.nasdara` mission.
  - `battleroyale.json`: the settings. Every field and its default is in `BR_Settings.c`.
  - `br_spots.json`: the loot and car spots.
  - `db/economy.xml`: switches the Central Economy off.
  - `expansion/missions/`: deliberately empty, which stops Expansion's own random airdrops.
  - `expansion/settings/MapSettings.json`: Expansion's map settings, with the compass on for
    everyone (`NeedCompassItemForHUDCompass` and `NeedGPSItemForHUDCompass` off).
- `tools/make_spots.py`: rebuilds `br_spots.json` from vanilla `mapgroupproto.xml`,
  `mapgrouppos.xml`, and the car spots in DayZMissions' `roles.nasdara`.
- The parachute, built the same way as the Molotov mod's model:
  - `blender/build_parachute.py` builds the model, `models/parachute.blend` and the preview
    renders in `models/`.
  - `tools/make_parachute_textures.py` makes the textures (sources in `textures/`).
  - `tools/parachute_layout.py` holds the shape, shared by both.
  - These need Blender 4.2 with the Arma 3 Object Builder add-on, Pillow, GE-Proton and
    DayZ Tools (ImageToPAA, Binarize).
- The storm wall, the same way: `blender/build_stormwall.py` builds one 10 m × 40 m panel,
  seen from both sides, with no collision or shadow. `tools/make_stormwall_texture.py` makes
  its see-through texture (source in `textures/`). Its glow is in `data/stormwall.rvmat`.
  `model.cfg` is shared, and `build_parachute.py` writes it.
- `tools/build.sh`: stages the mod in `build/BattleDayZ`, with the model binarized.

**Mods it needs:**
- From Expansion (the planes, supply drops, the map on M, server markers, notifications):
  CF, Dabs Framework, DayZ-Expansion-Licensed, -Core, -Navigation and -Missions.
- FlyingWithoutCTD: it keeps the view distance capped while on the server, since high up
  the game has far more to stream in and freezes.

The full list is in `mission/mods.txt`.

## Running it

It uses the DayZMissions tooling. `~/Projects/DayZMissions/mods/BattleDayZ` links to the
staged build, `build/BattleDayZ`, and `missions/battleroyale.nasdara` links to `mission/`.
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
- the circle cycle, on simulated time, down to the storm being everywhere
- a storm hit takes exactly the health it should
- the storm wall's model loads
- a supply-drop plane is sent, and a supply crate is cleaned up
- the clean-up leaves nothing behind

It can't check anything that needs a player: the HUD, the gas, the wall, the compass, F, the
view from the plane, steering, the canopy, the lines on the map, teleporting and the flinch.

## Known limits

- **Other players' steering:** only the jumper's own game and the server know their steering
  keys, so other players may see a steering jumper corrected now and then. Not yet tried with
  more than one player.
- **No hands-up pose under the parachute:** gestures don't play while the jump controls the
  character.
- **Test bots stand still:** they're only there to win or lose against.
- **The storm wall is local to each player:** only the stretch near you is there, and other
  players see their own.
- **Diagnostic logging:** the parachute's log every 2 s, the jump's every second and the
  client's heartbeat are off. `BR_Debug.LOG` in `BR_Common.c` turns them back on.
