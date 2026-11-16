// Shared by server and client: the round phases, the state the server sends every second,
// the jump request going the other way, and the screen tint for being outside the circle.

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

	int phase;
	bool inRound;      // this player is alive in the current round
	bool inPlane;      // ... and still on the plane: F jumps
	vector center;     // the circle right now
	float radius;
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

	void Write(ParamsWriteContext ctx)
	{
		ctx.Write(phase);
		ctx.Write(inRound);
		ctx.Write(inPlane);
		ctx.Write(center);
		ctx.Write(radius);
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
	}

	bool Read(ParamsReadContext ctx)
	{
		if (!ctx.Read(phase) || !ctx.Read(inRound) || !ctx.Read(inPlane) || !ctx.Read(center) || !ctx.Read(radius))
			return false;
		if (!ctx.Read(hasNext) || !ctx.Read(nextCenter) || !ctx.Read(nextRadius))
			return false;
		if (!ctx.Read(flying) || !ctx.Read(routeFrom) || !ctx.Read(routeTo) || !ctx.Read(planePos) || !ctx.Read(planeVel))
			return false;
		return ctx.Read(title) && ctx.Read(subtitle);
	}

	// horizontal distance from pos to the circle's edge; positive means outside
	float OutsideBy(vector pos)
	{
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

	static void RequestJump()
	{
		ScriptRPC rpc = new ScriptRPC();
		rpc.Send(null, BR_State.RPC_JUMP, true, null);
	}
}

// Steering the jump with the movement keys. A script command gets no movement from the game's
// input controller (it reads zero), so the jumper's own game reads the keys itself and sends
// them to the server; both run the parachute with the same value (x right, z forward, -1..1).
class BR_Steer
{
	static const int RPC = 1112670211;
	static vector s_Local;                                         // client: this player's keys
	static ref map<string, vector> s_Server = new map<string, vector>(); // server: by player id
	static vector s_Sent;
	static float s_SentAt;

	// client, every frame while jumping
	static void Update()
	{
		UAInputAPI api = GetUApi();
		float forward = api.GetInputByName("UAMoveForward").LocalValue() - api.GetInputByName("UAMoveBack").LocalValue();
		float right = api.GetInputByName("UAMoveRight").LocalValue() - api.GetInputByName("UAMoveLeft").LocalValue();
		s_Local = Vector(Math.Clamp(right, -1, 1), 0, Math.Clamp(forward, -1, 1));
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

// Server side: the round (5_Mission) listens here for jump requests.
class BR_Server
{
	static ref ScriptInvoker s_OnJump = new ScriptInvoker(); // (PlayerIdentity sender)
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
		if (rpc_type == BR_State.RPC_JUMP)
		{
			if (IsServer() && sender)
				BR_Server.s_OnJump.Invoke(sender);
			return;
		}
		super.OnRPC(sender, target, rpc_type, ctx);
	}
}

// Outside the circle the world turns blue: the blue sport glasses' tint, stronger, on the
// layer the contaminated areas use (so it doesn't fight real glasses).
class PPERequester_BRZone extends PPERequester_GameplayBase
{
	override protected void OnStart(Param par = null)
	{
		super.OnStart();
		SetTargetValueColor(PostProcessEffectType.Glow, PPEGlow.PARAM_COLORIZATIONCOLOR, {0.5, 0.5, 0.0, 0.0}, PPEGlow.L_23_TOXIC_TINT, PPOperators.SUBSTRACT);
	}
}
