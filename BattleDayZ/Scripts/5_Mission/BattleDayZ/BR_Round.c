// The round, server side. One tick a second:
//
//   LOBBY  every player not in the round waits at the lobby (can't be hurt, pulled back if
//          they wander off) while a clock counts down. Meanwhile the next round's circle is
//          picked and its loot and cars put out, a few buildings at a time. At zero, with
//          enough players, the round starts: everyone in the lobby is stripped, given the
//          same plain kit, and put on the plane.
//   GAME   the plane crosses the first circle on a straight route (drawn on the map); F
//          jumps, and anyone still on board is dropped at the end of the route. They come
//          down on a parachute (BR_Chute.c). Meanwhile the circle sits still, then shrinks
//          onto the next one, phase by phase (both drawn on the map). Outside it the screen
//          turns blue and health drains. Supply drops come down inside the next circle. A
//          player who dies (or leaves) is out, and respawns in the lobby. The last one
//          standing wins.
//   ENDED  the winner is announced, then everything the round spawned is deleted and
//          everyone goes back to the lobby.

class BR_Round
{
	static const float LOBBY_LEASH = 30;   // lobby players further than this from it are brought back
	static const int LOOT_STEP_MS = 100;   // the lobby puts out loot this often...
	static const int LOOT_PER_STEP = 30;   // ...in this many buildings (6000 take about 20 s)
	static const float STORM_HIT_SECONDS = 3;     // the storm cuts this often
	static const string STORM_AMMO = "MeleeZombie"; // a zombie scratch: 5 health, 10 shock, no cut of its own
	static const float STORM_AMMO_HEALTH = 5;
	static const int STORM_CUT_ONE_IN = 4;         // a bleeding cut on one storm hit in this many

	protected static ref BR_Round s_Instance;

	protected ref BR_Settings m_Settings;
	protected ref BR_Spots m_Spots;
	protected vector m_Lobby;     // the lobby's ground (with a lobby in the sky, the ground under it)
	protected ref BR_SkyLobby m_Sky;
	protected ref BR_SkyLobby m_SkyOld;   // the one taken down last, only to check it's gone (the self-test)
	protected ref BR_Map m_Map;           // what's particular to this map (BR_Maps.c)
	protected ref array<string> m_LobbyNames = new array<string>();
	protected int m_SkyNext;              // index into m_LobbyNames of the one after this
	protected int m_SkySlot;              // which of the SKY_SLOTS places in the sky the lobby is at
	protected ref map<string, ref array<float>> m_Falls = new map<string, ref array<float>>(); // player id -> when they were caught falling
	protected ref set<string> m_Grounded = new set<string>();  // players waiting on the ground instead
	protected ref set<string> m_Ready = new set<string>();     // players in the lobby who pressed F: start now
	protected float m_SkySince;
	static const int SKY_SLOTS = 4;          // places in a row the lobbies take in turn, so one isn't built
	                                         // where another was taken down only a round before
	static const float SKY_SLOT_APART = 600; // m between them (players' games clear 250 m round the others)
	static const int FALLS_TO_GROUND = 3;    // caught falling this often in FALLS_WINDOW s: on the ground instead
	static const float FALLS_WINDOW = 60;
	static const int SKY_TAKE_DOWN_MS = 5000; // the old one comes down this long after everyone's moved
	protected float m_MinX, m_MaxX, m_MinZ, m_MaxZ; // where the buildings are

	protected int m_Phase = BR_Phase.LOBBY;
	protected float m_PhaseEnds;  // lobby and ended: when the clock runs out
	protected int m_Round;

	protected ref map<string, PlayerBase> m_Alive = new map<string, PlayerBase>(); // player id -> character, in the round
	protected ref map<string, string> m_Names = new map<string, string>();
	protected int m_Started;      // players at the start of the round
	protected string m_Winner;
	protected ref array<Object> m_Spawned = new array<Object>();

	// the circle
	protected vector m_Center;
	protected float m_Radius;
	protected vector m_FromCenter;
	protected float m_FromRadius;
	protected vector m_NextCenter;
	protected float m_NextRadius;
	// the circle (see BR_Settings.circleSeconds): cooldown -> reveal -> notification -> close,
	// the closing running through the next circle's cooldown
	protected int m_Circle;       // which circle: 0 the first
	protected bool m_WholeMap;    // from take-off until the first closing: the whole map is safe
	protected bool m_Revealed;    // the next (white) circle is on the map, about to close in on
	protected bool m_Closing;     // the red circle is closing on the white one
	protected bool m_Storm;       // the last circle has closed to nothing: everyone is in the storm
	protected float m_StageStarted;
	protected float m_StageEnds;
	protected float m_NextDrop;
	protected float m_NextStormHit;
	protected string m_LastOut;
	protected ref map<string, PlayerBase> m_LobbyReset = new map<string, PlayerBase>(); // player id -> the character last reset in the lobby     // the name of whoever was knocked out last
	protected ref array<string> m_Markers = new array<string>();

	// the plane, while it's crossing the map, and who's still on it
	protected ref BR_Flight m_Flight;
	protected ref map<string, PlayerBase> m_InPlane = new map<string, PlayerBase>();

	// the next round's loot, put out during the lobby
	protected ref array<int> m_LootQueue;
	protected int m_LootNext;
	protected int m_LootBuildings;
	protected int m_PrepStarted;
	protected bool m_Prepared;

	static void Start()
	{
		if (s_Instance)
			return;
		s_Instance = new BR_Round();
		s_Instance.Init();
	}

	static BR_Round Get()
	{
		return s_Instance;
	}

	void Init()
	{
		m_Settings = BR_Settings.Load();
		m_Spots = BR_Spots.Load();

		array<string> types = BR_Loot.AllTypes();
		int missing = 0;
		for (int t = 0; t < types.Count(); t++)
		{
			if (!BR_Loot.TypeExists(types[t]))
			{
				Print("[BattleDayZ] unknown class name in the loot: " + types[t]);
				missing++;
			}
		}

		m_MinX = 1000000;
		m_MinZ = 1000000;
		m_MaxX = -1000000;
		m_MaxZ = -1000000;
		for (int i = 0; i < m_Spots.buildings.Count(); i++)
		{
			BR_Building b = m_Spots.buildings[i];
			m_MinX = Math.Min(m_MinX, b.x);
			m_MaxX = Math.Max(m_MaxX, b.x);
			m_MinZ = Math.Min(m_MinZ, b.z);
			m_MaxZ = Math.Max(m_MaxZ, b.z);
		}

		m_Lobby = FlatGround(m_Settings.lobbyX, m_Settings.lobbyZ);
		if (m_Settings.kitExtras)
			BR_Loot.s_KitExtras = m_Settings.kitExtras;
		m_Map = BR_Maps.Current();
		if (m_Settings.lobbies.Count() > 0)
			m_LobbyNames.InsertAll(m_Settings.lobbies);
		else
		{
			foreach (BR_LobbySpec spec : m_Map.m_Lobbies)
				m_LobbyNames.Insert(spec.name);
		}
		Print("[BattleDayZ] map: " + m_Map.m_Title + " (" + GetGame().GetWorldName() + ", " + BR_Maps.Count() + " map profiles), lobbies: " + m_LobbyNames.Count());
		if (m_Settings.lobbyHeight > 0 && m_LobbyNames.Count() > 0)
		{
			m_SkyNext = Math.RandomInt(0, m_LobbyNames.Count());
			ChangeLobby();
		}
		m_PhaseEnds = Now() + m_Settings.firstLobbySeconds;
		BR_Server.s_OnJump.Insert(OnJump);
		// cars made in the first seconds after a server start come out with no fuel; and the
		// headless test (-missiontest) puts out its own loot instead
		string test;
		if (!GetGame().CommandlineGetParam("missiontest", test))
			GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(PrepareRound, 30000, false);
		Print("[BattleDayZ] ready: " + m_Spots.buildings.Count() + " loot buildings, " + m_Spots.cars.Count() + " car spots, " + m_Settings.circleSeconds.Count() + " circle timings, " + missing + " unknown loot classes; lobby at " + m_Lobby + ", first round in " + m_Settings.firstLobbySeconds + " s");

		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(Tick, 1000, true);
	}

	static float Now()
	{
		return GetGame().GetTickTime();
	}

	// --- the tick --------------------------------------------------------------------

	void Tick()
	{
		float now = Now();
		StopSandstorm();
		m_Map.OnTick();

		array<Man> men = new array<Man>();
		GetGame().GetPlayers(men);

		if (m_Phase == BR_Phase.GAME)
			UpdateGame(now);

		// everyone alive and not in the round belongs in the lobby, and so does everyone still
		// on the plane: they watch it from a camera (BR_MissionGameplay.c) until they jump
		for (int i = 0; i < men.Count(); i++)
		{
			PlayerBase player = PlayerBase.Cast(men[i]);
			if (!player || !player.IsAlive() || !player.GetIdentity())
				continue;
			if (!InRound(player))
				ResetInLobby(player);
			if (!InRound(player) && Watch(player))
				continue; // watching the round from above someone still in it
			if (!InRound(player) || InPlane(player))
				KeepInLobby(player);
		}

		if (m_Phase == BR_Phase.LOBBY && m_Settings.lobbyChangeSeconds > 0 && now - m_SkySince >= m_Settings.lobbyChangeSeconds && now < m_PhaseEnds - 10)
			ChangeLobby();

		// not enough players waiting (nobody, say): the clock holds at its full time, so whoever
		// arrives first sees all of it (5 minutes after a server start, 3 between rounds)
		if (m_Phase == BR_Phase.LOBBY && LobbyPlayers(men).Count() < Math.Max(1, m_Settings.minPlayers))
		{
			if (m_Round == 0)
				m_PhaseEnds = now + m_Settings.firstLobbySeconds;
			else
				m_PhaseEnds = now + m_Settings.lobbySeconds;
		}

		// everyone waiting has pressed F: no need to wait out the clock
		if (m_Phase == BR_Phase.LOBBY && now < m_PhaseEnds)
		{
			array<PlayerBase> waitingNow = LobbyPlayers(men);
			if (waitingNow.Count() > 0 && waitingNow.Count() >= m_Settings.minPlayers && ReadyCount(waitingNow) == waitingNow.Count())
			{
				m_PhaseEnds = now;
				Print("[BattleDayZ] everyone waiting is ready (" + waitingNow.Count() + "): starting");
			}
		}

		if (m_Phase == BR_Phase.LOBBY && now >= m_PhaseEnds)
		{
			int waiting = LobbyPlayers(men).Count();
			if (waiting < m_Settings.minPlayers || waiting == 0)
				m_PhaseEnds = now + m_Settings.lobbySeconds;
			else if (m_Prepared)
				StartRound(men);
			// else: the loot isn't all out yet, start as soon as it is
		}
		else if (m_Phase == BR_Phase.ENDED && now >= m_PhaseEnds)
		{
			Cleanup();
			ChangeLobby();
			m_Phase = BR_Phase.LOBBY;
			m_PhaseEnds = now + m_Settings.lobbySeconds;
			PrepareRound();
		}

		Broadcast(men, now);
	}

	int ReadyCount(array<PlayerBase> players)
	{
		int ready = 0;
		foreach (PlayerBase p : players)
		{
			if (p.GetIdentity() && m_Ready.Find(p.GetIdentity().GetId()) >= 0)
				ready++;
		}
		return ready;
	}

	bool InRound(PlayerBase player)
	{
		if (!player.GetIdentity())
			return false;
		PlayerBase inRound;
		return m_Alive.Find(player.GetIdentity().GetId(), inRound) && inRound == player;
	}

	bool InPlane(PlayerBase player)
	{
		return player.GetIdentity() && m_InPlane.Contains(player.GetIdentity().GetId());
	}

	array<PlayerBase> LobbyPlayers(array<Man> men)
	{
		array<PlayerBase> lobby = new array<PlayerBase>();
		for (int i = 0; i < men.Count(); i++)
		{
			PlayerBase player = PlayerBase.Cast(men[i]);
			if (player && player.IsAlive() && player.GetIdentity() && !InRound(player))
				lobby.Insert(player);
		}
		return lobby;
	}

	// arriving in the lobby (back from a round, or a new or respawned character): the kit at full
	// health, once per arrival
	void ResetInLobby(PlayerBase player)
	{
		string id = player.GetIdentity().GetId();
		PlayerBase done;
		if (m_LobbyReset.Find(id, done) && done == player)
			return;
		BR_Loot.Kit(player);
		m_LobbyReset.Set(id, player);
		Print("[BattleDayZ] " + player.GetIdentity().GetName() + " reset in the lobby");
		// knocked out (or joined) while a round is on: straight to watching it
		if (m_Phase == BR_Phase.GAME && !m_InPlane.Contains(id))
			NextWatch(player);
	}

	// --- watching the round when out of it ---------------------------------------------
	// Someone out of the round watches a player still in it: their own (new) character is held,
	// unhurt, WATCH_ABOVE m above that player, so their game has them and what's round them, and
	// their camera follows them (BR_PlaneCamera.c). F moves on to the next; past the last, back
	// to the lobby (F there watches again). The round's end brings everyone back.

	static const float WATCH_ABOVE = 150;
	protected ref map<string, string> m_Watching = new map<string, string>(); // watcher id -> watched id

	// the next player in the round to watch after the one being watched (none: back to the lobby)
	void NextWatch(PlayerBase player)
	{
		string id = player.GetIdentity().GetId();
		string current;
		m_Watching.Find(id, current);
		array<string> ids = new array<string>();
		for (int i = 0; i < m_Alive.Count(); i++)
		{
			PlayerBase them = m_Alive.GetElement(i);
			if (them && them.IsAlive() && m_Alive.GetKey(i) != id)
				ids.Insert(m_Alive.GetKey(i));
		}
		ids.Sort();
		string next = "";
		if (current == "")
		{
			if (ids.Count() > 0)
				next = ids[0];
		}
		else
		{
			int at = ids.Find(current);
			if (at >= 0 && at + 1 < ids.Count())
				next = ids[at + 1];
			else if (at < 0 && ids.Count() > 0)
				next = ids[0]; // the one watched is out: the first of the rest
		}
		if (next == "")
		{
			m_Watching.Remove(id);
			Print("[BattleDayZ] " + player.GetIdentity().GetName() + " stops watching");
			return;
		}
		m_Watching.Set(id, next);
		Print("[BattleDayZ] " + player.GetIdentity().GetName() + " watches " + m_Names.Get(next));
	}

	// hold a watcher above whoever they watch; false if they aren't watching (any more)
	bool Watch(PlayerBase player)
	{
		string id = player.GetIdentity().GetId();
		string target;
		if (m_Phase != BR_Phase.GAME || !m_Watching.Find(id, target))
		{
			m_Watching.Remove(id);
			return false;
		}
		PlayerBase them;
		if (!m_Alive.Find(target, them) || !them || !them.IsAlive())
		{
			NextWatch(player);
			if (!m_Watching.Find(id, target) || !m_Alive.Find(target, them) || !them)
				return false;
		}
		player.SetAllowDamage(false);
		player.SetPosition(them.GetPosition() + Vector(0, WATCH_ABOVE, 0));
		return true;
	}

	void KeepInLobby(PlayerBase player)
	{
		player.SetAllowDamage(false);
		string id = "";
		string who = "a test body";
		if (player.GetIdentity())
		{
			id = player.GetIdentity().GetId();
			who = player.GetIdentity().GetName();
		}
		vector pos = player.GetPosition();
		if (!m_Sky || m_Grounded.Find(id) >= 0)
		{
			if (BR_Loot.Distance2D(pos, m_Lobby) > LOBBY_LEASH || pos[1] < GetGame().SurfaceY(pos[0], pos[2]) - 5)
				player.SetPosition(GroundSpot());
			return;
		}
		if (!m_Sky.Strayed(pos))
			return;
		// fallen through, not just walked off: a game that can't stand on the floor (it has an
		// old lobby's models in the way, say) would fall again and again: after a few, it waits
		// on the ground under the lobby instead, until the next round
		if (BR_Loot.Distance2D(pos, m_Sky.m_Origin) <= m_Sky.m_Leash)
		{
			array<float> falls;
			if (!m_Falls.Find(id, falls))
			{
				falls = new array<float>();
				m_Falls.Set(id, falls);
			}
			float now = Now();
			falls.Insert(now);
			while (falls.Count() > 0 && now - falls[0] > FALLS_WINDOW)
				falls.Remove(0);
			Print("[BattleDayZ] " + who + " fell through the lobby (" + falls.Count() + " in " + FALLS_WINDOW + " s), " + (pos[1] - m_Sky.m_Origin[1]) + " m below it");
			if (falls.Count() >= FALLS_TO_GROUND)
			{
				m_Grounded.Insert(id);
				falls.Clear();
				player.SetPosition(GroundSpot());
				Print("[BattleDayZ] " + who + " keeps falling through the lobby: waiting on the ground under it instead");
				return;
			}
		}
		player.SetPosition(LobbySpot());
	}

	// the flattest dry ground within 1.5 km of x, z: the lobby needs room to stand around
	vector FlatGround(float x, float z)
	{
		vector best = Vector(x, GetGame().SurfaceY(x, z), z);
		float bestScore = -1;
		for (float r = 0; r <= 1500; r += 50)
		{
			for (int step = 0; step < 16; step++)
			{
				float px = x + Math.Cos(Math.PI2 * step / 16) * r;
				float pz = z + Math.Sin(Math.PI2 * step / 16) * r;
				if (GetGame().SurfaceIsSea(px, pz) || GetGame().SurfaceIsPond(px, pz))
					continue;
				// worst slope over a 20 m square
				float score = 1;
				for (int dx = -1; dx <= 1; dx++)
				{
					for (int dz = -1; dz <= 1; dz++)
						score = Math.Min(score, GetGame().SurfaceGetNormal(px + dx * 10, pz + dz * 10)[1]);
				}
				if (score > bestScore)
				{
					bestScore = score;
					best = Vector(px, GetGame().SurfaceY(px, pz), pz);
				}
				if (r == 0)
					break;
			}
			if (bestScore > 0.99)
				break;
		}
		Print("[BattleDayZ] lobby ground: " + best + ", flatness " + bestScore);
		return best;
	}

	// the next lobby in the sky: built at the other place, everyone not in a round moved across,
	// then the old one taken down. Only one is up at a time (bar the moment of the swap); the
	// clients take the old one down on their side (BR_LobbyGhosts), told where it is by the state.
	void ChangeLobby()
	{
		m_SkySince = Now();
		int count = m_LobbyNames.Count();
		if (count == 0 || m_Settings.lobbyHeight <= 0 || (count == 1 && m_Sky))
			return;
		BR_SkyLobby next;
		for (int tries = 0; tries < count && !next; tries++)
		{
			string name = m_LobbyNames[m_SkyNext];
			m_SkyNext = (m_SkyNext + 1) % count;
			int slot = (m_SkySlot + 1) % SKY_SLOTS;
			next = BR_SkyLobby.Build(name, m_Lobby + Vector(slot * SKY_SLOT_APART, m_Settings.lobbyHeight, 0), m_Map);
			if (next)
				m_SkySlot = slot;
		}
		if (!next)
			return; // none could be built: keep what there is
		BR_SkyLobby old = m_Sky;
		m_Sky = next;
		if (!old)
			return;
		array<Man> men = new array<Man>();
		GetGame().GetPlayers(men);
		int moved = 0;
		foreach (Man man : men)
		{
			PlayerBase player = PlayerBase.Cast(man);
			if (player && player.IsAlive() && player.GetIdentity() && !InRound(player))
			{
				player.SetPosition(LobbySpot());
				moved++;
			}
		}
		// taken down a few seconds later: deleting the floor a player was standing on in the same
		// frame as moving them crashed the server (in its simulation of that player)
		m_SkyOld = old;
		m_SkyRetired.Insert(old);
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(TakeDown, SKY_TAKE_DOWN_MS, false, old);
		Print("[BattleDayZ] the lobby is now " + m_Sky.m_Title + ", " + moved + " moved across");
	}

	protected ref array<ref BR_SkyLobby> m_SkyRetired = new array<ref BR_SkyLobby>();

	void TakeDown(BR_SkyLobby old)
	{
		if (!old)
			return;
		old.Destroy();
		m_SkyRetired.RemoveItem(old);
	}

	// where a player in the lobby stands
	vector LobbySpot()
	{
		if (m_Sky)
			return m_Sky.Spot();
		return GroundSpot();
	}

	// the ground at the lobby (under it, with a lobby in the sky): the self-test's things go here
	vector GroundSpot()
	{
		float x = m_Lobby[0] + Math.RandomFloat(-8, 8);
		float z = m_Lobby[2] + Math.RandomFloat(-8, 8);
		return Vector(x, GetGame().SurfaceY(x, z), z);
	}

	// new characters are made at the lobby instead of a beach
	vector LobbyPosition()
	{
		return LobbySpot();
	}

	void StopSandstorm()
	{
		Weather weather = GetGame().GetWeather();
		if (!weather || !weather.GetSandstorm())
			return;
		if (weather.GetSandstorm().IsActive())
			weather.GetSandstorm().Stop(5, false);
	}

	// --- getting the next round ready (during the lobby) -----------------------------

	// pick the first circle and start putting out its loot and cars, a few buildings at a time
	void PrepareRound()
	{
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(LootStep);
		float r = m_Settings.startRadius;
		BR_Building anchor = m_Spots.buildings.GetRandomElement();
		float cx = OnMap(anchor.x, m_MinX, m_MaxX, r);
		float cz = OnMap(anchor.z, m_MinZ, m_MaxZ, r);
		m_Center = Vector(cx, 0, cz);
		m_Radius = r;

		m_LootQueue = BR_Loot.BuildingsInside(m_Spots, m_Center, r);
		m_LootNext = 0;
		m_LootBuildings = 0;
		m_Prepared = false;
		m_PrepStarted = GetGame().GetTime();
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(LootStep, LOOT_STEP_MS, true);
	}

	void LootStep()
	{
		int tried = 0;
		while (tried < LOOT_PER_STEP && m_LootNext < m_LootQueue.Count() && m_LootBuildings < m_Settings.lootBuildings)
		{
			if (BR_Loot.LootBuilding(m_Spots, m_LootQueue[m_LootNext], m_Spawned))
				m_LootBuildings++;
			m_LootNext++;
			tried++;
		}
		if (m_LootNext < m_LootQueue.Count() && m_LootBuildings < m_Settings.lootBuildings)
			return;

		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(LootStep);
		int items = m_Spawned.Count();
		// the vehicles go all over the map, the corners and edges outside the first circle too
		float world = GetGame().GetWorld().GetWorldSize();
		int cars = BR_Loot.Cars(m_Spots, Vector(world / 2, 0, world / 2), world, m_Settings.cars, m_Settings.carFuel, m_Settings.vehicles, m_Spawned, m_Settings.carSquare, m_Lobby);
		int boats = BR_Loot.Boats(m_Spots, m_Settings.boats, m_Settings.carFuel, m_Settings.boatTypes, m_Spawned);
		m_Prepared = true;
		float seconds = (GetGame().GetTime() - m_PrepStarted) / 1000.0;
		Print("[BattleDayZ] next round ready: circle " + m_Center + " r " + m_Radius + ", loot in " + m_LootBuildings + " buildings (" + items + " things), " + cars + " cars, " + boats + " boats, put out over " + seconds + " s");
	}

	// --- starting a round: everyone in the lobby onto the plane ------------------------

	void StartRound(array<Man> men)
	{
		m_Grounded.Clear(); // back up in the sky next time
		m_Ready.Clear();
		m_Falls.Clear();
		m_Round++;

		// noon, every round
		int year, month, day, hour, minute;
		GetGame().GetWorld().GetDate(year, month, day, hour, minute);
		GetGame().GetWorld().SetDate(year, month, day, 12, 0);

		if (!m_Settings.testStorm)
		{
			m_Flight = BR_Flight.Plan(m_Center, m_Radius, GetGame().GetWorld().GetWorldSize(), m_Settings.planeHeight, m_Settings.planeSpeed);
			m_Flight.TakeOff(Now());
			BR_Plane.Set(m_Flight.PositionAt(Now()), m_Flight.Velocity());
		}

		// everyone in the lobby onto the plane
		array<PlayerBase> players = LobbyPlayers(men);
		m_Alive.Clear();
		m_Names.Clear();
		m_InPlane.Clear();
		for (int i = 0; i < players.Count(); i++)
		{
			PlayerBase player = players[i];
			BR_Loot.Kit(player);
			string id = player.GetIdentity().GetId();
			m_Alive.Set(id, player);
			m_Names.Set(id, player.GetIdentity().GetName());
			m_LobbyReset.Remove(id); // reset again on coming back
			m_InPlane.Set(id, player);
		}
		m_Started = players.Count();
		BR_Leftovers.s_On = true;
		m_Winner = "";
		m_LastOut = "";

		// the first cooldown: no circle shown and the whole map safe (m_Center and m_Radius, the
		// start circle, are only where the first white circle is picked from)
		m_Circle = 0;
		m_WholeMap = true;
		m_Revealed = false;
		m_Closing = false;
		m_Storm = false;
		m_NextRadius = m_Radius;
		m_NextCenter = m_Center;
		m_StageEnds = Now() + m_Settings.CircleSeconds(0);
		m_NextDrop = m_StageEnds + m_Settings.supplyDropSeconds / 2; // not before the circle shows
		m_Phase = BR_Phase.GAME;
		if (m_Settings.testStorm)
		{
			TestStormStart(players);
			AddBots();
			UpdateMarkers();
			return;
		}
		AddBots();
		UpdateMarkers();

		Print("[BattleDayZ] round " + m_Round + " started: " + m_Started + " players, circle " + m_Center + " r " + m_Radius + ", plane " + m_Flight.from + " -> " + m_Flight.to + " (" + m_Flight.length + " m at " + m_Flight.altitude + " m, " + m_Flight.Duration() + " s)");
		Announce("Round " + m_Round + ": the plane is taking off", "Its route is on the map (M). Press F to jump.");
	}

	// testStorm: straight to the first red circle closing, with everyone on the ground
	// TEST_STORM_OUTSIDE m outside the white circle and the red TEST_STORM_RED m out, closing
	// on the white over TEST_STORM_SECONDS (it reaches them after about 35 s)
	static const float TEST_STORM_OUTSIDE = 150;
	static const float TEST_STORM_RED = 400;
	static const float TEST_STORM_SECONDS = 60; // at most
	void TestStormStart(array<PlayerBase> players)
	{
		float now = Now();
		PickNext(m_Settings.CircleFraction(0));
		if (m_Settings.testWhiteRadius > 0)
			m_NextRadius = m_Settings.testWhiteRadius;

		// a spot outside the white circle that's on the map and not in the sea
		float size = GetGame().GetWorld().GetWorldSize();
		float d = m_NextRadius + TEST_STORM_OUTSIDE;
		vector spot = m_NextCenter;
		for (int t = 0; t < 100; t++)
		{
			float a = Math.RandomFloat(0, Math.PI2);
			float x = m_NextCenter[0] + Math.Cos(a) * d;
			float z = m_NextCenter[2] + Math.Sin(a) * d;
			if (x < 200 || z < 200 || x > size - 200 || z > size - 200 || GetGame().SurfaceIsSea(x, z) || GetGame().SurfaceIsPond(x, z))
				continue;
			spot = Vector(x, GetGame().SurfaceY(x, z), z);
			break;
		}

		m_InPlane.Clear();
		for (int i = 0; i < players.Count(); i++)
		{
			vector pos = spot + Vector(Math.RandomFloat(-5, 5), 0, Math.RandomFloat(-5, 5));
			pos[1] = GetGame().SurfaceY(pos[0], pos[2]);
			players[i].SetPosition(pos);
			players[i].SetAllowDamage(true); // the lobby's protection, which landing would end otherwise
		}

		m_WholeMap = false;
		m_Revealed = false;
		m_Closing = true;
		m_Circle = 1;
		m_FromCenter = m_NextCenter;
		m_FromRadius = m_NextRadius + TEST_STORM_RED;
		m_Center = m_FromCenter;
		m_Radius = m_FromRadius;
		m_StageStarted = now;
		m_StageEnds = now + Math.Min(TEST_STORM_SECONDS, m_Settings.CircleSeconds(1));
		Print("[BattleDayZ] round " + m_Round + " started (testStorm): " + m_Started + " players at " + spot + ", white r " + m_NextRadius + " at " + m_NextCenter + ", red r " + m_Radius + " closing over " + (m_StageEnds - now) + " s");
		Announce("Round " + m_Round + " (storm test)", "The storm is coming: get inside the white circle on the map.");
	}

	// from the plane, a little spread out so a crowd doesn't land in one heap: where the
	// jumper's own game shows it (it can trail if the game was still loading at take-off),
	// unless that's implausibly far from where it really is
	void Jump(PlayerBase player, vector seenPlane = "0 0 0")
	{
		m_InPlane.Remove(player.GetIdentity().GetId());
		vector plane = m_Flight.PositionAt(Now());
		if (seenPlane != vector.Zero && vector.Distance(seenPlane, plane) < 300)
			plane = seenPlane;
		vector pos = plane + Vector(Math.RandomFloat(-15, 15), -4, Math.RandomFloat(-15, 15));
		player.BR_StartChute(); // also no damage until just after landing
		player.SetPosition(pos);
		Print("[BattleDayZ] " + player.GetIdentity().GetName() + " jumped at " + pos + ", " + BR_ChuteCommand.HeightAboveGround(pos) + " m above the ground");
	}

	// F: in the lobby, ready to start; out of the round, watch it (the next player); out of the
	// plane; or (falling) open the chute
	void OnJump(PlayerIdentity sender, vector seenPlane)
	{
		if (m_Phase == BR_Phase.GAME && sender && !m_Alive.Contains(sender.GetId()) && !m_InPlane.Contains(sender.GetId()))
		{
			PlayerBase watcher = PlayerBase.Cast(sender.GetPlayer());
			if (watcher && watcher.IsAlive())
				NextWatch(watcher);
			return;
		}
		if (m_Phase == BR_Phase.LOBBY && sender && m_Ready.Find(sender.GetId()) < 0)
		{
			m_Ready.Insert(sender.GetId());
			Print("[BattleDayZ] " + sender.GetName() + " is ready");
			return;
		}
		if (m_Phase != BR_Phase.GAME || !sender)
			return;
		PlayerBase player;
		if (m_Flight && m_InPlane.Find(sender.GetId(), player) && player && player.IsAlive())
		{
			Jump(player, seenPlane);
			return;
		}
		if (m_Alive.Find(sender.GetId(), player) && player)
		{
			Print("[BattleDayZ] " + sender.GetName() + " pressed F, " + BR_ChuteCommand.HeightAboveGround(player.GetPosition()) + " m above the ground, chuting " + player.BR_IsChuting() + ", command " + player.GetCommand_Script());
			if (player.BR_IsChuting())
				player.BR_OpenChute();
		}
	}

	void UpdateFlight(float now)
	{
		if (!m_Flight)
			return;
		BR_Plane.Set(m_Flight.FlyingAt(now), m_Flight.Velocity());
		if (!m_Flight.Over(now))
			return;
		// the end of the route: everyone still on board goes
		array<PlayerBase> left = new array<PlayerBase>();
		for (int i = 0; i < m_InPlane.Count(); i++)
		{
			if (m_InPlane.GetElement(i) && m_InPlane.GetElement(i).IsAlive())
				left.Insert(m_InPlane.GetElement(i));
		}
		for (int j = 0; j < left.Count(); j++)
			Jump(left[j]);
		m_InPlane.Clear();
		// the route and the plane stay on the map until it has flown off it
		if (m_Flight.OffMap(now, GetGame().GetWorld().GetWorldSize()))
			m_Flight = null;
	}

	// --- the round in progress -------------------------------------------------------

	void UpdateGame(float now)
	{
		// who's out
		for (int i = m_Alive.Count() - 1; i >= 0; i--)
		{
			PlayerBase player = m_Alive.GetElement(i);
			string id = m_Alive.GetKey(i);
			if (player && player.IsAlive() && (player.GetIdentity() || IsBot(id)))
				continue;
			m_Alive.RemoveElement(i);
			m_InPlane.Remove(id);
			if (player && !player.IsAlive() && m_Spawned.Find(player) < 0)
				m_Spawned.Insert(player); // the body goes with the round
			m_LastOut = m_Names.Get(id);
			Print("[BattleDayZ] " + m_LastOut + " is out, " + m_Alive.Count() + " left");
			if (m_Alive.Count() > 1)
				Announce(m_LastOut + " is out", m_Alive.Count().ToString() + " left.");
		}

		if ((m_Started >= 2 && m_Alive.Count() <= 1) || m_Alive.Count() == 0)
		{
			EndRound();
			return;
		}

		UpdateFlight(now);
		UpdateZone(now);

		// the storm cuts: a real hit every STORM_HIT_SECONDS (the player flinches, grunts, sees the
		// red splatter and may start bleeding), scaled so it takes circleDamage health a second
		if (now >= m_NextStormHit)
		{
			m_NextStormHit = now + STORM_HIT_SECONDS;
			float perHit = m_Settings.CircleDamage(m_Circle) * STORM_HIT_SECONDS;
			for (int a = 0; a < m_Alive.Count(); a++)
			{
				PlayerBase p = m_Alive.GetElement(a);
				if (InPlane(p) || m_WholeMap || p.BR_IsChuting())
					continue;
				if (BR_Loot.Distance2D(p.GetPosition(), m_Center) > m_Radius)
					StormCut(p, perHit);
			}
		}

		if (m_Settings.supplyDropSeconds > 0 && now >= m_NextDrop && m_NextRadius > 50 && !m_Storm)
		{
			m_NextDrop = now + m_Settings.supplyDropSeconds;
			float angle = Math.RandomFloat(0, Math.PI2);
			float dist = Math.Sqrt(Math.RandomFloat01()) * m_NextRadius * 0.7;
			vector drop = m_NextCenter + Vector(Math.Cos(angle) * dist, 0, Math.Sin(angle) * dist);
			if (BR_Loot.SupplyDrop(drop))
			{
				Print("[BattleDayZ] supply drop to " + drop);
				Announce("Supply drop", "A plane is on its way. Watch the map.");
			}
		}
	}

	void UpdateZone(float now)
	{
		if (m_Storm)
			return;

		// the red circle closing on the white one
		if (m_Closing)
		{
			float f = Math.Clamp((now - m_StageStarted) / Math.Max(m_StageEnds - m_StageStarted, 1), 0, 1);
			m_Center = m_FromCenter + (m_NextCenter - m_FromCenter) * f;
			m_Radius = m_FromRadius + (m_NextRadius - m_FromRadius) * f;
		}
		if (now < m_StageEnds)
			return;

		if (!m_Revealed)
		{
			// the end of a cooldown: the red has reached the white circle (if it was closing)...
			if (m_Closing)
			{
				m_Closing = false;
				m_Center = m_NextCenter;
				m_Radius = m_NextRadius;
				if (m_Radius <= 0)
				{
					m_Storm = true;
					Print("[BattleDayZ] circle " + m_Circle + ": closed to nothing, the storm is everywhere");
					UpdateMarkers();
					Announce("The storm is everywhere", "Last one standing wins.");
					return;
				}
			}
			// ...and the next white circle is revealed
			PickNext(m_Settings.CircleFraction(m_Circle));
			if (m_NextRadius < m_Settings.circleMinRadius)
			{
				m_NextRadius = 0;
				m_NextCenter = m_Center;
			}
			m_Revealed = true;
			m_StageStarted = now;
			m_StageEnds = now + m_Settings.CircleSeconds(m_Circle);
			Print("[BattleDayZ] circle " + m_Circle + ": white circle revealed at " + m_NextCenter + " r " + m_NextRadius + ", closes in " + m_Settings.CircleSeconds(m_Circle) + " s");
			UpdateMarkers();
			if (m_NextRadius > 0)
				Announce("The next circle is on the map", "It starts closing in " + Clock(m_StageEnds - now) + ".");
			else
				Announce("The last circle", "In " + Clock(m_StageEnds - now) + " it closes to nothing.");
			return;
		}

		// the end of the notification: the old white circle turns red and closes on the new one,
		// over the next circle's cooldown. The first time it starts from just round the map.
		m_Revealed = false;
		m_Closing = true;
		m_FromCenter = m_Center;
		m_FromRadius = m_Radius;
		if (m_WholeMap)
		{
			m_WholeMap = false;
			m_FromRadius = WholeMapRadius(m_Center);
			m_Radius = m_FromRadius;
		}
		m_Circle++;
		m_StageStarted = now;
		m_StageEnds = now + m_Settings.CircleSeconds(m_Circle);
		Print("[BattleDayZ] circle " + m_Circle + ": red closing from r " + m_FromRadius + " to r " + m_NextRadius + " over " + m_Settings.CircleSeconds(m_Circle) + " s");
		Announce("The circle is closing", "Get inside the white circle on the map.");
	}

	// testBots: AI survivors in the round as players, kitted like everyone else, standing near
	// the middle of the next circle (or the first circle with the plane). They're in the round's
	// roster under "bot:N" ids and go with the round's clean-up.
	static const string BOT_ID = "bot:";
	void AddBots()
	{
		for (int b = 1; b <= m_Settings.testBots; b++)
		{
			vector c = m_NextCenter;
			float x = c[0] + Math.RandomFloat(-10, 10);
			float z = c[2] + Math.RandomFloat(-10, 10);
			PlayerBase bot = PlayerBase.Cast(GetGame().CreateObject("SurvivorM_Boris", Vector(x, GetGame().SurfaceY(x, z), z), false, true));
			if (!bot)
				continue;
			BR_Loot.Kit(bot);
			bot.SetAllowDamage(true);
			m_Spawned.Insert(bot);
			string id = BOT_ID + b;
			m_Alive.Set(id, bot);
			m_Names.Set(id, "Bot " + b);
			m_Started++;
			Print("[BattleDayZ] Bot " + b + " joins the round at " + bot.GetPosition());
		}
	}

	static bool IsBot(string id)
	{
		return id.IndexOf(BOT_ID) == 0;
	}

	// one storm hit, taking exactly this much health: a scratch to the torso, scaled (limb hits
	// pass only a fraction on to health). It brings the flinch, grunt and red splatter; one hit in
	// STORM_CUT_ONE_IN also opens a bleeding cut on an arm or leg.
	// While the player is busy with an action (bandaging above all) there's no hit, only the same
	// health taken: a hit's flinch would cancel the action, and nobody could patch themselves up.
	void StormCut(PlayerBase player, float health)
	{
		if (player.GetActionManager() && player.GetActionManager().GetRunningAction())
			player.DecreaseHealth("", "", health);
		else
			player.ProcessDirectDamage(DamageType.CLOSE_COMBAT, player, "Torso", STORM_AMMO, "0 0 0", health / STORM_AMMO_HEALTH);
		if (Math.RandomInt(0, STORM_CUT_ONE_IN) == 0 && player.GetBleedingManagerServer())
		{
			array<string> limbs = {"LeftForeArm", "RightForeArm", "LeftArm", "RightArm", "LeftLeg", "RightLeg", "LeftUpLeg", "RightUpLeg"};
			player.GetBleedingManagerServer().AttemptAddBleedingSourceBySelection(limbs.GetRandomElement());
		}
	}

	// the circle round the whole map from here: through its farthest corner
	static float WholeMapRadius(vector center)
	{
		float size = GetGame().GetWorld().GetWorldSize();
		float dx = Math.Max(center[0], size - center[0]);
		float dz = Math.Max(center[2], size - center[2]);
		return Math.Sqrt(dx * dx + dz * dz);
	}

	// keeps the first circle over the buildings: its centre at least r in from their edge, or in
	// the middle when the circle is wider than the map (then it covers all of it)
	static float OnMap(float value, float min, float max, float r)
	{
		if (min + r >= max - r)
			return (min + max) * 0.5;
		return Math.Clamp(value, min + r, max - r);
	}

	// the next circle: smaller, wholly inside the current one, and off-centre by at most
	// circleDrift of the most that would still fit (so nobody has to cross the whole circle)
	void PickNext(float fraction)
	{
		m_NextRadius = m_Radius * fraction;
		float angle = Math.RandomFloat(0, Math.PI2);
		float dist = Math.RandomFloat(0, (m_Radius - m_NextRadius) * m_Settings.circleDrift);
		m_NextCenter = m_Center + Vector(Math.Cos(angle) * dist, 0, Math.Sin(angle) * dist);
	}

	void EndRound()
	{
		m_Watching.Clear(); // everyone watching comes back to the lobby
		m_Phase = BR_Phase.ENDED;
		m_PhaseEnds = Now() + m_Settings.winnerSeconds;
		if (m_Alive.Count() == 1)
		{
			m_Winner = m_Names.Get(m_Alive.GetKey(0));
			if (m_Started >= 2 && m_LastOut != "")
				Announce(m_Winner + " wins!", m_LastOut + " was the last one out. Round " + m_Round + " is over.");
			else
				Announce(m_Winner + " wins!", "Round " + m_Round + " is over.");
		}
		else
		{
			m_Winner = "";
			Announce("Nobody wins", "Round " + m_Round + " is over.");
		}
		Print("[BattleDayZ] round " + m_Round + " over, winner: '" + m_Winner + "'");
	}

	// everything the round spawned goes, and so does the round's roster: the winner
	// (and anyone else still standing) is sent back to the lobby by the next tick
	void Cleanup()
	{
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(LootStep);
		m_Prepared = false;
		m_Flight = null;
		m_InPlane.Clear();
		int count = 0;
		for (int i = 0; i < m_Spawned.Count(); i++)
		{
			if (m_Spawned[i])
			{
				GetGame().ObjectDelete(m_Spawned[i]);
				count++;
			}
		}
		m_Spawned.Clear();
		int drops = BR_Loot.ClearSupplyDrops();
		BR_Leftovers.s_On = false;
		int leftovers = BR_Leftovers.Clear();
		if (leftovers > 0)
			Print("[BattleDayZ] cleaned up " + leftovers + " wrecks, wreck parts and fires");
		m_Alive.Clear();
		ClearMarkers();
		if (count + drops > 0)
			Print("[BattleDayZ] cleaned up " + count + " things and " + drops + " supply drops");
	}

	// --- telling players -------------------------------------------------------------

	void Announce(string title, string text)
	{
		NotificationSystem.SendNotificationToPlayerIdentityExtended(null, 8, title, text);
	}

	static string Clock(float seconds)
	{
		int s = Math.Max(0, Math.Ceil(seconds));
		int m = s / 60;
		s = s - m * 60;
		string ss = s.ToString();
		if (s < 10)
			ss = "0" + ss;
		return m.ToString() + ":" + ss;
	}

	void Broadcast(array<Man> men, float now)
	{
		string title, subtitle;
		if (m_Phase == BR_Phase.LOBBY)
		{
			array<PlayerBase> lobbyNow = LobbyPlayers(men);
			int waiting = lobbyNow.Count();
			title = "Next round in " + Clock(m_PhaseEnds - now);
			if (now >= m_PhaseEnds && !m_Prepared)
				title = "Getting the map ready...";
			subtitle = waiting.ToString() + " waiting, " + ReadyCount(lobbyNow) + " ready";
			if (waiting < m_Settings.minPlayers)
				subtitle = subtitle + " (" + m_Settings.minPlayers + " needed)";
		}
		else if (m_Phase == BR_Phase.GAME)
		{
			if (m_Storm)
				title = "The storm: last one standing wins";
			else if (m_Closing)
				title = "Circle closing: " + Clock(m_StageEnds - now);
			else if (m_Revealed)
				title = "Circle closes in " + Clock(m_StageEnds - now);
			else
				title = "The circle appears in " + Clock(m_StageEnds - now);
			subtitle = m_Alive.Count().ToString() + " alive";
		}
		else
		{
			if (m_Winner != "")
				title = m_Winner + " wins!";
			else
				title = "Nobody wins";
			subtitle = "Back to the lobby in " + Clock(m_PhaseEnds - now);
		}

		BR_State state = new BR_State();
		state.phase = m_Phase;
		state.wholeMap = m_Phase == BR_Phase.GAME && m_WholeMap;
		state.closing = m_Phase == BR_Phase.GAME && m_Closing;
		state.center = m_Center;
		state.radius = m_Radius;
		state.closeLeft = 0;
		if (m_Closing)
			state.closeLeft = Math.Max(m_StageEnds - now, 0);
		state.hasNext = m_Phase == BR_Phase.GAME && (m_Revealed || m_Closing) && !m_Storm;
		state.nextCenter = m_NextCenter;
		state.nextRadius = m_NextRadius;
		state.flying = m_Phase == BR_Phase.GAME && m_Flight != null;
		string planeLine;
		if (state.flying)
		{
			state.routeFrom = m_Flight.from;
			state.routeTo = m_Flight.to;
			state.planePos = m_Flight.FlyingAt(now);
			state.planeVel = m_Flight.Velocity();
			planeLine = "Press F to jump. Everyone left is dropped at the end of the route in " + Clock(m_Flight.started + m_Flight.Duration() - now);
		}
		state.title = title;
		if (m_Sky)
		{
			state.lobbyAt = m_Sky.m_Origin;
			state.lobbyReach = m_Sky.Reach();
			if (m_Sky.m_Spec)
			{
				state.lobbyX = m_Sky.m_Spec.x;
				state.lobbyZ = m_Sky.m_Spec.z;
				state.lobbyRadius = m_Sky.m_Spec.radius;
				state.lobbyFloorY = m_Sky.m_Spec.floorY;
			}
			state.lobbySlot0 = m_Lobby + Vector(0, m_Settings.lobbyHeight, 0);
			state.lobbySlots = SKY_SLOTS;
			state.lobbyApart = SKY_SLOT_APART;
		}

		for (int i = 0; i < men.Count(); i++)
		{
			PlayerBase player = PlayerBase.Cast(men[i]);
			if (!player || !player.GetIdentity())
				continue;
			state.inPlane = state.flying && InPlane(player);
			state.inRound = m_Phase == BR_Phase.GAME && InRound(player) && !state.inPlane;
			state.subtitle = subtitle;
			if (state.inPlane)
				state.subtitle = planeLine;
			else if (m_Phase == BR_Phase.LOBBY && !state.inRound)
			{
				if (m_Ready.Find(player.GetIdentity().GetId()) >= 0)
					state.subtitle = subtitle + ": you're ready";
				else
					state.subtitle = subtitle + ": press F when you're ready to start";
			}
			// out of the round: who they watch (or the key to start watching)
			state.watching = "";
			state.watchLow = 0;
			state.watchHigh = 0;
			state.watchPos = vector.Zero;
			string watched;
			PlayerBase them;
			if (m_Phase == BR_Phase.GAME && !state.inRound && !state.inPlane)
			{
				if (m_Watching.Find(player.GetIdentity().GetId(), watched) && m_Alive.Find(watched, them) && them)
				{
					state.watching = m_Names.Get(watched);
					them.GetNetworkID(state.watchLow, state.watchHigh);
					state.watchPos = them.GetPosition();
					state.subtitle = "Watching " + state.watching + " - " + subtitle + " | F: next player";
				}
				else
					state.subtitle = subtitle + " | Press F to watch the round";
			}
			ScriptRPC rpc = new ScriptRPC();
			state.Write(rpc);
			rpc.Send(null, BR_State.RPC, false, player.GetIdentity());
		}
	}

	// --- headless check (mission started with -missiontest) -------------------------

	protected ref array<Object> m_TestCars = new array<Object>();
	protected PlayerBase m_TestBody;

	void SelfTest()
	{
		Print("[BRTest] " + m_Spots.protos.Count() + " building types, " + m_Spots.buildings.Count() + " buildings, " + m_Spots.cars.Count() + " car spots, " + m_Settings.circleSeconds.Count() + " circle timings, lobby " + m_Lobby);
		array<string> types = BR_Loot.AllTypes();
		int missing = 0;
		for (int t = 0; t < types.Count(); t++)
		{
			if (!BR_Loot.TypeExists(types[t]))
			{
				Print("[BRTest] unknown loot class " + types[t]);
				missing++;
			}
		}
		Print("[BRTest] " + types.Count() + " loot classes, " + missing + " unknown");

		BR_Building anchor = m_Spots.buildings.GetRandomElement();
		m_Center = Vector(anchor.x, 0, anchor.z);
		m_Radius = 1500;
		int buildings = BR_Loot.Buildings(m_Spots, m_Center, m_Radius, 300, m_Spawned);
		int items = m_Spawned.Count();
		int cars = BR_Loot.Cars(m_Spots, m_Center, 4000, 10, m_Settings.carFuel, m_Settings.vehicles, m_Spawned);
		int boatsMade = BR_Loot.Boats(m_Spots, Math.Min(m_Settings.boats, 3), m_Settings.carFuel, m_Settings.boatTypes, m_Spawned);
		Print("[BRTest] boats: " + boatsMade + " of " + Math.Min(m_Settings.boats, 3) + " (" + m_Spots.boats.Count() + " boat spots on the map)");
		for (int c = items; c < m_Spawned.Count(); c++)
			m_TestCars.Insert(m_Spawned[c]);
		// every vehicle class in the list exists, and one of each vehicle stands near the lobby
		int noSuchVehicle = 0;
		foreach (string entry : m_Settings.vehicles)
		{
			array<string> names = new array<string>();
			entry.Split("|", names);
			foreach (string vname : names)
			{
				if (!GetGame().ConfigIsExisting("CfgVehicles " + vname))
				{
					Print("[BRTest] no such vehicle: " + vname);
					noSuchVehicle++;
				}
			}
		}
		Print("[BRTest] vehicles: " + m_Settings.vehicles.Count() + " in the list, " + noSuchVehicle + " unknown classes");

		// where a round's vehicles would go: counted on a 4 x 4 grid of the map (north row first),
		// against the vanilla car spots, and how long the picking takes
		float worldSize = GetGame().GetWorld().GetWorldSize();
		array<vector> where = new array<vector>();
		array<float> headings = new array<float>();
		int pickStart = GetGame().GetTime();
		BR_Loot.CarSpots(m_Spots, Vector(worldSize / 2, 0, worldSize / 2), worldSize, m_Settings.cars, m_Settings.carSquare, where, headings, m_Lobby);
		int pickMs = GetGame().GetTime() - pickStart;
		Print("[BRTest] car spread: " + BR_Loot.s_Squares + " squares, " + BR_Loot.s_EmptySquares + " with no spot found; rejected: edge " + BR_Loot.s_Why[0] + ", water " + BR_Loot.s_Why[1] + ", slope " + BR_Loot.s_Why[2] + ", bumpy " + BR_Loot.s_Why[3] + ", near a car " + BR_Loot.s_Why[4] + ", objects " + BR_Loot.s_Why[5]);
		int vanillaUsed = 0;
		foreach (vector wp : where)
		{
			foreach (BR_CarSpot vs : m_Spots.cars)
			{
				if (Math.AbsFloat(vs.x - wp[0]) < 0.01 && Math.AbsFloat(vs.z - wp[2]) < 0.01)
					vanillaUsed++;
			}
		}
		Print("[BRTest] car spread: " + where.Count() + " of " + m_Settings.cars + " placed in " + pickMs + " ms: " + BR_Loot.s_Kinds[0] + " at vanilla spots (" + vanillaUsed + " counted), " + BR_Loot.s_Kinds[1] + " by structures, " + BR_Loot.s_Kinds[2] + " in the open");
		string byList = "";
		foreach (string byShape : BR_Loot.s_By)
		{
			byList += " " + byShape;
		}
		Print("[BRTest] car spread, parked by:" + byList);
		float quarter = worldSize / 4;
		for (int row = 3; row >= 0; row--)
		{
			string line = "";
			string vline = "";
			for (int col = 0; col < 4; col++)
			{
				int here = 0;
				int vhere = 0;
				foreach (vector cp : where)
				{
					if (Math.Floor(cp[0] / quarter) == col && Math.Floor(cp[2] / quarter) == row)
						here++;
				}
				foreach (BR_CarSpot cv : m_Spots.cars)
				{
					if (Math.Floor(cv.x / quarter) == col && Math.Floor(cv.z / quarter) == row)
						vhere++;
				}
				line += " " + here;
				vline += " " + vhere;
			}
			Print("[BRTest] car spread row " + row + ":" + line + "   (vanilla spots:" + vline + ")");
		}
		for (int v = 0; v < m_Settings.vehicles.Count(); v++)
		{
			array<string> colours = new array<string>();
			m_Settings.vehicles[v].Split("|", colours);
			vector at = GroundSpot() + Vector(20 + v * 12, 0, 25);
			at[1] = GetGame().SurfaceY(at[0], at[2]) + 0.3;
			EntityAI one = BR_Loot.Vehicle(colours[0], at, 0, m_Settings.carFuel);
			if (one)
			{
				m_Spawned.Insert(one);
				m_TestCars.Insert(one);
			}
		}
		Print("[BRTest] loot in " + buildings + " buildings (" + items + " things), " + cars + " cars near " + m_Center);

		// what the piles came out as, roughly: guns with a magazine in them
		int guns = 0;
		int loaded = 0;
		for (int i = 0; i < items; i++)
		{
			Weapon_Base gun = Weapon_Base.Cast(m_Spawned[i]);
			if (!gun)
				continue;
			guns++;
			if (gun.GetMagazine(gun.GetCurrentMuzzle()))
				loaded++;
		}
		Print("[BRTest] " + guns + " guns, " + loaded + " with a magazine in");

		PlayerBase body = PlayerBase.Cast(GetGame().CreateObject("SurvivorM_Mirek", GroundSpot(), false, true));
		if (body)
		{
			// kit a player who still has their gear, like a winner: a loaded gun in hand, a full bag
			EntityAI bag = body.GetInventory().CreateInInventory("TaloonBag_Blue");
			if (bag)
				bag.GetInventory().CreateInInventory("AmmoBox_762x39_20Rnd");
			Weapon_Base held = Weapon_Base.Cast(body.GetHumanInventory().CreateInHands("AKM"));
			if (held)
			{
				held.GetInventory().CreateAttachment("AK_WoodBttstck");
				held.SpawnAttachedMagazine("Mag_AKM_30Rnd");
			}
			BR_Loot.Kit(body);
			m_TestBody = body; // checked in SelfTestFinish: deletes only happen at the end of the frame
			m_Spawned.Insert(body);
		}

		PickNext(0.5);
		Print("[BRTest] next circle " + m_NextCenter + " r " + m_NextRadius + ", " + BR_Loot.Distance2D(m_NextCenter, m_Center) + " m from the centre (max " + (m_Radius - m_NextRadius) + ")");
		m_Revealed = true;
		UpdateMarkers();
		Print("[BRTest] " + m_Markers.Count() + " map markers");
		m_Revealed = false;
		SelfTestCircles();
		SelfTestHits();
		SelfTestGuns();
		SelfTestSky();

		// the plane over the whole-map circle
		vector mapCenter = Vector(OnMap(0, m_MinX, m_MaxX, m_Settings.startRadius), 0, OnMap(0, m_MinZ, m_MaxZ, m_Settings.startRadius));
		BR_Flight flight = BR_Flight.Plan(mapCenter, m_Settings.startRadius, GetGame().GetWorld().GetWorldSize(), m_Settings.planeHeight, m_Settings.planeSpeed);
		int planesBefore = ExpansionAirdropPlaneBase.s_Expansion_AirdropPlanes.Count();
		flight.TakeOff(Now());
		Print("[BRTest] plane route " + flight.from + " -> " + flight.to + ", " + flight.length + " m at " + flight.altitude + " m, " + flight.Duration() + " s; planes " + planesBefore + " -> " + ExpansionAirdropPlaneBase.s_Expansion_AirdropPlanes.Count() + "; halfway at " + flight.PositionAt(Now() + flight.Duration() / 2));

		// the parachute, on the kitted test character: 50 m up, about 8 s down at the chute's speed
		if (m_TestBody)
		{
			vector up = m_TestBody.GetPosition() + "0 50 0";
			m_TestBody.SetAllowDamage(true);
			m_TestBody.BR_StartChute();
			m_TestBody.SetPosition(up);
			Print("[BRTest] chute: jumped at " + BR_ChuteCommand.HeightAboveGround(m_TestBody.GetPosition()) + " m above the ground");
		}
		Print("[BRTest] supply drop sent: " + BR_Loot.SupplyDrop(m_NextCenter));
		// a plane takes over a minute to drop, longer than the test: put a crate down directly
		Object crate = GetGame().CreateObjectEx("ExpansionAirdropContainer_Military", GroundSpot(), ECE_PLACE_ON_SURFACE);
		Print("[BRTest] test crate " + crate + ", crates tracked: " + ExpansionAirdropContainerBase.s_BR_Crates.Count());
	}

	// the whole circle sequence on simulated time, with the configured fractions but short
	// timings (10, 8, 6, 4, 2, then 1 s): each step logs a "circle N:" line; it must end in the storm
	void SelfTestCircles()
	{
		array<int> real = m_Settings.circleSeconds;
		array<int> quick = {10, 8, 6, 4, 2, 1};
		m_Settings.circleSeconds = quick;
		vector center = Vector(OnMap(0, m_MinX, m_MaxX, m_Settings.startRadius), 0, OnMap(0, m_MinZ, m_MaxZ, m_Settings.startRadius));
		m_Center = center;
		m_Radius = m_Settings.startRadius;
		m_Circle = 0;
		m_WholeMap = true;
		m_Revealed = false;
		m_Closing = false;
		m_Storm = false;
		m_StageEnds = m_Settings.CircleSeconds(0);
		float t = 0;
		while (t < 600 && !m_Storm)
		{
			t += 0.5;
			UpdateZone(t);
		}
		Print("[BRTest] circles: storm " + m_Storm + " after " + t + " simulated s, " + m_Circle + " circles, radius " + m_Radius);
		m_Settings.circleSeconds = real;
		m_Storm = false;
		m_WholeMap = false;
		m_Closing = false;
		m_Revealed = false;
		ClearMarkers();
	}

	// the lobby in the sky: what got built, where players appear, that the floor holds someone
	// standing on it, and that someone who walks off the edge is caught and brought back
	protected PlayerBase m_SkyStander;
	protected PlayerBase m_SkyFaller;
	protected vector m_SkyStood;

	void SelfTestSky()
	{
		if (!m_Sky)
		{
			Print("[BRTest] sky lobby: none (" + m_LobbyNames.Count() + " lobbies, height " + m_Settings.lobbyHeight + ")");
			return;
		}
		Print("[BRTest] sky lobby: " + m_Sky.m_Title + ", " + m_Sky.m_Built.Count() + " pieces (" + m_Sky.m_Buildings + " buildings, " + m_Sky.m_Models + " models), " + m_Sky.m_Failed + " failed, worst miss " + m_Sky.m_WorstMiss + " m");
		int onFloor = 0;
		float far = 0;
		float lo = 1e9;
		float hi = -1e9;
		for (int i = 0; i < 20; i++)
		{
			vector at = LobbySpot();
			vector check;
			if (m_Sky.OnFloor(at[0], at[2], check) && Math.AbsFloat(check[1] - at[1]) < 0.3)
				onFloor++;
			far = Math.Max(far, BR_Loot.Distance2D(at, m_Sky.m_Origin));
			lo = Math.Min(lo, at[1] - m_Sky.m_Origin[1]);
			hi = Math.Max(hi, at[1] - m_Sky.m_Origin[1]);
		}
		Print("[BRTest] sky lobby spots: " + onFloor + " of 20 on the floor, up to " + far + " m from the middle, " + lo + " to " + hi + " m");
		m_SkyStander = PlayerBase.Cast(GetGame().CreateObject("SurvivorM_Mirek", LobbySpot(), false, true));
		if (m_SkyStander)
		{
			m_SkyStood = m_SkyStander.GetPosition();
			m_Spawned.Insert(m_SkyStander);
		}
		// just past the floor's edge, inside the leash: it falls
		vector edge = m_Sky.NearestTop(m_Sky.m_Origin + "500 0 0");
		vector off = edge + (edge - m_Sky.m_Origin).Normalized() * 8;
		off[1] = edge[1] + 1;
		m_SkyFaller = PlayerBase.Cast(GetGame().CreateObject("SurvivorM_Mirek", off, false, true));
		if (m_SkyFaller)
		{
			m_Spawned.Insert(m_SkyFaller);
			m_SkyFaller.SetAllowDamage(false);
		}
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(SelfTestSkyFall, 5000, false);
	}

	void SelfTestSkyFall()
	{
		if (m_SkyStander)
			Print("[BRTest] sky lobby: standing " + (m_SkyStander.GetPosition()[1] - m_SkyStood[1]) + " m from where it was put, after 5 s");
		if (!m_SkyFaller)
			return;
		vector p = m_SkyFaller.GetPosition();
		bool strayed = m_Sky.Strayed(p);
		KeepInLobby(m_SkyFaller);
		Print("[BRTest] sky lobby: walked off the edge, " + (p[1] - m_Sky.m_Origin[1]) + " m after 5 s, caught " + strayed + ", put back at " + (m_SkyFaller.GetPosition() - m_Sky.m_Origin));
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(SelfTestSkyBack, 3000, false);
	}

	void SelfTestSkyBack()
	{
		if (m_SkyFaller)
			Print("[BRTest] sky lobby: 3 s after being put back, " + (m_SkyFaller.GetPosition() - m_Sky.m_Origin) + ", strayed " + m_Sky.Strayed(m_SkyFaller.GetPosition()));
		// a game that keeps falling through: caught three times under the floor, it's put on the ground
		PlayerBase sinker = PlayerBase.Cast(GetGame().CreateObject("SurvivorM_Mirek", m_Sky.m_Origin + "2 -40 2", false, true));
		if (sinker)
		{
			m_Spawned.Insert(sinker);
			for (int f = 0; f < FALLS_TO_GROUND; f++)
			{
				sinker.SetPosition(m_Sky.m_Origin + "2 -40 2");
				KeepInLobby(sinker);
			}
			vector sunk = sinker.GetPosition();
			Print("[BRTest] sky lobby: fell through " + FALLS_TO_GROUND + " times, now " + (sunk[1] - GetGame().SurfaceY(sunk[0], sunk[2])) + " m above the ground, " + BR_Loot.Distance2D(sunk, m_Lobby) + " m from the lobby's ground");
			m_Grounded.Clear();
		}
		string before = m_Sky.m_Title;
		ChangeLobby();
		Print("[BRTest] sky lobby change: " + before + " -> " + m_Sky.m_Title + " (" + m_Sky.m_Built.Count() + " pieces, " + m_Sky.m_Failed + " failed, worst miss " + m_Sky.m_WorstMiss + " m) at " + m_Sky.m_Origin);
	}

	// deletes happen at the end of the frame: count what's left a moment later
	void SelfTestRefillAfter(PlayerBase gunner, Magazine mag, int had, int added, bool found)
	{
		array<EntityAI> items = new array<EntityAI>();
		gunner.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items);
		int boxes = 0;
		int loose = 0;
		foreach (EntityAI item : items)
		{
			if (item.GetType() == "AmmoBox_762x39_20Rnd")
				boxes++;
			Magazine pile = Magazine.Cast(item);
			if (pile && pile.IsAmmoPile())
				loose += pile.GetAmmoCount();
		}
		string spare = "none";
		if (mag)
			spare = mag.GetAmmoCount().ToString() + "/" + mag.GetAmmoMax();
		Print("[BRTest] refill: found the magazine on the gun " + found + ", " + had + " rounds to hand (10 loose, 2 boxes of 20), " + added + " into a new spare: " + spare + " (30/30 expected), " + boxes + " boxes left (1 expected), " + loose + " loose rounds left (0 expected)");
	}

	// what a hit of each candidate kind does to a fresh character (for the storm's cuts)
	void SelfTestHits()
	{
		// reload refills the magazine from loose rounds and unopened boxes
		PlayerBase gunner = PlayerBase.Cast(GetGame().CreateObject("SurvivorM_Mirek", GroundSpot() + "3 0 9", false, true));
		if (gunner)
		{
			m_Spawned.Insert(gunner);
			Weapon_Base akm = Weapon_Base.Cast(gunner.GetHumanInventory().CreateInHands("AKM"));
			EntityAI bag = gunner.GetInventory().CreateInInventory("TaloonBag_Blue");
			if (akm && bag)
			{
				Magazine mag = Magazine.Cast(akm.GetInventory().CreateAttachment("Mag_AKM_30Rnd"));
				if (mag)
				{
					mag.ServerSetAmmoCount(5);
					Magazine loose = Magazine.Cast(bag.GetInventory().CreateInInventory("Ammo_762x39"));
					if (loose)
						loose.ServerSetAmmoCount(10);
					bag.GetInventory().CreateInInventory("AmmoBox_762x39_20Rnd");
					bag.GetInventory().CreateInInventory("AmmoBox_762x39_20Rnd");
					int had = BR_Refill.Available(gunner, mag);
					Weapon_Base held;
					Magazine topped = BR_Refill.Topped(gunner, held);
					// the action the reload key runs: on the gun, first on that key, and possible now
					array<ActionBase_Basic> onKey = new array<ActionBase_Basic>();
					akm.GetActions(WeaponManipulationActionInput, onKey);
					string first = "none";
					if (onKey && onKey.Count() > 0)
						first = onKey[0].ClassName();
					ActionBase refill = gunner.GetActionManager().GetAction(BR_ActionRefillMagazine);
					bool can = refill && refill.Can(gunner, new ActionTarget(null, null, -1, vector.Zero, -1), akm);
					ActionBase vanilla = gunner.GetActionManager().GetAction(FirearmActionMechanicManipulate);
					string order = "";
					if (onKey)
					{
						foreach (ActionBase_Basic each : onKey)
							order += " " + each.ClassName();
					}
					Print("[BRTest] refill action: on the reload key:" + order + "; registered " + (refill != null) + " (a vanilla one: " + (vanilla != null) + "), possible " + can + ", action manager " + gunner.GetActionManager());
					int added;
					Magazine spare = BR_Refill.FillSpare(gunner, mag, added);
					GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(SelfTestRefillAfter, 500, false, gunner, spare, had, added, topped != null);
				}
				else
					Print("[BRTest] refill: no magazine on the AKM");
			}
		}

		// adrenaline heals fully, and stamina doesn't run down
		PlayerBase hurt = PlayerBase.Cast(GetGame().CreateObject("SurvivorM_Mirek", GroundSpot() + "0 0 9", false, true));
		if (hurt)
		{
			m_Spawned.Insert(hurt);
			hurt.SetHealth("", "", 30);
			hurt.SetHealth("GlobalHealth", "Blood", 3000);
			float before = hurt.GetHealth("", "");
			EntityAI epi = hurt.GetInventory().CreateInInventory("Epinephrine");
			Epinephrine shot = Epinephrine.Cast(epi);
			if (shot)
				shot.OnApply(hurt);
			Print("[BRTest] epinephrine: health " + before + " -> " + hurt.GetHealth("", "") + " of " + hurt.GetMaxHealth("", "") + ", blood " + hurt.GetHealth("GlobalHealth", "Blood"));
			StaminaHandler stamina = hurt.GetStaminaHandler();
			if (stamina)
			{
				float full = stamina.GetStamina();
				stamina.DepleteStamina(EStaminaModifiers.JUMP);
				Print("[BRTest] stamina: " + full + " -> " + stamina.GetStamina() + " after a jump (should stay)");
			}
		}
		array<string> ammo = {"MeleeZombie", "MeleeKnife", "TripWireHit", "MeleeWolf"};
		for (int i = 0; i < ammo.Count(); i++)
		{
			PlayerBase body = PlayerBase.Cast(GetGame().CreateObject("SurvivorM_Mirek", GroundSpot() + Vector(i * 3, 0, 0), false, true));
			if (!body)
				continue;
			m_Spawned.Insert(body);
			float hp = body.GetHealth("", "");
			float blood = body.GetHealth("", "Blood");
			float shock = body.GetHealth("", "Shock");
			body.ProcessDirectDamage(DamageType.CLOSE_COMBAT, body, "Torso", ammo[i], "0 0 0", 1);
			int bleeds = 0;
			if (body.GetBleedingManagerServer())
				bleeds = body.GetBleedingManagerServer().GetBleedingSourcesCount();
			Print("[BRTest] hit " + ammo[i] + ": health " + hp + " -> " + body.GetHealth("", "") + ", blood " + blood + " -> " + body.GetHealth("", "Blood") + ", shock " + shock + " -> " + body.GetHealth("", "Shock") + ", cuts " + bleeds);
		}
		// the storm wall's model (the client puts rows of them along the edge)
		Object panel = GetGame().CreateObjectEx("BR_StormWall", GroundSpot() + "0 0 12", ECE_NONE);
		if (panel)
		{
			vector box[2];
			panel.ClippingInfo(box);
			Print("[BRTest] storm wall panel: created, bounds " + box[0] + " .. " + box[1]);
			GetGame().ObjectDelete(panel);
		}
		else
			Print("[BRTest] storm wall panel: NOT created");
		// the storm's own cut, at the first circle's damage (1 a second, 3 per hit)
		PlayerBase cut = PlayerBase.Cast(GetGame().CreateObject("SurvivorM_Mirek", GroundSpot() + "0 0 6", false, true));
		if (cut)
		{
			m_Spawned.Insert(cut);
			StormCut(cut, 3);
			Print("[BRTest] storm cut (3): health 100 -> " + cut.GetHealth("", "") + ", shock 100 -> " + cut.GetHealth("", "Shock") + ", cuts " + cut.GetBleedingManagerServer().GetBleedingSourcesCount());
		}
	}

	// every gun comes loaded, and which attachments (candidates for the loot) fit which guns
	void SelfTestGuns()
	{
		BR_Loot.Init();
		array<Weapon_Base> guns = new array<Weapon_Base>();
		for (int g = 0; g < BR_Loot.s_Weapons.Count(); g++)
		{
			BR_Weapon w = BR_Loot.s_Weapons[g];
			int before = m_Spawned.Count();
			BR_Loot.WeaponPile(w, GroundSpot() + Vector(-20 - g * 2, 0, -20), m_Spawned);
			Weapon_Base gun = null;
			if (before < m_Spawned.Count())
				gun = Weapon_Base.Cast(m_Spawned[before]);
			if (!gun)
			{
				Print("[BRTest] gun " + w.weapon + ": NOT created");
				continue;
			}
			guns.Insert(gun);
			int mu = gun.GetCurrentMuzzle();
			int rounds = gun.GetInternalMagazineCartridgeCount(mu);
			for (int ch = 0; ch < gun.GetMuzzleCount(); ch++)
			{
				if (!gun.IsChamberEmpty(ch))
					rounds++;
			}
			Magazine mag = gun.GetMagazine(mu);
			if (mag)
				rounds += mag.GetAmmoCount();
			Print("[BRTest] gun " + w.weapon + ": " + rounds + " rounds loaded, magazine " + mag + ", " + (m_Spawned.Count() - before - 1) + " ammo boxes");
		}
		array<string> candidates = {"ACOGOptic", "ReflexOptic", "M68Optic", "KobraOptic", "PSO1Optic", "HuntingOptic", "M4_CarryHandleOptic", "FNP45_MRDSOptic", "PistolSuppressor", "M4_Suppressor", "AK_Suppressor", "Mag_STANAG_60Rnd", "Mag_AKM_Drum75Rnd", "Mag_M14_20Rnd", "Mag_Glock_15Rnd", "Mag_AK101_30Rnd", "UniversalLight", "TLRLight"};
		foreach (string c : candidates)
		{
			EntityAI part = EntityAI.Cast(GetGame().CreateObject(c, GroundSpot() + "0 0 -30"));
			if (!part)
			{
				Print("[BRTest] fits " + c + ": no such item");
				continue;
			}
			string fits = "";
			foreach (Weapon_Base fg : guns)
			{
				bool ok = fg.GetInventory().CanAddAttachment(part);
				if (!ok && Magazine.Cast(part))
				{
					TStringArray mags = new TStringArray();
					GetGame().ConfigGetTextArray("CfgWeapons " + fg.GetType() + " magazines", mags);
					ok = mags.Find(c) >= 0;
				}
				if (ok)
					fits += " " + fg.GetType();
			}
			BR_Loot.WithBattery(part);
			string battery = "";
			if (part.GetInventory().AttachmentCount() > 0)
				battery = " (with a battery)";
			Print("[BRTest] fits " + c + battery + ":" + fits);
			GetGame().ObjectDelete(part);
		}
	}

	void SelfTestFinish()
	{
		SelfTestKitCheck();
		if (m_SkyOld)
			Print("[BRTest] sky lobby change: " + m_SkyOld.StillThere() + " of " + m_SkyOld.m_Built.Count() + " pieces of " + m_SkyOld.m_Title + " left on the server");
		// and every lobby of this map, and every one in the mission's lobbies folder, builds
		array<string> names = new array<string>();
		foreach (BR_LobbySpec spec : m_Map.m_Lobbies)
			names.Insert(spec.name + ".json");
		string file;
		FileAttr attr;
		FindFileHandle found = FindFile("$mission:lobbies/*.json", file, attr, 0);
		if (found)
		{
			names.Insert(file);
			while (FindNextFile(found, file, attr))
				names.Insert(file);
			CloseFindFile(found);
		}
		float testAt = -200;
		foreach (string json : names)
		{
			string name = json.Substring(0, json.Length() - 5);
			// each at its own place: the one before is only deleted at the end of the frame
			testAt -= 400;
			BR_SkyLobby each = BR_SkyLobby.Build(name, m_Lobby + Vector(testAt, m_Settings.lobbyHeight, 0), m_Map);
			if (!each)
			{
				Print("[BRTest] sky lobby " + name + ": FAILED to build");
				continue;
			}
			int spots = 0;
			for (int i = 0; i < 10; i++)
			{
				vector at = each.Spot();
				vector check;
				if (each.OnFloor(at[0], at[2], check, true) && Math.AbsFloat(check[1] - at[1]) < 0.3)
					spots++;
			}
			if (spots < 10)
			{
				for (int pr = 0; pr < 3; pr++)
					Print("[BRTest] sky lobby " + name + " probe: " + each.Probe());
			}
			Print("[BRTest] sky lobby " + name + " (" + each.m_Title + "): " + each.m_Built.Count() + " pieces (" + each.m_Buildings + " buildings), " + each.m_Failed + " failed, worst miss " + each.m_WorstMiss + " m, " + spots + " of 10 spots on the floor");
			each.Destroy();
		}
		int fuelled = 0;
		for (int c = 0; c < m_TestCars.Count(); c++)
		{
			EntityAI car = EntityAI.Cast(m_TestCars[c]);
			if (!car)
				continue;
			float fuel = -1;
			if (CarScript.Cast(car))
				fuel = CarScript.Cast(car).GetFluidFraction(CarFluid.FUEL);
			else if (MotorbikeScript.Cast(car))
				fuel = MotorbikeScript.Cast(car).GetFluidFraction(MotorbikeFluid.FUEL);
			else if (BoatScript.Cast(car))
				fuel = BoatScript.Cast(car).GetFluidFraction(BoatFluid.FUEL);
			Print("[BRTest] " + car.GetType() + " fuel " + fuel + ", " + car.GetInventory().AttachmentCount() + " parts fitted, ruined " + car.IsRuined() + ", lifetime " + car.GetLifetime() + " s at " + car.GetPosition());
			if (fuel > 0.05 && fuel < 0.3)
				fuelled++;
		}
		Print("[BRTest] " + fuelled + " of " + m_TestCars.Count() + " cars with a little fuel (" + m_Settings.carFuel + ")");
		Print("[BRTest] cleaning up " + m_Spawned.Count() + " tracked things");
		Cleanup();
	}

	// deletion happens at the end of the frame, so look a moment later
	void SelfTestKitCheck()
	{
		if (!m_TestBody)
			return;
		Print("[BRTest] chute: now " + BR_ChuteCommand.HeightAboveGround(m_TestBody.GetPosition()) + " m above the ground, still chuting " + m_TestBody.BR_IsChuting() + ", command " + m_TestBody.GetCommand_Script() + ", alive " + m_TestBody.IsAlive() + ", health " + m_TestBody.GetHealth("", ""));
		array<EntityAI> carried = new array<EntityAI>();
		m_TestBody.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, carried);
		string belt = "no belt";
		EntityAI worn = m_TestBody.FindAttachmentBySlotName("Hips");
		if (worn)
		{
			belt = worn.GetType() + " with";
			for (int a = 0; a < worn.GetInventory().AttachmentCount(); a++)
				belt += " " + worn.GetInventory().GetAttachmentFromIndex(a).GetType();
		}
		Print("[BRTest] kit: " + m_TestBody.GetInventory().AttachmentCount() + " worn, " + (carried.Count() - 1) + " items in all (7 expected), " + belt + ", in hands: " + m_TestBody.GetHumanInventory().GetEntityInHands() + ", health " + m_TestBody.GetHealth("", ""));
	}

	void SelfTestGone()
	{
		int left = 0;
		for (int i = 0; i < m_TestCars.Count(); i++)
		{
			if (m_TestCars[i])
				left++;
		}
		Print("[BRTest] after clean-up: " + left + " of " + m_TestCars.Count() + " cars left, " + m_Markers.Count() + " markers left");
	}

	// --- the map ----------------------------------------------------------------------
	// The circles and the plane's route are drawn by the client (BR_Map.c). There are no
	// server markers any more: the "Safe zone" star at the next circle's centre, which also
	// floated in the world, is gone. This only clears any left over.

	void UpdateMarkers()
	{
		ClearMarkers();
	}

	void ClearMarkers()
	{
		ExpansionMarkerModule module;
		CF_Modules<ExpansionMarkerModule>.Get(module);
		if (module)
		{
			for (int i = 0; i < m_Markers.Count(); i++)
				module.RemoveServerMarker(m_Markers[i]);
		}
		m_Markers.Clear();
	}
}
