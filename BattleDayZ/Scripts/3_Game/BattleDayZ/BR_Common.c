// Shared by server and client: the round phases, the state the server sends every second,
// and the jump request going the other way.

// The periodic diagnostic logging (the parachute every 2 s, the jump every second, the
// client's heartbeat): on only while chasing a problem. One-line event logs stay on.
class BR_Debug
{
	static const bool LOG = false;
}

enum BR_Phase
{
	LOBBY,   // waiting for the next round
	GAME,    // round in progress (it starts with the plane)
	ENDED    // winner announced, back to the lobby shortly
}

// What one player is told about the round, every second (server -> that client).
class BR_State
{
	static const int RPC = 1112670209;      // "BR" + 1: server -> client, this state
	static const int RPC_JUMP = 1112670210; // client -> server: let me out of the plane
	static const int RPC_BOARDED = 1112670212; // client -> server: boarding, and this game is ready for take-off

	int phase;
	bool inRound;      // this player is alive in the current round
	bool inPlane;      // ... and still on the plane: F jumps
	bool wholeMap;     // the start of a round: the whole map is safe, no current circle yet
	bool closing;      // the current circle is closing on the next: drawn red (white otherwise)
	vector center;     // the circle right now
	float radius;
	float closeLeft;   // while closing: seconds until it reaches the next circle (it closes evenly)
	bool hasNext;      // the next circle is known (not after the last one has closed)
	vector nextCenter;
	float nextRadius;
	bool flying;       // the plane is crossing the map
	vector routeFrom;  // its straight route, drawn on the map
	vector routeTo;
	vector planePos;   // where it is now, and its velocity, so the map can move it between updates
	vector planeVel;
	string title;      // the HUD's top line, already formatted by the server
	string subtitle;   // and the one under it
	vector lobbyAt;    // the lobby in the sky (zero: none), and how far its pieces reach from
	float lobbyReach;  // there: a client takes down the old one when it moves (BR_LobbyGhosts)
	float lobbyX;      // the place on the map the lobby is a copy of, for each game to build its
	float lobbyZ;      // own (lobbyRadius 0: a lobby from a file, all the server's)
	float lobbyRadius;
	float lobbyFloorY;
	string watching;   // out of the round and watching this player ("": not watching)
	int watchLow;      // that player's network id, for the camera to follow it in this game
	int watchHigh;
	vector watchPos;   // and where they are, until this game has them
	vector lobbySlot0; // the places a lobby goes: lobbySlots of them, lobbyApart m apart
	int lobbySlots;    // along x from lobbySlot0; anything left at one but lobbyAt is an old one
	float lobbyApart;
	bool boarding;     // everyone's on the plane, held at the start of its route until every game
	int roundHour;     // shows the round's time of day (this hour, this day of the month) and
	int roundDay;      // plays the engine sound
	vector finalPos;   // testing (testShowFinal): where the last circle closes, marked on the map (zero: not shown)
	ref array<vector> vehicles = new array<vector>(); // testing (testShowVehicles): every car and boat,
	ref array<bool> vehicleIsBoat = new array<bool>(); // marked on the map
	ref array<vector> roofPiles = new array<vector>(); // testing (testRoofs): the piles on the roofs
	int timeSet;       // goes up every time the server sets the round's time: each game then sets its own

	void Write(ParamsWriteContext ctx)
	{
		ctx.Write(phase);
		ctx.Write(inRound);
		ctx.Write(inPlane);
		ctx.Write(wholeMap);
		ctx.Write(closing);
		ctx.Write(center);
		ctx.Write(radius);
		ctx.Write(closeLeft);
		ctx.Write(hasNext);
		ctx.Write(nextCenter);
		ctx.Write(nextRadius);
		ctx.Write(flying);
		ctx.Write(routeFrom);
		ctx.Write(routeTo);
		ctx.Write(planePos);
		ctx.Write(planeVel);
		ctx.Write(title);
		ctx.Write(subtitle);
		ctx.Write(lobbyAt);
		ctx.Write(lobbyReach);
		ctx.Write(watching);
		ctx.Write(watchLow);
		ctx.Write(watchHigh);
		ctx.Write(watchPos);
		ctx.Write(lobbyX);
		ctx.Write(lobbyZ);
		ctx.Write(lobbyRadius);
		ctx.Write(lobbyFloorY);
		ctx.Write(lobbySlot0);
		ctx.Write(lobbySlots);
		ctx.Write(lobbyApart);
		ctx.Write(boarding);
		ctx.Write(roundHour);
		ctx.Write(roundDay);
		ctx.Write(timeSet);
		ctx.Write(finalPos);
		ctx.Write(vehicles.Count());
		for (int v = 0; v < vehicles.Count(); v++)
		{
			ctx.Write(vehicles[v]);
			ctx.Write(vehicleIsBoat[v]);
		}
		ctx.Write(roofPiles.Count());
		foreach (vector pile : roofPiles)
			ctx.Write(pile);
	}

	bool Read(ParamsReadContext ctx)
	{
		if (!ctx.Read(phase) || !ctx.Read(inRound) || !ctx.Read(inPlane) || !ctx.Read(wholeMap) || !ctx.Read(closing) || !ctx.Read(center) || !ctx.Read(radius) || !ctx.Read(closeLeft))
			return false;
		if (!ctx.Read(hasNext) || !ctx.Read(nextCenter) || !ctx.Read(nextRadius))
			return false;
		if (!ctx.Read(flying) || !ctx.Read(routeFrom) || !ctx.Read(routeTo) || !ctx.Read(planePos) || !ctx.Read(planeVel))
			return false;
		if (!ctx.Read(title) || !ctx.Read(subtitle))
			return false;
		if (!ctx.Read(lobbyAt) || !ctx.Read(lobbyReach))
			return false;
		if (!ctx.Read(watching) || !ctx.Read(watchLow) || !ctx.Read(watchHigh) || !ctx.Read(watchPos))
			return false;
		if (!ctx.Read(lobbyX) || !ctx.Read(lobbyZ) || !ctx.Read(lobbyRadius) || !ctx.Read(lobbyFloorY))
			return false;
		if (!ctx.Read(lobbySlot0) || !ctx.Read(lobbySlots) || !ctx.Read(lobbyApart))
			return false;
		return ctx.Read(boarding) && ctx.Read(roundHour) && ctx.Read(roundDay) && ctx.Read(timeSet) && ctx.Read(finalPos) && ReadVehicles(ctx) && ReadRoofPiles(ctx);
	}

	bool ReadRoofPiles(ParamsReadContext ctx)
	{
		int count;
		if (!ctx.Read(count))
			return false;
		for (int i = 0; i < count; i++)
		{
			vector at;
			if (!ctx.Read(at))
				return false;
			roofPiles.Insert(at);
		}
		return true;
	}

	bool ReadVehicles(ParamsReadContext ctx)
	{
		int count;
		if (!ctx.Read(count))
			return false;
		for (int v = 0; v < count; v++)
		{
			vector at;
			bool boat;
			if (!ctx.Read(at) || !ctx.Read(boat))
				return false;
			vehicles.Insert(at);
			vehicleIsBoat.Insert(boat);
		}
		return true;
	}

	// horizontal distance from pos to the circle's edge; positive means outside
	float OutsideBy(vector pos)
	{
		if (wholeMap)
			return -1;
		return vector.Distance(Vector(pos[0], 0, pos[2]), Vector(center[0], 0, center[2])) - radius;
	}
}

// Where the plane is, on the server (set every tick by the round) and on each client (from
// the state): last known position and velocity, moved on by the time since. The client's
// camera on the plane and the map follow it every frame.
class BR_Plane
{
	static vector s_Pos;
	static vector s_Vel;
	static float s_At;
	static bool s_View;   // client: this player is watching from the plane (BR_PlaneCamera.c)
	static float s_ViewOffAt;

	static void SetView(bool on)
	{
		if (s_View && !on)
			s_ViewOffAt = GetGame().GetTickTime();
		s_View = on;
	}

	// on, or only just switched off (the camera change back is still under way)
	static bool ViewRecent()
	{
		return s_View || GetGame().GetTickTime() - s_ViewOffAt < 2;
	}

	static void Set(vector pos, vector vel)
	{
		s_Pos = pos;
		s_Vel = vel;
		s_At = GetGame().GetTickTime();
	}

	static vector Now()
	{
		return s_Pos + s_Vel * (GetGame().GetTickTime() - s_At);
	}

	// heard about recently (a client only knows once the first state of the flight arrives)
	static bool Known()
	{
		return s_At > 0 && GetGame().GetTickTime() - s_At < 5;
	}
}

// Client side: the latest state from the server.
class BR_Client
{
	static ref BR_State s_State;
	static float s_ReceivedAt;

	static vector PlaneNow()
	{
		return BR_Plane.Now();
	}

	// the circle as it is this frame: while it closes, moved on from the last state the way the
	// server moves it (evenly, onto the next circle), so the wall and the map glide rather
	// than step once a second
	static void CircleNow(out vector center, out float radius)
	{
		center = s_State.center;
		radius = s_State.radius;
		if (!s_State.closing || s_State.closeLeft <= 0 || !s_State.hasNext)
			return;
		float f = Math.Clamp((GetGame().GetTickTime() - s_ReceivedAt) / s_State.closeLeft, 0, 1);
		center = center + (s_State.nextCenter - center) * f;
		radius = radius + (s_State.nextRadius - radius) * f;
	}

	// F: with where this game shows the plane, so the jump starts at the plane the player sees
	static void RequestJump(vector seenPlane)
	{
		ScriptRPC rpc = new ScriptRPC();
		rpc.Write(seenPlane);
		rpc.Send(null, BR_State.RPC_JUMP, true, null);
	}

	// boarding: this game shows the round's time of day and plays the plane's engine (sent once
	// a second while it does, until the plane takes off)
	static float s_BoardedSentAt;
	static bool s_BoardedLogged;
	static void Boarding(bool ready)
	{
		if (!ready)
		{
			s_BoardedLogged = false;
			return;
		}
		float now = GetGame().GetTickTime();
		if (now - s_BoardedSentAt < 1)
			return;
		s_BoardedSentAt = now;
		if (!s_BoardedLogged)
			Print("[BattleDayZ] boarding: this game shows the round's time and plays the engine: ready for take-off");
		s_BoardedLogged = true;
		ScriptRPC rpc = new ScriptRPC();
		rpc.Send(null, BR_State.RPC_BOARDED, true, null);
	}

	// every time the server sets the round's time (the lobby opening for it, the round starting):
	// this game's clock straight onto it, rather than waiting up to half a minute for the
	// server's clock to reach it (the server's agrees when it does). Once per setting: the clock
	// runs on from there.
	static int s_TimeSetSeen;
	static void SetRoundTime(BR_State state)
	{
		if (state.timeSet == s_TimeSetSeen)
			return;
		s_TimeSetSeen = state.timeSet;
		if (TimeIsRound(state))
			return;
		int year, month, day, hour, minute;
		GetGame().GetWorld().GetDate(year, month, day, hour, minute);
		GetGame().GetWorld().SetDate(year, month, state.roundDay, state.roundHour, 0);
		Print("[BattleDayZ] this game's clock set to the round's " + state.roundHour + ":00 on day " + state.roundDay + " (it was " + hour + ":" + minute + " on day " + day + ")");
	}

	// this game's clock on the round's hour and day (the day sets the moon)
	static bool TimeIsRound(BR_State state)
	{
		int year, month, day, hour, minute;
		GetGame().GetWorld().GetDate(year, month, day, hour, minute);
		return hour == state.roundHour && day == state.roundDay;
	}
}

// Steering the jump with the movement keys. A script command gets no movement from the game's
// input controller (it reads zero), so the jumper's own game reads the keys itself and sends
// them to the server; both run the parachute with the same value (x right, z forward, -1..1).
class BR_Steer
{
	static const int RPC = 1112670211;
	static vector s_Local;                                         // client: this player's keys (x right, y shift, z forward)
	static ref map<string, vector> s_Server = new map<string, vector>(); // server: by player id
	static vector s_Sent;
	static float s_SentAt;

	// client, every frame while jumping
	static void Update()
	{
		UAInputAPI api = GetUApi();
		float forward = api.GetInputByName("UAMoveForward").LocalValue() - api.GetInputByName("UAMoveBack").LocalValue();
		float right = api.GetInputByName("UAMoveRight").LocalValue() - api.GetInputByName("UAMoveLeft").LocalValue();
		float fast = 0; // shift (sprint): the middle of the three, up and down having no key
		if (api.GetInputByName("UATurbo").LocalValue() > 0)
			fast = 1;
		s_Local = Vector(Math.Clamp(right, -1, 1), fast, Math.Clamp(forward, -1, 1));
		float now = GetGame().GetTickTime();
		if (vector.Distance(s_Local, s_Sent) > 0.05 || now - s_SentAt > 0.5)
		{
			s_Sent = s_Local;
			s_SentAt = now;
			ScriptRPC rpc = new ScriptRPC();
			rpc.Write(s_Local);
			rpc.Send(null, RPC, false, null);
		}
	}

	static vector ForPlayer(string id)
	{
		vector v;
		if (s_Server.Find(id, v))
			return v;
		return vector.Zero;
	}
}

// Client: watching another player after being knocked out (the server holds this player's own
// character, out of sight, above them, so this game has them and what's round them). The camera
// follows them (BR_PlaneCamera.c), by their network id once this game has them.
class BR_Watch
{
	static bool s_On;
	static string s_Name;
	static int s_Low;
	static int s_High;
	static vector s_Pos;
	static float s_OffAt;

	static void Seen(BR_State state)
	{
		bool on = state.watching != "";
		if (s_On && !on)
			s_OffAt = GetGame().GetTickTime();
		s_On = on;
		s_Name = state.watching;
		s_Low = state.watchLow;
		s_High = state.watchHigh;
		s_Pos = state.watchPos;
	}

	// on, or only just switched off (the camera change back is still under way)
	static bool Recent()
	{
		return s_On || GetGame().GetTickTime() - s_OffAt < 2;
	}

	// where the watched player is in this game (their position as the server last said, if
	// this game hasn't got them yet)
	static vector Target()
	{
		Object them = GetGame().GetObjectByNetworkId(s_Low, s_High);
		if (them)
			return them.GetPosition();
		return s_Pos;
	}
}

// Client: a lobby in the sky that's been taken down stays in this game (a server can't take
// back a static model it sent; a game that joins later can still be sent one, or keep one from
// before it reconnected), so this game clears the other places a lobby goes itself: every plain
// static object round them, up in the sky, whenever the lobby is somewhere new to it (the
// first time it hears too). Buildings (Land_ classes) are entities the server deletes properly;
// they're only counted.
class BR_LobbyGhosts
{
	static const float REACH = 250; // m round each other place: they're 600 m apart
	static vector s_At;

	static void Seen(BR_State state)
	{
		vector at = state.lobbyAt;
		if (at == vector.Zero || vector.Distance(at, s_At) < 1)
			return;
		s_At = at;
		for (int i = 0; i < state.lobbySlots; i++)
		{
			vector slot = state.lobbySlot0 + Vector(i * state.lobbyApart, 0, 0);
			if (vector.Distance(slot, at) > 1)
				Clear(slot, REACH);
		}
	}

	static void Clear(vector at, float reach)
	{
		array<Object> found = new array<Object>();
		GetGame().GetObjectsAtPosition3D(at, reach, found, null);
		int cleared = 0;
		int entities = 0;
		foreach (Object o : found)
		{
			if (!o || o.IsInherited(Man))
				continue;
			if (o.IsInherited(EntityAI))
			{
				entities++;
				continue;
			}
			GetGame().ObjectDelete(o);
			cleared++;
		}
		Print("[BattleDayZ] client: took down the old lobby at " + at + ": " + cleared + " pieces, " + entities + " entities left to the server");
	}
}

// Server side: the round (5_Mission) listens here for jump requests.
class BR_Server
{
	static ref ScriptInvoker s_OnJump = new ScriptInvoker(); // (PlayerIdentity sender, vector seenPlane)
	static ref ScriptInvoker s_OnBoarded = new ScriptInvoker(); // (PlayerIdentity sender)
}

modded class DayZGame
{
	override void OnRPC(PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx)
	{
		if (rpc_type == BR_State.RPC)
		{
			BR_State state = new BR_State();
			if (state.Read(ctx))
			{
				BR_Client.s_State = state;
				BR_Client.s_ReceivedAt = GetTickTime();
				if (state.flying && !IsServer())
					BR_Plane.Set(state.planePos, state.planeVel);
				if (!IsServer())
				{
					BR_LobbyGhosts.Seen(state);
					BR_Watch.Seen(state);
				}
			}
			return;
		}
		if (rpc_type == BR_Steer.RPC)
		{
			vector steer;
			if (IsServer() && sender && ctx.Read(steer))
				BR_Steer.s_Server.Set(sender.GetId(), steer);
			return;
		}
		if (rpc_type == BR_State.RPC_BOARDED)
		{
			if (IsServer() && sender)
				BR_Server.s_OnBoarded.Invoke(sender);
			return;
		}
		if (rpc_type == BR_State.RPC_JUMP)
		{
			if (IsServer() && sender)
			{
				vector seen;
				ctx.Read(seen);
				BR_Server.s_OnJump.Invoke(sender, seen);
			}
			return;
		}
		super.OnRPC(sender, target, rpc_type, ctx);
	}
}
