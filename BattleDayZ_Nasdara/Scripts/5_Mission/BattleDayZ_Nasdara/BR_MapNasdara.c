// Nasdara's profile for BattleDayZ (see BR_Maps.c there): the places copied up into the sky
// as the lobby, taking turns round by round.
class BR_MapNasdara: BR_Map
{
	void BR_MapNasdara()
	{
		m_World = "nasdara";
		m_Title = "Nasdara";
	}

	override void Lobbies()
	{
		AddLobby(new BR_LobbySpec("qalat_e_lut", "Qalat-e-Lut", 14837, 7853, 45, 3));     // the caravanserai, on ground 3 m above the dip at its middle
		AddLobby(new BR_LobbySpec("airport_junkyard", "the airport junkyard", 12290, 11350, 80));
		AddLobby(new BR_LobbySpec("kirov_dam", "Kirov dam", 3560, 14440, 60));
	}
}

modded class BR_Maps
{
	override void Register()
	{
		super.Register();
		Add(new BR_MapNasdara());
	}
}
