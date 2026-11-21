class CfgPatches
{
	class BattleDayZ
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"DZ_Data", "DZ_Scripts", "DayZExpansion_Missions_Scripts", "DayZExpansion_Navigation_Scripts", "FlyingWithoutCTD", "DZ_Weapons_Optics", "DZ_Weapons_Muzzles"};
	};
};

class CfgMods
{
	class BattleDayZ
	{
		dir = "BattleDayZ";
		name = "BattleDayZ";
		type = "mod";
		dependencies[] = {"Game", "World", "Mission"};

		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = {"BattleDayZ/Scripts/3_Game"};
			};
			class worldScriptModule
			{
				value = "";
				files[] = {"BattleDayZ/Scripts/4_World"};
			};
			class missionScriptModule
			{
				value = "";
				files[] = {"BattleDayZ/Scripts/5_Mission"};
			};
		};
	};
};

class CfgVehicles
{
	class HouseNoDestruct;

	//! The open parachute: visual only (no geometry, so it collides with nothing), created
	//! locally by each game above every jumper whose chute is open (BR_Chute.c)
	// No NATO / Soviet split for sights and suppressors: the rail sights in the loot (ACOG,
	// Baraka, Combat Sights) also go on the AK dovetail; both rifle suppressors fit the M4 and AK
	// muzzles, and the slot the DMR and SK 59/66 only have for an improvised suppressor. (The
	// M16-A2 has no sight slot.)
	class ItemOptics_Base;
	class ACOGOptic: ItemOptics_Base { inventorySlot[] = {"WeaponOptics", "weaponOpticsAK"}; };
	class ReflexOptic: ItemOptics_Base { inventorySlot[] = {"WeaponOptics", "weaponOpticsAK"}; };
	class M68Optic: ItemOptics_Base { inventorySlot[] = {"weaponOptics", "weaponOpticsAK"}; };
	class ItemSuppressor;
	class M4_Suppressor: ItemSuppressor { inventorySlot[] = {"weaponMuzzleM4", "weaponMuzzleAK", "suppressorImpro"}; };
	class AK_Suppressor: ItemSuppressor { inventorySlot[] = {"weaponMuzzleAK", "weaponMuzzleM4", "suppressorImpro"}; };

	class BR_Parachute: HouseNoDestruct
	{
		scope = 1;
		model = "\BattleDayZ\data\parachute.p3d";
	};
	// one 10 m panel of the glowing wall along the circle's edge (client-only, no collision)
	class BR_StormWall: HouseNoDestruct
	{
		scope = 1;
		model = "\BattleDayZ\data\stormwall.p3d";
	};
	// a big stone apple on top of the lobby fountain's column (no collision)
	class BR_StoneApple: HouseNoDestruct
	{
		scope = 1;
		model = "\BattleDayZ\data\stoneapple.p3d";
	};
};
