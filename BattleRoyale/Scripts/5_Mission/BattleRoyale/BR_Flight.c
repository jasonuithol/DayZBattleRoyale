// The plane at the start of a round: a straight route across the first circle, through a
// point near its middle, at a fixed height clear of the hills under it. Where the plane is
// comes from the clock (start + direction * speed * time); Expansion's airdrop plane flies
// the same line so there's something to see.
class BR_Flight
{
	vector from;      // the route, on the ground (y 0)
	vector to;
	vector dir;       // unit, from -> to
	float length;
	float altitude;   // above sea level
	float speed;
	float started;    // tick time it took off

	static const float ROUTE_INSIDE = 0.75; // the route stays within this much of the circle's radius

	// a route through the circle, ending well inside it and inside the box the map's
	// buildings sit in: anyone still on board is dropped at the end, and the circle's edge
	// (or empty land past the last buildings) is the worst place to land
	static BR_Flight Plan(vector center, float radius, float minX, float maxX, float minZ, float maxZ, float height, float planeSpeed)
	{
		BR_Flight f = new BR_Flight();
		f.speed = planeSpeed;

		// through a point within 30% of the radius of the middle, in a random direction
		float a = Math.RandomFloat(0, Math.PI2);
		float d = Math.Sqrt(Math.RandomFloat01()) * radius * 0.3;
		vector p = center + Vector(Math.Cos(a) * d, 0, Math.Sin(a) * d);
		float heading = Math.RandomFloat(0, Math.PI2);
		f.dir = Vector(Math.Cos(heading), 0, Math.Sin(heading));

		// where the line p + dir * t enters and leaves the inner 75% of the circle
		float inner = radius * ROUTE_INSIDE;
		vector pc = Vector(p[0] - center[0], 0, p[2] - center[2]);
		float b = f.dir[0] * pc[0] + f.dir[2] * pc[2];
		float c = pc[0] * pc[0] + pc[2] * pc[2] - inner * inner;
		float root = Math.Sqrt(Math.Max(b * b - c, 0));
		float t0 = -b - root;
		float t1 = -b + root;

		// and where it leaves the buildings (500 m in from the outermost)
		float margin = 500;
		Clip(p[0], f.dir[0], minX + margin, maxX - margin, t0, t1);
		Clip(p[2], f.dir[2], minZ + margin, maxZ - margin, t0, t1);

		f.from = p + f.dir * t0;
		f.to = p + f.dir * t1;
		f.length = Math.Max(t1 - t0, 1);

		// clear of the highest ground under the route
		float top = 0;
		for (float s = 0; s <= f.length; s += 100)
		{
			vector q = f.from + f.dir * s;
			top = Math.Max(top, GetGame().SurfaceY(q[0], q[2]));
		}
		f.altitude = top + height;
		return f;
	}

	// narrows [t0, t1] to where p + dir * t stays within [lo, hi] on one axis
	static void Clip(float p, float d, float lo, float hi, inout float t0, inout float t1)
	{
		if (Math.AbsFloat(d) < 0.0001)
			return;
		float ta = (lo - p) / d;
		float tb = (hi - p) / d;
		t0 = Math.Max(t0, Math.Min(ta, tb));
		t1 = Math.Min(t1, Math.Max(ta, tb));
	}

	float Duration()
	{
		return length / speed;
	}

	bool Over(float now)
	{
		return now - started >= Duration();
	}

	vector PositionAt(float now)
	{
		float s = Math.Clamp((now - started) * speed, 0, length);
		vector p = from + dir * s;
		p[1] = altitude;
		return p;
	}

	vector Velocity()
	{
		return dir * speed;
	}

	void TakeOff(float now)
	{
		started = now;
		SendPlane();
	}

	// Expansion's airdrop plane, flying this route at this height and speed. Its drop point is
	// set far past the end of the route, so it never drops anything: it flies off the edge of
	// the map and Expansion removes it.
	protected void SendPlane()
	{
		auto settings = GetExpansionSettings().GetAirdrop();
		string type = settings.AirdropPlaneClassName;
		if (!type || !GetGame().ConfigIsExisting("CfgVehicles " + type))
			type = "ExpansionAirdropPlane";
		if (!GetGame().ConfigIsExisting("CfgVehicles " + type))
			type = "ExpansionAirdropPlaneBasic";

		vector start = PositionAt(started);
		vector never = to + dir * 50000;
		never[1] = altitude;

		int flags = ECE_AIRBORNE;
		if (ExpansionAirdropPlaneBase.s_Expansion_CreatePlaneOnClient)
			flags |= ECE_LOCAL;
		ExpansionAirdropPlaneBase plane = ExpansionAirdropPlaneBase.Cast(GetGame().CreateObjectEx(type, start, flags));
		if (!plane)
		{
			Print("[BattleRoyale] couldn't create the plane (" + type + ")");
			return;
		}
		if (ExpansionAirdropPlaneBase.s_Expansion_CreatePlaneOnClient)
			plane.Expansion_SetAirdropPlaneID(ExpansionAirdropPlaneBase.s_Expansion_AirdropPlaneNextID++);

		// a container is required, though it's never dropped
		ExpansionLootContainer nothing = new ExpansionLootContainer("ExpansionAirdropContainer_Military", 0, 1, new array<ref ExpansionLoot>(), new TStringArray(), 0, 0, false, 4.5);
		plane.Expansion_SetupPlane(start, never, "Battle royale", 0, false, altitude, altitude, 0, speed, speed, 0, nothing);

		if (ExpansionAirdropPlaneBase.s_Expansion_CreatePlaneOnClient)
			plane.Expansion_SendCreatePlaneOnClient();
		plane.Expansion_EnableUpdate();
	}
}
