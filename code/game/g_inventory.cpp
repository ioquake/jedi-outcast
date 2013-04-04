//g_inventory.cpp

// leave this line at the top for all g_xxxx.cpp files...
#include "g_headers.h"

#include "g_local.h"

/*
================
Goodie Keys
================
*/
qboolean INV_GoodieKeyGive( gentity_t *target )
{
	if ( !target || !target->client )
	{
		return (qfalse);
	}

	for ( int i = INV_GOODIE_KEY1; i <= INV_GOODIE_KEY5; i++ )
	{
		if ( target->client->ps.inventory[i] == 0 )
		{//fill first empty slot
			target->client->ps.inventory[i] = 1;
			return (qtrue);
		}
	}
	//no empty slots
	return (qfalse);
}

qboolean INV_GoodieKeyTake( gentity_t *target )
{
	if ( !target || !target->client )
	{
		return (qfalse);
	}

	for ( int i = INV_GOODIE_KEY5; i >= INV_GOODIE_KEY1; i-- )
	{
		if ( target->client->ps.inventory[i] )
		{//remove last one
			target->client->ps.inventory[i] = 0;
			return (qtrue);
		}
	}
	//had no keys
	return (qfalse);
}

int INV_GoodieKeyCheck( gentity_t *target )
{
	if ( !target || !target->client )
	{
		return (qfalse);
	}

	for ( int i = INV_GOODIE_KEY5; i >= INV_GOODIE_KEY1; i-- )
	{
		if ( target->client->ps.inventory[i] )
		{//found a key
			return (i);
		}
	}
	//no keys
	return (qfalse);
}

/*
================
Security Keys
================
*/
qboolean INV_SecurityKeyGive( gentity_t *target, const char *keyname )
{
	if ( target == NULL || keyname == NULL || target->client == NULL )
	{
		return qfalse;
	}

	for ( int i = INV_SECURITY_KEY1; i <= INV_SECURITY_KEY5; i++ )
	{
		if ( target->client->ps.security_key_message[i-INV_SECURITY_KEY1][0] == NULL )
		{//fill in the first empty slot we find with this key
			target->client->ps.inventory[i] = 1;	// He got the key
			Q_strncpyz( target->client->ps.security_key_message[i-INV_SECURITY_KEY1], keyname, MAX_SECURITY_KEY_MESSSAGE, qtrue );
			return qtrue;
		}
	}
	//couldn't find an empty slot
	return qfalse;
}

void INV_SecurityKeyTake( gentity_t *target, char *keyname )
{
	if ( !target || !keyname || !target->client )
	{
		return;
	}

	for ( int i = INV_SECURITY_KEY1; i <= INV_SECURITY_KEY5; i++ )
	{
		if ( target->client->ps.security_key_message[i-INV_SECURITY_KEY1] )
		{
			if ( !Q_stricmp( keyname, target->client->ps.security_key_message[i-INV_SECURITY_KEY1] ) )
			{
				target->client->ps.inventory[i] = 0;	// Take the key
				target->client->ps.security_key_message[i-INV_SECURITY_KEY1][0] = NULL;
				return;
			}
		}
		/*
		//don't do this because we could have removed one that's between 2 valid ones
		else
		{
			break;
		}
		*/
	}
}

qboolean INV_SecurityKeyCheck( gentity_t *target, char *keyname )
{
	if ( !target || !keyname || !target->client )
	{
		return (qfalse);
	}

	for ( int i = INV_SECURITY_KEY1; i <= INV_SECURITY_KEY5; i++ )
	{
		if ( target->client->ps.inventory[i] && target->client->ps.security_key_message[i-INV_SECURITY_KEY1] )
		{
			if ( !Q_stricmp( keyname, target->client->ps.security_key_message[i-INV_SECURITY_KEY1] ) )
			{
				return (qtrue);
			}
		}
		/*
		//don't do this because we could have removed one that's between 2 valid ones
		else
		{
			break;
		}
		*/
	}

	return (qfalse);
}
