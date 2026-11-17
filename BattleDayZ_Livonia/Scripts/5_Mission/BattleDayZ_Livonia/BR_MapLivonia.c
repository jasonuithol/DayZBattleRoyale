// Livonia's profile for BattleDayZ (see BR_Maps.c there): the places copied up into the sky
// as the lobby, taking turns round by round. Livonia's world is called enoch.
class BR_MapLivonia: BR_Map
{
	void BR_MapLivonia()
	{
		m_World = "enoch";
		m_Title = "Livonia";
	}

	override void Lobbies()
	{
		AddLobby(new BR_LobbySpec("branzow_castle", "Branzow castle", 1060, 11400, 60));
		AddLobby(new BR_LobbySpec("brena_square", "Brena square", 6536, 11265, 45));      // round the monument
		AddLobby(new BR_LobbySpec("lukow_airfield", "Lukow airfield", 4225, 10408, 50));  // round the control tower
	}
}

modded class BR_Maps
{
	override void Register()
	{
		super.Register();
		Add(new BR_MapLivonia());
	}
}
