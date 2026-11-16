// What a round leaves lying about that it didn't spawn itself: Better Explosions' wrecks
// (vanilla Land_Wreck_* objects swapped in for a blown-up car), the parts it throws off, and
// its and Molotov's fires. Noted as they're made, so the round clean-up deletes exactly those,
// never the map's own abandoned wrecks. A building a molotov burns down stays a ruin until
// the server restarts, as the Molotov mod intends (deleting the ruin would leave a hole).
class BR_Leftovers
{
	static bool s_On;   // during a round
	static ref array<Object> s_List = new array<Object>();

	static bool Wanted(string type)
	{
		type.ToLower();
		if (type.IndexOf("land_wreck_") == 0 || type.IndexOf("betterexplosions") == 0)
			return true;
		return type == "molotovfire" || type == "molotovinferno" || type == "molotovblast" || type == "molotovflames";
	}

	static int Clear()
	{
		int n = 0;
		foreach (Object o : s_List)
		{
			if (o)
			{
				GetGame().ObjectDelete(o);
				n++;
			}
		}
		s_List.Clear();
		return n;
	}
}

modded class House
{
	override void EEInit()
	{
		super.EEInit();
		if (BR_Leftovers.s_On && GetGame().IsServer() && BR_Leftovers.Wanted(GetType()))
			BR_Leftovers.s_List.Insert(this);
	}
}
