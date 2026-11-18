// The plane at the start of a round: a straight route between two random points on opposite
// sides of the map (north to south, or west to east, either way), at a fixed height clear of
// the hills under it. It has nothing to do with the circles. Where the plane is comes from the
// clock (start + direction * speed * time); Expansion's airdrop plane flies the same line so
// there's something to see.
class BR_Flight
{
	vector from;      // the route, on the ground (y 0)
	vector to;
	vector dir;       // unit, from -> to
	float length;
	float altitude;   // above sea level
	float speed;
	float started;    // tick time it took off

	static const float EDGE_MARGIN = 100; // m in from the map's edge (Expansion removes a plane that leaves the map)

	static BR_Flight Plan(float worldSize, float height, float planeSpeed)
	{
		BR_Flight f = new BR_Flight();
		f.speed = planeSpeed;

		// a random point on each of two opposite sides
		float lo = EDGE_MARGIN;
		float hi = worldSize - EDGE_MARGIN;
		float a = Math.RandomFloat(lo, hi);
		float b = Math.RandomFloat(lo, hi);
		if (Math.RandomIntInclusive(0, 1) == 0)
		{
			f.from = Vector(a, 0, lo); // south to north
			f.to = Vector(b, 0, hi);
		}
		else
		{
			f.from = Vector(lo, 0, a); // west to east
			f.to = Vector(hi, 0, b);
		}
		if (Math.RandomIntInclusive(0, 1) == 0)
		{
			vector swap = f.from;
			f.from = f.to;
			f.to = swap;
		}
		vector line = f.to - f.from;
		f.length = Math.Max(line.Length(), 1);
		f.dir = line * (1 / f.length);

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

	// where the plane really is: it carries on past the end of the route to the map's edge
	vector FlyingAt(float now)
	{
		vector p = from + dir * ((now - started) * speed);
		p[1] = altitude;
		return p;
	}

	bool OffMap(float now, float worldSize)
	{
		vector p = FlyingAt(now);
		return p[0] < 0 || p[2] < 0 || p[0] > worldSize || p[2] > worldSize;
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
			Print("[BattleDayZ] couldn't create the plane (" + type + ")");
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
