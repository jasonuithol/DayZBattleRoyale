// Battle Royale on Chernarus: the vanilla Chernarus mission with the BattleDayZ mod running the
// rounds. The economy spawns no loot, cars, zombies or animals (db/economy.xml): the mod puts
// out its own loot and cars each round and removes them afterwards.
//
// Started with -missiontest (tools/test.sh battleroyale) it runs a headless check instead: the
// settings and map data loaded, every loot class exists, loot, cars and a supply drop spawn in
// a test circle, a test character can be kitted, the clean-up removes it all. It logs [BRTest]
// lines and shuts the server down.
void main()
{
	Hive ce = CreateHive();
	if (ce)
		ce.InitOffline();

	int year, month, day, hour, minute;
	GetGame().GetWorld().GetDate(year, month, day, hour, minute);
	GetGame().GetWorld().SetDate(year, 6, 15, 12, 0); // summer, at noon
}

class CustomMission: MissionServer
{
	override void OnInit()
	{
		super.OnInit();
		string value;
		if (GetGame().CommandlineGetParam("missiontest", value))
			GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(Test, 30000, false); // vehicles made in the first seconds get no fuel
	}

	void Test()
	{
		BR_Round round = BR_Round.Get();
		if (!round)
		{
			Print("[BRTest] the mod isn't running");
			GetGame().RequestExit(0);
			return;
		}
		round.SelfTest();
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(Finish, 15000, false);
	}

	void Finish()
	{
		BR_Round.Get().SelfTestFinish();
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(Gone, 2000, false);
	}

	void Gone()
	{
		BR_Round.Get().SelfTestGone();
		GetGame().RequestExit(0);
	}
};

Mission CreateCustomMission(string path)
{
	return new CustomMission();
}
