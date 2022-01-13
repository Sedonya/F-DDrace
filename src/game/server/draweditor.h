// made by fokkonaut

#ifndef GAME_SERVER_DRAWEDITOR_H
#define GAME_SERVER_DRAWEDITOR_H

#include <generated/protocol.h>
#include "entity.h"

class CCharacter;

class CDrawEditor
{
	enum
	{
		// do not re-sort draw pickups, order is important for SetPickup()
		// Pickup
		DRAW_PICKUP_HEART = 0,
		DRAW_PICKUP_SHIELD,
		DRAW_PICKUP_HAMMER,
		DRAW_PICKUP_GUN,
		DRAW_PICKUP_SHOTGUN,
		DRAW_PICKUP_GRENADE,
		DRAW_PICKUP_LASER,
		NUM_DRAW_PICKUPS,

		// Wall
		LASERWALL_COLLISION = 0,
		LASERWALL_THICKNESS,
		NUM_LASERWALL_SETTINGS,

		// Door
		LASERDOOR_NUMBER = 0,
		LASERDOOR_MODE,
		NUM_LASERDOOR_SETTINGS,

		// Speedup
		SPEEDUP_FORCE = 0,
		SPEEDUP_MAXSPEED,
		NUM_SPEEDUP_SETTINGS,

		// Categories
		CAT_UNINITIALIZED = -1,
		CAT_PICKUPS,
		CAT_LASERWALLS,
		CAT_LASERDOORS,
		CAT_SPEEDUPS,
		NUM_DRAW_CATEGORIES,
	};

	CGameContext *GameServer() const;
	IServer *Server() const;

	CCharacter *m_pCharacter;
	int GetCID();

	CEntity *CreateEntity(bool Preview = false);

	bool IsCategoryLaser() { return m_Category == CAT_LASERWALLS || m_Category == CAT_LASERDOORS; }
	void SetAngle(float Angle);
	void AddAngle(float Add);
	void AddLength(float Add);

	void HandleInput();
	struct DrawInput
	{
		int m_Jump;
		int m_Hook;
		int m_Direction;
	};
	DrawInput m_Input;
	DrawInput m_PrevInput;
	int m_PrevPlotID;

	bool CanPlace(bool Remove = false);
	bool CanRemove(CEntity *pEnt);
	int GetPlotID();
	int CurrentPlotID();
	int GetNumMaxDoors();
	int GetFirstFreeNumber();

	vec2 m_Pos;
	int m_Entity;
	bool m_RoundPos;

	bool m_Erasing;
	bool m_Selecting;
	int64 m_EditStartTick;

	void SendWindow();
	const char *GetCategory(int Category);
	void SetCategory(int Category);
	int m_Category;

	int GetNumSettings();
	void SetSetting(int Setting);
	const char *FormatSetting(const char *pSetting, int Setting);
	int m_Setting;

	const char *GetPickup(int Pickup);
	void SetPickup(int Pickup);

	struct
	{
		int m_Type;
		int m_SubType;
	} m_Pickup;

	struct
	{
		float m_Length;
		float m_Angle;
		// Walls
		bool m_Collision;
		int m_Thickness;
		// Doors
		int m_Number;
		bool m_ButtonMode;
	} m_Laser;

	struct
	{
		int m_Force;
		int m_MaxSpeed;
		int m_Angle;
	} m_Speedup;

	// preview
	void SetPreview();
	void RemovePreview();
	void UpdatePreview();
	CEntity *m_pPreview;

public:
	CDrawEditor(CCharacter *pChr);

	void Tick();

	bool Active();
	bool Selecting() { return Active() && m_Selecting; }

	void OnPlayerFire();
	void OnWeaponSwitch();
	void OnPlayerDeath();
	void OnPlayerKill();
	void OnInput(CNetObj_PlayerInput *pNewInput);

	// used in snap functions of available entities to draw, returns true if the SnappingClient is not able to see the preview
	bool OnSnapPreview(int SnappingClient);
};
#endif //GAME_SERVER_DRAWEDITOR_H
