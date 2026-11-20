// The jumper's own camera: in free fall it tilts with them (part of the way) and pulls back
// while they manoeuvre (further in the dive); under the
// canopy it moves further out and up, looking down on them (and the canopy), BR_CHUTE_DISTANCE
// m from their chest at BR_CHUTE_PITCH degrees, from which the mouse looks round as usual. It eases
// there as the canopy opens, and back as they land. The wind shakes it, and wisps stream past,
// more the faster they go.
modded class DayZPlayerCamera3rdPersonErc
{
	static const float BR_CHUTE_DISTANCE = 10;
	static const float BR_CHUTE_PITCH = -45;       // degrees: looking down from above
	static const float BR_CHUTE_PIVOT_Y = 1.3;     // m: the chest
	static const float BR_CHUTE_EASE = 1;          // s to ease there or back
	protected float m_BR_Chute;                    // 0 to 1: how far it has eased to the chute view
	static const float BR_MANOEUVRE_BACK = 3;      // m further back in free fall while any key is held
	static const float BR_DIVE_BACK = 5;           // m further back in the dive (W and shift)
	static const float BR_BACK_RATE = 4;           // m per s, easing back and in
	protected float m_BR_Back;
	protected bool m_BR_WasOpen;
	protected float m_BR_PitchFrom;

	override void OnUpdate(float pDt, out DayZPlayerCameraResult pOutResult)
	{
		super.OnUpdate(pDt, pOutResult);
		bool open = false;
		BR_ChuteCommand chute;
		if (m_pPlayer && m_pPlayer == GetGame().GetPlayer())
		{
			chute = BR_ChuteCommand.Cast(m_pPlayer.GetCommand_Script());
			open = chute && chute.IsOpen();
		}
		if (chute && !open)
			BR_FreeFallTilt(chute, pOutResult);
		// back a few metres while manoeuvring in free fall, further in the dive
		float back = 0;
		if (chute && !open)
		{
			vector keys = BR_Steer.s_Local;
			bool fast = keys[1] > 0.5;
			bool forward = keys[2] > 0.3;
			if (forward && fast)
				back = BR_DIVE_BACK;
			else if (fast || Math.AbsFloat(keys[2]) > 0.3 || Math.AbsFloat(keys[0]) > 0.3)
				back = BR_MANOEUVRE_BACK;
		}
		if (m_BR_Back < back)
			m_BR_Back = Math.Min(m_BR_Back + BR_BACK_RATE * pDt, back);
		else
			m_BR_Back = Math.Max(m_BR_Back - BR_BACK_RATE * pDt, back);
		pOutResult.m_fDistance = pOutResult.m_fDistance + m_BR_Back;
		float step = pDt / BR_CHUTE_EASE;
		// the canopy opening: the mouse's own up-down angle is set to the look down once, and
		// the view eases from where it was; after that the mouse looks round freely
		if (open && !m_BR_WasOpen)
		{
			m_BR_PitchFrom = m_CurrentCameraPitch;
			m_fUpDownAngle = BR_CHUTE_PITCH;
		}
		m_BR_WasOpen = open;
		if (open)
			m_BR_Chute = Math.Min(m_BR_Chute + step, 1);
		else
			m_BR_Chute = Math.Max(m_BR_Chute - step, 0);
		if (m_BR_Chute > 0)
		{
			float t = m_BR_Chute * m_BR_Chute * (3 - 2 * m_BR_Chute); // eased in and out
			float pitch = m_CurrentCameraPitch;
			if (open)
				pitch = Math.Lerp(m_BR_PitchFrom, m_CurrentCameraPitch, t);
			vector pivot = pOutResult.m_CameraTM[3];
			// behind them through the canopy's turns: the canopy turns them away from where the
			// mouse points, and the view turns with them (a heading turns the other way round to a yaw)
			float turn = 0;
			if (chute)
				turn = -chute.Turn();
			Math3D.YawPitchRollMatrix(Vector(m_CurrentCameraYaw + turn, pitch, 0), pOutResult.m_CameraTM);
			pOutResult.m_CameraTM[3] = pivot + (Vector(0, BR_CHUTE_PIVOT_Y, 0) - pivot) * t;
			pOutResult.m_fDistance = Math.Lerp(m_fDistance, BR_CHUTE_DISTANCE, t) + m_BR_Back;
		}
		if (chute)
		{
			BR_WindShake(chute, pDt, pOutResult); // last: on top of everything else
			BR_WindParticles(chute, pDt);
		}
	}

	// In free fall the camera tilts with the jumper, part of the way: forward as they track
	// or dive, back as they brake, rolled as they drift (BR_ChuteCommand's tilt)
	static const float BR_PITCH_SHARE = 0.5;
	static const float BR_ROLL_SHARE = 0.6;

	// Wind: a shake that grows with the speed through the air, from nothing at BR_SHAKE_FROM m/s
	// (the canopy's glide hardly shakes) to BR_SHAKE_MAX degrees at BR_SHAKE_FULL m/s (the dives);
	// waves at unrelated rates, so it buffets rather than wobbles
	static const float BR_SHAKE_FROM = 10;
	static const float BR_SHAKE_FULL = 50;
	static const float BR_SHAKE_MAX = 1.6;
	static const float BR_SHAKE_RATE = 1.3;    // how quickly it shakes (1: the first tuning)
	protected float m_BR_ShakeTime;

	protected void BR_WindShake(BR_ChuteCommand chute, float pDt, out DayZPlayerCameraResult pOutResult)
	{
		float strength = Math.Clamp((chute.AirSpeed() - BR_SHAKE_FROM) / (BR_SHAKE_FULL - BR_SHAKE_FROM), 0, 1) * BR_SHAKE_MAX;
		if (strength < 0.01)
			return;
		m_BR_ShakeTime += pDt * BR_SHAKE_RATE;
		float t = m_BR_ShakeTime;
		float yaw = strength * (0.5 * Math.Sin(t * 7.3) + 0.3 * Math.Sin(t * 13.1 + 1.7) + 0.2 * Math.Sin(t * 23.7 + 4.1));
		float pitch = strength * (0.5 * Math.Sin(t * 8.9 + 2.3) + 0.3 * Math.Sin(t * 15.7 + 0.4) + 0.2 * Math.Sin(t * 27.1 + 3.3));
		float roll = 0.6 * strength * (0.5 * Math.Sin(t * 6.1 + 5.2) + 0.3 * Math.Sin(t * 11.9 + 2.9) + 0.2 * Math.Sin(t * 21.3 + 0.8));
		vector shake[3];
		Math3D.YawPitchRollMatrix(Vector(yaw, pitch, roll), shake);
		vector axes[3];
		axes[0] = pOutResult.m_CameraTM[0];
		axes[1] = pOutResult.m_CameraTM[1];
		axes[2] = pOutResult.m_CameraTM[2];
		vector turned[3];
		Math3D.MatrixMultiply3(axes, shake, turned);
		pOutResult.m_CameraTM[0] = turned[0];
		pOutResult.m_CameraTM[1] = turned[1];
		pOutResult.m_CameraTM[2] = turned[2];
	}

	// Wind streaming past: faint wisps ahead along the way they're going, a few metres to the
	// side, blown back at them as fast as they fall, so they pass at twice their speed. More the
	// faster they go: none at BR_SHAKE_FROM m/s, BR_WISPS_FULL a second at BR_SHAKE_FULL. This
	// game only.
	static const float BR_WISPS_FULL = 25;
	static const float BR_WISPS_AHEAD = 2.4;   // s ahead along the way, at their own speed (they meet in half that)
	static const float BR_WISPS_NEAR = 2.5;    // m to the side, at the least
	static const float BR_WISPS_FAR = 9;       // m to the side, at the most
	protected float m_BR_Wisps;                // wisps owed (a fraction carries over)

	protected void BR_WindParticles(BR_ChuteCommand chute, float pDt)
	{
		float speed = chute.AirSpeed();
		float rate = Math.Clamp((speed - BR_SHAKE_FROM) / (BR_SHAKE_FULL - BR_SHAKE_FROM), 0, 1) * BR_WISPS_FULL;
		m_BR_Wisps += rate * pDt;
		if (m_BR_Wisps < 1)
			return;
		vector v = chute.AirVelocity();
		vector way = v.Normalized();
		// two directions square to the way
		vector side = BR_Cross(way, "0 1 0");
		if (side.Length() < 0.1)
			side = "1 0 0";
		side.Normalize();
		vector other = BR_Cross(way, side);
		vector from = m_pPlayer.GetPosition() + "0 1 0";
		while (m_BR_Wisps >= 1)
		{
			m_BR_Wisps -= 1;
			float a = Math.RandomFloat(0, Math.PI2);
			float r = Math.RandomFloat(BR_WISPS_NEAR, BR_WISPS_FAR);
			vector at = from + v * (BR_WISPS_AHEAD * Math.RandomFloat(0.6, 1.4)) + side * (Math.Cos(a) * r) + other * (Math.Sin(a) * r);
			Particle wisp = Particle.PlayInWorld(ParticleList.BREATH_VAPOUR_LIGHT, at);
			if (!wisp)
				continue;
			// an emitter blows along its own up: turned to blow back along the way, at their speed
			vector m[4];
			m[1] = -way;
			m[0] = side;
			m[2] = BR_Cross(m[0], m[1]);
			m[3] = at;
			wisp.SetTransform(m);
			wisp.SetParticleParam(EmitorParam.VELOCITY, speed);
			wisp.SetParticleParam(EmitorParam.VELOCITY_RND, 0);
			wisp.SetParticleParam(EmitorParam.AIR_RESISTANCE, 0);
			wisp.SetParticleParam(EmitorParam.GRAVITY_SCALE, 0);
		}
	}

	static vector BR_Cross(vector a, vector b)
	{
		return Vector(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
	}

	protected void BR_FreeFallTilt(BR_ChuteCommand chute, out DayZPlayerCameraResult pOutResult)
	{
		float pitch = chute.TiltPitch() * BR_PITCH_SHARE;
		float roll = chute.TiltRoll() * BR_ROLL_SHARE;
		if (Math.AbsFloat(pitch) < 0.1 && Math.AbsFloat(roll) < 0.1)
			return;
		vector tilt[3];
		Math3D.YawPitchRollMatrix(Vector(0, -pitch, roll), tilt);
		vector axes[3];
		axes[0] = pOutResult.m_CameraTM[0];
		axes[1] = pOutResult.m_CameraTM[1];
		axes[2] = pOutResult.m_CameraTM[2];
		vector turned[3];
		Math3D.MatrixMultiply3(axes, tilt, turned);
		pOutResult.m_CameraTM[0] = turned[0];
		pOutResult.m_CameraTM[1] = turned[1];
		pOutResult.m_CameraTM[2] = turned[2];
	}
}
