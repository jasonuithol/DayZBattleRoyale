class CfgPatches
{
	class BattleRoyale
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"DZ_Data", "DZ_Scripts", "DayZExpansion_Missions_Scripts", "DayZExpansion_Navigation_Scripts", "FlyingWithoutCTD"};
	};
};

class CfgMods
{
	class BattleRoyale
	{
		dir = "BattleRoyale";
		name = "Battle Royale";
		type = "mod";
		dependencies[] = {"Game", "World", "Mission"};

		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = {"BattleRoyale/Scripts/3_Game"};
			};
			class worldScriptModule
			{
				value = "";
				files[] = {"BattleRoyale/Scripts/4_World"};
			};
			class missionScriptModule
			{
				value = "";
				files[] = {"BattleRoyale/Scripts/5_Mission"};
			};
		};
	};
};

class CfgVehicles
{
	class HouseNoDestruct;

	//! The open parachute: visual only (no geometry, so it collides with nothing), created
	//! locally by each game above every jumper whose chute is open (BR_Chute.c)
	class BR_Parachute: HouseNoDestruct
	{
		scope = 1;
		model = "\BattleRoyale\data\parachute.p3d";
	};
};
