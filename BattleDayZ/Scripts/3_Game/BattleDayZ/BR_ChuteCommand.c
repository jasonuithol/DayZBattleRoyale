// The jump's script command (the free fall and the chute), started by the character in
// 4_World/BR_Chute.c. It lives here in 3_Game, not with the character in 4_World: the game keeps
// a finished script command until its own shutdown, and one whose class was in 4_World (the
// script module unloaded first) crashed every game and server that had run a jump, as it shut
// down. A command that did nothing at all crashed it the same way; one in 3_Game doesn't. So it
// knows the character only as a DayZPlayer, and the character tells it whether F was pressed.
// The jump from the plane: free fall until the player opens the chute with F (or it opens by
// itself low down), then a glide to the ground under the canopy. Nobody has a gun on the way
// down, so it's only movement. The keys manoeuvre: in free fall they track, dive, plummet, brake
// and drift, the body tilting and its limbs moving with them; under the canopy they speed up,
// slow down, sink and bank into spiralling turns, the jumper swinging under the canopy. A script
// can't tilt a character, so the mod's own player animation graph does it (tools/make_anims.py),
// from variables this sets every frame; the jumper's own camera goes with it (BR_ChuteCamera.c).
//
// It's a script command (it replaces walking/falling while it runs). The server starts it by
// bumping m_BR_ChuteId, which is synced, so the server, the jumper's own game and everyone
// watching all run the same command from the same inputs. It ends on landing, without the
// fall damage a normal fall from that height would do.
class BR_ChuteCommand: HumanCommandScript
{
	static const float FREEFALL_DOWN = 22.5; // m/s: half a real skydiver's, for more time to pick a spot
	// Free fall, in m/s forward (the way the camera looks) and sideways, by the keys held:
	static const float FREEFALL_DRIFT = 8;   // none: a modest forward drift
	static const float FREEFALL_TRACK = 30;  // W: tilted forward, tracking
	static const float FREEFALL_DIVE = 40;   // W and shift: steeper still, and faster down too
	static const float FREEFALL_DIVE_DOWN = 32;
	static const float FREEFALL_PLUMMET = 50; // shift: head down, straight down fast, no forward
	static const float FREEFALL_BRAKE = 2;   // S: tilted back, hardly any forward
	static const float FREEFALL_SIDE = 8;    // A or D: rolled that way, drifting that way
	static const float FREEFALL_ACCEL = 12;  // m/s per s: how quickly the speeds change
	static const float CHUTE_BRAKE = 30;     // m/s per s: how quickly the opening chute slows the fall
	// and how far the jumper tilts (degrees), and how quickly
	static const float TILT_TRACK = 25;
	static const float TILT_DIVE = 45;
	static const float TILT_PLUMMET = 80;
	static const float TILT_BRAKE = 25;
	static const float TILT_ROLL = 30;
	static const float TILT_RATE = 90;       // degrees per s
	// The lying pose throughout (no stance change: that plays standing up). Tilted forward, the
	// arms go straight down the sides as if standing (the graph has the exact rotations);
	// tilted back, the arms and legs spread SPREAD further apart (degrees, at the full brake)
	static const float SPREAD = 25;
	static const float CHUTE_DOWN = 6;
	// Under the canopy, in m/s and degrees: the canopy glides forward; W lets it lead (the
	// jumper trails, the whole assembly tilts forward) and glide faster, S the opposite (hardly
	// any forward); A or D bank it, the jumper swinging out, and drift it that way. The canopy
	// is the fulcrum: the jumper swings about it, CHUTE_FULCRUM m above the hips.
	static const float CHUTE_GLIDE = 6;
	static const float CHUTE_GLIDE_FAST = 12;
	static const float CHUTE_GLIDE_SLOW = 1;
	static const float CHUTE_SIDE = 5;
	static const float CHUTE_TILT_FORWARD = 20;
	static const float CHUTE_TILT_BACK = 15;
	static const float CHUTE_TILT_SIDE = 20;
	static const float CHUTE_SWING_RATE = 30;  // degrees per s: a slower, pendulum swing
	static const float CHUTE_DOWN_FAST = 12;   // shift: sinking faster
	static const float CHUTE_TURN_RATE = 25;   // degrees per s at the full bank: banked, it turns that way and spirals down
	static const float CHUTE_FULCRUM = 6.65;   // m: the canopy (7.65 m up, blender/build_parachute.py) above the hips
	static const float AUTO_OPEN_HEIGHT = 150; // m above the ground, if the player hasn't opened it (it opens once level, below)
	static const float LEVEL = 1;              // degrees: the tilt counted as level, neutral, for the chute to open
	static const float HANDS_UP = 1;           // s: under the canopy, the hands go up to the lines over this long
	static const float HANDOVER_HEIGHT = 1.5;  // m: vanilla's own fall takes over from here, and lands
	static const float BLOCKED_HEIGHT = 60;    // m: below this, being unable to go down is a landing (on a roof, a rock)
	static const float LOOK_DOWN = 150;        // m above the terrain: below this, roofs and the like count as the ground

	protected DayZPlayer m_Player;
	protected bool m_OpenAsked; // the player pressed F: the character tells it every frame (SetOpenAsked)
	protected HumanInputController m_Input;
	protected int m_VarStance;
	protected int m_VarPitch;   // the graph's BR_FallPitch and BR_FallRoll (tools/make_anims.py)
	protected int m_VarRoll;
	protected int m_VarSpread;
	protected int m_VarArms;
	protected int m_VarHands;
	protected int m_VarGrip;
	protected int m_VarSwayX;
	protected int m_VarSwayY;
	protected int m_VarSwayZ;
	protected bool m_Opening;   // asked to open: back to neutral free fall first, then the canopy
	protected float m_Hands;    // 0 to 1: under the canopy, the hands up to the lines
	protected bool m_TiltGiven; // another player's jumper: the tilt comes from the server (SetTilt)
	protected float m_GivenPitch;
	protected float m_GivenRoll;
	protected float m_Turn;     // degrees the canopy has turned the jumper from where they look (banked turns)
	protected float m_GivenTurn;
	protected bool m_Finished;
	protected bool m_Open;
	protected bool m_Landing;
	protected bool m_Airborne;  // has been properly in the air: only then can it land
	protected float m_Age;
	protected float m_TargetY;  // where it should be by now: the descent is steered to this
	protected float m_Blocked;  // how long it's been stuck well above its target (a roof, a rock)
	protected float m_Forward;  // free fall: m/s forward, sideways and down now, eased towards the keys'
	protected float m_Side;
	protected float m_Down = FREEFALL_DOWN;
	protected float m_Pitch;    // degrees: + leans forward (head down)
	protected float m_Roll;     // degrees: + rolls right
	protected float m_Speed;    // diagnostics: the movement input
	protected vector m_Dir;
	protected int m_Pre;        // diagnostics: how often each update ran
	protected int m_Post;
	protected float m_LogAt;

	void BR_ChuteCommand(Human human)
	{
		m_Player = DayZPlayer.Cast(human);
		m_Input = human.GetInputController();
		m_VarStance = human.GetAnimInterface().BindVariableInt("Stance");
		m_VarPitch = human.GetAnimInterface().BindVariableFloat("BR_FallPitch");
		m_VarRoll = human.GetAnimInterface().BindVariableFloat("BR_FallRoll");
		m_VarSpread = human.GetAnimInterface().BindVariableFloat("BR_FallSpread");
		m_VarArms = human.GetAnimInterface().BindVariableFloat("BR_FallArms");
		m_VarHands = human.GetAnimInterface().BindVariableFloat("BR_ChuteHands");
		m_VarGrip = human.GetAnimInterface().BindVariableFloat("BR_ChuteGrip");
		m_VarSwayX = human.GetAnimInterface().BindVariableFloat("BR_ChuteSwayX");
		m_VarSwayY = human.GetAnimInterface().BindVariableFloat("BR_ChuteSwayY");
		m_VarSwayZ = human.GetAnimInterface().BindVariableFloat("BR_ChuteSwayZ");
	}

	bool IsOpen()
	{
		return m_Open;
	}

	// the tilt, in degrees (+ forward, head down; + rolled right). A script can't tilt the
	// character: the mod's player animation graph does, from BR_FallPitch and BR_FallRoll
	// (tools/make_anims.py), which this sets every frame
	float TiltPitch()
	{
		return m_Pitch;
	}

	float TiltRoll()
	{
		return m_Roll;
	}

	// m/s through the air: forward, sideways and down together (the camera's wind shake)
	float AirSpeed()
	{
		return Math.Sqrt(m_Forward * m_Forward + m_Side * m_Side + m_Down * m_Down);
	}

	// the velocity through the air, in the world (the wind particles streaming past)
	vector AirVelocity()
	{
		vector ahead = m_Player.GetDirection();
		ahead[1] = 0;
		if (ahead.Length() < 0.01)
			ahead = "0 0 1";
		ahead.Normalize();
		vector right = Vector(ahead[2], 0, -ahead[0]);
		vector v = ahead * m_Forward + right * m_Side;
		v[1] = -m_Down;
		return v;
	}

	// someone else's jump, on this game: their tilt, as the server has it (it has their keys)
	void SetTilt(float pitch, float roll, float turn)
	{
		m_TiltGiven = true;
		m_GivenPitch = pitch;
		m_GivenRoll = roll;
		m_GivenTurn = turn;
	}

	float Turn()
	{
		return m_Turn;
	}

	void SetOpenAsked(bool asked)
	{
		m_OpenAsked = asked;
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
		Print("[BattleDayZ] chute command started (" + m_Player.GetInstanceType() + ") " + HeightAboveGround(m_Player.GetPosition()) + " m up, tilt variables " + m_VarPitch + " " + m_VarRoll);
	}

	override void OnDeactivate()
	{
		dBodyEnableGravity(m_Player, true);
		Print("[BattleDayZ] chute command ended (" + m_Player.GetInstanceType() + ") " + HeightAboveGround(m_Player.GetPosition()) + " m up, open " + m_Open + ", " + m_Pre + " pre / " + m_Post + " post updates");
	}

	override void PreAnimUpdate(float pDt)
	{
		// face where the camera looks, turned further by the canopy's banked turns
		if (m_TiltGiven)
			m_Turn = m_GivenTurn;
		SetHeading(m_Input.GetHeadingAngle() + m_Turn * Math.DEG2RAD);
		PreAnim_SetInt(m_VarStance, GetCurrentStance());
		// the tilt, back to none on the way out: the graph keeps what it was last given
		if (m_Landing)
		{
			m_Pitch = 0;
			m_Roll = 0;
			m_Hands = 0;
		}
		PreAnim_SetFloat(m_VarHands, m_Hands);
		PreAnim_SetFloat(m_VarGrip, m_Hands); // the hands close on the lines as they go up
		// the limbs follow the tilt (so other games, which get only the tilt, show them too):
		// forward, the arms down the sides; back, the limbs spread
		float arms = 0;
		float spread = 0;
		// (under the canopy the tilt is the swing: the limbs keep the hands-up pose)
		if (!m_Open && m_Pitch > 0)
			arms = Math.Min(m_Pitch / TILT_TRACK, 1);
		else if (!m_Open)
			spread = Math.Min(-m_Pitch / TILT_BRAKE, 1) * SPREAD;
		// under the canopy, the hips go where swinging about the canopy puts them: forward
		// tilt, the jumper trails back and a little up; banked, they swing out to the other side
		vector sway = vector.Zero;
		if (m_Open)
		{
			float p = m_Pitch * Math.DEG2RAD;
			float r = m_Roll * Math.DEG2RAD;
			sway[0] = -CHUTE_FULCRUM * Math.Sin(r);
			sway[1] = CHUTE_FULCRUM * ((1 - Math.Cos(p)) + (1 - Math.Cos(r)));
			sway[2] = -CHUTE_FULCRUM * Math.Sin(p);
		}
		PreAnim_SetFloat(m_VarSwayX, sway[0]);
		PreAnim_SetFloat(m_VarSwayY, sway[1]);
		PreAnim_SetFloat(m_VarSwayZ, sway[2]);
		PreAnim_SetFloat(m_VarPitch, m_Pitch);
		PreAnim_SetFloat(m_VarRoll, m_Roll);
		PreAnim_SetFloat(m_VarArms, arms);
		PreAnim_SetFloat(m_VarSpread, spread);

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

		if (s_Measure && !m_MeasuredStand && m_Player == GetGame().GetPlayer())
		{
			m_MeasuredStand = true;
			LogBones("stand", m_Player);
		}
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

		// opening: first back to neutral free fall (level, no keys), so the canopy always opens
		// from the same pose; then it opens, and the hands go up to the lines
		if (!m_Open && (m_OpenAsked || agl < AUTO_OPEN_HEIGHT))
			m_Opening = true;
		if (m_Opening && !m_Open && Math.AbsFloat(m_Pitch) < LEVEL && Math.AbsFloat(m_Roll) < LEVEL)
		{
			m_Open = true;
			m_Pitch = 0;
			m_Roll = 0;
		}
		if (m_Open)
			m_Hands = Math.Min(m_Hands + pDt / HANDS_UP, 1);

		// the keys (BR_Steer): the jumper's own game reads them, the server hears them from it
		vector keys = vector.Zero;
		if (GetGame().IsServer())
		{
			if (m_Player.GetIdentity())
				keys = BR_Steer.ForPlayer(m_Player.GetIdentity().GetId());
		}
		else if (m_Player == GetGame().GetPlayer())
			keys = BR_Steer.s_Local;
		if (m_Opening && !m_Open)
			keys = vector.Zero; // neutral: the keys wait for the canopy
		bool fast = keys[1] > 0.5;
		vector dir = Vector(keys[0], 0, keys[2]);
		m_Speed = dir.Length();
		m_Dir = keys;

		// what the keys ask for: forward and sideways speed, the fall, the tilt
		float wantForward;
		float wantSide;
		float wantDown;
		float wantPitch;
		float wantRoll;
		if (m_Open)
		{
			// under the canopy: always gliding forward; W faster, S hardly; A or D drift
			wantForward = CHUTE_GLIDE;
			if (keys[2] > 0.3)
			{
				wantForward = CHUTE_GLIDE_FAST;
				wantPitch = CHUTE_TILT_FORWARD;
			}
			else if (keys[2] < -0.3)
			{
				wantForward = CHUTE_GLIDE_SLOW;
				wantPitch = -CHUTE_TILT_BACK;
			}
			if (Math.AbsFloat(keys[0]) > 0.3)
			{
				float bank = Math.Clamp(keys[0], -1, 1);
				wantSide = bank * CHUTE_SIDE;
				wantRoll = bank * CHUTE_TILT_SIDE;
			}
			wantDown = CHUTE_DOWN;
			if (fast)
				wantDown = CHUTE_DOWN_FAST;
			// banked, it turns that way, as far as it's banked (so it spirals down)
			if (!m_TiltGiven)
			{
				m_Turn -= (m_Roll / CHUTE_TILT_SIDE) * CHUTE_TURN_RATE * pDt; // (the heading turns the other way round to the roll)
				if (m_Turn > 180)
					m_Turn -= 360;
				else if (m_Turn < -180)
					m_Turn += 360;
			}
		}
		else
		{
			bool forward = keys[2] > 0.3;
			bool back = keys[2] < -0.3;
			wantDown = FREEFALL_DOWN;
			if (back)
			{
				wantForward = FREEFALL_BRAKE;
				wantPitch = -TILT_BRAKE;
			}
			else if (forward && fast)
			{
				wantForward = FREEFALL_DIVE;
				wantDown = FREEFALL_DIVE_DOWN;
				wantPitch = TILT_DIVE;
			}
			else if (forward)
			{
				wantForward = FREEFALL_TRACK;
				wantPitch = TILT_TRACK;
			}
			else if (fast)
			{
				wantForward = 0;
				wantDown = FREEFALL_PLUMMET;
				wantPitch = TILT_PLUMMET;
			}
			else
				wantForward = FREEFALL_DRIFT;
			if (Math.AbsFloat(keys[0]) > 0.3)
			{
				float side = Math.Clamp(keys[0], -1, 1);
				wantSide = side * FREEFALL_SIDE;
				wantRoll = side * TILT_ROLL;
			}
		}
		if (m_TiltGiven && !(m_Opening && !m_Open))
		{
			wantPitch = m_GivenPitch;
			wantRoll = m_GivenRoll;
		}
		float step = FREEFALL_ACCEL * pDt;
		m_Forward = Approach(m_Forward, wantForward, step);
		m_Side = Approach(m_Side, wantSide, step);
		float downStep = step;
		if (m_Open)
			downStep = CHUTE_BRAKE * pDt; // the canopy catches the fall quickly, even from a plummet
		m_Down = Approach(m_Down, wantDown, downStep);
		float tiltRate = TILT_RATE;
		if (m_Open)
			tiltRate = CHUTE_SWING_RATE;
		m_Pitch = Approach(m_Pitch, wantPitch, tiltRate * pDt);
		m_Roll = Approach(m_Roll, wantRoll, tiltRate * pDt);

		// The server's height wins: when it corrects this game's position (a frame's head start
		// makes the two targets drift apart by a few metres), take the corrected height as the
		// target instead of steering back against it, which jittered up and down.
		if (!GetGame().IsServer() && Math.AbsFloat(pos[1] - m_TargetY) > 0.5)
			m_TargetY = pos[1];

		float ground = pos[1] - agl;
		m_TargetY = Math.Max(m_TargetY - m_Down * pDt, ground);

		// forward is the way the character faces (the camera's way, SetHeading), level; the
		// translation is in the character's own space, which the tilt turns, so it goes through that
		vector ahead = m_Player.GetDirection();
		ahead[1] = 0;
		if (ahead.Length() < 0.01)
			ahead = "0 0 1";
		ahead.Normalize();
		vector right = Vector(ahead[2], 0, -ahead[0]);
		vector world = (ahead * m_Forward + right * m_Side) * pDt;
		world[1] = m_TargetY - pos[1];
		PrePhys_SetTranslation(m_Player.VectorToLocal(world));

		// landed; or stuck on something (a roof, a rock) well above where it should be, low down
		if (agl < HANDOVER_HEIGHT)
			Land("ground", agl);
		if (agl < BLOCKED_HEIGHT && pos[1] > m_TargetY + 3)
			m_Blocked += pDt;
		else
			m_Blocked = 0;
		if (m_Blocked > 1.0)
			Land("blocked", agl);

		if (s_Measure && !m_Measured && !m_Open && m_Age >= 3 && m_Player == GetGame().GetPlayer())
		{
			m_Measured = true;
			LogBones("prone", m_Player);
		}
		if (s_Measure && !m_MeasuredHands && m_Open && m_Hands >= 1 && m_Player == GetGame().GetPlayer())
		{
			m_MeasuredHands = true;
			LogBones("hands", m_Player, s_FingerBones);
		}
		if (s_Measure && !m_MeasuredArms && !m_Open && m_Pitch >= TILT_TRACK && m_Player == GetGame().GetPlayer())
		{
			m_MeasuredArms = true;
			LogBones("armsdown", m_Player);
		}
		if (BR_Debug.LOG && m_Age - m_LogAt >= 2)
		{
			m_LogAt = m_Age;
			Print("[BattleDayZ] chute (" + m_Player.GetInstanceType() + ") t " + m_Age + " agl " + agl + " y " + pos[1] + " target " + m_TargetY + " open " + m_Open + " input " + m_Speed + " " + m_Dir + " forward " + m_Forward + " side " + m_Side + " down " + m_Down + " pitch " + m_Pitch + " roll " + m_Roll + " pre " + m_Pre + " post " + m_Post);
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

	// Measuring (s_Measure, for tools/solve_arms.py): bone frames in model space, logged as [BRMeasure] lines
	static ref array<string> s_MeasureBones = {"Spine3", "LeftShoulder", "LeftArm", "LeftArmRoll", "LeftForeArm", "LeftForeArmRoll", "LeftHand", "RightShoulder", "RightArm", "RightArmRoll", "RightForeArm", "RightForeArmRoll", "RightHand", "Pelvis"};
	static bool s_Measure = false; // the jumper's own game: bone frames for tools/solve_arms.py (arms: standing before the jump, lying 3 s in; fingers: fists raised in the lobby, hands up under the canopy)
	static ref array<string> s_FingerBones = {"LeftHand", "RightHand",
		"LeftHandThumb1", "LeftHandThumb2", "LeftHandThumb3", "LeftHandIndex1", "LeftHandIndex2", "LeftHandIndex3",
		"LeftHandMiddle1", "LeftHandMiddle2", "LeftHandMiddle3", "LeftHandRing1", "LeftHandRing2", "LeftHandRing3",
		"LeftHandPinky1", "LeftHandPinky2", "LeftHandPinky3",
		"RightHandThumb1", "RightHandThumb2", "RightHandThumb3", "RightHandIndex1", "RightHandIndex2", "RightHandIndex3",
		"RightHandMiddle1", "RightHandMiddle2", "RightHandMiddle3", "RightHandRing1", "RightHandRing2", "RightHandRing3",
		"RightHandPinky1", "RightHandPinky2", "RightHandPinky3"};
	protected bool m_MeasuredHands;
	protected bool m_Measured;
	protected bool m_MeasuredStand;
	protected bool m_MeasuredArms;

	static void LogBones(string label, DayZPlayer p, array<string> bones = null)
	{
		if (!bones)
			bones = s_MeasureBones;
		foreach (string b : bones)
		{
			int i = p.GetBoneIndexByName(b);
			vector m[4];
			p.GetBoneTransformMS(i, m);
			Print("[BRMeasure] " + label + " " + b + " " + i + " " + m[0].ToString(false) + " | " + m[1].ToString(false) + " | " + m[2].ToString(false) + " | " + m[3].ToString(false));
		}
	}

	static float Approach(float now, float want, float step)
	{
		if (now < want)
			return Math.Min(now + step, want);
		return Math.Max(now - step, want);
	}
}

