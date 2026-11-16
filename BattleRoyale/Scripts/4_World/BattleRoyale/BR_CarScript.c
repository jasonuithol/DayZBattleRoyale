// Battle royale cars come with only a little petrol. OnDebugSpawn (every part fitted) fills
// every fluid via FillUpCarFluids; while s_BR_Fuel is set, the tank gets that fraction instead.
// (Draining a full tank afterwards with Leak / LeakAll did nothing.)
modded class CarScript
{
	static float s_BR_Fuel = -1;

	override protected void FillUpCarFluids()
	{
		if (s_BR_Fuel < 0)
		{
			super.FillUpCarFluids();
			return;
		}
		Fill(CarFluid.FUEL, GetFluidCapacity(CarFluid.FUEL) * s_BR_Fuel);
		Fill(CarFluid.COOLANT, 200.0);
		Fill(CarFluid.OIL, 200.0);
	}
}
