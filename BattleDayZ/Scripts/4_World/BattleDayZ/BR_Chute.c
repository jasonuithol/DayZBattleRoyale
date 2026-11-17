// The jump from the plane, on the character: starting it, the pack, the canopy every game
// shows, and the camera. The jump itself (the script command) is in 3_Game/BR_ChuteCommand.c.

modded class PlayerBase
{
	protected int m_BR_ChuteId;      // synced: the server bumps it to start a jump
	protected int m_BR_ChuteDoneId;  // local: the last jump this game has finished
	protected int m_BR_ChuteOpenId;  // synced: the jump whose chute the player opened (F)
	protected float m_BR_LoggedAt;
	protected Object m_BR_Canopy;    // client: the parachute above this player (local, visual only)
	protected EntityAI m_BR_Pack;    // server: the parachute pack on the jumper's back, for the jump only

	static const string BR_PACK = "AssaultBag_Black"; // a compact vanilla bag stands in for the rig

	override void Init()
	{
		super.Init();
		RegisterNetSyncVariableInt("m_BR_ChuteId");
		RegisterNetSyncVariableInt("m_BR_ChuteOpenId");
	}

	// server: out of the plane. No damage until shortly after landing: a disagreement between
	// the jumper's game and the server at the hand-over once dropped the player from 18 m.
	void BR_StartChute()
	{
		m_BR_ChuteId++;
		SetSynchDirty();
		SetAllowDamage(false);
		// the pack on the back, for the jump (only if the back is free: the kit has no bag)
		if (!m_BR_Pack && !FindAttachmentBySlotName("Back"))
			m_BR_Pack = GetInventory().CreateAttachment(BR_PACK);
	}

	protected void BR_Landed()
	{
		if (!GetGame().IsServer())
			return;
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(BR_DamageAgain, 3000, false);
		if (m_BR_Pack)
		{
			GetGame().ObjectDelete(m_BR_Pack); // the pack goes once they're down
			m_BR_Pack = null;
		}
	}

	protected void BR_DamageAgain()
	{
		if (!BR_IsChuting())
			SetAllowDamage(true);
	}

	// server: F during the fall
	void BR_OpenChute()
	{
		m_BR_ChuteOpenId = m_BR_ChuteId;
		SetSynchDirty();
	}

	bool BR_ChuteOpened()
	{
		return m_BR_ChuteOpenId == m_BR_ChuteId;
	}

	// still in free fall: the HUD asks for F
	bool BR_InFreeFall()
	{
		BR_ChuteCommand chute = BR_ChuteCommand.Cast(GetCommand_Script());
		return BR_IsChuting() && chute && !chute.IsOpen();
	}

	bool BR_IsChuting()
	{
		return m_BR_ChuteId != m_BR_ChuteDoneId;
	}

	// the plane's camera mode while watching from it; third person for the whole jump
	override int CameraHandler(int pCameraMode)
	{
		if (GetGame().GetPlayer() == this)
		{
			if (BR_Plane.s_View || BR_Watch.s_On)
				return DayZPlayerCameras.DAYZCAMERA_1ST_UNCONSCIOUS; // the plane view, or watching a player (BR_PlaneCamera.c)
			if (BR_IsChuting())
				return DayZPlayerCameras.DAYZCAMERA_3RD_ERC;
		}
		return super.CameraHandler(pCameraMode);
	}

	// The canopy every game shows above every jumper whose chute is open (this one included):
	// a local, visual-only object (no collision) attached to the character, so it follows and
	// turns with them. Model: BattleDayZ/data/parachute.p3d (blender/build_parachute.py).
	protected void BR_Canopy(bool open)
	{
		if (GetGame().IsDedicatedServer())
			return;
		if (open && !m_BR_Canopy)
		{
			m_BR_Canopy = GetGame().CreateObjectEx("BR_Parachute", GetPosition(), ECE_LOCAL);
			if (m_BR_Canopy)
			{
				m_BR_Canopy.SetPosition(vector.Zero);
				m_BR_Canopy.SetOrientation(vector.Zero);
				AddChild(m_BR_Canopy, -1);
			}
		}
		else if (!open && m_BR_Canopy)
		{
			RemoveChild(m_BR_Canopy);
			GetGame().ObjectDeleteOnClient(m_BR_Canopy);
			m_BR_Canopy = null;
		}
	}

	override void EEDelete(EntityAI parent)
	{
		BR_Canopy(false);
		super.EEDelete(parent);
	}

	override bool ModCommandHandlerBefore(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)
	{
		if (super.ModCommandHandlerBefore(pDt, pCurrentCommandID, pCurrentCommandFinished))
			return true;
		if (!BR_IsChuting() || !IsAlive())
		{
			BR_Canopy(false);
			return false;
		}

		BR_ChuteCommand chute = BR_ChuteCommand.Cast(GetCommand_Script());
		// once a second while jumping: what this game is running for the jumper
		float now = GetGame().GetTickTime();
		if (BR_Debug.LOG && now - m_BR_LoggedAt >= 1)
		{
			m_BR_LoggedAt = now;
			Print("[BattleDayZ] jumping (" + GetInstanceType() + "): command " + pCurrentCommandID + ", ours " + (chute != null) + ", finished " + pCurrentCommandFinished + ", " + BR_ChuteCommand.HeightAboveGround(GetPosition()) + " m up");
		}
		if (pCurrentCommandID == DayZPlayerConstants.COMMANDID_SCRIPT && chute)
		{
			if (!pCurrentCommandFinished)
			{
				chute.SetOpenAsked(BR_ChuteOpened());
				BR_Canopy(chute.IsOpen());
				return true; // still coming down
			}
			BR_Canopy(false);
			m_BR_ChuteDoneId = m_BR_ChuteId; // landed; vanilla goes back to walking
			BR_Landed();
			return false;
		}

		StartCommand_ScriptInst(BR_ChuteCommand);
		return true;
	}
}
