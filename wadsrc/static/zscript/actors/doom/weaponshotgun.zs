// --------------------------------------------------------------------------
//
// Shotgun
//
// --------------------------------------------------------------------------

class Shotgun : DoomWeapon
{
	Default
	{
		Weapon.SelectionOrder 1300;
		Weapon.AmmoUse 1;
		Weapon.AmmoGive 8;
		Weapon.AmmoType "Shell";
		Inventory.PickupMessage "$GOTSHOTGUN";
		Obituary "$OB_MPSHOTGUN";
		Tag "$TAG_SHOTGUN";
	}
	States
	{
	Ready:
		SHTG A 1 A_WeaponReady;
		Loop;
	Deselect:
		SHTG A 1 A_Lower;
		Loop;
	Select:
		SHTG A 1 A_Raise;
		Loop;
	Fire:
		SHTG A 3;
		SHTG A 7 A_FireShotgun;
		SHTG BC 5;
		SHTG D 4;
		SHTG CB 5;
		SHTG A 3;
		SHTG A 7 A_ReFire;
		Goto Ready;
	Flash:
		SHTF A 4 Bright A_Light1;
		SHTF B 3 Bright A_Light2;
		Goto LightDone;
	Spawn:
		SHOT A -1;
		Stop;
	}
}
	
//===========================================================================
//
// Code (must be attached to StateProvider)
//
//===========================================================================

extend class StateProvider
{

	action void A_FireShotgun()
	{
		if (player == null)
		{
			return;
		}
		int alflags = 0;
		int snd_channel = CHAN_WEAPON;
		Weapon weap = invoker == player.OffhandWeapon ? player.OffhandWeapon : player.ReadyWeapon;
		if (weap != null && invoker == weap && stateinfo != null && stateinfo.mStateType == STATE_Psprite)
		{
			snd_channel = weap.bOffhandWeapon ? CHAN_OFFWEAPON : CHAN_WEAPON;
			alflags |= weap.bOffhandWeapon ? ALF_ISOFFHAND : 0;
			if (!weap.DepleteAmmo (weap.bAltFire, true, 1))
				return;
			
			player.SetPsprite(PSP_FLASH, weap.FindState('Flash'), true, weap);
		}
		player.mo.PlayAttacking2 ();
		A_StartSound ("weapons/shotgf", snd_channel);

		double pitch = BulletSlope (aimflags: alflags);

		for (int i = 0; i < 7; i++)
		{
			GunShot (false, "BulletPuff", pitch);
		}
	}

}	

