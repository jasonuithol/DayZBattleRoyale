// Reload refills the magazine: with a magazine on the gun that isn't full and ammo for it in
// the inventory (loose rounds, or the boxes they come in, unopened), the reload key tops that
// magazine up from it instead of swapping in another. With no ammo for it, reload is vanilla's.
//
// The player's game asks (the reload key, PlayerBase.QuickReloadWeapon) and the server does it:
// it takes the rounds from loose piles first, then from boxes (a box's rounds left over stay
// in the inventory as a loose pile), and adds them to the magazine.
class BR_Refill
{
	static const int RPC = 1112670213; // "BR" + 5: client -> server, refill the magazine on this gun

	// the magazine on the gun in this player's hands, if it isn't full
	static Magazine Topped(PlayerBase player, out Weapon_Base gun)
	{
		gun = Weapon_Base.Cast(player.GetHumanInventory().GetEntityInHands());
		if (!gun)
			return null;
		Magazine mag = gun.GetMagazine(gun.GetCurrentMuzzle());
		if (!mag || mag.IsAmmoPile() || mag.GetAmmoCount() >= mag.GetAmmoMax())
			return null;
		return mag;
	}

	// the loose rounds that fit this magazine (its config's ammoItems[])
	static void AmmoFor(Magazine mag, out TStringArray rounds)
	{
		rounds = new TStringArray();
		GetGame().ConfigGetTextArray("CfgMagazines " + mag.GetType() + " ammoItems", rounds);
	}

	// what a box of rounds holds: the type of round and how many (its config's Resources)
	static string BoxHolds(EntityAI box, out int count)
	{
		count = 0;
		string path = "CfgVehicles " + box.GetType() + " Resources";
		if (!GetGame().ConfigIsExisting(path) || GetGame().ConfigGetChildrenCount(path) == 0)
			return "";
		string round;
		GetGame().ConfigGetChildName(path, 0, round);
		count = GetGame().ConfigGetInt(path + " " + round + " value");
		return round;
	}

	// rounds this player carries that fit the magazine
	static int Available(PlayerBase player, Magazine mag)
	{
		TStringArray rounds;
		AmmoFor(mag, rounds);
		array<EntityAI> items = new array<EntityAI>();
		player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items);
		int total = 0;
		foreach (EntityAI item : items)
		{
			Magazine pile = Magazine.Cast(item);
			if (pile && pile.IsAmmoPile() && rounds.Find(pile.GetType()) >= 0)
			{
				total += pile.GetAmmoCount();
				continue;
			}
			if (Box_Base.Cast(item))
			{
				int inBox;
				if (rounds.Find(BoxHolds(item, inBox)) >= 0)
					total += inBox;
			}
		}
		return total;
	}

	// server: top the magazine up from the inventory; how many rounds went in
	static int Refill(PlayerBase player, Magazine mag)
	{
		int need = mag.GetAmmoMax() - mag.GetAmmoCount();
		if (need <= 0)
			return 0;
		TStringArray rounds;
		AmmoFor(mag, rounds);
		array<EntityAI> items = new array<EntityAI>();
		player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items);
		int taken = 0;
		// loose rounds first
		foreach (EntityAI item : items)
		{
			if (taken >= need)
				break;
			Magazine pile = Magazine.Cast(item);
			if (!pile || !pile.IsAmmoPile() || rounds.Find(pile.GetType()) < 0 || pile == mag)
				continue;
			int fromPile = Math.Min(pile.GetAmmoCount(), need - taken);
			taken += fromPile;
			if (fromPile >= pile.GetAmmoCount())
				GetGame().ObjectDelete(pile);
			else
				pile.ServerAddAmmoCount(-fromPile);
		}
		// then boxes, unopened
		foreach (EntityAI boxItem : items)
		{
			if (taken >= need)
				break;
			if (!Box_Base.Cast(boxItem))
				continue;
			int inBox;
			string round = BoxHolds(boxItem, inBox);
			if (inBox <= 0 || rounds.Find(round) < 0)
				continue;
			int fromBox = Math.Min(inBox, need - taken);
			taken += fromBox;
			GetGame().ObjectDelete(boxItem);
			if (inBox > fromBox)
			{
				// what's left of the box stays with the player, loose
				Magazine rest = Magazine.Cast(player.GetInventory().CreateInInventory(round));
				if (!rest)
					rest = Magazine.Cast(GetGame().CreateObjectEx(round, player.GetPosition(), ECE_PLACE_ON_SURFACE));
				if (rest)
					rest.ServerSetAmmoCount(inBox - fromBox);
			}
		}
		if (taken > 0)
			mag.ServerAddAmmoCount(taken);
		return taken;
	}
}

modded class PlayerBase
{
	// the reload key: refill the magazine on the gun if there's ammo for it, else vanilla's reload
	override void QuickReloadWeapon(EntityAI weapon)
	{
		if (GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_CLIENT)
		{
			Weapon_Base gun;
			Magazine mag = BR_Refill.Topped(this, gun);
			if (mag && gun == weapon && BR_Refill.Available(this, mag) > 0)
			{
				ScriptRPC rpc = new ScriptRPC();
				rpc.Send(this, BR_Refill.RPC, true, null);
				return;
			}
		}
		super.QuickReloadWeapon(weapon);
	}

	override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
	{
		if (rpc_type == BR_Refill.RPC)
		{
			if (GetGame().IsServer() && sender && GetIdentity() && sender.GetId() == GetIdentity().GetId() && IsAlive())
			{
				Weapon_Base gun;
				Magazine mag = BR_Refill.Topped(this, gun);
				if (mag)
				{
					int added = BR_Refill.Refill(this, mag);
					Print("[BattleDayZ] " + sender.GetName() + " refilled " + mag.GetType() + " with " + added + " rounds, now " + mag.GetAmmoCount() + "/" + mag.GetAmmoMax());
				}
			}
			return;
		}
		super.OnRPC(sender, rpc_type, ctx);
	}
}
