// The circles and the plane's route, drawn as real lines over Expansion's map (M). A canvas
// kept exactly over the map widget is cleared and redrawn every frame from the latest state,
// so it follows panning and zooming: world positions go through the map's own MapToScreen.
// The canvas sits on the menu, beside the map (a map widget doesn't draw widgets inside it,
// which is also where Expansion puts its markers), and every line is clipped to the map.
modded class ExpansionMapMenu
{
	static const int CIRCLE_SEGMENTS = 128;

	protected CanvasWidget m_BR_Canvas;
	protected float m_BR_W, m_BR_H; // the map's size on screen; lines are clipped to it

	override Widget Init()
	{
		Widget root = super.Init();
		// one canvas per menu: opening the map again must not add another (the old ones kept their
		// last lines, so a moving circle left a trail of old circles behind)
		if (m_MapWidget && layoutRoot)
		{
			m_BR_Canvas = CanvasWidget.Cast(layoutRoot.FindAnyWidget("BR_MapCanvas"));
			if (!m_BR_Canvas)
				m_BR_Canvas = CanvasWidget.Cast(GetGame().GetWorkspace().CreateWidgets("BattleRoyale/gui/layouts/br_mapcanvas.layout", layoutRoot));
		}
		return root;
	}

	// Always open zoomed out, centred on the player (or on the plane while watching from it:
	// the character is standing in the lobby then), not where the map was last left
	static const float BR_MAP_SCALE = 0.45; // Expansion: lower zooms in, 0.5 shows about the whole map

	override protected void SetMapPosition()
	{
		if (!m_MapWidget)
			return;
		vector pos = GetGame().ConfigGetVector(string.Format("CfgWorlds %1 centerPosition", GetGame().GetWorldName()));
		PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
		if (BR_Plane.s_View)
			pos = BR_SeenPlane();
		else if (player)
			pos = player.GetPosition();
		m_MapWidget.SetScale(BR_MAP_SCALE);
		m_MapWidget.SetMapPos(pos);
	}

	override void OnShow()
	{
		super.OnShow();
		SetMapPosition();
	}

	override void Update(float timeslice)
	{
		super.Update(timeslice);
		BR_Draw();
	}

	protected void BR_Draw()
	{
		if (!m_BR_Canvas || !m_MapWidget)
			return;
		float x, y, w, h;
		m_MapWidget.GetScreenPos(x, y);
		m_MapWidget.GetScreenSize(w, h);
		m_BR_Canvas.SetScreenPos(x, y);
		m_BR_Canvas.SetScreenSize(w, h);
		m_BR_W = w;
		m_BR_H = h;
		m_BR_Canvas.Clear();

		BR_State state = BR_Client.s_State;
		if (!state || GetGame().GetTickTime() - BR_Client.s_ReceivedAt > 5 || state.phase == BR_Phase.LOBBY)
			return;

		if (state.hasNext)
			BR_Circle(state.nextCenter, state.nextRadius, 2, ARGB(255, 255, 255, 255));
		if (!state.wholeMap) // at the start of a round the whole map is safe: no circles yet
			BR_Circle(state.center, state.radius, 3, ARGB(255, 230, 50, 50));

		if (state.flying)
		{
			BR_Line(state.routeFrom, state.routeTo, 2, ARGB(200, 255, 210, 60));
			BR_DrawPlane(BR_Client.PlaneNow(), state.planeVel, ARGB(255, 255, 210, 60));
		}
	}

	// world position -> canvas coordinates (the canvas is exactly over the map)
	protected vector BR_ToCanvas(vector world)
	{
		vector screen = m_MapWidget.MapToScreen(world);
		float x, y;
		m_MapWidget.GetScreenPos(x, y);
		return Vector(screen[0] - x, screen[1] - y, 0);
	}

	protected void BR_Line(vector a, vector b, float width, int color)
	{
		BR_Draw2D(BR_ToCanvas(a), BR_ToCanvas(b), width, color);
	}

	// a line in canvas coordinates, clipped to the map (Liang-Barsky)
	protected void BR_Draw2D(vector p, vector q, float width, int color)
	{
		float dx = q[0] - p[0];
		float dy = q[1] - p[1];
		float t0 = 0;
		float t1 = 1;
		if (!BR_Clip(-dx, p[0], t0, t1) || !BR_Clip(dx, m_BR_W - p[0], t0, t1))
			return;
		if (!BR_Clip(-dy, p[1], t0, t1) || !BR_Clip(dy, m_BR_H - p[1], t0, t1))
			return;
		m_BR_Canvas.DrawLine(p[0] + dx * t0, p[1] + dy * t0, p[0] + dx * t1, p[1] + dy * t1, width, color);
	}

	protected bool BR_Clip(float den, float num, inout float t0, inout float t1)
	{
		if (den == 0)
			return num >= 0;
		float t = num / den;
		if (den < 0)
		{
			if (t > t1)
				return false;
			if (t > t0)
				t0 = t;
		}
		else
		{
			if (t < t0)
				return false;
			if (t < t1)
				t1 = t;
		}
		return true;
	}

	protected void BR_Circle(vector center, float radius, float width, int color)
	{
		if (radius < 1)
			return;
		vector prev = BR_ToCanvas(center + Vector(radius, 0, 0));
		for (int i = 1; i <= CIRCLE_SEGMENTS; i++)
		{
			float a = Math.PI2 * i / CIRCLE_SEGMENTS;
			vector next = BR_ToCanvas(center + Vector(Math.Cos(a) * radius, 0, Math.Sin(a) * radius));
			BR_Draw2D(prev, next, width, color);
			prev = next;
		}
	}

	// an arrowhead pointing the way the plane flies, a fixed size on screen
	protected void BR_DrawPlane(vector pos, vector vel, int color)
	{
		vector tip = BR_ToCanvas(pos);
		vector ahead = BR_ToCanvas(pos + vel.Normalized() * 100);
		vector fwd = ahead - tip;
		if (fwd.Length() < 0.001)
			return;
		fwd = fwd.Normalized();
		vector side = Vector(-fwd[1], fwd[0], 0);
		vector back = tip - fwd * 18;
		vector l = back + side * 9;
		vector r = back - side * 9;
		vector nose = tip + fwd * 4;
		BR_Draw2D(nose, l, 3, color);
		BR_Draw2D(nose, r, 3, color);
		BR_Draw2D(l, r, 3, color);
	}
}
