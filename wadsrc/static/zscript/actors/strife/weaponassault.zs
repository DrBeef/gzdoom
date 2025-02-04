// Assault Gun --------------------------------------------------------------

class AssaultGun : StrifeWeapon
{
	Default
	{
		+FLOORCLIP
		Weapon.SelectionOrder 600;
		Weapon.AmmoUse1 1;
		Weapon.AmmoGive1 20;
		Weapon.AmmoType1 "ClipOfBullets";
		Inventory.Icon "RIFLA0";
		Tag "$TAG_ASSAULTGUN";
		Inventory.PickupMessage "$TXT_ASSAULTGUN";
		Obituary "$OB_MPASSAULTGUN";
	}
	States
	{
	Spawn:
		RIFL A -1;
		Stop;
	Ready:
		RIFG A 1 A_WeaponReady;
		Loop;
	Deselect:
		RIFG B 1 A_Lower;
		Loop;
	Select:
		RIFG A 1 A_Raise;
		Loop;
	Fire:
		RIFF AB 3 A_FireAssaultGun;
		RIFG D 3 A_FireAssaultGun;
		RIFG C 0 A_ReFire;
		RIFG B 2 A_Light0;
		Goto Ready;
	}
}

extend class StateProvider
{
	//============================================================================
	//
	// A_FireAssaultGun
	//
	//============================================================================

	action void A_FireAssaultGun()
	{
		if (player == null)
		{
			return;
		}
		int laflags = 0;
		int alflags = 0;
		int snd_channel = CHAN_WEAPON;
		Weapon weapon = invoker == player.OffhandWeapon ? player.OffhandWeapon : player.ReadyWeapon;
		if (weapon != null)
		{
			snd_channel = weapon.bOffhandWeapon ? CHAN_OFFWEAPON : CHAN_WEAPON;
			laflags |= weapon.bOffhandWeapon ? LAF_ISOFFHAND : 0;
			alflags |= weapon.bOffhandWeapon ? ALF_ISOFFHAND : 0;
			if (!weapon.DepleteAmmo (weapon.bAltFire))
				return;
		}
		A_StartSound ("weapons/assaultgun", snd_channel);
		player.mo.PlayAttacking2 ();

		int damage = 4 * random[StrifeGun](1, 3);
		double ang = angle;

		if (player.refire)
		{
			ang += Random2[StrifeGun]() * (22.5 / 256) * AccuracyFactor();
		}
		LineAttack (ang, PLAYERMISSILERANGE, BulletSlope (aimflags: alflags), damage, 'Hitscan', "StrifePuff", laflags);
	}
}


// Standing variant of the assault gun --------------------------------------

class AssaultGunStanding : WeaponGiver
{
	Default
	{
		DropItem "AssaultGun";
		Inventory.PickupMessage "$TXT_ASSAULTGUN";
	}
	States
	{
	Spawn:
		RIFL B -1;
		Stop;
	}
}


