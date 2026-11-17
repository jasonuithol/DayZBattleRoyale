// Client: the two HUD lines at the top of the screen, the storm outside the circle (a vanilla
// gas zone's effects on the player, and a wall of its gas along the nearby edge so it can be
// seen coming), and F (out of the plane, then open the chute). The view and object distances are capped by
// FlyingWithoutCTD while on a battle royale server.
//
// On the plane the player watches from the plane's camera mode (BR_PlaneCamera.c) while the
// character waits in the lobby. Not a 'staticcamera': switched off in gameplay it left the
// player's view stuck.
modded class MissionGameplay
{
	protected Widget m_BR_Root;
	protected TextWidget m_BR_Title;
	protected TextWidget m_BR_Subtitle;
	protected ref array<Particle> m_BR_Wall = new array<Particle>(); // gas along the nearby edge
	protected ref array<Object> m_BR_Panels = new array<Object>();   // the glowing wall along it

	static const float BR_WALL_NEAR = 800;     // m from the edge: closer than this, the wall shows
	static const float BR_WALL_SPACING = 30;   // m between gas clouds along the edge
	static const float BR_WALL_DEPTH = 50;     // m between the two rows
	static const int BR_WALL_CLOUDS = 25;      // clouds either side of the nearest point, per row
	static const float BR_PANEL_WIDTH = 10;    // m, the wall model's width (blender/build_stormwall.py)
	static const int BR_PANELS = 60;           // panels either side of the nearest point (1.2 km of wall)
	protected float m_BR_JumpAsked;
	protected float m_BR_LeftPlaneAt;

	override void OnMissionStart()
	{
		super.OnMissionStart();
		m_BR_Root = GetGame().GetWorkspace().CreateWidgets("BattleDayZ/gui/layouts/br_hud.layout");
		if (m_BR_Root)
		{
			m_BR_Title = TextWidget.Cast(m_BR_Root.FindAnyWidget("Title"));
			m_BR_Subtitle = TextWidget.Cast(m_BR_Root.FindAnyWidget("Subtitle"));
			m_BR_Root.Show(false);
		}
	}

	override void OnMissionFinish()
	{
		BR_Storm(null, false);
		BR_Wall(null, vector.Zero);
		BR_Plane.SetView(false);
		if (m_BR_Root)
			m_BR_Root.Unlink();
		m_BR_Root = null;
		BR_ClientLobby.Clear(); // this game's own lobby goes with the connection
		BR_PlaneSound.Stop();
		super.OnMissionFinish();
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);
		BR_ClientLobby.Update();
		BR_PlaneSound.Update();
		BR_UpdateHud();
		BR_Heartbeat();
	}

	// a line every 2 s while in a round, so a hang shows in the log as the lines stopping
	protected float m_BR_BeatAt;
	protected void BR_Heartbeat()
	{
		if (!BR_Debug.LOG)
			return;
		float now = GetGame().GetTickTime();
		if (now - m_BR_BeatAt < 2)
			return;
		m_BR_BeatAt = now;
		PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
		BR_State state = BR_Client.s_State;
		if (!player || !state || state.phase != BR_Phase.GAME)
			return;
		Print("[BattleDayZ] heartbeat: " + BR_ChuteCommand.HeightAboveGround(player.GetPosition()) + " m up, chuting " + player.BR_IsChuting() + ", command " + player.GetCommand_Script());
	}

	protected void BR_UpdateHud()
	{
		BR_State state = BR_Client.s_State;
		// nothing heard for a while: not on a battle royale server, or it went quiet
		if (!state || GetGame().GetTickTime() - BR_Client.s_ReceivedAt > 5)
		{
			if (m_BR_Root)
				m_BR_Root.Show(false);
			BR_Storm(null, false);
			BR_Wall(null, vector.Zero);
			BR_Plane.SetView(false);
			return;
		}

		string subtitle = state.subtitle;
		bool outside = false;
		PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
		if (player && player.IsAlive() && state.phase == BR_Phase.GAME && state.inRound)
		{
			float by = -1;
			if (!state.wholeMap)
			{
				vector nowCenter;
				float nowRadius;
				BR_Client.CircleNow(nowCenter, nowRadius);
				by = BR_Loot.Distance2D(player.GetPosition(), nowCenter) - nowRadius;
			}
			if (by > 0)
			{
				outside = true;
				if (state.radius > 0)
					subtitle = "IN THE STORM - " + Math.Ceil(by).ToString() + " m to safety | " + subtitle;
				else
					subtitle = "IN THE STORM | " + subtitle;
			}
		}
		BR_Storm(player, outside);
		if (player && player.IsAlive() && state.phase == BR_Phase.GAME && !state.wholeMap)
			BR_Wall(state, player.GetPosition());
		else
			BR_Wall(null, vector.Zero);
		if (player && player.IsAlive() && player.BR_IsChuting())
			BR_Steer.Update();
		bool falling = player && player.IsAlive() && player.BR_InFreeFall();
		if (falling)
			subtitle = "Press F to open your parachute";
		else if (player && player.IsAlive() && player.BR_IsChuting())
			subtitle = "Parachute open: steer with the movement keys";
		BR_JumpKey(state, falling);

		// the view from the plane until this player's character has actually been moved out to
		// it (not straight back to a character still standing in the lobby, kilometres away)
		bool riding = false;
		float now = GetGame().GetTickTime();
		if (player && player.IsAlive())
		{
			if (state.inPlane && state.flying)
			{
				riding = true;
				m_BR_LeftPlaneAt = 0;
			}
			else if (BR_Plane.s_View)
			{
				if (m_BR_LeftPlaneAt == 0)
					m_BR_LeftPlaneAt = now;
				riding = BR_Loot.Distance2D(player.GetPosition(), BR_Plane.Now()) > 1000 && now - m_BR_LeftPlaneAt < 10;
			}
		}
		if (BR_Plane.s_View != riding)
			Print("[BattleDayZ] plane view " + riding);
		BR_Plane.SetView(riding);



		if (m_BR_Root && m_BR_Title && m_BR_Subtitle)
		{
			m_BR_Root.Show(true);
			m_BR_Title.SetText(state.title);
			m_BR_Subtitle.SetText(subtitle);
			if (outside)
				m_BR_Subtitle.SetColor(ARGB(255, 240, 190, 110));
			else
				m_BR_Subtitle.SetColor(ARGB(255, 255, 255, 255));
		}
	}

	// F in the lobby says ready to start, F on the plane jumps, F in free fall opens the chute
	// (not while typing in chat or in a menu); at most once a second while the server catches up. KeyState remembers a press
	// until it's cleared, so it's cleared every time it's read.
	protected void BR_JumpKey(BR_State state, bool falling)
	{
		bool pressed = KeyState(KeyCode.KC_F) != 0;
		if (pressed)
			ClearKey(KeyCode.KC_F);
		bool lobby = state.phase == BR_Phase.LOBBY && !state.inRound;
		if (!pressed || !(state.inPlane || falling || lobby) || GetGame().GetUIManager().GetMenu())
			return;
		float now = GetGame().GetTickTime();
		if (now - m_BR_JumpAsked < 1)
			return;
		m_BR_JumpAsked = now;
		Print("[BattleDayZ] F: in the lobby " + lobby + ", in the plane " + state.inPlane + ", falling " + falling);
		BR_Client.RequestJump(BR_SeenPlane());
	}

	// FlyingWithoutCTD: capped for the whole time on a battle royale server (it comes back on
	// leaving). The game froze twice the moment the full view distance came back after a jump,
	// and a battle royale is played at 3 km anyway.
	override protected string FW_WhyFlying(Transport ride)
	{
		if (BR_Client.s_State)
			return "a battle royale server";
		return super.FW_WhyFlying(ride);
	}

	// In the storm: everything a vanilla gas zone does to a player standing in it (the gas round
	// them, the yellow tint and grain, the ambient sound, muffled hearing), switched on and off here
	// rather than by a zone's trigger
	protected PlayerBase m_BR_GasOn;
	protected void BR_Storm(PlayerBase player, bool on)
	{
		if (m_BR_GasOn && (!on || m_BR_GasOn != player))
		{
			m_BR_GasOn.SetContaminatedEffectEx(false, PPERequesterBank.REQ_CONTAMINATEDAREA);
			m_BR_GasOn = null;
		}
		if (on && player && !m_BR_GasOn)
		{
			player.SetContaminatedEffectEx(true, PPERequesterBank.REQ_CONTAMINATEDAREA, ParticleList.CONTAMINATED_AREA_GAS_AROUND, ParticleList.CONTAMINATED_AREA_GAS_TINY, "ContaminatedArea_SoundSet");
			m_BR_GasOn = player;
		}
	}

	// the storm's edge along the stretch nearest the player, so it can be seen coming from
	// inside: the glowing wall (a panel every BR_PANEL_WIDTH m) and a gas zone's big clouds
	// behind it in two rows, on the edge and BR_WALL_DEPTH m out, every BR_WALL_SPACING m. Moved
	// every frame with the closing circle. Each panel and cloud has a fixed place round the
	// circle (they're all alike, so which one fills a place doesn't show): walking along the
	// edge doesn't slide them. Gone when the player is far from the edge (state null: clear it)
	protected void BR_Wall(BR_State state, vector pos)
	{
		array<vector> spots = new array<vector>();
		array<vector> panels = new array<vector>();   // pairs: where, and which way it faces
		vector c;
		float radius;
		if (state)
			BR_Client.CircleNow(c, radius);
		if (state && radius > 0)
		{
			float dx = pos[0] - c[0];
			float dz = pos[2] - c[2];
			float dist = Math.Sqrt(dx * dx + dz * dz);
			if (Math.AbsFloat(dist - radius) < BR_WALL_NEAR)
			{
				float a0 = Math.Atan2(dz, dx);
				float step = BR_WALL_SPACING / radius;
				int n = Math.Min(BR_WALL_CLOUDS, Math.Floor(Math.PI / step));
				// the glowing wall, one panel every BR_PANEL_WIDTH m, standing on the lower of
				// its two ends (the bright line along its foot is the circle on the ground)
				float pstep = BR_PANEL_WIDTH / radius;
				int pn = Math.Min(BR_PANELS, Math.Floor(Math.PI / pstep));
				float pbase = Math.Round(a0 / pstep) * pstep;
				for (int q = -pn; q <= pn; q++)
				{
					float pa = pbase + q * pstep;
					vector radial = Vector(Math.Cos(pa), 0, Math.Sin(pa));
					vector mid = c + radial * radius;
					vector along = Vector(-radial[2], 0, radial[0]) * (BR_PANEL_WIDTH / 2);
					vector e1 = mid + along;
					vector e2 = mid - along;
					mid[1] = Math.Min(GetGame().SurfaceY(e1[0], e1[2]), GetGame().SurfaceY(e2[0], e2[2])) - 0.2;
					panels.Insert(mid);
					panels.Insert(radial);
				}
				float base = Math.Round(a0 / step) * step;
				for (int row = 0; row < 2; row++)
				{
					float r = radius + row * BR_WALL_DEPTH;
					float shift = row * step / 2; // the second row fills the first's gaps
					for (int k = -n; k <= n; k++)
					{
						float a = base + k * step + shift;
						float x = c[0] + Math.Cos(a) * r;
						float z = c[2] + Math.Sin(a) * r;
						spots.Insert(Vector(x, GetGame().SurfaceY(x, z), z));
					}
				}
			}
		}

		// reuse the clouds already playing, add or stop the rest
		for (int i = 0; i < spots.Count(); i++)
		{
			if (i < m_BR_Wall.Count() && m_BR_Wall[i])
				m_BR_Wall[i].SetPosition(spots[i]);
			else
			{
				Particle p = Particle.PlayInWorld(ParticleList.CONTAMINATED_AREA_GAS_BIGASS, spots[i]);
				if (i < m_BR_Wall.Count())
					m_BR_Wall[i] = p;
				else
					m_BR_Wall.Insert(p);
			}
		}
		for (int j = m_BR_Wall.Count() - 1; j >= spots.Count(); j--)
		{
			if (m_BR_Wall[j])
				m_BR_Wall[j].Stop();
			m_BR_Wall.Remove(j);
		}

		// the same for the wall's panels (local objects: only this game has them)
		int want = panels.Count() / 2;
		for (int w = 0; w < want; w++)
		{
			Object panel;
			if (w < m_BR_Panels.Count())
				panel = m_BR_Panels[w];
			if (!panel)
			{
				panel = GetGame().CreateObjectEx("BR_StormWall", panels[w * 2], ECE_LOCAL);
				if (w < m_BR_Panels.Count())
					m_BR_Panels[w] = panel;
				else
					m_BR_Panels.Insert(panel);
			}
			if (panel)
			{
				panel.SetPosition(panels[w * 2]);
				panel.SetDirection(panels[w * 2 + 1]);
			}
		}
		for (int g = m_BR_Panels.Count() - 1; g >= want; g--)
		{
			if (m_BR_Panels[g])
				GetGame().ObjectDeleteOnClient(m_BR_Panels[g]);
			m_BR_Panels.Remove(g);
		}
	}
}
