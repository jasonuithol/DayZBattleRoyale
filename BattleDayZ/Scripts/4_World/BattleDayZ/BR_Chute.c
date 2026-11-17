// The jump from the plane: free fall until the player opens the chute with F (or it opens by
// itself low down), then a slow, steerable glide to the ground. Nobody has a gun on the way down, so it's only movement.
//
// It's a script command (it replaces walking/falling while it runs). The server starts it by
// bumping m_BR_ChuteId, which is synced, so the server, the jumper's own game and everyone
// watching all run the same command from the same inputs. It ends on landing, without the
// fall damage a normal fall from that height would do.
class BR_ChuteCommand: HumanCommandScript
{
	static const float FREEFALL_DOWN = 22.5; // m/s: half a real skydiver's, for more time to pick a spot
	static const float FREEFALL_GLIDE = 30;  // m/s sideways, steering with the movement keys
	static const float CHUTE_DOWN = 6;
	static const float CHUTE_GLIDE = 9;
	static const float AUTO_OPEN_HEIGHT = 100; // m above the ground, if the player hasn't opened it
	static const float HANDOVER_HEIGHT = 1.5;  // m: vanilla's own fall takes over from here, and lands
	static const float BLOCKED_HEIGHT = 60;    // m: below this, being unable to go down is a landing (on a roof, a rock)
	static const float LOOK_DOWN = 150;        // m above the terrain: below this, roofs and the like count as the ground

	protected PlayerBase m_Player;
	protected HumanInputController m_Input;
	protected int m_VarStance;
	protected bool m_Finished;
	protected bool m_Open;
	protected bool m_Landing;
	protected bool m_Airborne;  // has been properly in the air: only then can it land
	protected float m_Age;
	protected float m_TargetY;  // where it should be by now: the descent is steered to this
	protected float m_Blocked;  // how long it's been stuck well above its target (a roof, a rock)
	protected float m_Speed;    // diagnostics: the movement input
	protected vector m_Dir;
	protected int m_Pre;        // diagnostics: how often each update ran
	protected int m_Post;
	protected float m_LogAt;

	void BR_ChuteCommand(Human human)
	{
		m_Player = PlayerBase.Cast(human);
		m_Input = human.GetInputController();
		m_VarStance = human.GetAnimInterface().BindVariableInt("Stance");
	}

	bool IsOpen()
	{
		return m_Open;
	}

	static float HeightAboveGround(vector pos)
	{
		return pos[1] - GetGame().SurfaceY(pos[0], pos[2]);
	}

	// What it would land on: the terrain, or whatever is under the player above it (a roof, a
	// car, a rock), seen with a ray straight down through the collision geometry. The terrain
	// alone pulled a jumper coming down on a roof through it to the ground. Plants don't count
	// (a treetop is no place to land): the terrain under them does.
	static float LandingHeight(vector pos, Object ignore)
	{
		float terrain = GetGame().SurfaceY(pos[0], pos[2]);
		if (pos[1] - terrain > LOOK_DOWN)
			return terrain;
		vector hit, normal;
		int comp;
		set<Object> objects = new set<Object>;
		vector from = Vector(pos[0], pos[1] + 0.5, pos[2]);
		vector to = Vector(pos[0], terrain - 0.5, pos[2]);
		if (!DayZPhysics.RaycastRV(from, to, hit, normal, comp, objects, null, ignore, true, false, ObjIntersectGeom))
			return terrain;
		for (int i = 0; i < objects.Count(); i++)
		{
			if (objects[i] && (objects[i].IsTree() || objects[i].IsBush()))
				return terrain;
		}
		return Math.Max(hit[1], terrain);
	}

	// Something solid just under the player (a floor, a roof): standing, not in the air, however
	// high above the terrain that is (the lobby in the sky is a kilometre up)
	static bool OnSomething(vector pos, Object ignore)
	{
		vector hit, normal;
		int comp;
		set<Object> objects = new set<Object>;
		return DayZPhysics.RaycastRV(pos + "0 0.5 0", pos - "0 5 0", hit, normal, comp, objects, null, ignore, true, false, ObjIntersectGeom);
	}

	// The pose comes from the stance alone: lying on the belly in free fall, standing under the
	// chute. No fall or land animation commands: calling the land animation from here and then
	// ending the command froze the jumper's own game every time. Instead the command hands over
	// HANDOVER_HEIGHT above the ground, and vanilla starts its own fall command, which lands the
	// player properly (a short, light landing).
	override int GetCurrentStance()
	{
		if (m_Open)
			return DayZPlayerConstants.STANCEIDX_ERECT;
		return DayZPlayerConstants.STANCEIDX_PRONE;
	}

	// Gravity is off for the jump, on every machine running it: the jumper's own game couldn't
	// push back against it (it fell up to 40 m behind the server, which then yanked it back up).
	override void OnActivate()
	{
		dBodyEnableGravity(m_Player, false);
		Print("[BattleDayZ] chute command started (" + m_Player.GetInstanceType() + ") " + HeightAboveGround(m_Player.GetPosition()) + " m up");
	}

	override void OnDeactivate()
	{
		dBodyEnableGravity(m_Player, true);
		Print("[BattleDayZ] chute command ended (" + m_Player.GetInstanceType() + ") " + HeightAboveGround(m_Player.GetPosition()) + " m up, open " + m_Open + ", " + m_Pre + " pre / " + m_Post + " post updates");
	}

	override void PreAnimUpdate(float pDt)
	{
		SetHeading(m_Input.GetHeadingAngle()); // face where the camera looks
		PreAnim_SetInt(m_VarStance, GetCurrentStance());

		// landed (seen last frame in PrePhysUpdate): finish. No hands-up gesture under the chute:
		// neither as a modifier (AddCommandModifier_Action) nor straight through the anim graph
		// (CMD_Action) does it play during a script command.
		if (m_Landing)
		{
			m_Finished = true;
			SetFlagFinished(true);
		}
	}

	// Everything happens here (it runs every frame): the height is steered to a target that
	// comes down at the free fall or chute speed. The game's own gravity keeps adding to the
	// fall under a script command, so a fixed speed isn't enough: each frame makes up whatever
	// gap there is to the target, up or down.
	override void PrePhysUpdate(float pDt)
	{
		m_Pre++;
		m_Age += pDt;
		vector pos = m_Player.GetPosition();
		float agl = HeightAboveGround(pos);
		if (m_Airborne)
			agl = pos[1] - LandingHeight(pos, m_Player); // roofs count once it's in the air

		// the jumper's own game can hear about the jump before the move out to the plane: wait,
		// still standing in the lobby (on the ground or up in the sky), until it's in the air
		// (10 s at most)
		if (!m_Airborne)
		{
			if (agl > 5 && !OnSomething(pos, m_Player))
			{
				m_Airborne = true;
				m_TargetY = pos[1];
			}
			else
			{
				if (m_Age > 10)
					Land("never got into the air", agl);
				PrePhys_SetTranslation(vector.Zero);
				return;
			}
		}

		if (!m_Open && (m_Player.BR_ChuteOpened() || agl < AUTO_OPEN_HEIGHT))
			m_Open = true;
		float down = FREEFALL_DOWN;
		float glide = FREEFALL_GLIDE;
		if (m_Open)
		{
			down = CHUTE_DOWN;
			glide = CHUTE_GLIDE;
		}

		// The server's height wins: when it corrects this game's position (a frame's head start
		// makes the two targets drift apart by a few metres), take the corrected height as the
		// target instead of steering back against it, which jittered up and down.
		if (!GetGame().IsServer() && Math.AbsFloat(pos[1] - m_TargetY) > 0.5)
			m_TargetY = pos[1];

		float ground = pos[1] - agl;
		m_TargetY = Math.Max(m_TargetY - down * pDt, ground);

		// the keys (BR_Steer): the jumper's own game reads them, the server hears them from it.
		// The character faces the camera (SetHeading), so this is relative to the view.
		vector dir = vector.Zero;
		if (GetGame().IsServer())
		{
			if (m_Player.GetIdentity())
				dir = BR_Steer.ForPlayer(m_Player.GetIdentity().GetId());
		}
		else if (m_Player == GetGame().GetPlayer())
			dir = BR_Steer.s_Local;
		m_Speed = dir.Length();
		m_Dir = dir;
		vector move = vector.Zero;
		if (dir.Length() > 0.05)
		{
			if (dir.Length() > 1)
				dir = dir.Normalized();
			move = dir * glide * pDt;
		}
		move[1] = m_TargetY - pos[1];
		PrePhys_SetTranslation(move);

		// landed; or stuck on something (a roof, a rock) well above where it should be, low down
		if (agl < HANDOVER_HEIGHT)
			Land("ground", agl);
		if (agl < BLOCKED_HEIGHT && pos[1] > m_TargetY + 3)
			m_Blocked += pDt;
		else
			m_Blocked = 0;
		if (m_Blocked > 1.0)
			Land("blocked", agl);

		if (BR_Debug.LOG && m_Age - m_LogAt >= 2)
		{
			m_LogAt = m_Age;
			Print("[BattleDayZ] chute (" + m_Player.GetInstanceType() + ") t " + m_Age + " agl " + agl + " y " + pos[1] + " target " + m_TargetY + " open " + m_Open + " input " + m_Speed + " " + m_Dir + " pre " + m_Pre + " post " + m_Post);
		}
	}

	protected void Land(string why, float agl)
	{
		if (m_Landing)
			return;
		m_Landing = true; // finished next frame, in PreAnimUpdate
		string who = "";
		if (m_Player.GetIdentity())
			who = m_Player.GetIdentity().GetName() + " ";
		Print("[BattleDayZ] chute landing " + who + "(" + m_Player.GetInstanceType() + ") " + why + ", " + agl + " m up at " + m_Player.GetPosition());
	}

	override bool PostPhysUpdate(float pDt)
	{
		m_Post++;
		return !m_Finished;
	}
}

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
			if (BR_Plane.s_View)
				return DayZPlayerCameras.DAYZCAMERA_1ST_UNCONSCIOUS; // the plane view (BR_PlaneCamera.c)
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
