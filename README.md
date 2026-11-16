# DayZ Battle Royale

Solo battle royale rounds on DayZ, starting with Nasdara. This is the simplest version of
every feature, built to get a playable demo running. `DESIGN.md` has the bigger picture and
what comes later.

## How a round goes

1. **Lobby.** Everyone who isn't in a round waits in the lobby, on a patch of flat open ground
   off the edge of the map, near (220, 1780). There nobody can be hurt, and anyone who walks
   more than 30 m off is brought back. A clock at the top of the screen counts down to the
   next round. It runs 2 minutes after a server start (`firstLobbySeconds`), so people can log
   in, and 1 minute between rounds. New characters are created here, and so is anyone who
   dies. Meanwhile the next round is got ready: an 8 km circle (radius), which covers all of
   Nasdara, with loot in 6,000 buildings inside it (1 to 4 piles each, by size, about 25,000
   items) and 40 cars at the vanilla car spawn points. It goes out a few buildings at a time
   over about 25 s, so the server never stalls.
2. **The plane.** When the clock runs out, everyone in the lobby is stripped, healed and given
   the same plain kit (T-shirt, jeans, trainers and a bandage), and put on the plane. The time
   is set to noon. The plane crosses the circle on a straight route at 400 m above the highest
   ground under it, in about 2 minutes (`planeSpeed`, `planeHeight`). Its route and where it is
   now are drawn on the map. Players wait in the lobby, watching the map, until they jump.
   - **F** jumps. Anyone still on board at the end of the route is dropped there.
   - **The jump:** free fall at 45 m/s, steering with the movement keys, until 150 m above the
     ground, where the parachute opens by itself. Then a slow glide down at 6 m/s, steering
     at 9 m/s. Landing does no damage. There's no canopy model yet: you come down in the
     falling pose.
3. **Game.** The circle stays still for a while, then shrinks onto a smaller one inside it.
   This repeats over seven phases, about 27 minutes in total from take-off, until the last circle closes
   completely.
   - **The map (M):** the current circle is drawn in red and the next one in white, as real
     lines over Expansion's map. The next circle's centre has a "Safe zone" marker you can
     also see in the world.
   - **Outside the circle:** the screen turns blue (the blue sport-glasses tint, stronger) and
     you lose health every second. The loss grows each phase: 1, 1, 2, 4, 6, 8, then 10 per second,
     out of 100.
   - **Supply drops:** every 4 minutes, Expansion's plane drops a crate inside the next circle.
     It flies in fast (200 m/s), so it arrives about a minute after it's sent.
     It holds top-tier armour, an M4 with an ACOG and suppressor, an SVD with a PSO, ammo and
     morphine. The crate shows on the map.
   - A player who dies or leaves is out. Notifications announce who is out and how many are left.
4. **End.** The last player standing wins, and everyone is told. With only one player in the
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
  - `3_Game`: the state the server sends each player every second, and the blue tint.
  - `4_World`: lets a car be spawned with only part of a tank.
  - `4_World` also has the parachute (`BR_Chute.c`), and a hook that lets the clean-up find
    supply crates (`BR_Crates.c`).
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

**Mods it needs** (from Expansion: supply drops, the map on M, server markers, notifications):
CF, Dabs Framework, DayZ-Expansion-Licensed, -Core, -Navigation and -Missions. The full list is
in `mission/mods.txt`.

## Running it

It uses the DayZMissions tooling. `~/Projects/DayZMissions/mods/BattleRoyale` and
`missions/battleroyale.nasdara` are symlinks into this repo.

```bash
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

It can't check anything that needs a player: the HUD, the tint, F, steering the parachute,
the lines on the map, teleporting and damage.
