// Every airdrop crate, so the round clean-up can find them. Expansion keeps its own list
// (s_Expansion_AirdropContainers) only when it builds crates on the client too, which this
// setup doesn't. Entries go null when a crate is deleted.
modded class ExpansionAirdropContainerBase
{
	static ref array<ExpansionAirdropContainerBase> s_BR_Crates = new array<ExpansionAirdropContainerBase>();

	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
			s_BR_Crates.Insert(this);
	}
}
