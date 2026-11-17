// Reload from your ammo: with a magazine on the gun that isn't full, ammo for it in the
// inventory (loose rounds, or the boxes they come in, unopened) and no fuller magazine of that
// kind to swap in, the reload key fills a spare magazine of the same kind from that ammo (the
// one the last reload took off, or a new one) and swaps it in with vanilla's own reload. With
// no ammo for it, or a spare already full enough, reload is vanilla's.
//
// Not the magazine on the gun itself: a gun keeps its own count of what its magazine holds,
// changed only by its own reload actions, so a server-side top-up there never reached the
// player's game (it still showed it empty, and refused to fire). A magazine in the inventory
// does sync. The weapon action (BR_ActionRefillMagazine, below) has the server fill the spare
// (loose rounds first, then boxes; what's left of a box stays as loose rounds), then the
// player's game swaps it in (vanilla's swap: the animation, and both sides in step).
class BR_Refill
{
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

	// a magazine of the same kind as the one on the gun, in the inventory (not on any gun):
	// the fullest
	static Magazine Spare(PlayerBase player, Magazine onGun)
	{
		array<EntityAI> items = new array<EntityAI>();
		player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items);
		Magazine best;
		foreach (EntityAI item : items)
		{
			Magazine mag = Magazine.Cast(item);
			if (!mag || mag == onGun || mag.GetType() != onGun.GetType() || Weapon_Base.Cast(mag.GetHierarchyParent()))
				continue;
			if (!best || mag.GetAmmoCount() > best.GetAmmoCount())
				best = mag;
		}
		return best;
	}

	// server: the spare, filled from the ammo carried (a new, empty one first if there's none)
	static Magazine FillSpare(PlayerBase player, Magazine onGun, out int added)
	{
		added = 0;
		Magazine spare = Spare(player, onGun);
		if (!spare)
		{
			spare = Magazine.Cast(player.GetInventory().CreateInInventory(onGun.GetType()));
			if (!spare)
				return null; // no room for one
			spare.ServerSetAmmoCount(0);
		}
		added = Refill(player, spare);
		return spare;
	}

	// the player's game, a moment after the server filled the spare: swap it in (vanilla's swap)
	static void SwapIn(PlayerBase player)
	{
		if (!player || !player.IsAlive())
			return;
		Weapon_Base gun;
		Magazine onGun = Topped(player, gun);
		if (!onGun)
			return;
		Magazine spare = Spare(player, onGun);
		if (spare && spare.GetAmmoCount() > onGun.GetAmmoCount() && player.GetWeaponManager().CanSwapMagazine(gun, spare))
			player.GetWeaponManager().SwapMagazine(spare);
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

// The reload key (UAReloadMagazine) runs whichever weapon action fits, in the order they were
// added: this one goes first, ahead of vanilla's quick magazine swap. It ends straight away
// (FirearmActionBase ends once the weapon isn't busy); the swap that follows is vanilla's.
class BR_ActionRefillMagazine : FirearmActionBase
{
	override bool HasTarget()
	{
		return false;
	}

	override typename GetInputType()
	{
		return WeaponManipulationActionInput;
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINonRuined();
		m_ConditionTarget = new CCTSelf();
	}

	override bool HasProgress()
	{
		return false;
	}

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!super.ActionCondition(player, target, item))
			return false;
		Weapon_Base gun;
		Magazine mag = BR_Refill.Topped(player, gun);
		if (!mag || gun != item)
			return false;
		// a spare already fuller than this one: vanilla's own quick swap does it
		Magazine spare = BR_Refill.Spare(player, mag);
		if (spare && spare.GetAmmoCount() > mag.GetAmmoCount())
			return false;
		return BR_Refill.Available(player, mag) > 0;
	}

	override void Start(ActionData action_data)
	{
		super.Start(action_data);
		PlayerBase player = action_data.m_Player;
		if (!GetGame().IsServer())
		{
			// once the filled spare has reached this game: swap it in
			GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(BR_Refill.SwapIn, SWAP_AFTER_MS, false, player);
			return;
		}
		Weapon_Base gun;
		Magazine mag = BR_Refill.Topped(player, gun);
		if (!mag)
			return;
		int added;
		Magazine spare = BR_Refill.FillSpare(player, mag, added);
		string who = "someone";
		if (player.GetIdentity())
			who = player.GetIdentity().GetName();
		if (spare)
			Print("[BattleDayZ] " + who + " refilled a spare " + spare.GetType() + " with " + added + " rounds, now " + spare.GetAmmoCount() + "/" + spare.GetAmmoMax());
		else
			Print("[BattleDayZ] " + who + " has no room for a spare " + mag.GetType());
	}

	static const int SWAP_AFTER_MS = 600;
}

modded class Weapon_Base
{
	override void SetActions()
	{
		AddAction(BR_ActionRefillMagazine); // first: ahead of vanilla's quick swap on the reload key
		super.SetActions();
	}
}

modded class ActionConstructor
{
	override void RegisterActions(TTypenameArray actions)
	{
		super.RegisterActions(actions);
		actions.Insert(BR_ActionRefillMagazine);
	}
}
