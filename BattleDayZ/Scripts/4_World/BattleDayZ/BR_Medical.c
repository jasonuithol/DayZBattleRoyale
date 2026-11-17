// No stamina, and adrenaline that heals.
//
// Stamina: always full, on every character, in every game (the server and each player's game
// both run the stamina handler): sprinting, jumping and climbing never tire, and a heavy load
// doesn't lower the most there is. (Vanilla's own switch, SetStaminaDisabled, is only in
// developer builds.)
modded class StaminaHandler
{
	override void ProcessStaminaChanges(float deltaT, bool isServerOrSingleplayer)
	{
		m_StaminaDepletion = 0;
		if (m_StaminaDelta < 0)
			m_StaminaDelta = 0;
		m_Stamina = m_StaminaCap;
		super.ProcessStaminaChanges(deltaT, isServerOrSingleplayer);
	}

	override void RecalculateStaminaCap()
	{
		m_StaminaCap = CfgGameplayHandler.GetStaminaMax();
		m_StaminaCapSynced = m_StaminaCap;
	}

	// one-off costs (a jump, a climb, a melee swing): none
	override void DepleteStaminaEx(EStaminaModifiers modifier, float dT = -1, float coef = 1.0)
	{
	}
}

// Epinephrine (adrenaline): its vanilla boost, and back to full health too.
modded class Epinephrine
{
	override void OnApply(PlayerBase player)
	{
		super.OnApply(player);
		if (player && GetGame().IsServer())
			BR_Heal.Full(player);
	}
}

class BR_Heal
{
	// health back to 100% everywhere (every limb and overall), and blood and shock full
	static void Full(PlayerBase player)
	{
		array<string> zones = new array<string>();
		player.GetDamageZones(zones);
		for (int z = 0; z < zones.Count(); z++)
			player.SetHealthMax(zones[z], "Health");
		player.SetHealth("", "", player.GetMaxHealth("", ""));
		player.SetHealth("GlobalHealth", "Blood", player.GetMaxHealth("GlobalHealth", "Blood"));
		player.SetHealth("GlobalHealth", "Shock", player.GetMaxHealth("GlobalHealth", "Shock"));
	}
}
