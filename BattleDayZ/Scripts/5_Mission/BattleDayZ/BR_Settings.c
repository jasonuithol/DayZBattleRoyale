// battleroyale.json in the mission folder. Anything missing from the file keeps the
// default below; with no file at all the defaults are used as they are.

class BR_Settings
{
	static const string FILE = "$mission:battleroyale.json";

	int minPlayers = 1;            // a round starts when the lobby countdown ends with at least this many
	int firstLobbySeconds = 300;   // after a server start, so everyone can log in
	int lobbySeconds = 180;        // between rounds (or until everyone waiting has pressed F for ready)
	int winnerSeconds = 15;        // the winner is shown this long before everyone goes back to the lobby
	float lobbyX = 1000;           // the lobby: over a spot outside the map's playable area
	float lobbyZ = 1000;
	ref array<string> lobbies;     // places copied up into the sky for it, taking turns (the next after
	                               // every round): the map's own (BR_Maps.c) or the mission's
	                               // lobbies/<name>.json (tools/make_lobby.py). Empty: all of the map's own
	float lobbyHeight = 1000;      // m above the ground (0: the lobby is on the ground, at lobbyX/lobbyZ)
	int lobbyChangeSeconds = 0;    // also change it this often while players wait (0: only between rounds)
	float startRadius = 8000;      // the first circle: all of Nasdara (its buildings cover about 13 x 14 km)
	float circleDrift = 0.5;       // how far off-centre each next circle can be: 0 always centred, 1 anywhere inside
	int lootBuildings = 6000;      // buildings inside the first circle that get loot (bigger ones get up to 4 piles)
	int cars = 100;                // vehicles, spread evenly over the whole map (none near the lobby):
	float carSquare = 1365;        // it's tiled with squares about this size (m), lined up with its
	                               // edges, that each have the same chance of one: at a vanilla car
	                               // spot in it, parked by a structure, or out in the open
	ref array<string> vehicles;    // what spawns at the car spots, one entry per vehicle, equally likely:
	                               // its colours as "A|B|C", one picked at random
	float carFuel = 0.15;          // fraction of a full tank (boats too)
	ref array<string> kitExtras;   // more things everyone gets in the lobby's kit (testing, events): class names
	int boats = 0;                 // boats each round, at the map's vanilla boat spots (if it has any)
	string boatTypes = "Boat_01_Orange|Boat_01_Blue|Boat_01_Black|Boat_01_Camo"; // one picked at random
	int supplyDropSeconds = 240;   // a supply drop this often, inside the next circle (0: none)
	float planeSpeed = 120;        // m/s: the plane crosses the first circle (about 15 km) in about 2 minutes
	float planeHeight = 400;       // m above the highest ground under its route
	// The circle repeats three steps, circle after circle:
	//   cooldown      nobody knows where the next circle is (at the start of a round no circle
	//                 is shown and the whole map is safe; after that the red circle is closing on
	//                 the white one meanwhile)
	//   reveal        the next (white) circle is drawn and announced
	//   notification  then the old white circle turns red and closes on the new white one, over
	//                 the next cooldown (the first time, from a red circle just round the map)
	// Each list is per circle, the last value repeating. A white circle smaller than
	// circleMinRadius becomes nothing: when the red has closed on it, the storm is everywhere and
	// the last one alive wins.
	ref array<int> circleSeconds;       // the cooldown, and the notification, of each circle
	ref array<float> circleFractions;   // each white circle's radius, as a fraction of the one before
	ref array<float> circleDamage;      // health per second outside the circle (players have 100)
	float circleMinRadius = 25;
	bool testStorm = false;        // testing only: no plane; everyone starts on the ground just outside
	                               // the first white circle, with the red already closing on them
	float testWhiteRadius = 0;     // testing only, with testStorm: that first white circle's radius (0: the usual)
	int testBots = 0;              // testing only: this many AI survivors join each round, inside the
	                               // first white circle, and play it out as players (they just stand there)

	// per circle, the last value repeating
	int CircleSeconds(int circle)
	{
		return circleSeconds[Math.Min(circle, circleSeconds.Count() - 1)];
	}

	float CircleFraction(int circle)
	{
		return circleFractions[Math.Min(circle, circleFractions.Count() - 1)];
	}

	float CircleDamage(int circle)
	{
		return circleDamage[Math.Min(circle, circleDamage.Count() - 1)];
	}

	static BR_Settings Load()
	{
		BR_Settings s = new BR_Settings();
		string error;
		if (FileExist(FILE))
		{
			if (!JsonFileLoader<BR_Settings>.LoadFile(FILE, s, error))
				Print("[BattleDayZ] " + FILE + ": " + error + " - using the defaults");
		}
		else
			Print("[BattleDayZ] no " + FILE + " - using the defaults");

		array<int> seconds = {300, 240, 180, 120, 60, 30};
		array<float> fractions = {0.7, 0.65, 0.6, 0.55, 0.5, 0.4, 0.5};
		array<float> damage = {1, 1, 2, 4, 6, 8, 10};
		if (!s.circleSeconds || s.circleSeconds.Count() == 0)
			s.circleSeconds = seconds;
		if (!s.circleFractions || s.circleFractions.Count() == 0)
			s.circleFractions = fractions;
		if (!s.circleDamage || s.circleDamage.Count() == 0)
			s.circleDamage = damage;
		if (!s.vehicles || s.vehicles.Count() == 0)
		{
			array<string> defaultVehicles = {
				"Motorbike_02_Red|Motorbike_02_Yellow|Motorbike_02_Blue|Motorbike_02_Green",
				"OffroadHatchback|OffroadHatchback_Blue|OffroadHatchback_White",
				"Offroad_02",
				"Expansion_Landrover|Expansion_Landrover_Yellow|Expansion_Landrover_Green|Expansion_Landrover_Red|Expansion_Landrover_Grey|Expansion_Landrover_Blue|Expansion_Landrover_Orange"};
			s.vehicles = defaultVehicles;
		}
		return s;
	}
}

// br_spots.json in the mission folder, made by tools/make_spots.py.
class BR_Proto
{
	string type;
	ref array<float> pts; // loot points in the building's model space, x y z triples
}

class BR_Building
{
	int t; // index into protos
	float x;
	float y;
	float z;
}

class BR_CarSpot
{
	float x;
	float z;
	float h;      // heading, degrees
	string types; // "A|B|C", one picked at random
}

class BR_Spots
{
	static const string FILE = "$mission:br_spots.json";

	ref array<ref BR_Proto> protos;
	ref array<ref BR_Building> buildings;
	ref array<ref BR_CarSpot> cars;
	ref array<ref BR_CarSpot> boats;

	static BR_Spots Load()
	{
		BR_Spots s = new BR_Spots();
		string error;
		if (!JsonFileLoader<BR_Spots>.LoadFile(FILE, s, error))
			Print("[BattleDayZ] " + FILE + ": " + error);
		if (!s.protos)
			s.protos = new array<ref BR_Proto>();
		if (!s.buildings)
			s.buildings = new array<ref BR_Building>();
		if (!s.cars)
			s.cars = new array<ref BR_CarSpot>();
		if (!s.boats)
			s.boats = new array<ref BR_CarSpot>();
		return s;
	}
}
