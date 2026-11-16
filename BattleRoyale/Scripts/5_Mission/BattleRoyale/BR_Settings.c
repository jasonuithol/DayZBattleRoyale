// battleroyale.json in the mission folder. Anything missing from the file keeps the
// default below; with no file at all the defaults are used as they are.

class BR_ZonePhase
{
	int waitSeconds;       // the circle sits still (the next one is already on the map)
	int shrinkSeconds;     // then closes in on the next one over this long
	float radiusFraction;  // the next circle's radius as a fraction of the current one (0 closes it completely)
	float damagePerSecond; // health lost per second outside the circle while this phase lasts (players have 100)

	void BR_ZonePhase(int wait = 60, int shrink = 60, float fraction = 0.5, float damage = 1)
	{
		waitSeconds = wait;
		shrinkSeconds = shrink;
		radiusFraction = fraction;
		damagePerSecond = damage;
	}
}

class BR_Settings
{
	static const string FILE = "$mission:battleroyale.json";

	int minPlayers = 1;            // a round starts when the lobby countdown ends with at least this many
	int firstLobbySeconds = 300;   // after a server start, so everyone can log in
	int lobbySeconds = 60;         // between rounds
	int winnerSeconds = 15;        // the winner is shown this long before everyone goes back to the lobby
	float lobbyX = 1000;           // the lobby: open ground outside the map's playable area
	float lobbyZ = 1000;
	float startRadius = 8000;      // the first circle: all of Nasdara (its buildings cover about 13 x 14 km)
	float circleDrift = 0.5;       // how far off-centre each next circle can be: 0 always centred, 1 anywhere inside
	int lootBuildings = 6000;      // buildings inside the first circle that get loot (bigger ones get up to 4 piles)
	int cars = 40;                 // cars at vanilla car spawn points inside the first circle
	float carFuel = 0.15;          // fraction of a full tank
	int supplyDropSeconds = 240;   // a supply drop this often, inside the next circle (0: none)
	float planeSpeed = 120;        // m/s: the plane crosses the first circle (about 15 km) in about 2 minutes
	float planeHeight = 400;       // m above the highest ground under its route
	ref array<ref BR_ZonePhase> phases;

	static BR_Settings Load()
	{
		BR_Settings s = new BR_Settings();
		string error;
		if (FileExist(FILE))
		{
			if (!JsonFileLoader<BR_Settings>.LoadFile(FILE, s, error))
				Print("[BattleRoyale] " + FILE + ": " + error + " - using the defaults");
		}
		else
			Print("[BattleRoyale] no " + FILE + " - using the defaults");

		if (!s.phases || s.phases.Count() == 0)
		{
			s.phases = new array<ref BR_ZonePhase>();
			s.phases.Insert(new BR_ZonePhase(300, 210, 0.7, 1)); // nothing shown for the first 5 minutes
			s.phases.Insert(new BR_ZonePhase(180, 180, 0.65, 1));
			s.phases.Insert(new BR_ZonePhase(150, 150, 0.6, 2));
			s.phases.Insert(new BR_ZonePhase(120, 90, 0.55, 4));
			s.phases.Insert(new BR_ZonePhase(75, 60, 0.5, 6));
			s.phases.Insert(new BR_ZonePhase(60, 45, 0.4, 8));
			s.phases.Insert(new BR_ZonePhase(45, 45, 0.0, 10));
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

	static BR_Spots Load()
	{
		BR_Spots s = new BR_Spots();
		string error;
		if (!JsonFileLoader<BR_Spots>.LoadFile(FILE, s, error))
			Print("[BattleRoyale] " + FILE + ": " + error);
		if (!s.protos)
			s.protos = new array<ref BR_Proto>();
		if (!s.buildings)
			s.buildings = new array<ref BR_Building>();
		if (!s.cars)
			s.cars = new array<ref BR_CarSpot>();
		return s;
	}
}
