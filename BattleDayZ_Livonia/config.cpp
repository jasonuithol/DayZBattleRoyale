// BattleDayZ on Livonia: the map's profile (BR_Maps.c in BattleDayZ) - its own places for the
// lobby in the sky. Server only (-serverMod=), after BattleDayZ, on Livonia servers.
class CfgPatches
{
	class BattleDayZ_Livonia
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"BattleDayZ"};
	};
};

class CfgMods
{
	class BattleDayZ_Livonia
	{
		dir = "BattleDayZ_Livonia";
		name = "BattleDayZ Livonia";
		type = "mod";
		dependencies[] = {"Mission"};

		class defs
		{
			class missionScriptModule
			{
				value = "";
				files[] = {"BattleDayZ_Livonia/Scripts/5_Mission"};
			};
		};
	};
};
