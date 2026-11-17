// The plane's engine, heard from the moment a player is on board. Expansion's plane starts its
// own engine loop only once it reaches this game over the network, which for a player whose
// character waits in the lobby (far off, in a corner of the map) is a quarter or half the way
// across, and then it plays at full volume right next to the camera. So while this player
// watches from the plane, this game plays the same loop itself, at the plane, turned down, and
// Expansion's own loop stays quiet (supply-drop planes, heard from the ground, keep theirs).
class BR_PlaneSound
{
	static const string SOUND_SET = "Expansion_C130J_Loop_SoundSet";
	static const float VOLUME = 0.35;

	protected static EffectSound s_Sound;

	// every frame, on the client (MissionGameplay.OnUpdate)
	static void Update()
	{
		if (GetGame().IsDedicatedServer())
			return;
		if (!BR_Plane.s_View)
		{
			Stop();
			return;
		}
		vector at = BR_SeenPlane();
		if (!s_Sound)
		{
			s_Sound = SEffectManager.PlaySound(SOUND_SET, at, 2.0, 2.0, true);
			if (s_Sound)
			{
				s_Sound.SetSoundAutodestroy(true);
				s_Sound.SetSoundVolume(VOLUME);
				Print("[BattleDayZ] plane engine sound on");
			}
			return;
		}
		s_Sound.SetCurrentPosition(at);
	}

	static void Stop()
	{
		if (!s_Sound)
			return;
		s_Sound.SoundStop();
		s_Sound = null;
		Print("[BattleDayZ] plane engine sound off");
	}
}

modded class ExpansionAirdropPlaneBase
{
	override void Expansion_PlayEngineSoundLoop()
	{
		// this game plays the engine itself while watching from the plane (BR_PlaneSound)
		if (!GetGame().IsDedicatedServer() && BR_Plane.s_View)
			return;
		super.Expansion_PlayEngineSoundLoop();
	}
}
