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
	protected int m_Stage;        // index into the settings' phases; Count() once the last one is done
	protected bool m_Shrinking;
	protected float m_StageStarted;
	protected float m_StageEnds;
	protected float m_NextDrop;
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
				Print("[BattleRoyale] unknown class name in the loot: " + types[t]);
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
		Print("[BattleRoyale] ready: " + m_Spots.buildings.Count() + " loot buildings, " + m_Spots.cars.Count() + " car spots, " + m_Settings.phases.Count() + " circle phases, " + missing + " unknown loot classes; lobby at " + m_Lobby + ", first round in " + m_Settings.firstLobbySeconds + " s");

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
		Print("[BattleRoyale] lobby ground: " + best + ", flatness " + bestScore);
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
		int cars = BR_Loot.Cars(m_Spots, m_Center, m_Radius, m_Settings.cars, m_Settings.carFuel, m_Spawned);
		m_Prepared = true;
		float seconds = (GetGame().GetTime() - m_PrepStarted) / 1000.0;
		Print("[BattleRoyale] next round ready: circle " + m_Center + " r " + m_Radius + ", loot in " + m_LootBuildings + " buildings (" + items + " things), " + cars + " cars, put out over " + seconds + " s");
	}

	// --- starting a round: everyone in the lobby onto the plane ------------------------

	void StartRound(array<Man> men)
	{
		m_Round++;

		// noon, every round
		int year, month, day, hour, minute;
		GetGame().GetWorld().GetDate(year, month, day, hour, minute);
		GetGame().GetWorld().SetDate(year, month, day, 12, 0);

		m_Flight = BR_Flight.Plan(m_Center, m_Radius, m_MinX, m_MaxX, m_MinZ, m_MaxZ, m_Settings.planeHeight, m_Settings.planeSpeed);
		m_Flight.TakeOff(Now());
		BR_Plane.Set(m_Flight.PositionAt(Now()), m_Flight.Velocity());

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
			m_InPlane.Set(id, player);
		}
		m_Started = players.Count();
		m_Winner = "";

		m_Stage = 0;
		m_Shrinking = false;
		BR_ZonePhase phase = m_Settings.phases[0];
		PickNext(phase.radiusFraction);
		m_StageEnds = Now() + phase.waitSeconds;
		m_NextDrop = Now() + m_Settings.supplyDropSeconds;
		UpdateMarkers();
		m_Phase = BR_Phase.GAME;

		Print("[BattleRoyale] round " + m_Round + " started: " + m_Started + " players, circle " + m_Center + " r " + m_Radius + ", plane " + m_Flight.from + " -> " + m_Flight.to + " (" + m_Flight.length + " m at " + m_Flight.altitude + " m, " + m_Flight.Duration() + " s)");
		Announce("Round " + m_Round + ": the plane is taking off", "Its route is on the map (M). Press F to jump.");
	}

	// from the plane's position now, a little spread out so a crowd doesn't land in one heap
	void Jump(PlayerBase player)
	{
		m_InPlane.Remove(player.GetIdentity().GetId());
		vector pos = m_Flight.PositionAt(Now()) + Vector(Math.RandomFloat(-15, 15), -4, Math.RandomFloat(-15, 15));
		player.BR_StartChute(); // also no damage until just after landing
		player.SetPosition(pos);
		Print("[BattleRoyale] " + player.GetIdentity().GetName() + " jumped at " + pos + ", " + BR_ChuteCommand.HeightAboveGround(pos) + " m above the ground");
	}

	// F: out of the plane, or (falling) open the chute
	void OnJump(PlayerIdentity sender)
	{
		if (m_Phase != BR_Phase.GAME || !sender)
			return;
		PlayerBase player;
		if (m_Flight && m_InPlane.Find(sender.GetId(), player) && player && player.IsAlive())
		{
			Jump(player);
			return;
		}
		if (m_Alive.Find(sender.GetId(), player) && player)
		{
			Print("[BattleRoyale] " + sender.GetName() + " pressed F, " + BR_ChuteCommand.HeightAboveGround(player.GetPosition()) + " m above the ground, chuting " + player.BR_IsChuting() + ", command " + player.GetCommand_Script());
			if (player.BR_IsChuting())
				player.BR_OpenChute();
		}
	}

	void UpdateFlight(float now)
	{
		if (!m_Flight)
			return;
		BR_Plane.Set(m_Flight.PositionAt(now), m_Flight.Velocity());
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
		m_Flight = null;
	}

	// --- the round in progress -------------------------------------------------------

	void UpdateGame(float now)
	{
		// who's out
		for (int i = m_Alive.Count() - 1; i >= 0; i--)
		{
			PlayerBase player = m_Alive.GetElement(i);
			if (player && player.IsAlive() && player.GetIdentity())
				continue;
			string id = m_Alive.GetKey(i);
			m_Alive.RemoveElement(i);
			m_InPlane.Remove(id);
			if (player && !player.IsAlive())
				m_Spawned.Insert(player); // the body goes with the round
			if (m_Alive.Count() > 1)
				Announce(m_Names.Get(id) + " is out", m_Alive.Count().ToString() + " left.");
		}

		if ((m_Started >= 2 && m_Alive.Count() <= 1) || m_Alive.Count() == 0)
		{
			EndRound();
			return;
		}

		UpdateFlight(now);
		UpdateZone(now);

		int stage = Math.Min(m_Stage, m_Settings.phases.Count() - 1);
		float damage = m_Settings.phases[stage].damagePerSecond;
		for (int a = 0; a < m_Alive.Count(); a++)
		{
			PlayerBase p = m_Alive.GetElement(a);
			if (InPlane(p))
				continue;
			if (BR_Loot.Distance2D(p.GetPosition(), m_Center) > m_Radius)
				p.DecreaseHealth("", "", damage);
		}

		if (m_Settings.supplyDropSeconds > 0 && now >= m_NextDrop && m_NextRadius > 50)
		{
			m_NextDrop = now + m_Settings.supplyDropSeconds;
			float angle = Math.RandomFloat(0, Math.PI2);
			float dist = Math.Sqrt(Math.RandomFloat01()) * m_NextRadius * 0.7;
			vector drop = m_NextCenter + Vector(Math.Cos(angle) * dist, 0, Math.Sin(angle) * dist);
			if (BR_Loot.SupplyDrop(drop))
			{
				Print("[BattleRoyale] supply drop to " + drop);
				Announce("Supply drop", "A plane is on its way. Watch the map.");
			}
		}
	}

	void UpdateZone(float now)
	{
		if (m_Stage >= m_Settings.phases.Count())
			return; // the last circle has closed
		BR_ZonePhase phase = m_Settings.phases[m_Stage];

		if (!m_Shrinking)
		{
			if (now < m_StageEnds)
				return;
			m_Shrinking = true;
			m_FromCenter = m_Center;
			m_FromRadius = m_Radius;
			m_StageStarted = now;
			m_StageEnds = now + phase.shrinkSeconds;
			Announce("The circle is closing", "Get inside the white circle on the map.");
			return;
		}

		float f = 1;
		if (phase.shrinkSeconds > 0)
			f = Math.Clamp((now - m_StageStarted) / phase.shrinkSeconds, 0, 1);
		m_Center = m_FromCenter + (m_NextCenter - m_FromCenter) * f;
		m_Radius = m_FromRadius + (m_NextRadius - m_FromRadius) * f;
		if (f < 1)
			return;

		m_Shrinking = false;
		m_Stage++;
		if (m_Stage < m_Settings.phases.Count())
		{
			BR_ZonePhase next = m_Settings.phases[m_Stage];
			PickNext(next.radiusFraction);
			m_StageEnds = now + next.waitSeconds;
		}
		UpdateMarkers();
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
			Announce(m_Winner + " wins!", "Round " + m_Round + " is over.");
		}
		else
		{
			m_Winner = "";
			Announce("Nobody wins", "Round " + m_Round + " is over.");
		}
		Print("[BattleRoyale] round " + m_Round + " over, winner: '" + m_Winner + "'");
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
		m_Alive.Clear();
		ClearMarkers();
		if (count + drops > 0)
			Print("[BattleRoyale] cleaned up " + count + " things and " + drops + " supply drops");
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
			if (m_Stage >= m_Settings.phases.Count())
				title = "Final circle";
			else if (m_Shrinking)
				title = "Circle closing: " + Clock(m_StageEnds - now);
			else
				title = "Circle closes in " + Clock(m_StageEnds - now);
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
		state.center = m_Center;
		state.radius = m_Radius;
		state.hasNext = m_Phase == BR_Phase.GAME && m_Stage < m_Settings.phases.Count();
		state.nextCenter = m_NextCenter;
		state.nextRadius = m_NextRadius;
		state.flying = m_Phase == BR_Phase.GAME && m_Flight != null;
		string planeLine;
		if (state.flying)
		{
			state.routeFrom = m_Flight.from;
			state.routeTo = m_Flight.to;
			state.planePos = m_Flight.PositionAt(now);
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
		Print("[BRTest] " + m_Spots.protos.Count() + " building types, " + m_Spots.buildings.Count() + " buildings, " + m_Spots.cars.Count() + " car spots, " + m_Settings.phases.Count() + " phases, lobby " + m_Lobby);
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
		int cars = BR_Loot.Cars(m_Spots, m_Center, 4000, 10, m_Settings.carFuel, m_Spawned);
		for (int c = items; c < m_Spawned.Count(); c++)
			m_TestCars.Insert(m_Spawned[c]);
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
		UpdateMarkers();
		Print("[BRTest] " + m_Markers.Count() + " map markers");

		// the plane over the whole-map circle
		vector mapCenter = Vector(OnMap(0, m_MinX, m_MaxX, m_Settings.startRadius), 0, OnMap(0, m_MinZ, m_MaxZ, m_Settings.startRadius));
		BR_Flight flight = BR_Flight.Plan(mapCenter, m_Settings.startRadius, m_MinX, m_MaxX, m_MinZ, m_MaxZ, m_Settings.planeHeight, m_Settings.planeSpeed);
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

	void SelfTestFinish()
	{
		SelfTestKitCheck();
		int fuelled = 0;
		for (int c = 0; c < m_TestCars.Count(); c++)
		{
			CarScript car = CarScript.Cast(m_TestCars[c]);
			if (!car)
				continue;
			Print("[BRTest] " + car.GetType() + " fuel " + car.GetFluidFraction(CarFluid.FUEL) + " at " + car.GetPosition());
			if (car.GetFluidFraction(CarFluid.FUEL) > 0.05 && car.GetFluidFraction(CarFluid.FUEL) < 0.3)
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
		Print("[BRTest] kit: " + m_TestBody.GetInventory().AttachmentCount() + " worn, " + (carried.Count() - 1) + " items in all (4 expected), in hands: " + m_TestBody.GetHumanInventory().GetEntityInHands() + ", health " + m_TestBody.GetHealth("", ""));
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
	// The circles and the plane's route are drawn by the client (BR_Map.c). The one server
	// marker is the next circle's centre, which also shows in the world.

	void UpdateMarkers()
	{
		ClearMarkers();
		ExpansionMarkerModule module;
		CF_Modules<ExpansionMarkerModule>.Get(module);
		if (!module || m_Stage >= m_Settings.phases.Count())
			return;
		ExpansionMarkerData centre = module.CreateServerMarker("Safe zone", "Star", m_NextCenter, ARGB(255, 255, 255, 255), true);
		if (centre)
			m_Markers.Insert(centre.GetUID());
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
