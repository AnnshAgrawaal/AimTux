#include "aimbot.h"

// Default aimbot settings
bool Settings::Aimbot::enabled = true;
bool Settings::Aimbot::AutoAim::enabled = true;
bool Settings::Aimbot::AutoShoot::enabled = false;
bool Settings::Aimbot::RCS::enabled = true;
bool Settings::Aimbot::AutoCrouch::enabled = false;
bool Settings::Aimbot::AutoStop::enabled = false;

void Aimbot::CheckAngles (QAngle& angle)
{
	if (angle[0] > 89	) angle[0] = 89;
	if (angle[0] < -89	) angle[0] = -89;
	if (angle[1] > 180	) angle[1] -= 360;
	if (angle[1] < -180	) angle[1] += 360;
	angle[2] = 0;
}

double hyp;

void CalculateAngle (Vector& src, Vector& dst, QAngle& angles)
{
	// Angle deltas
	double delta[3] = { (src[0]-dst[0]), (src[1]-dst[1]), (src[2]-dst[2]) };

	// Hypotenuse
	hyp = sqrt(delta[0] * delta[0] + delta[1] * delta[1]);

	angles[0] = (float) (atan(delta[2]/hyp) * 57.295779513082f);
	angles[1] = (float) (atan(delta[1]/delta[0]) * 57.295779513082f);
	angles[2] = 0.0f;	// <=== NEVER EVER  EVER EVER EVER CHANGE THIS VALUE
						//					... unless you teh VAC ban ...

	if (delta[0] >= 0.0) angles[1] += 180.0f;

	// Safeguards
	if (angles[1] > 180) angles[1] -= 360;
	if (angles[1] < -180) angles[1] += 360;
	if (angles[0] > 89) angles[0] = 89;
	if (angles[0] < -89) angles[0] = -89;
}


C_BaseEntity* GetClosestEnemy ()
{
	C_BasePlayer* localplayer = (C_BasePlayer*)entitylist->GetClientEntity(engine->GetLocalPlayer());
	C_BaseEntity* closestEntity = NULL;
	float dist = 10000000.0f;

	if (!localplayer)
		return NULL;

	for (int i = 0; i < 64; ++i)
	{
		C_BaseEntity* entity = entitylist->GetClientEntity(i);

		if (!entity
			|| entity == (C_BaseEntity*)localplayer
			|| entity->GetDormant()
			|| entity->GetLifeState() != 0
			|| entity->GetHealth() <= 0
			|| entity->GetTeam() == localplayer->GetTeam())
			continue;

		float e_dist = localplayer->GetVecOrigin().DistToSqr (entity->GetVecOrigin());

		if (e_dist < dist)
		{
			closestEntity = entity;
			dist = e_dist;
		}
	}

	return closestEntity;
}

C_BaseEntity* GetClosestVisibleEnemy ()
{
	C_BasePlayer* localplayer = (C_BasePlayer*)entitylist->GetClientEntity(engine->GetLocalPlayer());
	C_BaseEntity* closestEntity = NULL;
	float dist = 10000000.0f;

	if (!localplayer)
		return NULL;

	for (int i = 0; i < 64; ++i)
	{
		C_BaseEntity* entity = entitylist->GetClientEntity(i);

		if (!entity
			|| entity == (C_BaseEntity*)localplayer
			|| entity->GetDormant()
			|| entity->GetLifeState() != 0
			|| entity->GetHealth() <= 0
			|| entity->GetTeam() == localplayer->GetTeam()
			|| !Entity::IsVisible (localplayer, entity, 6))
			continue;

		float e_dist = localplayer->GetVecOrigin().DistToSqr (entity->GetVecOrigin());

		if (e_dist < dist)
		{
			closestEntity = entity;
			dist = e_dist;
		}
	}

	return closestEntity;
}

void Aimbot::RCS (QAngle& angle)
{
	C_BasePlayer* localplayer = reinterpret_cast<C_BasePlayer*>(entitylist->GetClientEntity(engine->GetLocalPlayer()));
	angle -= localplayer->GetAimPunchAngle() * 2.f;
}


void Aimbot::CorrectMovement (QAngle vOldAngles, CUserCmd* pCmd, float fOldForward, float fOldSidemove)
{
	//side/forward move correction
	float deltaView = pCmd->viewangles.y - vOldAngles.y;
	float f1;
	float f2;
	
	if (vOldAngles.y < 0.f)
		f1 = 360.0f + vOldAngles.y;
	else
		f1 = vOldAngles.y;
	
	if (pCmd->viewangles.y < 0.0f)
		f2 = 360.0f + pCmd->viewangles.y;
	else
		f2 = pCmd->viewangles.y;
	
	if (f2 < f1)
		deltaView = abs(f2 - f1);
	else
		deltaView = 360.0f - abs(f1 - f2);
	deltaView = 360.0f - deltaView;
	
	pCmd->forwardmove = cos(DEG2RAD(deltaView)) * fOldForward + cos(DEG2RAD(deltaView + 90.f)) * fOldSidemove;
	pCmd->sidemove = sin(DEG2RAD(deltaView)) * fOldForward + sin(DEG2RAD(deltaView + 90.f)) * fOldSidemove;
}

void Aimbot::CreateMove (CUserCmd* cmd)
{
	if (!Settings::Aimbot::enabled)
		return;

	QAngle oldAngle = cmd->viewangles;
	float oldForward = cmd->forwardmove;
	float oldSideMove = cmd->sidemove;
	
	QAngle angle = cmd->viewangles;
	
	C_BasePlayer* localplayer = reinterpret_cast<C_BasePlayer*>(entitylist->GetClientEntity(engine->GetLocalPlayer()));
	
	C_BaseEntity* entity = GetClosestVisibleEnemy ();
	
	if (entity)
	{
		if (Settings::Aimbot::AutoAim::enabled &&
			(Settings::Aimbot::AutoShoot::enabled || cmd->buttons & IN_ATTACK))
		{
			Vector e_vecHead = GetBone (entity, 6);
			Vector p_vecHead = localplayer->GetVecOrigin() + localplayer->GetVecViewOffset();
			
			CalculateAngle (p_vecHead, e_vecHead, angle);
		}
		
		if (Settings::Aimbot::AutoShoot::enabled)
		{
			C_BaseViewModel* viewmodel = reinterpret_cast<C_BaseViewModel*>(entitylist->GetClientEntity(localplayer->GetViewModel() & 0xFFF));
			C_BaseCombatWeapon* active_weapon = reinterpret_cast<C_BaseCombatWeapon*>(entitylist->GetClientEntity(viewmodel->GetWeapon() & 0xFFF));
			
			if (active_weapon->GetAmmo() > 0)
			{
				if (*active_weapon->GetItemDefinitionIndex() == WEAPON_REVOLVER)
					cmd->buttons |= IN_ATTACK2;
				else
					cmd->buttons |= IN_ATTACK;
			}
		}
		
		if (Settings::Aimbot::AutoCrouch::enabled)
		{
			cmd->buttons |= IN_DUCK;
		}

		if (Settings::Aimbot::AutoStop::enabled)
		{
			oldForward = 0;
			oldSideMove = 0;
			cmd->upmove = 0;
		}
	}
	else
	{
		cmd->buttons &= ~IN_ATTACK;
	}


	if (Settings::Aimbot::RCS::enabled)
	{
		RCS (angle);
	}

	// Check the angle to make sure it's invalid
	Aimbot::CheckAngles (angle);
	
	cmd->viewangles = angle;

	Aimbot::CorrectMovement (oldAngle, cmd, oldForward, oldSideMove);
}



