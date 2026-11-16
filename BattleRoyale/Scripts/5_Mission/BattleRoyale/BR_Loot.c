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
	static const float SUPPLY_PLANE_SPEED = 200; // m/s, about a minute from the map edge
	static const float CRATE_LIFETIME = 3600;    // s; at 0 (Expansion's default) the economy deletes a landed crate once nobody is near it. Rounds clear crates anyway

	protected static ref array<ref BR_Weapon> s_Weapons;
	protected static ref array<string> s_Helmets;     // tier 1, 2, 3
	protected static ref array<string> s_Vests;
	protected static ref array<string> s_Backpacks;
	protected static ref array<string> s_Attachments;

	static void Init()
	{
		if (s_Weapons)
			return;
		s_Weapons = new array<ref BR_Weapon>();
		BR_Weapon w;
		w = new BR_Weapon("AKM", "Mag_AKM_30Rnd", "AmmoBox_762x39_20Rnd");
		w.parts.Insert("AK_WoodBttstck");
		w.parts.Insert("AK_WoodHndgrd");
		s_Weapons.Insert(w);
		w = new BR_Weapon("M4A1", "Mag_STANAG_30Rnd", "AmmoBox_556x45_20Rnd");
		w.parts.Insert("M4_OEBttstck");
		w.parts.Insert("M4_RISHndgrd");
		s_Weapons.Insert(w);
		s_Weapons.Insert(new BR_Weapon("MP5K", "Mag_MP5_30Rnd", "AmmoBox_9x19_25rnd"));
		s_Weapons.Insert(new BR_Weapon("SVD", "Mag_SVD_10Rnd", "AmmoBox_762x54_20Rnd"));
		s_Weapons.Insert(new BR_Weapon("Saiga", "Mag_Saiga_8Rnd", "AmmoBox_00buck_10rnd"));

		s_Helmets = {"Ssh68Helmet", "BallisticHelmet_Black", "GorkaHelmet"};
		s_Vests = {"PoliceVest", "PressVest_Blue", "PlateCarrierVest"};
		s_Backpacks = {"ChildBag_Blue", "TaloonBag_Blue", "AliceBag_Green"};
		s_Attachments = {"ACOGOptic", "ReflexOptic", "KobraOptic", "PSO1Optic", "M4_Suppressor", "AK_Suppressor", "Mag_STANAG_60Rnd", "Mag_AKM_Drum75Rnd"};
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
		all.Insert("CanisterGasoline");
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
			Print("[BattleRoyale] could not create " + type);
			return null;
		}
		item.SetOrientation(Vector(Math.RandomFloat(0, 360), 0, 0));
		spawned.Insert(item);
		return item;
	}

	// a gun with a full magazine in it and its ammo boxes beside it
	static void WeaponPile(BR_Weapon w, vector pos, array<Object> spawned)
	{
		Weapon_Base gun = Weapon_Base.Cast(Item(w.weapon, pos, spawned));
		if (!gun)
			return;
		for (int p = 0; p < w.parts.Count(); p++)
			gun.GetInventory().CreateAttachment(w.parts[p]);
		gun.SpawnAttachedMagazine(w.mag);
		for (int b = 0; b < AMMO_BOXES; b++)
		{
			vector offset = Vector(Math.RandomFloat(-0.4, 0.4), 0.05, Math.RandomFloat(-0.4, 0.4));
			Item(w.ammo, pos + offset, spawned);
		}
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
		else if (roll < 70)
			Item(s_Attachments.GetRandomElement(), pos, spawned);
		else if (roll < 88)
		{
			if (Math.RandomInt(0, 3) == 0)
				Item("Morphine", pos, spawned);
			else
				Item("BandageDressing", pos, spawned);
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

	// Ready-to-drive cars at vanilla car spawn points inside the circle, with a little fuel.
	static int Cars(BR_Spots spots, vector center, float radius, int count, float fuel, array<Object> spawned)
	{
		array<int> inside = new array<int>();
		for (int i = 0; i < spots.cars.Count(); i++)
		{
			if (Distance2D(Vector(spots.cars[i].x, 0, spots.cars[i].z), center) < radius)
				inside.Insert(i);
		}

		int done = 0;
		while (done < count && inside.Count() > 0)
		{
			int pick = Math.RandomInt(0, inside.Count());
			BR_CarSpot spot = spots.cars[inside[pick]];
			inside.Remove(pick);

			array<string> types = new array<string>();
			spot.types.Split("|", types);
			vector pos = Vector(spot.x, GetGame().SurfaceY(spot.x, spot.z) + 0.3, spot.z); // a hair below ground and it falls through the map
			CarScript car = CarScript.Cast(GetGame().CreateObjectEx(types.GetRandomElement(), pos, ECE_PLACE_ON_SURFACE | ECE_SETUP));
			if (!car)
				continue;
			car.SetOrientation(Vector(spot.h, 0, 0));
			CarScript.s_BR_Fuel = fuel;
			car.OnDebugSpawn(); // every part fitted, every fluid full except the tank
			CarScript.s_BR_Fuel = -1;
			spawned.Insert(car);
			done++;
		}
		return done;
	}

	// --- supply drops (Expansion's airdrop plane) -------------------------------------

	static bool SupplyDrop(vector pos)
	{
		array<ref ExpansionLoot> loot = new array<ref ExpansionLoot>();
		loot.Insert(new ExpansionLoot("M4A1", Parts("Mag_STANAG_60Rnd|ACOGOptic|M4_Suppressor|M4_OEBttstck|M4_RISHndgrd"), 1));
		loot.Insert(new ExpansionLoot("SVD", Parts("Mag_SVD_10Rnd|PSO1Optic"), 1));
		loot.Insert(new ExpansionLoot("AmmoBox_556x45_20Rnd", null, 1));
		loot.Insert(new ExpansionLoot("AmmoBox_762x54_20Rnd", null, 1));
		loot.Insert(new ExpansionLoot("GorkaHelmet", null, 1));
		loot.Insert(new ExpansionLoot("PlateCarrierVest", null, 1));
		loot.Insert(new ExpansionLoot("AliceBag_Green", null, 1));
		loot.Insert(new ExpansionLoot("Morphine", null, 1));

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

	static void Kit(PlayerBase player)
	{
		Strip(player);
		player.SetHealth("", "", player.GetMaxHealth("", ""));
		player.SetHealth("GlobalHealth", "Blood", player.GetMaxHealth("GlobalHealth", "Blood"));
		player.SetHealth("GlobalHealth", "Shock", player.GetMaxHealth("GlobalHealth", "Shock"));
		player.GetStatEnergy().Set(player.GetStatEnergy().GetMax());
		player.GetStatWater().Set(player.GetStatWater().GetMax());
		if (player.GetBleedingManagerServer())
			player.GetBleedingManagerServer().RemoveAllSources();
		player.GetInventory().CreateInInventory("TShirt_Grey");
		player.GetInventory().CreateInInventory("Jeans_Blue");
		player.GetInventory().CreateInInventory("AthleticShoes_Black");
		player.GetInventory().CreateInInventory("BandageDressing");
	}
}
