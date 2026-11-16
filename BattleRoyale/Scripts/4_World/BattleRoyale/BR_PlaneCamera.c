// The view from the plane, as one of the player's own camera modes rather than a separate
// scripted camera (switching a 'staticcamera' off in gameplay left the player's view stuck).
//
// It borrows the unconscious camera's slot instead of registering a new camera type:
// registering one (id 120) crashed the client to desktop. Nobody on the plane is
// unconscious, and the slot behaves exactly as vanilla whenever BR_Plane.s_View is off.
//
// The camera sits relative to the plane (its owner transform is overridden to the plane's),
// orbiting it with the mouse, while the character waits in the lobby.
modded class DayZPlayerCamera1stPersonUnconscious
{
	static const float BR_DISTANCE = 75;   // m from the plane
	static const float BR_ABOVE = 8;

	protected float m_BR_Yaw;
	protected float m_BR_Pitch = -15;

	override void OnUpdate(float pDt, out DayZPlayerCameraResult pOutResult)
	{
		if (!BR_Plane.s_View)
		{
			super.OnUpdate(pDt, pOutResult);
			return;
		}

		StdFovUpdate(pDt, pOutResult);

		vector aim = m_pInput.GetAimChange();
		m_BR_Yaw += aim[0] * Math.RAD2DEG;
		m_BR_Pitch = Math.Clamp(m_BR_Pitch + aim[1] * Math.RAD2DEG, -70, 30);

		// the plane's frame: at the plane, facing where it flies
		vector forward = BR_Plane.s_Vel;
		forward[1] = 0;
		if (forward.Length() < 0.1)
			forward = "0 0 1";
		vector owner[4];
		Math3D.DirectionAndUpMatrix(forward.Normalized(), "0 1 0", owner);
		owner[3] = BR_Plane.Now();
		for (int i = 0; i < 4; i++)
			pOutResult.m_OwnerTM[i] = owner[i];
		pOutResult.m_bOwnerTMOverride = true;

		// the camera in that frame: out behind along the orbit, looking at the plane
		vector rot[4];
		Math3D.YawPitchRollMatrix(Vector(m_BR_Yaw, m_BR_Pitch, 0), rot);
		for (int j = 0; j < 3; j++)
			pOutResult.m_CameraTM[j] = rot[j];
		pOutResult.m_CameraTM[3] = rot[2] * -BR_DISTANCE + Vector(0, BR_ABOVE, 0);

		pOutResult.m_fPositionModelSpace = 1.0;
		pOutResult.m_fUseHeading = 0.0;
		pOutResult.m_fDistance = 0.0;
		pOutResult.m_iDirectBone = -1;
		pOutResult.m_iDirectBoneMode = 0;
		pOutResult.m_fInsideCamera = 0.0;
		pOutResult.m_fIgnoreParentRoll = 0;
		pOutResult.m_fIgnoreParentPitch = 0;
		pOutResult.m_fIgnoreParentYaw = 0;
		pOutResult.m_bUpdateEveryFrame = true;
		pOutResult.m_CollisionIgnoreEntity = m_pPlayer;
	}
}

modded class DayZPlayerCameras
{
	// straight in and straight out of the plane view: no blend across the kilometres between
	// the plane and the character
	override static float OnCameraChange(DayZPlayer pPlayer, int pFrom, int pTo)
	{
		if ((pFrom == DAYZCAMERA_1ST_UNCONSCIOUS || pTo == DAYZCAMERA_1ST_UNCONSCIOUS) && BR_Plane.ViewRecent())
			return 0;
		return super.OnCameraChange(pPlayer, pFrom, pTo);
	}
}
