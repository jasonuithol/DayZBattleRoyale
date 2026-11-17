// The lobby in the sky: a copy of a place, floating lobbyHeight metres up. Two kinds:
//   - a place on the map the server is running, named in the map's profile (BR_Maps.c): the
//     server copies whatever stands there when it builds the lobby, on a level floor of slabs
//   - a file in the mission's lobbies folder (tools/make_lobby.py), for a place from another
//     map: every object with its model and transform, and the floor
// "lobbies" in battleroyale.json lists the ones to take turns, the next after every round
// (empty: all of the map's own). Only one is up at a time; the clients take the old one down
// themselves (BR_LobbyGhosts), since a server can't take back a static model it has sent.
//
// Buildings with a Land_ class go up as that class (doors and ladders work), everything else
// straight from its model. Both kinds are sent to every game that joins.

class BR_LobbyPiece
{
	string p3d;
	string cls;              // a Land_ class, when the model has one
	int floor;               // 1: a floor slab, its top centre on the point in m
	ref array<float> m;      // aside, up, dir, position (relative to the spot, its ground at y 0)
}

class BR_LobbyFile
{
	string title;
	string source;
	float leash = 40;        // m from the middle, flat: further than this, players are brought back
	ref array<float> spawn;  // x, z (relative) and radius where players appear
	ref array<ref BR_LobbyPiece> pieces;
}

class BR_SkyLobby
{
	static const float FALL_LIMIT = 15;   // m below the lowest floor: a player down there has fallen off
	static const int SPOT_TRIES = 40;
	static const string FLOOR_MODEL = "dz\\structures\\rail\\misc\\rail_crossing_block_10.p3d"; // a concrete slab, 2 m thick
	static const float FLOOR_OVERLAP = 0.1;

	string m_Name;
	string m_Title;
	vector m_Origin;                       // the spot's original ground, up in the sky
	float m_Leash;
	protected vector m_SpawnAt;
	protected float m_SpawnRadius;
	protected float m_Lowest = 1e9;        // the lowest floor top
	protected float m_Highest = -1e9;
	protected ref array<Object> m_Floor = new array<Object>();
	protected ref array<vector> m_FloorTops = new array<vector>();
	ref array<Object> m_Built = new array<Object>();
	// diagnostics, for the self-test
	int m_Buildings;
	int m_Models;
	int m_Failed;
	float m_WorstMiss;                     // m between where a piece should be and where it is
	string m_FloorSize;

	// the map's own place of that name, or else the mission's file of that name
	static BR_SkyLobby Build(string name, vector origin, BR_Map profile)
	{
		BR_LobbySpec spec;
		if (profile)
			spec = profile.Lobby(name);
		if (spec)
		{
			BR_LobbyFile copy = Copy(spec);
			if (!copy)
				return null;
			BR_SkyLobby live = new BR_SkyLobby();
			live.m_Name = name;
			live.Make(copy, origin);
			return live;
		}
		string path = "$mission:lobbies/" + name + ".json";
		if (!FileExist(path))
		{
			Print("[BattleDayZ] FAIL no lobby " + name + " on " + profile.m_Title + " and no file " + path);
			return null;
		}
		BR_LobbyFile file;
		string error;
		if (!JsonFileLoader<BR_LobbyFile>.LoadFile(path, file, error) || !file || !file.pieces || file.pieces.Count() == 0)
		{
			Print("[BattleDayZ] FAIL lobby file " + path + " can't be read (" + error + ")");
			return null;
		}
		BR_SkyLobby lobby = new BR_SkyLobby();
		lobby.m_Name = name;
		lobby.Make(file, origin);
		return lobby;
	}

	// what stands at a place on this map, as a lobby: the same rules as tools/make_lobby.py.
	// Buildings, rocks and whatever stands within a building keep their height (foundations go
	// into the floor, rocks below it hang under the island); trees, bushes and small things
	// out in the open stand on the floor as they stood on the ground. Things that move (players, items, vehicles, animals) stay behind.
	static BR_LobbyFile Copy(BR_LobbySpec spec)
	{
		float ground = GetGame().SurfaceY(spec.x, spec.z);
		vector centre = Vector(spec.x, ground, spec.z);
		array<Object> found = new array<Object>();
		GetGame().GetObjectsAtPosition(centre, spec.radius, found, null);

		BR_LobbyFile file = new BR_LobbyFile();
		file.title = spec.title;
		file.source = "the map at " + spec.x + " " + spec.z + ", " + spec.radius + " m";
		file.leash = spec.radius + 10;
		float spawnRadius = spec.spawnRadius;
		if (spawnRadius <= 0)
			spawnRadius = spec.radius * 0.5;
		file.spawn = new array<float>();
		file.spawn.Insert(0);
		file.spawn.Insert(0);
		file.spawn.Insert(spawnRadius);
		file.pieces = new array<ref BR_LobbyPiece>();
		// the buildings first: anything standing within one belongs to it (a pool in its
		// courtyard, a decal on its wall, its furniture) and keeps its height with it
		array<Object> buildings = new array<Object>();
		foreach (Object b : found)
		{
			if (b && b.IsInherited(Building) && b.GetType().IndexOf("Land_") == 0 && BR_Loot.Distance2D(b.GetPosition(), centre) <= spec.radius)
				buildings.Insert(b);
		}
		foreach (Object o : found)
		{
			if (!o || BR_Loot.Distance2D(o.GetPosition(), centre) > spec.radius)
				continue;
			if (o.IsInherited(Man) || o.IsInherited(ItemBase) || o.IsInherited(Transport) || o.IsInherited(DayZCreature))
				continue;
			string shape = o.GetShapeName();
			string type = o.GetType();
			bool building = o.IsInherited(Building) && type.IndexOf("Land_") == 0;
			if (!building && shape == "")
				continue;
			vector p = o.GetPosition();
			if (p[1] - GetGame().SurfaceY(p[0], p[2]) > 300)
				continue; // up in the sky: a lobby itself
			vector mat[4];
			o.GetTransform(mat);
			float y = p[1] - ground;
			if (!building && !o.IsRock() && !Within(p, buildings))
				y += spec.floorY - (GetGame().SurfaceY(p[0], p[2]) - ground);
			BR_LobbyPiece piece = new BR_LobbyPiece();
			piece.p3d = shape;
			if (building)
				piece.cls = type;
			piece.m = Matrix(mat[0], mat[1], mat[2], Vector(p[0] - spec.x, y, p[2] - spec.z));
			file.pieces.Insert(piece);
		}
		AddFloor(file, spec.radius, spec.floorY);
		return file;
	}

	static array<float> Matrix(vector aside, vector up, vector dir, vector pos)
	{
		array<float> m = new array<float>();
		for (int i = 0; i < 3; i++)
			m.Insert(aside[i]);
		for (i = 0; i < 3; i++)
			m.Insert(up[i]);
		for (i = 0; i < 3; i++)
			m.Insert(dir[i]);
		for (i = 0; i < 3; i++)
			m.Insert(pos[i]);
		return m;
	}

	// inside one of these buildings' footprints (its collision box, turned with it)
	static bool Within(vector p, array<Object> buildings)
	{
		foreach (Object b : buildings)
		{
			vector box[2];
			if (!b.GetCollisionBox(box))
				continue;
			vector inModel = b.WorldToModel(p);
			if (inModel[0] >= box[0][0] && inModel[0] <= box[1][0] && inModel[2] >= box[0][2] && inModel[2] <= box[1][2])
				return true;
		}
		return false;
	}

	// a level floor over the circle: slabs edge to edge by the size of their collision box
	static void AddFloor(BR_LobbyFile file, float radius, float y)
	{
		Object probe = GetGame().CreateStaticObjectUsingP3D(FLOOR_MODEL, "0 0 0", "0 0 0", 1.0, true);
		if (!probe)
			return;
		vector box[2];
		bool sized = probe.GetCollisionBox(box);
		GetGame().ObjectDelete(probe);
		if (!sized)
			return;
		float sx = box[1][0] - box[0][0] - FLOOR_OVERLAP;
		float sz = box[1][2] - box[0][2] - FLOOR_OVERLAP;
		if (sx < 1 || sz < 1)
			return;
		int nx = Math.Ceil(radius / sx);
		int nz = Math.Ceil(radius / sz);
		float margin = Math.Max(sx, sz) * 0.5;
		for (int i = -nx; i <= nx; i++)
		{
			for (int j = -nz; j <= nz; j++)
			{
				float dx = i * sx;
				float dz = j * sz;
				if (Math.Sqrt(dx * dx + dz * dz) > radius + margin)
					continue;
				BR_LobbyPiece slab = new BR_LobbyPiece();
				slab.p3d = FLOOR_MODEL;
				slab.floor = 1;
				slab.m = Matrix("1 0 0", "0 1 0", "0 0 1", Vector(dx, y, dz));
				file.pieces.Insert(slab);
			}
		}
	}

	protected void Make(BR_LobbyFile file, vector origin)
	{
		m_Title = file.title;
		m_Origin = origin;
		m_Leash = file.leash;
		m_SpawnAt = origin;
		m_SpawnRadius = 10;
		if (file.spawn && file.spawn.Count() >= 3)
		{
			m_SpawnAt = origin + Vector(file.spawn[0], 0, file.spawn[1]);
			m_SpawnRadius = file.spawn[2];
		}
		foreach (BR_LobbyPiece piece : file.pieces)
		{
			if (!piece.m || piece.m.Count() < 12)
				continue;
			Object o = Place(piece);
			if (!o)
			{
				m_Failed++;
				continue;
			}
			m_Built.Insert(o);
		}
		Print("[BattleDayZ] lobby in the sky: " + m_Title + " at " + m_Origin + ", " + m_Built.Count() + " pieces (" + m_Buildings + " buildings, " + m_Models + " models, " + m_Floor.Count() + " floor slabs), " + m_Failed + " failed, floor " + (m_Lowest - m_Origin[1]) + " to " + (m_Highest - m_Origin[1]) + " m, worst miss " + m_WorstMiss + " m");
	}

	protected Object Place(BR_LobbyPiece piece)
	{
		vector mat[4];
		mat[0] = Vector(piece.m[0], piece.m[1], piece.m[2]);
		mat[1] = Vector(piece.m[3], piece.m[4], piece.m[5]);
		mat[2] = Vector(piece.m[6], piece.m[7], piece.m[8]);
		mat[3] = m_Origin + Vector(piece.m[9], piece.m[10], piece.m[11]);
		// a map can scale an object (trees, rocks): the axes are that long
		float scale = mat[1].Length();
		if (scale < 0.01)
			scale = 1;
		vector axes[3];
		float unscale = 1 / scale;
		axes[0] = mat[0] * unscale;
		axes[1] = mat[1] * unscale;
		axes[2] = mat[2] * unscale;
		vector ypr = Math3D.MatrixToAngles(axes);
		vector want = mat[3];

		if (piece.cls != "" && !piece.floor && GetGame().ConfigIsExisting("CfgVehicles " + piece.cls))
		{
			Object b = GetGame().CreateObjectEx(piece.cls, want, ECE_SETUP | ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN | ECE_NOLIFETIME | ECE_NOPERSISTENCY_WORLD); // never saved: a lobby is rebuilt at every start
			if (b)
			{
				b.SetOrientation(ypr);
				b.SetPosition(want);
				b.Update();
				Missed(b, want);
				m_Buildings++;
				return b;
			}
		}

		// from the model: a server-only copy first, to see where the engine actually puts it
		// (it shifts a model from where it was asked) and, for a slab, where its top is; the
		// real one then goes where that copy says. Clients only ever see where a static model
		// was created, never a later move.
		Object probe = GetGame().CreateStaticObjectUsingP3D(piece.p3d, want, ypr * Math.DEG2RAD, scale, true);
		if (!probe)
			return null;
		vector shift = probe.GetPosition() - want;
		vector target = want;
		if (piece.floor)
		{
			vector box[2];
			if (probe.GetCollisionBox(box))
			{
				vector top = Vector((box[0][0] + box[1][0]) * 0.5, box[1][1], (box[0][2] + box[1][2]) * 0.5);
				target = want - (probe.ModelToWorld(top) - probe.GetPosition());
			}
		}
		GetGame().ObjectDelete(probe);
		Object o = GetGame().CreateStaticObjectUsingP3D(piece.p3d, target - shift, ypr * Math.DEG2RAD, scale);
		if (!o)
			return null;
		Missed(o, target);
		m_Models++;
		if (piece.floor)
		{
			m_Floor.Insert(o);
			m_FloorTops.Insert(want);
			m_Lowest = Math.Min(m_Lowest, want[1]);
			m_Highest = Math.Max(m_Highest, want[1]);
		}
		return o;
	}

	protected void Missed(Object o, vector want)
	{
		m_WorstMiss = Math.Max(m_WorstMiss, vector.Distance(o.GetPosition(), want));
	}

	// gone from the server (deletes happen at the end of the frame). The games connected to it
	// keep the static models: they take them down themselves (BR_LobbyGhosts).
	void Destroy()
	{
		foreach (Object o : m_Built)
		{
			if (o)
				GetGame().ObjectDelete(o);
		}
		Print("[BattleDayZ] lobby in the sky: " + m_Title + " taken down (" + m_Built.Count() + " pieces)");
	}

	// somewhere to stand: a random point near the spawn spot whose first thing below is at floor
	// height (not a roof, a wall or a tree), or failing that anything flat; failing that too,
	// the top of the slab nearest the spot
	vector Spot()
	{
		// at floor height if there's any; else anything flat out in the open (a building that
		// fills the place, like Qalat-e-Lut's caravanserai: its courtyard, its roofs)
		for (int pass = 0; pass < 2; pass++)
		{
			for (int i = 0; i < SPOT_TRIES; i++)
			{
				float a = Math.RandomFloat(0, Math.PI2);
				float r = m_SpawnRadius * Math.Sqrt(Math.RandomFloat01());
				vector at;
				if (OnFloor(m_SpawnAt[0] + Math.Cos(a) * r, m_SpawnAt[2] + Math.Sin(a) * r, at, pass == 1))
					return at;
			}
		}
		return NearestTop(m_SpawnAt) + "0 0.2 0";
	}

	bool OnFloor(float x, float z, out vector at, bool anyFlat = false)
	{
		vector from = Vector(x, m_Highest + 80, z);
		vector to = Vector(x, m_Lowest - 5, z);
		vector hit, normal;
		int comp;
		set<Object> objects = new set<Object>;
		if (!DayZPhysics.RaycastRV(from, to, hit, normal, comp, objects, null, null, true, false, ObjIntersectGeom))
			return false;
		if (objects.Count() == 0)
			return false;
		// the first thing below is at floor height (a slab, a courtyard, a building's ground
		// floor out in the open) and not a plant: not a roof, a wall top or a treetop
		if (!anyFlat && (hit[1] < m_Lowest - 0.5 || hit[1] > m_Highest + 1.0))
			return false;
		// flat: the first thing below half a metre away either side is about as high (the
		// raycast's own normal isn't usable: it comes back pointing down, any length)
		if (!Level(x + 0.5, z, hit[1]) || !Level(x, z + 0.5, hit[1]))
			return false;
		for (int i = 0; i < objects.Count(); i++)
		{
			if (objects[i] && (objects[i].IsTree() || objects[i].IsBush()))
				return false;
		}
		at = hit + "0 0.1 0";
		return true;
	}

	protected bool Level(float x, float z, float y)
	{
		vector hit, normal;
		int comp;
		set<Object> objects = new set<Object>;
		if (!DayZPhysics.RaycastRV(Vector(x, y + 3, z), Vector(x, y - 3, z), hit, normal, comp, objects, null, null, true, false, ObjIntersectGeom))
			return false;
		return Math.AbsFloat(hit[1] - y) < 0.25;
	}

	// diagnostics: what's under a random spawn point (height above the floor, and what it is)
	string Probe()
	{
		float a = Math.RandomFloat(0, Math.PI2);
		float r = m_SpawnRadius * Math.Sqrt(Math.RandomFloat01());
		float x = m_SpawnAt[0] + Math.Cos(a) * r;
		float z = m_SpawnAt[2] + Math.Sin(a) * r;
		vector hit, normal;
		int comp;
		set<Object> objects = new set<Object>;
		if (!DayZPhysics.RaycastRV(Vector(x, m_Highest + 80, z), Vector(x, m_Lowest - 5, z), hit, normal, comp, objects, null, null, true, false, ObjIntersectGeom))
			return "nothing";
		string what = "";
		for (int i = 0; i < objects.Count(); i++)
		{
			if (objects[i])
				what += " " + objects[i].GetType() + "/" + objects[i].GetShapeName();
		}
		return (hit[1] - m_Lowest).ToString() + what;
	}

	vector NearestTop(vector p)
	{
		vector best = m_Origin;
		float bestD = 1e9;
		foreach (vector t : m_FloorTops)
		{
			float d = BR_Loot.Distance2D(t, p);
			if (d < bestD)
			{
				bestD = d;
				best = t;
			}
		}
		return best;
	}

	int StillThere()
	{
		int n = 0;
		foreach (Object o : m_Built)
		{
			if (o)
				n++;
		}
		return n;
	}

	// how far from the middle its pieces can be (for the clients' clean-up)
	float Reach()
	{
		return m_Leash + 60;
	}

	// wandered off the edge or fallen off it
	bool Strayed(vector p)
	{
		return BR_Loot.Distance2D(p, m_Origin) > m_Leash || p[1] < m_Lowest - FALL_LIMIT;
	}
}
