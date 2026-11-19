// The plane's engine, heard from the moment a player is on board (boarding, while they wait in
// the lobby for every game to be ready, it's played where they are). Expansion's plane starts its
// own engine loop only once it reaches this game over the network, which for a player whose
// character waits in the lobby (far off, in a corner of the map) is a quarter or half the way
// across, and then it plays at full volume right next to the camera. So while this player
// watches from the plane, this game plays the same loop itself, at the plane, turned down, and
// Expansion's own loop stays quiet (supply-drop planes, heard from the ground, keep theirs).
class BR_PlaneSound
{
	static const string SOUND_SET = "Expansion_C130J_Loop_SoundSet";
	static const float VOLUME = 0.35;
	static const float BOARDING_ABOVE = 30; // m over the player while they wait in the lobby

	protected static EffectSound s_Sound;
	static bool s_Boarding; // waiting in the lobby for take-off: the engine starting up, heard there

	// every frame, on the client (MissionGameplay.OnUpdate)
	static void Update()
	{
		if (GetGame().IsDedicatedServer())
			return;
		if (!BR_Plane.s_View && !s_Boarding)
		{
			Stop();
			return;
		}
		vector at;
		if (BR_Plane.s_View)
			at = BR_SeenPlane();
		else
			at = GetGame().GetCurrentCameraPosition() + Vector(0, BOARDING_ABOVE, 0); // boarding: overhead
			// (played exactly at the camera, the game never started it: it started only once
			// the view moved to the plane, 60 s later)
		if (!s_Sound)
		{
			s_Sound = SEffectManager.PlaySound(SOUND_SET, at, 2.0, 2.0, true);
			if (s_Sound)
			{
				s_Sound.SetSoundAutodestroy(true);
				s_Sound.SetSoundVolume(VOLUME);
				s_Sound.Event_OnSoundWaveStarted.Insert(OnStarted);
				s_AskedAt = GetGame().GetTickTime();
				Print("[BattleDayZ] plane engine sound on");
			}
			return;
		}
		s_Sound.SetCurrentPosition(at);
	}

	protected static float s_AskedAt;
	static void OnStarted(EffectSound sound)
	{
		Print("[BattleDayZ] plane engine sound: the wave started playing " + (GetGame().GetTickTime() - s_AskedAt) + " s after it was asked for");
	}

	static string Describe()
	{
		if (!s_Sound)
			return "none";
		return "playing " + s_Sound.IsSoundPlaying() + ", volume " + s_Sound.GetSoundVolume();
	}

	static bool Playing()
	{
		return s_Sound && s_Sound.IsSoundPlaying();
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
