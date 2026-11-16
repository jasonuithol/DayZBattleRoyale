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
	protected vector m_Lobby;
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
			if (!InRound(player) || InPlane(player))
				KeepInLobby(player);
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
			m_Phase = BR_Phase.LOBBY;
			m_PhaseEnds = now + m_Settings.lobbySeconds;
			PrepareRound();
		}

		Broadcast(men, now);
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
	}

	void KeepInLobby(PlayerBase player)
	{
		player.SetAllowDamage(false);
		if (BR_Loot.Distance2D(player.GetPosition(), m_Lobby) > LOBBY_LEASH)
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

	vector LobbySpot()
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
		int cars = BR_Loot.Cars(m_Spots, m_Center, m_Radius, m_Settings.cars, m_Settings.carFuel, m_Settings.vehicles, m_Spawned, m_Settings.carSquare, m_Settings.carHotspots);
		m_Prepared = true;
		float seconds = (GetGame().GetTime() - m_PrepStarted) / 1000.0;
		Print("[BattleDayZ] next round ready: circle " + m_Center + " r " + m_Radius + ", loot in " + m_LootBuildings + " buildings (" + items + " things), " + cars + " cars, put out over " + seconds + " s");
	}

	// --- starting a round: everyone in the lobby onto the plane ------------------------

	void StartRound(array<Man> men)
	{
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

	// F: out of the plane, or (falling) open the chute
	void OnJump(PlayerIdentity sender, vector seenPlane)
	{
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
			int waiting = LobbyPlayers(men).Count();
			title = "Next round in " + Clock(m_PhaseEnds - now);
			if (now >= m_PhaseEnds && !m_Prepared)
				title = "Getting the map ready...";
			subtitle = waiting.ToString() + " waiting";
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
		vector whole = Vector(OnMap(0, m_MinX, m_MaxX, m_Settings.startRadius), 0, OnMap(0, m_MinZ, m_MaxZ, m_Settings.startRadius));
		array<vector> where = new array<vector>();
		array<float> headings = new array<float>();
		int pickStart = GetGame().GetTime();
		int atHotspots = BR_Loot.CarSpots(m_Spots, whole, m_Settings.startRadius, m_Settings.cars, m_Settings.carSquare, m_Settings.carHotspots, where, headings);
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
		Print("[BRTest] car spread: " + where.Count() + " of " + m_Settings.cars + " placed in " + pickMs + " ms, " + atHotspots + " at hotspots, " + vanillaUsed + " at vanilla spots");
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
					if (Math.Floor(cp[0] / 4096) == col && Math.Floor(cp[2] / 4096) == row)
						here++;
				}
				foreach (BR_CarSpot cv : m_Spots.cars)
				{
					if (Math.Floor(cv.x / 4096) == col && Math.Floor(cv.z / 4096) == row)
						vhere++;
				}
				line += " " + here;
				vline += " " + vhere;
			}
			Print("[BRTest] car spread row " + row + ":" + line + "   (vanilla spots:" + vline + ")");
		}
		foreach (string hs : m_Settings.carHotspots)
		{
			array<string> f = new array<string>();
			hs.Split(" ", f);
			vector hc = Vector(f[0].ToFloat(), 0, f[1].ToFloat());
			int nearHs = 0;
			foreach (vector hp : where)
			{
				if (BR_Loot.Distance2D(hp, hc) <= f[2].ToFloat())
					nearHs++;
			}
			Print("[BRTest] car spread: " + nearHs + " within " + f[2] + " m of the hotspot " + f[0] + " " + f[1]);
		}
		for (int v = 0; v < m_Settings.vehicles.Count(); v++)
		{
			array<string> colours = new array<string>();
			m_Settings.vehicles[v].Split("|", colours);
			vector at = LobbySpot() + Vector(20 + v * 12, 0, 25);
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

		PlayerBase body = PlayerBase.Cast(GetGame().CreateObject("SurvivorM_Mirek", LobbySpot(), false, true));
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
		Object crate = GetGame().CreateObjectEx("ExpansionAirdropContainer_Military", LobbySpot(), ECE_PLACE_ON_SURFACE);
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

	// what a hit of each candidate kind does to a fresh character (for the storm's cuts)
	void SelfTestHits()
	{
		array<string> ammo = {"MeleeZombie", "MeleeKnife", "TripWireHit", "MeleeWolf"};
		for (int i = 0; i < ammo.Count(); i++)
		{
			PlayerBase body = PlayerBase.Cast(GetGame().CreateObject("SurvivorM_Mirek", LobbySpot() + Vector(i * 3, 0, 0), false, true));
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
		Object panel = GetGame().CreateObjectEx("BR_StormWall", LobbySpot() + "0 0 12", ECE_NONE);
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
		PlayerBase cut = PlayerBase.Cast(GetGame().CreateObject("SurvivorM_Mirek", LobbySpot() + "0 0 6", false, true));
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
			BR_Loot.WeaponPile(w, LobbySpot() + Vector(-20 - g * 2, 0, -20), m_Spawned);
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
			EntityAI part = EntityAI.Cast(GetGame().CreateObject(c, LobbySpot() + "0 0 -30"));
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
		Print("[BRTest] kit: " + m_TestBody.GetInventory().AttachmentCount() + " worn, " + (carried.Count() - 1) + " items in all (6 expected), " + belt + ", in hands: " + m_TestBody.GetHumanInventory().GetEntityInHands() + ", health " + m_TestBody.GetHealth("", ""));
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
