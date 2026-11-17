// BattleDayZ on Nasdara: the map's profile (BR_Maps.c in BattleDayZ) - its own places for the
// lobby in the sky. Server only (-serverMod=), after BattleDayZ, on Nasdara servers.
class CfgPatches
{
	class BattleDayZ_Nasdara
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"BattleDayZ"};
	};
};

class CfgMods
{
	class BattleDayZ_Nasdara
	{
		dir = "BattleDayZ_Nasdara";
		name = "BattleDayZ Nasdara";
		type = "mod";
		dependencies[] = {"Mission"};

		class defs
		{
			class missionScriptModule
			{
				value = "";
				files[] = {"BattleDayZ_Nasdara/Scripts/5_Mission"};
			};
		};
	};
};
