// Street lamps lit at night: Expansion's main mod lights only one lamp in LampAmount_OneInX
// (a whole number: all, a half, a third...), set in its GeneralSettings.json, which every
// mission on a server shares. A battle royale round lights a share of its own (lampsLit in
// battleroyale.json, all of them by default), picked spread out over the map the way Expansion's
// FARTHEST_RANDOM picks them. The lamps light up at night, as Expansion's always do.
//
// Expansion picks its lamps once, as the mission loads, from the lamps the server has by then;
// a lamp the server only makes later stays dark. With every lamp lit, those are lit as they come.
#ifdef EXPANSIONMOD
modded class ExpansionLampLightBase
{
	static float s_BR_LitShare = 0; // 0: Expansion's own setting
	static bool s_BR_Picked = false; // the lamps lit as the mission loaded have been picked
	static int s_BR_Late = 0;        // lamps lit since
	static int s_BR_LateLogged = 0;

	void ExpansionLampLightBase()
	{
		if (s_BR_Picked && s_BR_LitShare >= 1 && GetGame().IsServer())
			GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(BR_LightLate, 0, false);
	}

	protected void BR_LightLate()
	{
		LateInit();
		s_BR_Late++;
	}

	override static void LateInitLamps()
	{
		if (s_BR_LitShare <= 0)
		{
			super.LateInitLamps();
			return;
		}
		int total = s_Lamps.Count();
		map<string, int> types = new map<string, int>();
		foreach (ExpansionLampLightBase lamp : s_Lamps)
			types.Set(lamp.GetType(), types.Get(lamp.GetType()) + 1);
		string byType = "";
		for (int i = 0; i < types.Count(); i++)
			byType += " " + types.GetKey(i) + " " + types.GetElement(i);

		int amount = Math.Round(total * Math.Min(s_BR_LitShare, 1));
		if (amount >= total)
			SelectAllLamps();
		else if (amount > 0)
			SelectFarthestLamps_Optimized(amount, true);
		else
			s_Lamps.Clear();
		s_BR_Picked = true;
		Print("[BattleDayZ] street lamps: " + s_Lamps.Count() + " of " + total + " lit at night (lampsLit " + s_BR_LitShare + "):" + byType);
		BR_SpeakerLamp.LightWaiting();
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(BR_LogLate, 60000, true);
	}

	static void BR_LogLate()
	{
		if (s_BR_Late + BR_SpeakerLamp.s_Lit == s_BR_LateLogged)
			return;
		s_BR_LateLogged = s_BR_Late + BR_SpeakerLamp.s_Lit;
		Print("[BattleDayZ] street lamps: " + s_BR_Late + " more lit since the mission loaded (" + s_Lamps.Count() + " known), and " + BR_SpeakerLamp.s_Lit + " lamps with loudspeakers");
	}
}

// The light itself, on players' games. The game only draws so many lights at once: past that,
// lit patches flicker on and off as it keeps changing its mind which to draw. So each game
// lights only the BR_MOST_LIT lamps nearest its camera (the rest wait, dark, until they're
// among the nearest), and none of them casts shadows.
modded class ExpansionLampLightBase_Lamp
{
	static const int BR_MOST_LIT = 16;
	static ref array<ExpansionLampLightBase_Lamp> s_BR_Here; // this game's lamps
	static int s_BR_Checks = 0;

	void ExpansionLampLightBase_Lamp()
	{
	#ifndef SERVER
		if (!s_BR_Here)
		{
			s_BR_Here = new array<ExpansionLampLightBase_Lamp>();
			GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(BR_LightNearest, 250, true);
		}
		s_BR_Here.Insert(this);
	#endif
	}

	void ~ExpansionLampLightBase_Lamp()
	{
		if (s_BR_Here)
		{
			int i = s_BR_Here.Find(this);
			if (i >= 0)
				s_BR_Here.Remove(i);
		}
	}

	override void Expansion_OnClientSettingsUpdated()
	{
		super.Expansion_OnClientSettingsUpdated();
		if (m_Expansion_Light)
			m_Expansion_Light.SetCastShadow(false);
	}

	bool BR_HasLight()
	{
		return m_Expansion_Light != null;
	}

	void BR_SetLit(bool on)
	{
		if (!m_Expansion_Light)
			return;
		if (m_Expansion_Light.IsEnabled() != on)
			m_Expansion_Light.SetEnabled(on);
	}

	static void BR_LightNearest()
	{
		vector camera = GetGame().GetCurrentCameraPosition();
		array<float> far = new array<float>();
		foreach (ExpansionLampLightBase_Lamp lamp : s_BR_Here)
		{
			if (lamp && lamp.BR_HasLight())
				far.Insert(vector.DistanceSq(lamp.GetPosition(), camera));
		}
		float limit = 1e12;
		if (far.Count() > BR_MOST_LIT)
		{
			array<float> sorted = new array<float>();
			sorted.Copy(far);
			sorted.Sort();
			limit = sorted[BR_MOST_LIT - 1];
		}
		int lit = 0;
		int k = 0;
		foreach (ExpansionLampLightBase_Lamp each : s_BR_Here)
		{
			if (!each || !each.BR_HasLight())
				continue;
			bool on = far[k] <= limit;
			k++;
			each.BR_SetLit(on);
			if (on)
				lit++;
		}
		s_BR_Checks++;
		if (s_BR_Checks % 40 == 1) // every 10 s
			Print("[BattleDayZ] street lights here: " + far.Count() + " lamps, the nearest " + lit + " lit (nearest " + Math.Sqrt(BR_Least(far)) + " m)");
	}

	static float BR_Least(array<float> far)
	{
		float least = 1e12;
		foreach (float f : far)
			least = Math.Min(least, f);
		return least;
	}
}

// Lamps with loudspeakers on them (the vanilla public address lamps: on a metal street pole, a
// wooden or a concrete power pole) are vanilla classes Expansion doesn't light. They get the same
// light, lit with the rest (lampsLit: all of them, or that share at random).
class BR_SpeakerLamp
{
	static ref array<EntityAI> s_Waiting = new array<EntityAI>();
	static int s_Lit = 0;

	static void Wait(EntityAI lamp)
	{
		if (ExpansionLampLightBase.s_BR_Picked)
			GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(LightOne, 0, false, lamp);
		else
			s_Waiting.Insert(lamp);
	}

	static void LightWaiting()
	{
		foreach (EntityAI lamp : s_Waiting)
			LightOne(lamp);
		s_Waiting.Clear();
	}

	static void LightOne(EntityAI lamp)
	{
		float share = ExpansionLampLightBase.s_BR_LitShare;
		if (!lamp || share <= 0 || (share < 1 && Math.RandomFloat01() >= share))
			return;
		if (GetExpansionSettings().GetGeneral().EnableLamps == LampModeEnum.Disabled)
			return;
		Land_Lamp_City1_amp city = Land_Lamp_City1_amp.Cast(lamp);
		Land_Power_Pole_Wood1_Lamp_Amp wood = Land_Power_Pole_Wood1_Lamp_Amp.Cast(lamp);
		Land_Power_Pole_Conc4_Lamp_Amp conc = Land_Power_Pole_Conc4_Lamp_Amp.Cast(lamp);
		if ((city && city.BR_Light()) || (wood && wood.BR_Light()) || (conc && conc.BR_Light()))
			s_Lit++;
	}

	// where on the lamp, and the colour, as Expansion's lamps without loudspeakers have them
	// a lamp's model with no class (in the lobby in the sky, copied from the map): the light
	// its class would have, by its model
	static ExpansionLampLightBase_Lamp LightModel(Object model, string p3d)
	{
		float share = ExpansionLampLightBase.s_BR_LitShare;
		if (!model || share <= 0 || (share < 1 && Math.RandomFloat01() >= share))
			return null;
		string name = p3d;
		name.Replace("/", "\\");
		name.ToLower();
		int slash = name.LastIndexOf("\\");
		if (slash >= 0)
			name = name.Substring(slash + 1, name.Length() - slash - 1);
		vector offset = "0 5 0";
		vector color = "1.0 0.7 0.4";
		switch (name)
		{
			case "lamp_city1.p3d":
			case "lamp_city2.p3d":
			case "lamp_city3.p3d":
			case "lamp_city1_amp.p3d":
				color = "0.9 0.8 0.6";
				break;
			case "lamp_ind.p3d":
			case "lamp_rail.p3d":
				break;
			case "power_pole_wood1_lamp.p3d":
			case "power_pole_wood1_lamp_amp.p3d":
				offset = "-0.000977 3.65 0.418945";
				break;
			case "power_pole_conc4_lamp.p3d":
			case "power_pole_conc4_lamp_amp.p3d":
				offset = "-0.81982 5.0445 -0.01017";
				break;
			default:
				return null;
		}
		return Light(model, offset, color);
	}

	static ExpansionLampLightBase_Lamp Light(Object lamp, vector offset, vector color)
	{
		if (lamp.MemoryPointExists("light"))
			offset = lamp.GetMemoryPointPos("light");
		int flags = ECE_NOLIFETIME | ECE_KEEPHEIGHT | ECE_NOPERSISTENCY_WORLD;
		Object obj = GetGame().CreateObjectEx("ExpansionLampLightBase_Lamp", lamp.ModelToWorld(offset), flags);
		ExpansionLampLightBase_Lamp light;
		if (!Class.CastTo(light, obj))
		{
			if (obj)
				GetGame().ObjectDelete(obj);
			return null;
		}
		light.SetOrientation("0 0 0");
		light.Expansion_SetLampColor(color);
		return light;
	}
}

modded class Land_Lamp_City1_amp
{
	protected ExpansionLampLightBase_Lamp m_BR_Light;

	void Land_Lamp_City1_amp()
	{
		if (GetGame().IsServer())
			BR_SpeakerLamp.Wait(this);
	}

	void ~Land_Lamp_City1_amp()
	{
		if (GetGame() && m_BR_Light)
			GetGame().ObjectDelete(m_BR_Light);
	}

	bool BR_Light()
	{
		if (!m_BR_Light)
			m_BR_Light = BR_SpeakerLamp.Light(this, "0 5 0", "0.9 0.8 0.6");
		return m_BR_Light != null;
	}
}

modded class Land_Power_Pole_Wood1_Lamp_Amp
{
	protected ExpansionLampLightBase_Lamp m_BR_Light;

	void Land_Power_Pole_Wood1_Lamp_Amp()
	{
		if (GetGame().IsServer())
			BR_SpeakerLamp.Wait(this);
	}

	void ~Land_Power_Pole_Wood1_Lamp_Amp()
	{
		if (GetGame() && m_BR_Light)
			GetGame().ObjectDelete(m_BR_Light);
	}

	bool BR_Light()
	{
		if (!m_BR_Light)
			m_BR_Light = BR_SpeakerLamp.Light(this, "-0.000977 3.65 0.418945", "1.0 0.7 0.4");
		return m_BR_Light != null;
	}
}

modded class Land_Power_Pole_Conc4_Lamp_Amp
{
	protected ExpansionLampLightBase_Lamp m_BR_Light;

	void Land_Power_Pole_Conc4_Lamp_Amp()
	{
		if (GetGame().IsServer())
			BR_SpeakerLamp.Wait(this);
	}

	void ~Land_Power_Pole_Conc4_Lamp_Amp()
	{
		if (GetGame() && m_BR_Light)
			GetGame().ObjectDelete(m_BR_Light);
	}

	bool BR_Light()
	{
		if (!m_BR_Light)
			m_BR_Light = BR_SpeakerLamp.Light(this, "-0.81982 5.0445 -0.01017", "1.0 0.7 0.4");
		return m_BR_Light != null;
	}
}
#endif
