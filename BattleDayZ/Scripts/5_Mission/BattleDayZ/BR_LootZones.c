// Loot put out as players come near, not all over the map at the start: once a player in the
// round is within lootRadius m of a building (across) and no higher than lootHeight above the
// ground (under the canopy, or on it), that building's loot is decided and put out, a few
// buildings at a time (PutOut) so the server never stalls. A building is decided once a round:
//   inside   none (1 - lootShare), armed (enough for 2-3 players: BR_Loot.Armed), or an
//            armoury (armouryShare of the looted ones: BR_Loot.Armoury)
//   roof     a flat roof (BR_Loot.RoofSpot) has an outfit on it roofLootShare of the time,
//            whatever is inside; the bare buildings (the tower blocks: no loot points, and a
//            way down from the roof) always have, two outfits
// Past lootItemCap things out at once, buildings get the light loot instead (1-4 things,
// nothing on the roof). Loot well outside the circle is deleted as it shrinks (Clear).
class BR_LootZones
{
	static const int ITEMS_PER_STEP = 150;  // things a step puts out, at the most (a building at least)
	static const int BARE_OUTFITS = 2;

	protected BR_Spots m_Spots;
	protected BR_Settings m_Settings;
	protected ref array<BR_Building> m_All;           // the loot buildings, then the bare ones
	protected int m_LootCount;                        // how many of m_All are loot buildings
	protected float m_Cell;                           // m: the grid's squares
	protected ref map<int, ref array<int>> m_Cells = new map<int, ref array<int>>();
	protected ref array<bool> m_Decided = new array<bool>();
	protected ref array<int> m_Queue = new array<int>();
	protected int m_QueueNext;
	protected ref array<Object> m_Items = new array<Object>(); // the loot out this round

	// this round's counts, for the log
	int m_Empty;
	int m_Armed;
	int m_Armouries;
	int m_Light;
	int m_Roofs;
	int m_Missing;
	int m_Made;
	int m_Cleared;

	void BR_LootZones(BR_Spots spots, BR_Settings settings)
	{
		m_Spots = spots;
		m_Settings = settings;
		m_All = spots.RoofBuildings();
		m_LootCount = spots.buildings.Count();
		m_Cell = Math.Max(settings.lootRadius, 50);
		for (int i = 0; i < m_All.Count(); i++)
		{
			int key = Key(Math.Floor(m_All[i].x / m_Cell), Math.Floor(m_All[i].z / m_Cell));
			if (!m_Cells.Contains(key))
				m_Cells.Set(key, new array<int>());
			m_Cells.Get(key).Insert(i);
			m_Decided.Insert(false);
		}
	}

	protected static int Key(int cx, int cz)
	{
		return cx * 10000 + cz;
	}

	// a new round: nothing decided, nothing out (the last round's loot went with DeleteAll)
	void Reset()
	{
		for (int i = 0; i < m_Decided.Count(); i++)
			m_Decided[i] = false;
		m_Queue.Clear();
		m_QueueNext = 0;
		m_Items.Clear();
		m_Empty = 0;
		m_Armed = 0;
		m_Armouries = 0;
		m_Light = 0;
		m_Roofs = 0;
		m_Missing = 0;
		m_Made = 0;
		m_Cleared = 0;
	}

	// a player here: every building within lootRadius not yet decided joins the queue
	void Near(vector pos)
	{
		float r = m_Settings.lootRadius;
		int x0 = Math.Floor((pos[0] - r) / m_Cell);
		int x1 = Math.Floor((pos[0] + r) / m_Cell);
		int z0 = Math.Floor((pos[2] - r) / m_Cell);
		int z1 = Math.Floor((pos[2] + r) / m_Cell);
		for (int cx = x0; cx <= x1; cx++)
		{
			for (int cz = z0; cz <= z1; cz++)
			{
				array<int> cell = m_Cells.Get(Key(cx, cz));
				if (!cell)
					continue;
				foreach (int i : cell)
				{
					if (m_Decided[i])
						continue;
					BR_Building b = m_All[i];
					float dx = b.x - pos[0];
					float dz = b.z - pos[2];
					if (dx * dx + dz * dz > r * r)
						continue;
					m_Decided[i] = true;
					m_Queue.Insert(i);
				}
			}
		}
	}

	// whether a player's position counts: low enough over the ground
	bool Counts(vector pos)
	{
		return pos[1] - GetGame().SurfaceY(pos[0], pos[2]) <= m_Settings.lootHeight;
	}

	int Waiting()
	{
		return m_Queue.Count() - m_QueueNext;
	}

	int Out()
	{
		return m_Items.Count();
	}

	// put out the next buildings' loot, up to ITEMS_PER_STEP things (at least one building).
	// (Not called Step: a method of that name here is never called, the engine's own instead.)
	void PutOut()
	{
		int before = m_Items.Count();
		while (m_QueueNext < m_Queue.Count())
		{
			int made = m_Items.Count() - before;
			if (made >= ITEMS_PER_STEP)
				break;
			int index = m_Queue[m_QueueNext];
			m_QueueNext++;
			LootOne(index);
		}
		if (m_QueueNext >= m_Queue.Count() && m_QueueNext > 0)
		{
			m_Queue.Clear();
			m_QueueNext = 0;
		}
	}

	// everything waiting, now (the headless test)
	void Drain()
	{
		while (Waiting() > 0)
		{
			int waiting = Waiting();
			PutOut();
			if (Waiting() >= waiting)
			{
				Print("[BattleDayZ] loot: a step put out nothing, " + Waiting() + " buildings still waiting");
				return;
			}
		}
	}

	protected void LootOne(int index)
	{
		BR_Building b = m_All[index];
		BR_Proto kind = m_Spots.protos[b.t];
		Object building = BR_Loot.FindBuilding(kind.type, Vector(b.x, b.y, b.z));
		if (!building)
		{
			m_Missing++;
			return;
		}
		int before = m_Items.Count();
		bool bare = index >= m_LootCount;
		bool full = m_Items.Count() >= m_Settings.lootItemCap;
		if (!bare)
		{
			BR_Points points = new BR_Points(building, kind);
			if (full)
			{
				BR_Loot.Light(points, m_Items);
				m_Light++;
			}
			else if (Math.RandomFloat01() >= m_Settings.lootShare)
				m_Empty++;
			else if (Math.RandomFloat01() < m_Settings.armouryShare)
			{
				BR_Loot.Armoury(points, m_Items);
				m_Armouries++;
			}
			else
			{
				BR_Loot.Armed(points, m_Items);
				m_Armed++;
			}
		}
		if (!full && (bare || Math.RandomFloat01() < m_Settings.roofLootShare))
		{
			int outfits = 1;
			if (bare)
				outfits = BARE_OUTFITS;
			bool roof = false;
			for (int n = 0; n < outfits; n++)
			{
				vector spot;
				if (!BR_Loot.RoofSpot(building, spot))
					break;
				BR_Loot.Outfit(spot, m_Items);
				roof = true;
			}
			if (roof)
				m_Roofs++;
		}
		m_Made += m_Items.Count() - before;
	}

	// the loot more than lootClearBeyond m outside the circle goes: lying about, or in
	// something lying about (not on anyone, not in a car). Returns how many things went.
	int Clear(vector center, float radius)
	{
		float limit = radius + m_Settings.lootClearBeyond;
		int deleted = 0;
		for (int i = m_Items.Count() - 1; i >= 0; i--)
		{
			Object o = m_Items[i];
			if (!o)
			{
				m_Items.Remove(i);
				continue;
			}
			EntityAI item = EntityAI.Cast(o);
			if (item && item.GetHierarchyParent())
				continue; // in something: it goes with that, or stays with whoever has it
			if (BR_Loot.Distance2D(o.GetPosition(), center) <= limit)
				continue;
			GetGame().ObjectDelete(o);
			m_Items.Remove(i);
			deleted++;
		}
		m_Cleared += deleted;
		return deleted;
	}

	// the end of the round: all of it
	int DeleteAll()
	{
		int count = 0;
		foreach (Object o : m_Items)
		{
			if (!o)
				continue;
			EntityAI item = EntityAI.Cast(o);
			if (item && item.GetHierarchyParent())
				continue; // goes with what it's in (players are reset, bodies and cars deleted)
			GetGame().ObjectDelete(o);
			count++;
		}
		m_Items.Clear();
		m_Queue.Clear();
		m_QueueNext = 0;
		return count;
	}

	// the loot out, to someone else's list (the headless test's, which checks it and deletes it)
	void Handover(array<Object> to)
	{
		foreach (Object o : m_Items)
		{
			if (o)
				to.Insert(o);
		}
		m_Items.Clear();
	}

	string Summary()
	{
		int decided = m_Empty + m_Armed + m_Armouries + m_Light;
		string text = decided.ToString() + " buildings decided (" + m_Empty + " empty, " + m_Armed + " armed, ";
		text += m_Armouries.ToString() + " armouries, " + m_Light + " light), " + m_Roofs + " roofs, " + m_Missing + " not found; ";
		text += m_Made.ToString() + " things put out, " + m_Cleared + " cleared outside the circle, " + Out() + " still out";
		return text;
	}
}
