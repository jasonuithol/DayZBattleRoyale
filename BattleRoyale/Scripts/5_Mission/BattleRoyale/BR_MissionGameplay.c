// Client: the two HUD lines at the top of the screen, the blue world outside the circle, and
// F (out of the plane, then open the chute). The view and object distances are capped by
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
	protected bool m_BR_Tinted;
	protected float m_BR_JumpAsked;
	protected float m_BR_LeftPlaneAt;

	override void OnMissionStart()
	{
		super.OnMissionStart();
		if (!PPERequesterBank.GetRequester(PPERequester_BRZone))
			PPERequesterBank.RegisterRequester(PPERequester_BRZone);
		m_BR_Root = GetGame().GetWorkspace().CreateWidgets("BattleRoyale/gui/layouts/br_hud.layout");
		if (m_BR_Root)
		{
			m_BR_Title = TextWidget.Cast(m_BR_Root.FindAnyWidget("Title"));
			m_BR_Subtitle = TextWidget.Cast(m_BR_Root.FindAnyWidget("Subtitle"));
			m_BR_Root.Show(false);
		}
	}

	override void OnMissionFinish()
	{
		BR_Tint(false);
		BR_Plane.SetView(false);
		if (m_BR_Root)
			m_BR_Root.Unlink();
		m_BR_Root = null;
		super.OnMissionFinish();
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);
		BR_UpdateHud();
		BR_Heartbeat();
	}

	// a line every 2 s while in a round, so a hang shows in the log as the lines stopping
	protected float m_BR_BeatAt;
	protected void BR_Heartbeat()
	{
		float now = GetGame().GetTickTime();
		if (now - m_BR_BeatAt < 2)
			return;
		m_BR_BeatAt = now;
		PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
		BR_State state = BR_Client.s_State;
		if (!player || !state || state.phase != BR_Phase.GAME)
			return;
		Print("[BattleRoyale] heartbeat: " + BR_ChuteCommand.HeightAboveGround(player.GetPosition()) + " m up, chuting " + player.BR_IsChuting() + ", command " + player.GetCommand_Script());
	}

	protected void BR_UpdateHud()
	{
		BR_State state = BR_Client.s_State;
		// nothing heard for a while: not on a battle royale server, or it went quiet
		if (!state || GetGame().GetTickTime() - BR_Client.s_ReceivedAt > 5)
		{
			if (m_BR_Root)
				m_BR_Root.Show(false);
			BR_Tint(false);
			BR_Plane.SetView(false);
			return;
		}

		string subtitle = state.subtitle;
		bool outside = false;
		PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
		if (player && player.IsAlive() && state.phase == BR_Phase.GAME && state.inRound)
		{
			float by = state.OutsideBy(player.GetPosition());
			if (by > 0)
			{
				outside = true;
				subtitle = "OUTSIDE THE CIRCLE - " + Math.Ceil(by).ToString() + " m to safety | " + subtitle;
			}
		}
		BR_Tint(outside);
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
			Print("[BattleRoyale] plane view " + riding);
		BR_Plane.SetView(riding);



		if (m_BR_Root && m_BR_Title && m_BR_Subtitle)
		{
			m_BR_Root.Show(true);
			m_BR_Title.SetText(state.title);
			m_BR_Subtitle.SetText(subtitle);
			if (outside)
				m_BR_Subtitle.SetColor(ARGB(255, 120, 170, 255));
			else
				m_BR_Subtitle.SetColor(ARGB(255, 255, 255, 255));
		}
	}

	// F on the plane jumps, F in free fall opens the chute (not while typing in chat or in a
	// menu); at most once a second while the server catches up. KeyState remembers a press
	// until it's cleared, so it's cleared every time it's read.
	protected void BR_JumpKey(BR_State state, bool falling)
	{
		bool pressed = KeyState(KeyCode.KC_F) != 0;
		if (pressed)
			ClearKey(KeyCode.KC_F);
		if (!pressed || !(state.inPlane || falling) || GetGame().GetUIManager().GetMenu())
			return;
		float now = GetGame().GetTickTime();
		if (now - m_BR_JumpAsked < 1)
			return;
		m_BR_JumpAsked = now;
		Print("[BattleRoyale] F: in the plane " + state.inPlane + ", falling " + falling);
		BR_Client.RequestJump();
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

	protected void BR_Tint(bool on)
	{
		if (on == m_BR_Tinted)
			return;
		PPERequesterBase req = PPERequesterBank.GetRequester(PPERequester_BRZone);
		if (!req)
			return;
		if (on)
			req.Start();
		else
			req.Stop();
		m_BR_Tinted = on;
	}
}
