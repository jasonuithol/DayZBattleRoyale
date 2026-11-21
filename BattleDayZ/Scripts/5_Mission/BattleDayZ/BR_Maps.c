// What's particular to each map, kept apart from the rules of the game: a profile per map,
// picked by the world's name when the server starts. The mod has Chernarus, Livonia and
// Nasdara's (below); another map gets one by adding a BR_Map subclass to Register, here or
// from a small extension mod of its own with
//
//   modded class BR_Maps
//   {
//       override void Register()
//       {
//           super.Register();
//           Add(new BR_MapSomewhere());
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
	ref array<vector> m_Islands = new array<vector>(); // a point in the middle of each island (y 0)
	ref array<string> m_IslandNames = new array<string>();

	// the map's lobbies, in the order they take turns
	void Lobbies()
	{
	}

	void AddLobby(BR_LobbySpec spec)
	{
		m_Lobbies.Insert(spec);
	}

	// the map's islands: each gets boat spots round its shore (BR_Round.IslandBoatSpots), as
	// the map's own boat spots (cfgeventspawns) are all on the mainland
	void Islands()
	{
	}

	void AddIsland(string name, float x, float z)
	{
		m_IslandNames.Insert(name);
		m_Islands.Insert(Vector(x, 0, z));
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

	override void Islands()
	{
		AddIsland("Prison Island", 2700, 1300);
		AddIsland("Skalisty Island", 13600, 3050);
	}
}

// Livonia's world is called enoch
class BR_MapLivonia: BR_Map
{
	void BR_MapLivonia()
	{
		m_World = "enoch";
		m_Title = "Livonia";
	}

	override void Lobbies()
	{
		AddLobby(new BR_LobbySpec("branzow_castle", "Branzow castle", 1060, 11400, 60));
		AddLobby(new BR_LobbySpec("brena_square", "Brena square", 6536, 11265, 45));      // round the monument
		AddLobby(new BR_LobbySpec("lukow_airfield", "Lukow airfield", 4225, 10408, 50));  // round the control tower
	}
}

class BR_MapNasdara: BR_Map
{
	void BR_MapNasdara()
	{
		m_World = "nasdara";
		m_Title = "Nasdara";
	}

	override void Lobbies()
	{
		AddLobby(new BR_LobbySpec("qalat_e_lut", "Qalat-e-Lut", 14837, 7853, 45, 3));     // the caravanserai, on ground 3 m above the dip at its middle
		AddLobby(new BR_LobbySpec("airport_junkyard", "the airport junkyard", 12290, 11350, 80));
		AddLobby(new BR_LobbySpec("kirov_dam", "Kirov dam", 3560, 14440, 60));
	}
}

class BR_Maps
{
	protected static ref array<ref BR_Map> s_Maps;

	// the profiles: an extension mod can add one by overriding this (see the top of the file)
	void Register()
	{
		Add(new BR_MapChernarus());
		Add(new BR_MapLivonia());
		Add(new BR_MapNasdara());
	}

	void Add(BR_Map profile)
	{
		profile.Lobbies();
		profile.Islands();
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
