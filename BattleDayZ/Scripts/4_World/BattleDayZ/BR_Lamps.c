// Street lamps lit at night: Expansion's main mod lights only one lamp in LampAmount_OneInX
// (a whole number: all, a half, a third...), set in its GeneralSettings.json, which every
// mission on a server shares. A battle royale round lights a share of its own (lampsLit in
// battleroyale.json, all of them by default), picked spread out over the map the way Expansion's
// FARTHEST_RANDOM picks them. The lamps light up at night, as Expansion's always do.
#ifdef EXPANSIONMOD
modded class ExpansionLampLightBase
{
	static float s_BR_LitShare = 0; // 0: Expansion's own setting

	override static void LateInitLamps()
	{
		if (s_BR_LitShare <= 0)
		{
			super.LateInitLamps();
			return;
		}
		int total = s_Lamps.Count();
		int amount = Math.Round(total * Math.Min(s_BR_LitShare, 1));
		if (amount >= total)
			SelectAllLamps();
		else if (amount > 0)
			SelectFarthestLamps_Optimized(amount, true);
		else
			s_Lamps.Clear();
		Print("[BattleDayZ] street lamps: " + s_Lamps.Count() + " of " + total + " lit at night (lampsLit " + s_BR_LitShare + ")");
	}
}
#endif
