// What's particular to each map, kept apart from the rules of the game: a profile per map,
// picked by the world's name when the server starts. The core mod has Chernarus (it comes
// with the game); a DLC or third-party map gets its profile from a small extension mod
// (BattleDayZ_Nasdara, say), which adds it with
//
//   modded class BR_Maps
//   {
//       override void Register()
//       {
//           super.Register();
//           Add(new BR_MapNasdara());
//       }
//   }
//
// A map without a profile still works: the lobby is in the mission's lobbies folder or on
// the ground, and everything else comes from the mission's battleroyale.json and br_spots.json.

// A place on this map, copied up into the sky as the lobby while the server runs (nothing to
// generate beforehand: whatever stands there is copied, on a level floor).
class BR_LobbySpec
{
	string name;
	string title;
	float x;
	float z;
	float radius;        // m: everything within this goes up, on a floor this big
	float floorY;        // m above the ground at x, z
	float spawnRadius;   // m round x, z where players appear (0: half the radius)

	void BR_LobbySpec(string pName, string pTitle, float pX, float pZ, float pRadius, float pFloorY = 0, float pSpawnRadius = 0)
	{
		name = pName;
		title = pTitle;
		x = pX;
		z = pZ;
		radius = pRadius;
		floorY = pFloorY;
		spawnRadius = pSpawnRadius;
	}
}

class BR_Map
{
	string m_World;   // the world's name in lower case (GetWorldName); "" for the fallback
	string m_Title = "this map";
	ref array<ref BR_LobbySpec> m_Lobbies = new array<ref BR_LobbySpec>();

	// the map's lobbies, in the order they take turns
	void Lobbies()
	{
	}

	void AddLobby(BR_LobbySpec spec)
	{
		m_Lobbies.Insert(spec);
	}

	BR_LobbySpec Lobby(string name)
	{
		foreach (BR_LobbySpec spec : m_Lobbies)
		{
			if (spec.name == name)
				return spec;
		}
		return null;
	}

	// every second on the server, for anything a map needs done all the time
	void OnTick()
	{
	}
}

class BR_MapChernarus: BR_Map
{
	void BR_MapChernarus()
	{
		m_World = "chernarusplus";
		m_Title = "Chernarus";
	}

	override void Lobbies()
	{
		AddLobby(new BR_LobbySpec("zub", "Zub castle", 6543, 5597, 42));
		AddLobby(new BR_LobbySpec("cherno_square", "Chernogorsk square", 6576, 2448, 45));
		AddLobby(new BR_LobbySpec("prison", "Prison Island", 2783, 1240, 60));
	}
}

class BR_Maps
{
	protected static ref array<ref BR_Map> s_Maps;

	// the profiles: extension mods add theirs by overriding this (see the top of the file)
	void Register()
	{
		Add(new BR_MapChernarus());
	}

	void Add(BR_Map profile)
	{
		profile.Lobbies();
		s_Maps.Insert(profile);
	}

	// the profile of the map the server is running (a plain one if there's none for it)
	static BR_Map Current()
	{
		if (!s_Maps)
		{
			s_Maps = new array<ref BR_Map>();
			BR_Maps maps = new BR_Maps();
			maps.Register();
		}
		string world = GetGame().GetWorldName();
		world.ToLower();
		foreach (BR_Map profile : s_Maps)
		{
			if (profile.m_World == world)
				return profile;
		}
		BR_Map plain = new BR_Map();
		plain.m_World = world;
		plain.m_Title = world;
		return plain;
	}

	static int Count()
	{
		if (!s_Maps)
			return 0;
		return s_Maps.Count();
	}
}
