// What spawns, and where: loot in buildings, cars, supply drops, the players' starting kit.
// Everything created here is added to `spawned` so the round can delete it afterwards.

class BR_Weapon
{
	string weapon;
	string mag;
	string ammo;
	ref array<string> parts; // fitted so the gun works properly (stock, handguard)

	void BR_Weapon(string w, string m, string a)
	{
		weapon = w;
		mag = m;
		ammo = a;
		parts = new array<string>();
	}
}

class BR_Loot
{
	static const int AMMO_BOXES = 3;
	static const float VEHICLE_LIFETIME = 86400; // s: a day, longer than any round
	static const ref array<string> BELT_HOLSTERS = {"PlateCarrierHolster", "ChestHolster"};
	static const float SUPPLY_PLANE_SPEED = 200; // m/s, about a minute from the map edge
	static const float CRATE_LIFETIME = 3600;    // s; at 0 (Expansion's default) the economy deletes a landed crate once nobody is near it. Rounds clear crates anyway

	static ref array<ref BR_Weapon> s_Weapons;
	protected static ref array<string> s_Helmets;     // tier 1, 2, 3
	protected static ref array<string> s_Vests;
	protected static ref array<string> s_Backpacks;
	protected static ref array<string> s_Attachments;
	static ref array<string> s_Grenades = {"M67Grenade", "RGD5Grenade"};

	static void Init()
	{
		if (s_Weapons)
			return;
		s_Weapons = new array<ref BR_Weapon>();
		BR_Weapon w;
		// two of each calibre; a gun with no magazine ("") loads from inside (internal magazine)
		s_Weapons.Insert(new BR_Weapon("Glock19", "Mag_Glock_15Rnd", "AmmoBox_9x19_25rnd"));       // Mlock-91
		s_Weapons.Insert(new BR_Weapon("MP5K", "Mag_MP5_30Rnd", "AmmoBox_9x19_25rnd"));            // SG5-K
		s_Weapons.Insert(new BR_Weapon("FNX45", "Mag_FNX45_15Rnd", "AmmoBox_45ACP_25rnd"));        // FX-45
		s_Weapons.Insert(new BR_Weapon("UMP45", "Mag_UMP_25Rnd", "AmmoBox_45ACP_25rnd"));          // USG-45
		s_Weapons.Insert(new BR_Weapon("Magnum", "", "AmmoBox_357_20Rnd"));                        // Revolver
		s_Weapons.Insert(new BR_Weapon("Repeater", "", "AmmoBox_357_20Rnd"));                      // Repeater Carbine (lever action)
		s_Weapons.Insert(new BR_Weapon("Winchester70", "", "AmmoBox_308Win_20Rnd"));               // M70 Tundra
		s_Weapons.Insert(new BR_Weapon("M14", "Mag_M14_10Rnd", "AmmoBox_308Win_20Rnd"));           // DMR
		s_Weapons.Insert(new BR_Weapon("Izh43Shotgun", "", "AmmoBox_00buck_10rnd"));               // BK-43
		s_Weapons.Insert(new BR_Weapon("R12", "", "AmmoBox_00buck_10rnd"));                        // R12
		w = new BR_Weapon("AKM", "Mag_AKM_30Rnd", "AmmoBox_762x39_20Rnd");                         // KA-M
		w.parts.Insert("AK_WoodBttstck");
		w.parts.Insert("AK_WoodHndgrd");
		s_Weapons.Insert(w);
		s_Weapons.Insert(new BR_Weapon("SKS", "", "AmmoBox_762x39_20Rnd"));                        // SK 59/66
		s_Weapons.Insert(new BR_Weapon("M16A2", "Mag_STANAG_30Rnd", "AmmoBox_556x45_20Rnd"));      // M16-A2
		w = new BR_Weapon("AK101", "Mag_AK101_30Rnd", "AmmoBox_556x45_20Rnd");                     // KA-101
		w.parts.Insert("AK_PlasticBttstck");
		w.parts.Insert("AK_PlasticHndgrd");
		s_Weapons.Insert(w);

		s_Helmets = {"Ssh68Helmet", "BallisticHelmet_Black", "GorkaHelmet"};
		s_Vests = {"PoliceVest", "PressVest_Blue", "PlateCarrierVest"};
		s_Backpacks = {"ChildBag_Blue", "TaloonBag_Blue", "AliceBag_Green"};
		// each fits at least one of the guns (the headless test lists which)
		// sights: Mini, Baraka, ACOG, Combat Sights and the hunting scope only
		s_Attachments = {"FNP45_MRDSOptic", "ReflexOptic", "ACOGOptic", "M68Optic", "HuntingOptic", "PistolSuppressor", "M4_Suppressor", "AK_Suppressor", "Mag_STANAG_60Rnd", "Mag_AKM_Drum75Rnd", "Mag_M14_20Rnd"};
	}

	// every class name the loot uses, for the startup check
	static array<string> AllTypes()
	{
		Init();
		array<string> all = new array<string>();
		for (int i = 0; i < s_Weapons.Count(); i++)
		{
			all.Insert(s_Weapons[i].weapon);
			all.Insert(s_Weapons[i].mag);
			all.Insert(s_Weapons[i].ammo);
			all.InsertAll(s_Weapons[i].parts);
		}
		all.InsertAll(s_Helmets);
		all.InsertAll(s_Vests);
		all.InsertAll(s_Backpacks);
		all.InsertAll(s_Attachments);
		all.Insert("BandageDressing");
		all.Insert("Morphine");
		all.Insert("Epinephrine");
		all.Insert("AntiChemInjector");
		all.Insert("CanisterGasoline");
		all.InsertAll(s_Grenades);
		all.Insert("Molotov");
		all.Insert("Matchbox");
		return all;
	}

	static bool TypeExists(string type)
	{
		return GetGame().ConfigIsExisting("CfgVehicles " + type) || GetGame().ConfigIsExisting("CfgWeapons " + type) || GetGame().ConfigIsExisting("CfgMagazines " + type);
	}

	// --- items -----------------------------------------------------------------------

	static EntityAI Item(string type, vector pos, array<Object> spawned)
	{
		EntityAI item = EntityAI.Cast(GetGame().CreateObjectEx(type, pos, ECE_SETUP | ECE_UPDATEPATHGRAPH | ECE_CREATEPHYSICS));
		if (!item)
		{
			Print("[BattleDayZ] could not create " + type);
			return null;
		}
		item.SetOrientation(Vector(Math.RandomFloat(0, 360), 0, 0));
		spawned.Insert(item);
		return item;
	}

	// a loaded gun (a full magazine in it, or its internal magazine full) and its ammo boxes beside it
	static void WeaponPile(BR_Weapon w, vector pos, array<Object> spawned)
	{
		Weapon_Base gun = Weapon_Base.Cast(Item(w.weapon, pos, spawned));
		if (!gun)
			return;
		for (int p = 0; p < w.parts.Count(); p++)
			gun.GetInventory().CreateAttachment(w.parts[p]);
		if (w.mag != "")
			gun.SpawnAttachedMagazine(w.mag);
		else
		{
			gun.SpawnAmmo("", WeaponWithAmmoFlags.CHAMBER);
			gun.FillChamber("", WeaponWithAmmoFlags.CHAMBER); // every chamber: the revolver's six, both of the BK-43's barrels
		}
		for (int b = 0; b < AMMO_BOXES; b++)
		{
			vector offset = Vector(Math.RandomFloat(-0.4, 0.4), 0.05, Math.RandomFloat(-0.4, 0.4));
			Item(w.ammo, pos + offset, spawned);
		}
	}

	// a sight (or anything else) that runs on a 9V battery comes with one fitted; a no-op for
	// everything without a battery slot
	static void WithBattery(EntityAI item)
	{
		if (item && item.GetInventory().GetAttachmentSlotsCount() > 0)
			item.GetInventory().CreateAttachment("Battery9V");
	}

	// 1, 2 or 3 at 60 / 30 / 10 percent
	static string Tiered(array<string> tiers)
	{
		float roll = Math.RandomFloat01();
		if (roll < 0.6)
			return tiers[0];
		if (roll < 0.9)
			return tiers[1];
		return tiers[2];
	}

	static void Pile(vector pos, array<Object> spawned)
	{
		Init();
		int roll = Math.RandomInt(0, 100);
		if (roll < 25)
			WeaponPile(s_Weapons.GetRandomElement(), pos, spawned);
		else if (roll < 35)
			Item(Tiered(s_Helmets), pos, spawned);
		else if (roll < 45)
			Item(Tiered(s_Vests), pos, spawned);
		else if (roll < 55)
			Item(Tiered(s_Backpacks), pos, spawned);
		else if (roll < 65)
			WithBattery(Item(s_Attachments.GetRandomElement(), pos, spawned));
		else if (roll < 71)
			Item(s_Grenades.GetRandomElement(), pos, spawned);
		else if (roll < 77)
		{
			// a molotov (the Molotov mod), and something to light it with
			Item("Molotov", pos, spawned);
			Item("Matchbox", pos, spawned);
		}
		else if (roll < 91)
		{
			// medical: a bandage half the time, else morphine, epinephrine (adrenaline, which
			// heals fully: BR_Medical.c) or the PO-X antidote (gas poisoning)
			int med = Math.RandomInt(0, 20);
			if (med < 10)
				Item("BandageDressing", pos, spawned);
			else if (med < 14)
				Item("Morphine", pos, spawned);
			else if (med < 17)
				Item("Epinephrine", pos, spawned);
			else
				Item("AntiChemInjector", pos, spawned);
		}
		else
		{
			ItemBase can = ItemBase.Cast(Item("CanisterGasoline", pos, spawned));
			if (can)
				can.SetQuantity(can.GetQuantityMax() * 0.5);
		}
	}

	// Loot in up to `count` buildings within radius of center: one pile, two in bigger buildings.
	// every building inside the circle, in random order: the lobby works through them a few
	// at a time (BR_Round.LootStep) so the server never stalls
	static array<int> BuildingsInside(BR_Spots spots, vector center, float radius)
	{
		array<int> inside = new array<int>();
		for (int i = 0; i < spots.buildings.Count(); i++)
		{
			BR_Building b = spots.buildings[i];
			if (Distance2D(Vector(b.x, 0, b.z), center) < radius)
				inside.Insert(i);
		}
		// shuffle
		for (int k = inside.Count() - 1; k > 0; k--)
		{
			int swap = Math.RandomInt(0, k + 1);
			int keep = inside[k];
			inside[k] = inside[swap];
			inside[swap] = keep;
		}
		return inside;
	}

	// loot in one building: one pile per 5 loot points, at least 1 and at most 4, each on a
	// different point. False if the building isn't there (destroyed, or the map changed).
	static bool LootBuilding(BR_Spots spots, int index, array<Object> spawned)
	{
		BR_Building building = spots.buildings[index];
		BR_Proto kind = spots.protos[building.t];
		Object obj = FindBuilding(kind.type, Vector(building.x, building.y, building.z));
		if (!obj)
			return false;

		int points = kind.pts.Count() / 3;
		int piles = Math.Clamp(1 + points / 5, 1, 4);
		array<int> free = new array<int>();
		for (int f = 0; f < points; f++)
			free.Insert(f);
		for (int n = 0; n < piles && free.Count() > 0; n++)
		{
			int at = Math.RandomInt(0, free.Count());
			int p = free[at];
			free.Remove(at);
			vector point = Vector(kind.pts[p * 3], kind.pts[p * 3 + 1], kind.pts[p * 3 + 2]);
			Pile(OnFloor(obj.ModelToWorld(point)), spawned);
		}
		return true;
	}

	// all at once (the headless test)
	static int Buildings(BR_Spots spots, vector center, float radius, int count, array<Object> spawned)
	{
		array<int> inside = BuildingsInside(spots, center, radius);
		int done = 0;
		for (int i = 0; i < inside.Count() && done < count; i++)
		{
			if (LootBuilding(spots, inside[i], spawned))
				done++;
		}
		return done;
	}

	static Object FindBuilding(string type, vector pos)
	{
		array<Object> objects = new array<Object>();
		GetGame().GetObjectsAtPosition3D(pos, 1.5, objects, null);
		for (int i = 0; i < objects.Count(); i++)
		{
			if (objects[i].GetType() == type)
				return objects[i];
		}
		return null;
	}

	// the floor (or ground) at or just below a loot point
	static vector OnFloor(vector pos)
	{
		vector hit, normal;
		int component;
		if (DayZPhysics.RaycastRV(pos + "0 0.5 0", pos - "0 1.0 0", hit, normal, component, null, null, null, false, false, ObjIntersectGeom))
			return hit + "0 0.05 0";
		return pos + "0 0.05 0";
	}

	static float Distance2D(vector a, vector b)
	{
		return vector.Distance(Vector(a[0], 0, a[2]), Vector(b[0], 0, b[2]));
	}

	// --- cars ------------------------------------------------------------------------

	// Ready-to-drive vehicles at vanilla car spawn points inside the circle, with a little fuel:
	// each one of `vehicles` (an entry per vehicle, its colours "A|B|C"), equally likely.
	// Where the round's vehicles go, spread evenly, so every part of the map has the same
	// chance of one: Nasdara's vanilla car spots bunch up in the towns of the centre and west
	// (7 of 147 in the whole eastern quarter). The map is tiled with squares of about `square`
	// m, lined up with its edges (1365 m makes 12 x 12, so every 4 km quarter-of-a-quarter holds
	// 3 x 3 of them), and those reaching into the circle are shuffled and dealt the vehicles one
	// at a time, round and round (CarSpotIn). No spot is outside the circle or within AVOID m of
	// `avoid` (the lobby). Each spot is x, ground y, z, and its heading in out_h.
	static const float AVOID = 1000;

	static int CarSpots(BR_Spots spots, vector center, float radius, int count, float square, array<vector> out_pos, array<float> out_h, vector avoid = "0 0 0")
	{
		s_Avoid = avoid;
		array<bool> used = new array<bool>();
		used.Resize(spots.cars.Count());
		for (int u = 0; u < used.Count(); u++)
			used[u] = false;

		// the squares reaching into the circle, in random order
		float world = GetGame().GetWorld().GetWorldSize();
		// a whole number of squares to each quarter (4 x 4 of them), so every quarter has as
		// many and gets as many vehicles: 12 on Nasdara and Chernarus, 8 on Livonia
		int n = Math.Max(4, Math.Round(world / square / 4) * 4);
		square = world / n;
		array<vector> squares = new array<vector>();
		for (int i = 0; i < n; i++)
		{
			for (int j = 0; j < n; j++)
			{
				vector sq = Vector(i * square, 0, j * square); // its south-west corner
				// the square's nearest point to the centre
				vector near = Vector(Math.Clamp(center[0], sq[0], sq[0] + square), 0, Math.Clamp(center[2], sq[2], sq[2] + square));
				if (Distance2D(near, center) < radius)
					squares.Insert(sq);
			}
		}
		for (int s = squares.Count() - 1; s > 0; s--)
			squares.SwapItems(s, Math.RandomInt(0, s + 1));
		squares = Interleaved(squares, world / 4);
		s_Squares = squares.Count();

		// round and round, until there are enough or a whole pass finds nowhere to put one
		bool any = true;
		bool first = true;
		while (out_pos.Count() < count && any && squares.Count() > 0)
		{
			any = false;
			foreach (vector c : squares)
			{
				if (out_pos.Count() >= count)
					break;
				if (CarSpotIn(spots, used, c[0], c[2], c[0] + square, c[2] + square, center, radius, out_pos, out_h))
					any = true;
				else if (first)
					s_EmptySquares++;
			}
			first = false;
		}
		return out_pos.Count();
	}

	// The (shuffled) squares dealt round the map's 4 x 4 quarters in turn - one from each, then
	// a second from each, and so on - so every quarter gets its share of the vehicles (6 or 7
	// of 100), not just the same chance of them.
	protected static array<vector> Interleaved(array<vector> squares, float block)
	{
		array<ref array<vector>> byBlock = new array<ref array<vector>>();
		for (int b = 0; b < 16; b++)
			byBlock.Insert(new array<vector>());
		foreach (vector sq : squares)
		{
			int bx = Math.Clamp(Math.Floor((sq[0] + 1) / block), 0, 3);
			int bz = Math.Clamp(Math.Floor((sq[2] + 1) / block), 0, 3);
			byBlock[bx * 4 + bz].Insert(sq);
		}
		for (int k = byBlock.Count() - 1; k > 0; k--)
			byBlock.SwapItems(k, Math.RandomInt(0, k + 1));
		array<vector> dealt = new array<vector>();
		bool more = true;
		for (int round = 0; more; round++)
		{
			more = false;
			foreach (array<vector> inBlock : byBlock)
			{
				if (round < inBlock.Count())
				{
					dealt.Insert(inBlock[round]);
					more = true;
				}
			}
		}
		return dealt;
	}

	// One spot in the box x0..x1, z0..z1 (and the circle), picked the way a car might have been
	// left there: half the time a free vanilla car spot (by a road, in a town) if the box has one;
	// otherwise parked by whatever structure is about (a house, a hangar, a solar farm, an oil
	// derrick, a fort, a wreck: StructureNear); otherwise out in the open.
	protected static bool CarSpotIn(BR_Spots spots, array<bool> used, float x0, float z0, float x1, float z1, vector center, float radius, array<vector> out_pos, array<float> out_h)
	{
		array<int> vanilla = new array<int>();
		for (int i = 0; i < spots.cars.Count(); i++)
		{
			BR_CarSpot cs = spots.cars[i];
			if (!used[i] && cs.x >= x0 && cs.x < x1 && cs.z >= z0 && cs.z < z1 && Distance2D(Vector(cs.x, 0, cs.z), center) < radius && (s_Avoid == vector.Zero || Distance2D(Vector(cs.x, 0, cs.z), s_Avoid) >= AVOID))
				vanilla.Insert(i);
		}
		if (vanilla.Count() > 0 && Math.RandomInt(0, 2) == 0)
		{
			TakeVanilla(spots, used, vanilla.GetRandomElement(), out_pos, out_h);
			s_Kinds[0] = s_Kinds[0] + 1;
			return true;
		}

		vector pos;
		for (int look = 0; look < 8; look++)
		{
			Object by = StructureNear(Math.RandomFloat(x0, x1), Math.RandomFloat(z0, z1));
			if (!by)
				continue;
			vector bp = by.GetPosition();
			vector box[2];
			float reach = Math.Min(by.ClippingInfo(box), 40); // its size: park just outside it
			for (int t = 0; t < 6; t++)
			{
				float a = Math.RandomFloat(0, Math.PI2);
				float d = reach + Math.RandomFloat(3, 12);
				float x = bp[0] + Math.Cos(a) * d;
				float z = bp[2] + Math.Sin(a) * d;
				if (x < x0 || x >= x1 || z < z0 || z >= z1 || Distance2D(Vector(x, 0, z), center) >= radius)
					continue;
				if (FreeSpot(x, z, out_pos, pos))
				{
					out_pos.Insert(pos);
					out_h.Insert(Math.RandomFloat(0, 360));
					s_Kinds[1] = s_Kinds[1] + 1;
					if (s_By.Count() < 40)
						s_By.Insert(ShapeOf(by));
					return true;
				}
			}
		}

		for (int o = 0; o < 20; o++)
		{
			float ox = Math.RandomFloat(x0, x1);
			float oz = Math.RandomFloat(z0, z1);
			if (Distance2D(Vector(ox, 0, oz), center) < radius && FreeSpot(ox, oz, out_pos, pos))
			{
				out_pos.Insert(pos);
				out_h.Insert(Math.RandomFloat(0, 360));
				s_Kinds[2] = s_Kinds[2] + 1;
				return true;
			}
		}

		if (vanilla.Count() > 0)
		{
			TakeVanilla(spots, used, vanilla.GetRandomElement(), out_pos, out_h);
			s_Kinds[0] = s_Kinds[0] + 1;
			return true;
		}
		return false;
	}

	protected static void TakeVanilla(BR_Spots spots, array<bool> used, int pick, array<vector> out_pos, array<float> out_h)
	{
		used[pick] = true;
		BR_CarSpot v = spots.cars[pick];
		out_pos.Insert(Vector(v.x, GetGame().SurfaceY(v.x, v.z), v.z));
		out_h.Insert(v.h);
	}

	// a structure within 100 m of x, z, picked at random: a feature a car might be left at (a
	// house, a hangar, a solar farm, an oil derrick, a fort, a wreck, a ruin, a station), many
	// of them without a class of their own, at least shed-sized. Not walls, fences or road
	// pieces, which are everywhere and would win nearly every pick, nor rocks, plants, power
	// lines, pylons or rubble.
	static Object StructureNear(float x, float z)
	{
		array<Object> found = new array<Object>();
		GetGame().GetObjectsAtPosition(Vector(x, GetGame().SurfaceY(x, z), z), 100, found, null);
		array<Object> built = new array<Object>();
		foreach (Object o : found)
		{
			if (IsStructure(o))
				built.Insert(o);
		}
		if (built.Count() == 0)
			return null;
		return built.GetRandomElement();
	}

	static ref array<string> FEATURE_FOLDERS = {"\\residential\\", "\\industrial\\", "\\military\\", "\\specific\\", "\\wrecks\\", "\\ruins\\", "\\rail\\"};
	static ref array<string> NOT_FEATURES = {"\\forest\\", "power_wire", "rubble", "dirtpile", "\\walls\\", "\\roads\\", "fence", "_wall", "power_vhv", "power_hv", "power_wire", "irrigationtunnel", "shellcrater", "barbedwire", "misc_timbers"};

	static bool IsStructure(Object o)
	{
		if (!o || o.IsTree() || o.IsBush() || o.IsRock())
			return false;
		if (o.IsInherited(ItemBase) || o.IsInherited(Transport) || o.IsInherited(Man))
			return false;
		string shape = ShapeOf(o);
		if (shape.IndexOf("structures") < 0)
			return false;
		bool feature = false;
		foreach (string folder : FEATURE_FOLDERS)
		{
			if (shape.IndexOf(folder) >= 0)
				feature = true;
		}
		if (!feature)
			return false;
		foreach (string clutter : NOT_FEATURES)
		{
			if (shape.IndexOf(clutter) >= 0)
				return false;
		}
		vector box[2];
		return o.ClippingInfo(box) >= 3;
	}

	static string ShapeOf(Object o)
	{
		string shape = o.GetShapeName();
		shape.ToLower();
		return shape;
	}

	static ref array<int> s_Kinds = {0, 0, 0}; // diagnostics: at a vanilla spot, by a structure, in the open
	static ref array<string> s_By = new array<string>();

	static ref array<int> s_Why = {0, 0, 0, 0, 0, 0}; // diagnostics: edge, water, slope, bumpy, near a car, objects
	static int s_Squares;
	static int s_EmptySquares;

	protected static vector s_Avoid;

	static bool FreeSpot(float x, float z, array<vector> taken, out vector pos)
	{
		float edge = GetGame().GetWorld().GetWorldSize() - 50;
		if (x < 50 || z < 50 || x > edge || z > edge || (s_Avoid != vector.Zero && Distance2D(Vector(x, 0, z), s_Avoid) < AVOID))
		{
			s_Why[0] = s_Why[0] + 1;
			return false;
		}
		if (GetGame().SurfaceIsSea(x, z) || GetGame().SurfaceIsPond(x, z))
		{
			s_Why[1] = s_Why[1] + 1;
			return false;
		}
		vector normal = GetGame().SurfaceGetNormal(x, z);
		if (normal[1] < 0.94) // steeper than about 20 degrees
		{
			s_Why[2] = s_Why[2] + 1;
			return false;
		}
		float y = GetGame().SurfaceY(x, z);
		array<float> dx = {-2.5, 2.5, -2.5, 2.5}; // the corners of a 5 x 5 m patch
		array<float> dz = {-2.5, -2.5, 2.5, 2.5};
		for (int c = 0; c < 4; c++)
		{
			if (Math.AbsFloat(GetGame().SurfaceY(x + dx[c], z + dz[c]) - y) > 1.2) // a hump or a ditch
			{
				s_Why[3] = s_Why[3] + 1;
				return false;
			}
		}
		pos = Vector(x, y, z);
		foreach (vector other : taken)
		{
			if (Distance2D(other, pos) < 15)
			{
				s_Why[4] = s_Why[4] + 1;
				return false;
			}
		}
		array<Object> objects = new array<Object>();
		GetGame().GetObjectsAtPosition(pos, 6, objects, null);
		if (objects.Count() > 0)
		{
			s_Why[5] = s_Why[5] + 1;
			return false;
		}
		return true;
	}

	static int Cars(BR_Spots spots, vector center, float radius, int count, float fuel, array<string> vehicles, array<Object> spawned, float square = 1365, vector avoid = "0 0 0")
	{
		array<vector> where = new array<vector>();
		array<float> headings = new array<float>();
		CarSpots(spots, center, radius, count, square, where, headings, avoid);

		int done = 0;
		for (int i = 0; i < where.Count(); i++)
		{
			array<string> colours = new array<string>();
			vehicles.GetRandomElement().Split("|", colours);
			vector pos = where[i] + "0 0.3 0"; // a hair below ground and it falls through the map
			EntityAI vehicle = Vehicle(colours.GetRandomElement(), pos, headings[i], fuel);
			if (!vehicle)
				continue;
			Print("[BattleDayZ] vehicle " + vehicle.GetType() + " at " + vehicle.GetPosition());
			spawned.Insert(vehicle);
			done++;
		}
		return done;
	}

	// one vehicle with every part fitted, every fluid full except the tank, which gets `fuel`
	static EntityAI Vehicle(string type, vector pos, float heading, float fuel)
	{
		EntityAI vehicle = EntityAI.Cast(GetGame().CreateObjectEx(type, pos, ECE_PLACE_ON_SURFACE | ECE_SETUP));
		if (!vehicle)
		{
			Print("[BattleDayZ] could not create the vehicle " + type);
			return null;
		}
		vehicle.SetOrientation(Vector(heading, 0, 0));
		CarScript car = CarScript.Cast(vehicle);
		MotorbikeScript bike = MotorbikeScript.Cast(vehicle);
		if (car)
		{
			CarScript.s_BR_Fuel = fuel;
			car.OnDebugSpawn(); // see BR_CarScript.c
			CarScript.s_BR_Fuel = -1;
			// Expansion's vehicles (the Land Rover) fill their tank their own way
			if (Math.AbsFloat(car.GetFluidFraction(CarFluid.FUEL) - fuel) > 0.02)
			{
				car.LeakAll(CarFluid.FUEL);
				car.Fill(CarFluid.FUEL, car.GetFluidCapacity(CarFluid.FUEL) * fuel);
			}
		}
		else
		{
			vehicle.OnDebugSpawn();
			if (bike)
			{
				// vanilla's own way to a part-filled tank (MotorbikeScript): empty it, then fill
				bike.LeakAll(MotorbikeFluid.FUEL);
				bike.Fill(MotorbikeFluid.FUEL, bike.GetFluidCapacity(MotorbikeFluid.FUEL) * fuel);
			}
			BoatScript boat = BoatScript.Cast(vehicle);
			if (boat)
			{
				boat.LeakAll(BoatFluid.FUEL);
				boat.Fill(BoatFluid.FUEL, boat.GetFluidCapacity(BoatFluid.FUEL) * fuel);
			}
		}
		// last, after the parts and fluids (a car's setup puts its economy lifetime back): the
		// economy's own lifetime would end it mid-round. Nasdara's types.xml gives the Ada and
		// some motorbike colours 3 s (the economy deleted them ~35 s after they spawned) and the
		// M1025 30 minutes. The round deletes its vehicles itself.
		vehicle.SetLifetime(VEHICLE_LIFETIME);
		return vehicle;
	}

	// Boats at the map's vanilla boat spots, as many as asked for (as many as there are spots
	// at most), afloat: at the water's surface, a spark plug in (OnDebugSpawn), part of a tank.
	static int Boats(BR_Spots spots, int count, float fuel, string types, array<Object> spawned)
	{
		if (count <= 0 || spots.boats.Count() == 0)
			return 0;
		array<string> colours = new array<string>();
		types.Split("|", colours);
		array<int> order = new array<int>();
		for (int i = 0; i < spots.boats.Count(); i++)
			order.Insert(i);
		order.ShuffleArray();
		int made = 0;
		for (int k = 0; k < order.Count() && made < count; k++)
		{
			BR_CarSpot spot = spots.boats[order[k]];
			vector pos = Vector(spot.x, GetGame().SurfaceY(spot.x, spot.z), spot.z);
			float depth = GetGame().GetWaterDepth(pos);
			if (depth > 0)
				pos[1] = pos[1] + depth;
			EntityAI boat = Vehicle(colours.GetRandomElement(), pos, spot.h, fuel);
			if (!boat)
				continue;
			spawned.Insert(boat);
			made++;
			Print("[BattleDayZ] boat " + boat.GetType() + " at " + boat.GetPosition());
		}
		return made;
	}

	// --- supply drops (Expansion's airdrop plane) -------------------------------------

	static bool SupplyDrop(vector pos)
	{
		array<ref ExpansionLoot> loot = new array<ref ExpansionLoot>();
		loot.Insert(new ExpansionLoot("M16A2", Parts("Mag_STANAG_60Rnd|M4_Suppressor"), 1));
		loot.Insert(new ExpansionLoot("M14", Parts("Mag_M14_20Rnd|ACOGOptic"), 1));
		loot.Insert(new ExpansionLoot("AmmoBox_556x45_20Rnd", null, 1));
		loot.Insert(new ExpansionLoot("AmmoBox_308Win_20Rnd", null, 1));
		loot.Insert(new ExpansionLoot("M67Grenade", null, 1));
		loot.Insert(new ExpansionLoot("GorkaHelmet", null, 1));
		loot.Insert(new ExpansionLoot("PlateCarrierVest", null, 1));
		loot.Insert(new ExpansionLoot("AliceBag_Green", null, 1));
		loot.Insert(new ExpansionLoot("Morphine", null, 1));
		loot.Insert(new ExpansionLoot("Epinephrine", null, 1));

		ExpansionLootContainer container = new ExpansionLootContainer("ExpansionAirdropContainer_Military", 2, 1, loot, new TStringArray(), 8, 0, false, 4.5);
		// Expansion's plane starts at the map edge; at its usual 35 m/s it takes ~7 minutes to
		// arrive, longer than a circle phase. Fly in fast, then slow down over the drop as usual.
		auto air = GetExpansionSettings().GetAirdrop();
		float speed = Math.Max(air.Speed, SUPPLY_PLANE_SPEED);
		StringLocaliser closing = new StringLocaliser("A supply drop is almost there.");
		StringLocaliser dropped = new StringLocaliser("Supply drop is down.");
		ExpansionAirdropPlaneBase plane = ExpansionAirdropPlaneBase.Expansion_CreatePlane(Vector(pos[0], 0, pos[2]), "Supply drop", 0, air.Height, air.DropZoneHeight, speed, air.DropZoneSpeed, container, closing, dropped, CRATE_LIFETIME);
		return plane != null;
	}

	// "A|B|C" -> attachments for an Expansion loot entry, each always fitted
	static array<ref ExpansionLootVariant> Parts(string names)
	{
		array<string> list = new array<string>();
		names.Split("|", list);
		array<ref ExpansionLootVariant> parts = new array<ref ExpansionLootVariant>();
		for (int i = 0; i < list.Count(); i++)
			parts.Insert(new ExpansionLootVariant(list[i], null, 1));
		return parts;
	}

	// delete every airdrop crate (between rounds)
	static int ClearSupplyDrops()
	{
		// planes still on their way would drop into the next round
		map<int, ExpansionAirdropPlaneBase> planes = ExpansionAirdropPlaneBase.s_Expansion_AirdropPlanes;
		array<ExpansionAirdropPlaneBase> flying = new array<ExpansionAirdropPlaneBase>();
		for (int k = 0; k < planes.Count(); k++)
		{
			if (planes.GetElement(k))
				flying.Insert(planes.GetElement(k));
		}
		for (int j = 0; j < flying.Count(); j++)
			flying[j].Expansion_RemovePlane();

		array<ExpansionAirdropContainerBase> all = ExpansionAirdropContainerBase.s_BR_Crates;
		int deleted = 0;
		for (int i = 0; i < all.Count(); i++)
		{
			if (all[i])
			{
				GetGame().ObjectDelete(all[i]);
				deleted++;
			}
		}
		all.Clear();
		return deleted;
	}

	// --- players ---------------------------------------------------------------------

	// stripped, healed, fed, dressed in the same plain clothes as everyone else
	// everything the player carries, deepest first (a magazine before its gun, a backpack's
	// contents before the backpack). Vanilla RemoveAllItems goes parent first, so on a player
	// who kept their gear (a winner, or someone saved mid-round) it trips over items that went
	// with their container: "LocalDestroyEntity: No inventory location".
	static void Strip(PlayerBase player)
	{
		array<EntityAI> items = new array<EntityAI>();
		player.GetInventory().EnumerateInventory(InventoryTraversalType.POSTORDER, items);
		for (int i = 0; i < items.Count(); i++)
		{
			EntityAI item = items[i];
			if (!item || item == player)
				continue;
			InventoryLocation where = new InventoryLocation();
			if (item.GetInventory().GetCurrentInventoryLocation(where) && where.IsValid())
				player.GetInventory().LocalDestroyEntity(item);
			else
				GetGame().ObjectDelete(item);
		}
		// the hands don't always let go of a held weapon that way
		EntityAI held = player.GetHumanInventory().GetEntityInHands();
		if (held)
			GetGame().ObjectDelete(held);
	}

	// a fresh start: stripped, fully healed (every limb, broken legs, bleeding, diseases, food
	// and water) and given the plain kit: T-shirt, jeans, trainers, a belt with a pistol holster
	// and a bandage
	static void Kit(PlayerBase player)
	{
		Strip(player);
		BR_Heal.Full(player);
		player.GetStatEnergy().Set(player.GetStatEnergy().GetMax());
		player.GetStatWater().Set(player.GetStatWater().GetMax());
		if (player.GetBleedingManagerServer())
			player.GetBleedingManagerServer().RemoveAllSources();
		player.RemoveAllAgents();
		if (player.GetBrokenLegs() != eBrokenLegs.NO_BROKEN_LEGS)
			player.SetBrokenLegs(eBrokenLegs.NO_BROKEN_LEGS);
		player.GetInventory().CreateInInventory("TShirt_Grey");
		player.GetInventory().CreateInInventory("Jeans_Blue");
		player.GetInventory().CreateInInventory("AthleticShoes_Black");
		player.GetInventory().CreateInInventory("WorkingGloves_Black");
		// a belt with a pistol holster on it: the first of these the belt takes
		EntityAI belt = player.GetInventory().CreateAttachment("MilitaryBelt");
		if (belt)
		{
			foreach (string holster : BELT_HOLSTERS)
			{
				if (belt.GetInventory().CreateAttachment(holster))
					break;
			}
		}
		player.GetInventory().CreateInInventory("BandageDressing");
		// the settings' kitExtras: "Class", or a gun with its magazine as "Gun|Magazine" (loaded)
		foreach (string extra : s_KitExtras)
		{
			array<string> parts = new array<string>();
			extra.Split("|", parts);
			EntityAI thing = player.GetInventory().CreateInInventory(parts[0]);
			Weapon_Base gun = Weapon_Base.Cast(thing);
			if (gun && parts.Count() > 1)
				gun.SpawnAttachedMagazine(parts[1]);
		}
	}

	static ref array<string> s_KitExtras = new array<string>();
}
