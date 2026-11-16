modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();
		if (GetGame().IsDedicatedServer())
			BR_Round.Start();
	}

	// every new character (first join or respawn after being knocked out) starts at the lobby
	override PlayerBase CreateCharacter(PlayerIdentity identity, vector pos, ParamsReadContext ctx, string characterName)
	{
		BR_Round round = BR_Round.Get();
		if (round)
			pos = round.LobbyPosition();
		return super.CreateCharacter(identity, pos, ctx, characterName);
	}
}
