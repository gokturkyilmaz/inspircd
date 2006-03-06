/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include <unistd.h>
#include <sys/errno.h>
#include <time.h>
#include <string>
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#include <map>
#include <sstream>
#include <vector>
#include <deque>
#include "connection.h"
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "message.h"
#include "commands.h"
#include "xline.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "mode.h"

extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern InspIRCd* ServerInstance;
extern ServerConfig* Config;

extern time_t TIME;

char* ModeParser::GiveOps(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	
	if ((!user) || (!dest) || (!chan) || (!*dest))
	{
		log(DEFAULT,"*** BUG *** GiveOps was given an invalid parameter");
		return NULL;
	}
	d = Find(dest);
	if (!d)
	{
		log(DEFAULT,"the target nickname given to GiveOps couldnt be found");
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	else
	{
		if (user->server == d->server)
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_OP));
			
			if (MOD_RESULT == ACR_DENY)
				return NULL;
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_OP) && (!is_uline(user->server)) && (IS_LOCAL(user)))
				{
					log(DEBUG,"%s cant give ops to %s because they nave status %d and needs %d",user->nick,dest,status,STATUS_OP);
					WriteServ(user->fd,"482 %s %s :You're not a channel operator",user->nick, chan->name);
					return NULL;
				}
			}
		}


		for (unsigned int i = 0; i < d->chans.size(); i++)
		{
			if ((d->chans[i].channel != NULL) && (chan != NULL))
			if (!strcasecmp(d->chans[i].channel->name,chan->name))
			{
				if (d->chans[i].uc_modes & UCMODE_OP)
				{
					/* mode already set on user, dont allow multiple */
					return NULL;
				}
				d->chans[i].uc_modes = d->chans[i].uc_modes | UCMODE_OP;
				d->chans[i].channel->AddOppedUser((char*)d);
				log(DEBUG,"gave ops: %s %s",d->chans[i].channel->name,d->nick);
				return d->nick;
			}
		}
		log(DEFAULT,"The target channel given to GiveOps was not in the users mode list");
	}
	return NULL;
}

char* ModeParser::GiveHops(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	
	if ((!user) || (!dest) || (!chan) || (!*dest))
	{
		log(DEFAULT,"*** BUG *** GiveHops was given an invalid parameter");
		return NULL;
	}

	d = Find(dest);
	if (!d)
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	else
	{
		if (user->server == d->server)
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_HALFOP));
		
			if (MOD_RESULT == ACR_DENY)
				return NULL;
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_OP) && (!is_uline(user->server)) && (IS_LOCAL(user)))
				{
					WriteServ(user->fd,"482 %s %s :You're not a channel operator",user->nick, chan->name);
					return NULL;
				}
			}
		}

		for (unsigned int i = 0; i < d->chans.size(); i++)
		{
			if ((d->chans[i].channel != NULL) && (chan != NULL))
			if (!strcasecmp(d->chans[i].channel->name,chan->name))
			{
				if (d->chans[i].uc_modes & UCMODE_HOP)
				{
					/* mode already set on user, dont allow multiple */
					return NULL;
				}
				d->chans[i].uc_modes = d->chans[i].uc_modes | UCMODE_HOP;
				d->chans[i].channel->AddHalfoppedUser((char*)d);
				log(DEBUG,"gave h-ops: %s %s",d->chans[i].channel->name,d->nick);
				return d->nick;
			}
		}
	}
	return NULL;
}

char* ModeParser::GiveVoice(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	
	if ((!user) || (!dest) || (!chan) || (!*dest))
	{
		log(DEFAULT,"*** BUG *** GiveVoice was given an invalid parameter");
		return NULL;
	}

	d = Find(dest);
	if (!d)
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	else
	{
		if (user->server == d->server)
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_VOICE));
			
			if (MOD_RESULT == ACR_DENY)
				return NULL;
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_HOP) && (!is_uline(user->server)) && (IS_LOCAL(user)))
				{
					WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, chan->name);
					return NULL;
				}
			}
		}

		for (unsigned int i = 0; i < d->chans.size(); i++)
		{
			if ((d->chans[i].channel != NULL) && (chan != NULL))
			if (!strcasecmp(d->chans[i].channel->name,chan->name))
			{
				if (d->chans[i].uc_modes & UCMODE_VOICE)
				{
					/* mode already set on user, dont allow multiple */
					return NULL;
				}
				d->chans[i].uc_modes = d->chans[i].uc_modes | UCMODE_VOICE;
				d->chans[i].channel->AddVoicedUser((char*)d);
				log(DEBUG,"gave voice: %s %s",d->chans[i].channel->name,d->nick);
				return d->nick;
			}
		}
	}
	return NULL;
}

char* ModeParser::TakeOps(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	
	if ((!user) || (!dest) || (!chan) || (!*dest))
	{
		log(DEFAULT,"*** BUG *** TakeOps was given an invalid parameter");
		return NULL;
	}

	d = Find(dest);
	if (!d)
	{
		log(DEBUG,"TakeOps couldnt resolve the target nickname: %s",dest);
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	else
	{
		if (user->server == d->server)
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_DEOP));
			
			if (MOD_RESULT == ACR_DENY)
				return NULL;
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_OP) && (!is_uline(user->server)) && (IS_LOCAL(user)) && (IS_LOCAL(user)))
				{
					WriteServ(user->fd,"482 %s %s :You are not a channel operator",user->nick, chan->name);
					return NULL;
				}
			}
		}

		for (unsigned int i = 0; i < d->chans.size(); i++)
		{
			if ((d->chans[i].channel != NULL) && (chan != NULL))
			if (!strcasecmp(d->chans[i].channel->name,chan->name))
			{
				if ((d->chans[i].uc_modes & UCMODE_OP) == 0)
				{
					/* mode already set on user, dont allow multiple */
					return NULL;
				}
				d->chans[i].uc_modes ^= UCMODE_OP;
				d->chans[i].channel->DelOppedUser((char*)d);
				log(DEBUG,"took ops: %s %s",d->chans[i].channel->name,d->nick);
				return d->nick;
			}
		}
		log(DEBUG,"TakeOps couldnt locate the target channel in the target users list");
	}
	return NULL;
}

char* ModeParser::TakeHops(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	
	if ((!user) || (!dest) || (!chan) || (!*dest))
	{
		log(DEFAULT,"*** BUG *** TakeHops was given an invalid parameter");
		return NULL;
	}

	d = Find(dest);
	if (!d)
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	else
	{
		if (user->server == d->server)
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_DEHALFOP));
			
			if (MOD_RESULT == ACR_DENY)
				return NULL;
			if (MOD_RESULT == ACR_DEFAULT)
			{
				/* Tweak by Brain suggested by w00t, allow a halfop to dehalfop themselves */
				if ((user != d) && ((status < STATUS_OP) && (!is_uline(user->server))) && (IS_LOCAL(user)))
				{
					WriteServ(user->fd,"482 %s %s :You are not a channel operator",user->nick, chan->name);
					return NULL;
				}
			}
		}

		for (unsigned int i = 0; i < d->chans.size(); i++)
		{
			if ((d->chans[i].channel != NULL) && (chan != NULL))
			if (!strcasecmp(d->chans[i].channel->name,chan->name))
			{
				if ((d->chans[i].uc_modes & UCMODE_HOP) == 0)
				{
					/* mode already set on user, dont allow multiple */
					return NULL;
				}
				d->chans[i].uc_modes ^= UCMODE_HOP;
				d->chans[i].channel->DelHalfoppedUser((char*)d);
				log(DEBUG,"took h-ops: %s %s",d->chans[i].channel->name,d->nick);
				return d->nick;
			}
		}
	}
	return NULL;
}

char* ModeParser::TakeVoice(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	
	if ((!user) || (!dest) || (!chan) || (!*dest))
	{
		log(DEFAULT,"*** BUG *** TakeVoice was given an invalid parameter");
		return NULL;
	}

	d = Find(dest);
	if (!d)
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	else
	{
		if (user->server == d->server)
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_DEVOICE));
			
			if (MOD_RESULT == ACR_DENY)
				return NULL;
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_HOP) && (!is_uline(user->server)) && (IS_LOCAL(user)))
				{
					WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, chan->name);
					return NULL;
				}
			}
		}

		for (unsigned int i = 0; i < d->chans.size(); i++)
		{
			if ((d->chans[i].channel != NULL) && (chan != NULL))
			if (!strcasecmp(d->chans[i].channel->name,chan->name))
			{
				if ((d->chans[i].uc_modes & UCMODE_VOICE) == 0)
				{
					/* mode already set on user, dont allow multiple */
					return NULL;
				}
				d->chans[i].uc_modes ^= UCMODE_VOICE;
				d->chans[i].channel->DelVoicedUser((char*)d);
				log(DEBUG,"took voice: %s %s",d->chans[i].channel->name,d->nick);
				return d->nick;
			}
		}
	}
	return NULL;
}

char* ModeParser::AddBan(userrec *user,char *dest,chanrec *chan,int status)
{
	BanItem b;
	int toomanyexclamation = 0;
	int toomanyat = 0;

	if ((!user) || (!dest) || (!chan) || (!*dest))
	{
		log(DEFAULT,"*** BUG *** AddBan was given an invalid parameter");
		return NULL;
	}

	for (char* i = dest; *i; i++)
	{
		if ((*i < 32) || (*i > 126))
		{
			return NULL;
		}
		else if (*i == '!')
		{
			toomanyexclamation++;
		}
		else if (*i == '@')
		{
			toomanyat++;
		}
	}

	if (toomanyexclamation != 1 || toomanyat != 1)
		/*
		 * this stops sillyness like n!u!u!u@h, though note that most
		 * ircds don't actually verify banmask validity. --w00t
		 */
		return NULL;

	long maxbans = GetMaxBans(chan->name);
	if ((unsigned)chan->bans.size() > (unsigned)maxbans)
	{
		WriteServ(user->fd,"478 %s %s :Channel ban list for %s is full (maximum entries for this channel is %d)",user->nick, chan->name,chan->name,maxbans);
		return NULL;
	}

	log(DEBUG,"AddBan: %s %s",chan->name,user->nick);

	int MOD_RESULT = 0;
	FOREACH_RESULT(I_OnAddBan,OnAddBan(user,chan,dest));
	if (MOD_RESULT)
		return NULL;

	TidyBan(dest);
	for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
	{
		if (!strcasecmp(i->data,dest))
		{
			// dont allow a user to set the same ban twice
			return NULL;
		}
	}

	b.set_time = TIME;
	strlcpy(b.data,dest,MAXBUF);
	if (*user->nick)
	{
		strlcpy(b.set_by,user->nick,NICKMAX-1);
	}
	else
	{
		strlcpy(b.set_by,Config->ServerName,NICKMAX-1);
	}
	chan->bans.push_back(b);
	return dest;
}

char* ModeParser::TakeBan(userrec *user,char *dest,chanrec *chan,int status)
{
	if ((!user) || (!dest) || (!chan) || (!*dest)) {
		log(DEFAULT,"*** BUG *** TakeBan was given an invalid parameter");
		return 0;
	}

	log(DEBUG,"del_ban: %s %s",chan->name,user->nick);
	for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
	{
		if (!strcasecmp(i->data,dest))
		{
		        int MOD_RESULT = 0;
		        FOREACH_RESULT(I_OnDelBan,OnDelBan(user,chan,dest));
		        if (MOD_RESULT)
		                return NULL;
			chan->bans.erase(i);
			return dest;
		}
	}
	return NULL;
}

// tidies up redundant modes, e.g. +nt-nt+i becomes +-+i,
// a section further down the chain tidies up the +-+- crap.
std::string ModeParser::CompressModes(std::string modes,bool channelmodes)
{
	int counts[127];
	bool active[127];
	memset(counts,0,sizeof(counts));
	memset(active,0,sizeof(active));
	for (unsigned int i = 0; i < modes.length(); i++)
	{
		if ((modes[i] == '+') || (modes[i] == '-'))
			continue;
		if (channelmodes)
		{
			if ((strchr("itnmsp",modes[i])) || ((ModeDefined(modes[i],MT_CHANNEL)) && (ModeDefinedOn(modes[i],MT_CHANNEL)==0) && (ModeDefinedOff(modes[i],MT_CHANNEL)==0)))
			{
				log(DEBUG,"Tidy mode %c",modes[i]);
				counts[(unsigned int)modes[i]]++;
				active[(unsigned int)modes[i]] = true;
			}
		}
		else
		{
			log(DEBUG,"Tidy mode %c",modes[i]);
			counts[(unsigned int)modes[i]]++;
			active[(unsigned int)modes[i]] = true;
		}
	}
	for (int j = 65; j < 127; j++)
	{
		if ((counts[j] > 1) && (active[j] == true))
		{
			static char v[2];
			v[0] = (unsigned char)j;
			v[1] = '\0';
			std::string mode_str = v;
			std::string::size_type pos = modes.find(mode_str);
			if (pos != std::string::npos)
			{
				log(DEBUG,"all occurances of mode %c to be deleted...",(unsigned char)j);
				while (modes.find(mode_str) != std::string::npos)
					modes.erase(modes.find(mode_str),1);
				log(DEBUG,"New mode line: %s",modes.c_str());
			}
		}
	}
	return modes;
}

void ModeParser::ProcessModes(char **parameters,userrec* user,chanrec *chan,int status, int pcnt, bool servermode, bool silent, bool local)
{
	if (!parameters) {
		log(DEFAULT,"*** BUG *** process_modes was given an invalid parameter");
		return;
	}

	char outlist[MAXBUF];
	char *outpars[32];
	int param = 2;
	int pc = 0;
	int ptr = 0;
	int mdir = 1;
	char* r = NULL;
	bool k_set = false, l_set = false, previously_set_l = false, previously_unset_l = false, previously_set_k = false, previously_unset_k = false;

	if (pcnt < 2)
	{
		return;
	}

	int MOD_RESULT = 0;
	
	if (IS_LOCAL(user))
	{
		FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,NULL,chan,AC_GENERAL_MODE));	
		if (MOD_RESULT == ACR_DENY)
			return;
	}

	log(DEBUG,"process_modes: start: parameters=%d",pcnt);

	char* modelist = parameters[1];         /* mode list, e.g. +oo-o *
						 * parameters[2] onwards are parameters for
					 	 * modes that require them :) */
	*outlist = *modelist;
	char* outl = outlist+1;

	mdir = (*modelist == '+');

	log(DEBUG,"process_modes: modelist: %s",modelist);

	std::string tidied = this->CompressModes(modelist,true);
	strlcpy(modelist,tidied.c_str(),MAXBUF);

	int len = tidied.length();
	while (modelist[len-1] == ' ')
		modelist[--len] = '\0';

	bool next_cant_be_modifier = false;
	char* modechar;

	for (modechar = (modelist + 1); *modechar; ptr++, modechar++)
	{
		r = NULL;

		/* If we have more than MAXMODES changes in one line,
		 * drop all after the MAXMODES
		 */
		if (pc > MAXMODES-1)
			break;

		log(DEBUG,"Mode %c",*modechar);

		{
			switch (*modechar)
			{
				case '-':
					*outl++ = '-';
					mdir = 0;
					next_cant_be_modifier = true;
					
				break;			

				case '+':
					*outl++ = '+';
					mdir = 1;
					next_cant_be_modifier = true;
				break;

				case 'o':
					log(DEBUG,"Ops");
					if ((param >= pcnt)) break;
					log(DEBUG,"Enough parameters left");
					r = NULL;
					if (mdir == 1)
					{
						MOD_RESULT = 0;
						FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'o', parameters[param], true, 1));
						if (!MOD_RESULT)
						{
							log(DEBUG,"calling GiveOps");
							r = GiveOps(user,parameters[param++],chan,status);
						}
						else param++;
					}
					else
					{
                                                MOD_RESULT = 0;
                                                FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'o', parameters[param], false, 1));
                                                if (!MOD_RESULT)
                                                {
							log(DEBUG,"calling TakeOps");
							r = TakeOps(user,parameters[param++],chan,status);
						}
						else param++;
					}
					if (r)
					{
						*outl++ = 'o';
						outpars[pc++] = r;
					}
				break;
			
				case 'h':
					if (((param >= pcnt)) || (!Config->AllowHalfop)) break;
					r = NULL;
					if (mdir == 1)
					{
                                                MOD_RESULT = 0;
                                                FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'h', parameters[param], true, 1));
                                                if (!MOD_RESULT)
                                                {
							r = GiveHops(user,parameters[param++],chan,status);
						}
						else param++;
					}
					else
					{
                                                MOD_RESULT = 0;
                                                FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'h', parameters[param], false, 1));
                                                if (!MOD_RESULT)
                                                {
							r = TakeHops(user,parameters[param++],chan,status);
						}
						else param++;
					}
					if (r)
					{
						*outl++ = 'h';
						outpars[pc++] = r;
					}
				break;
			
				
				case 'v':
					if ((param >= pcnt)) break;
					r = NULL;
					if (mdir == 1)
					{
                                                MOD_RESULT = 0;
                                                FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'v', parameters[param], true, 1));
                                                if (!MOD_RESULT)
                                                {
							r = GiveVoice(user,parameters[param++],chan,status);
						}
						else param++;
					}
					else
					{
                                                MOD_RESULT = 0;
                                                FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'v', parameters[param], false, 1));
                                                if (!MOD_RESULT)
                                                {
							r = TakeVoice(user,parameters[param++],chan,status);
						}
						else param++;
					}
					if (r)
					{
						*outl++ = 'v';
						outpars[pc++] = r;
					}
				break;
				
				case 'b':
					if ((param >= pcnt)) break;
					r = NULL;
					if (mdir == 1)
					{
                                                MOD_RESULT = 0;
                                                FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'b', parameters[param], true, 1));
                                                if (!MOD_RESULT)
                                                {
							r = AddBan(user,parameters[param++],chan,status);
						}
						else param++;
					}
					else
					{
                                                MOD_RESULT = 0;
                                                FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'b', parameters[param], false, 1));
                                                if (!MOD_RESULT)
                                                {
							r = TakeBan(user,parameters[param++],chan,status);
						}
						else param++;
					}
					if (r)
					{
						*outl++ = 'b';
						outpars[pc++] = parameters[param-1];
					}
				break;


				case 'k':
					if ((param >= pcnt))
						break;

					if (mdir == 1)
					{
						if (k_set)
							break;

						if (previously_unset_k)
							break;
						previously_set_k = true;
						
						if (!*chan->key)
						{
							MOD_RESULT = 0;
							FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'k', parameters[param], true, 1));
							if (!MOD_RESULT)
							{
								*outl++ = 'k';
								char key[MAXBUF];
								strlcpy(key,parameters[param++],32);
								outpars[pc++] = key;
								strlcpy(chan->key,key,MAXBUF);
								k_set = true;
							}
							else param++;
						}
					}
					else
					{
						/* checks on -k are case sensitive and only accurate to the
  						   first 32 characters */
						if (previously_set_k)
							break;
						previously_unset_k = true;

						char key[MAXBUF];
						MOD_RESULT = 0;
						FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'k', parameters[param], false, 1));
						if (!MOD_RESULT)
						{
							strlcpy(key,parameters[param++],32);
							/* only allow -k if correct key given */
							if (!strcmp(chan->key,key))
							{
								*outl++ = 'k';
								*chan->key = 0;
								outpars[pc++] = key;
							}
						}
						else param++;
					}
				break;
				
				case 'l':
					if (mdir == 0)
					{
						if (previously_set_l)
							break;
						previously_unset_l = true;
                                                MOD_RESULT = 0;
                                                FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'l', "", false, 0));
                                                if (!MOD_RESULT)
                                                {
							if (chan->limit)
							{
								*outl++ = 'l';
								chan->limit = 0;
							}
						}
					}
					
					if ((param >= pcnt)) break;
					if (mdir == 1)
					{
						if (l_set)
							break;
						if (previously_unset_l)
							break;
						previously_set_l = true;
						bool invalid = false;
						for (char* f = parameters[param]; *f; f++)
						{
							if ((*f < '0') || (*f > '9'))
							{
								invalid = true;
							}
						}
						/* If the limit is < 1, or the new limit is the current limit, dont allow */
						if ((atoi(parameters[param]) < 1) || ((chan->limit > 0) && (atoi(parameters[param]) == chan->limit)))
						{
							invalid = true;
						}

						if (invalid)
							break;

                                                MOD_RESULT = 0;
                                                FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'l', parameters[param], true, 1));
                                                if (!MOD_RESULT)
                                                {
	
							chan->limit = atoi(parameters[param]);
							
							// reported by mech: large values cause underflow
							if (chan->limit < 0)
								chan->limit = 0x7FFF;
						}
							
						if (chan->limit)
						{
							*outl++ = 'l';
							outpars[pc++] = parameters[param++];
							l_set = true;
						}
					}
				break;
				
				case 'i':
                                        MOD_RESULT = 0;
                                        FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'i', "", mdir, 0));
                                        if (!MOD_RESULT)
                                        {
						if (mdir)
						{
							if (!(chan->binarymodes & CM_INVITEONLY)) *outl++ = 'i';
							chan->binarymodes |= CM_INVITEONLY;
						}
						else
						{
							if (chan->binarymodes & CM_INVITEONLY) *outl++ = 'i';
							chan->binarymodes &= ~CM_INVITEONLY;
						}
					}
				break;
				
				case 't':
                                        MOD_RESULT = 0;
                                        FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 't', "", mdir, 0));
                                        if (!MOD_RESULT)
                                        {
						if (mdir)
                                                {
							if (!(chan->binarymodes & CM_TOPICLOCK)) *outl++ = 't';
                                                        chan->binarymodes |= CM_TOPICLOCK;
                                                }
                                                else
                                                {
							if (chan->binarymodes & CM_TOPICLOCK) *outl++ = 't';
                                                        chan->binarymodes &= ~CM_TOPICLOCK;
                                                }
					}
				break;
				
				case 'n':
                                        MOD_RESULT = 0;
                                        FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'n', "", mdir, 0));
                                        if (!MOD_RESULT)
                                        {
                                                if (mdir)
                                                {
							if (!(chan->binarymodes & CM_NOEXTERNAL)) *outl++ = 'n';
                                                        chan->binarymodes |= CM_NOEXTERNAL;
                                                }
                                                else
                                                {
							if (chan->binarymodes & CM_NOEXTERNAL) *outl++ = 'n';
                                                        chan->binarymodes &= ~CM_NOEXTERNAL;
                                                }
					}
				break;
				
				case 'm':
                                        MOD_RESULT = 0;
                                        FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'm', "", mdir, 0));
                                        if (!MOD_RESULT)
                                        {
                                                if (mdir)
                                                {
                                                        if (!(chan->binarymodes & CM_MODERATED)) *outl++ = 'm';
                                                        chan->binarymodes |= CM_MODERATED;
                                                }
                                                else
                                                {
                                                        if (chan->binarymodes & CM_MODERATED) *outl++ = 'm';
                                                        chan->binarymodes &= ~CM_MODERATED;
                                                }
					}
				break;
				
				case 's':
                                        MOD_RESULT = 0;
                                        FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 's', "", mdir, 0));
                                        if (!MOD_RESULT)
                                        {
                                                if (mdir)
                                                {
                                                        if (!(chan->binarymodes & CM_SECRET)) *outl++ = 's';
                                                        chan->binarymodes |= CM_SECRET;
                                                        if (chan->binarymodes & CM_PRIVATE)
                                                        {
                                                                chan->binarymodes &= ~CM_PRIVATE;
                                                                if (mdir)
                                                                {
									*outl++ = '-'; *outl++ = 'p'; *outl++ = '+';
                                                                }
                                                        }
                                                }
                                                else
                                                {
                                                        if (chan->binarymodes & CM_SECRET) *outl++ = 's';
                                                        chan->binarymodes &= ~CM_SECRET;
                                                }
					}
				break;
				
				case 'p':
                                        MOD_RESULT = 0;
                                        FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'p', "", mdir, 0));
                                        if (!MOD_RESULT)
                                        {
                                                if (mdir)
                                                {
                                                        if (!(chan->binarymodes & CM_PRIVATE)) *outl++ = 'p';
                                                        chan->binarymodes |= CM_PRIVATE;
                                                        if (chan->binarymodes & CM_SECRET)
                                                        {
                                                                chan->binarymodes &= ~CM_SECRET;
                                                                if (mdir)
                                                                {
									*outl++ = '-'; *outl++ = 's'; *outl++ = '+';
                                                                }
                                                        }
                                                }
                                                else
                                                {
                                                        if (chan->binarymodes & CM_PRIVATE) *outl++ = 'p';
                                                        chan->binarymodes &= ~CM_PRIVATE;
                                                }
					}
				break;
				
				default:
					log(DEBUG,"Preprocessing custom mode %c: modelist: %s",*modechar,chan->custom_modes);
					string_list p;
					p.clear();
					if (((!strchr(chan->custom_modes,*modechar)) && (!mdir)) || ((strchr(chan->custom_modes,*modechar)) && (mdir)))
					{
						if (!ModeIsListMode(*modechar,MT_CHANNEL))
						{
							log(DEBUG,"Mode %c isnt set on %s but trying to remove!",*modechar,chan->name);
							break;
						}
					}
					if (ModeDefined(*modechar,MT_CHANNEL))
					{
						log(DEBUG,"A module has claimed this mode");
						if (param<pcnt)
						{
     							if ((ModeDefinedOn(*modechar,MT_CHANNEL)>0) && (mdir))
							{
      								p.push_back(parameters[param]);
  							}
							if ((ModeDefinedOff(*modechar,MT_CHANNEL)>0) && (!mdir))
							{
      								p.push_back(parameters[param]);
  							}
  						}
  						bool handled = false;
  						if (param>=pcnt)
  						{
  							// we're supposed to have a parameter, but none was given... so dont handle the mode.
  							if (((ModeDefinedOn(*modechar,MT_CHANNEL)>0) && (mdir)) || ((ModeDefinedOff(*modechar,MT_CHANNEL)>0) && (!mdir)))	
  							{
  								log(DEBUG,"Not enough parameters for module-mode %c",*modechar);
  								handled = true;
  								param++;
  							}
  						}

						// BIG ASS IDIOTIC CODER WARNING!
						// Using OnRawMode on another modules mode's behavour 
						// will confuse the crap out of admins! just because you CAN
						// do it, doesnt mean you SHOULD!
	                                        MOD_RESULT = 0;
						std::string para = "";
						if (p.size())
							para = p[0];
        	                                FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, *modechar, para, mdir, pcnt));
                	                        if (!MOD_RESULT)
                        	                {
  							for (int i = 0; i <= MODCOUNT; i++)
							{
								if (!handled)
								{
									int t = modules[i]->OnExtendedMode(user,chan,*modechar,MT_CHANNEL,mdir,p);
									if (t != 0)
									{
										log(DEBUG,"OnExtendedMode returned nonzero for a module");
										if (ModeIsListMode(*modechar,MT_CHANNEL))
										{
											if (t == -1)
											{
												//pc++;
												param++;
											}
											else
											{
												if (param < pcnt)
												{
													*outl++ = *modechar;
												}
												outpars[pc++] = parameters[param++];
											}
										}
										else
										{
											if (param < pcnt)
											{
												*outl++ = *modechar;
												chan->SetCustomMode(*modechar,mdir);
												// include parameters in output if mode has them
												if ((ModeDefinedOn(*modechar,MT_CHANNEL)>0) && (mdir))
												{
													chan->SetCustomModeParam(modelist[ptr],parameters[param],mdir);
													outpars[pc++] = parameters[param++];
												}
											}
										}
										// break, because only one module can handle the mode.
										handled = true;
       	 		 						}
       	 	 						}
	     						}
						}
     					}
					else
					{
						WriteServ(user->fd,"472 %s %c :is unknown mode char to me",user->nick,*modechar);
					}
				break;
				
			}
		}
	}

	/* Null terminate it now we're done */
	*outl = 0;


	/************ Fast, but confusing string tidying ************/
	outl = outlist;
	while (*outl && (*outl < 'A'))
		outl++;
	/* outl now points to the first mode character after +'s and -'s */
	outl--;
	/* Now points at first mode-modifier + or - symbol */
	char* trim = outl;
	/* Now we tidy off any trailing -'s etc */
	while (*trim++);
	trim--;
	while ((*--trim == '+') || (*trim == '-'))
		*trim = 0;
	/************ Done wih the string tidy functions ************/


	/* The mode change must be at least two characters long (+ or - and at least one mode) */
	if (((*outl == '+') || (*outl == '-')) && *(outl+1))
	{
		for (ptr = 0; ptr < pc; ptr++)
		{
			charlcat(outl,' ',MAXBUF);
			strlcat(outl,outpars[ptr],MAXBUF-1);
		}
		if (local)
		{
			log(DEBUG,"Local mode change");
			WriteChannelLocal(chan, user, "MODE %s %s",chan->name,outl);
			FOREACH_MOD(I_OnMode,OnMode(user, chan, TYPE_CHANNEL, outl));
		}
		else
		{
			if (servermode)
			{
				if (!silent)
				{
					WriteChannelWithServ(Config->ServerName,chan,"MODE %s %s",chan->name,outl);
				}
					
			}
			else
			{
				if (!silent)
				{
					WriteChannel(chan,user,"MODE %s %s",chan->name,outl);
					FOREACH_MOD(I_OnMode,OnMode(user, chan, TYPE_CHANNEL, outl));
				}
			}
		}
	}
}

// based on sourcemodes, return true or false to determine if umode is a valid mode a user may set on themselves or others.

bool ModeParser::AllowedUmode(char umode, char* sourcemodes,bool adding,bool serveroverride)
{
	log(DEBUG,"Allowed_umode: %c %s",umode,sourcemodes);
	// Servers can +o and -o arbitrarily
	if ((serveroverride == true) && (umode == 'o'))
	{
		return true;
	}
	// RFC1459 specified modes
	if ((umode == 'w') || (umode == 's') || (umode == 'i'))
	{
		log(DEBUG,"umode %c allowed by RFC1459 scemantics",umode);
		return true;
	}
	
	// user may not +o themselves or others, but an oper may de-oper other opers or themselves
	if ((strchr(sourcemodes,'o')) && (!adding))
	{
		log(DEBUG,"umode %c allowed by RFC1459 scemantics",umode);
		return true;
	}
	else if (umode == 'o')
	{
		log(DEBUG,"umode %c allowed by RFC1459 scemantics",umode);
		return false;
	}
	
	// process any module-defined modes that need oper
	if ((ModeDefinedOper(umode,MT_CLIENT)) && (strchr(sourcemodes,'o')))
	{
		log(DEBUG,"umode %c allowed by module handler (oper only mode)",umode);
		return true;
	}
	else
	if (ModeDefined(umode,MT_CLIENT))
	{
		// process any module-defined modes that don't need oper
		log(DEBUG,"umode %c allowed by module handler (non-oper mode)",umode);
		if ((ModeDefinedOper(umode,MT_CLIENT)) && (!strchr(sourcemodes,'o')))
		{
			// no, this mode needs oper, and this user 'aint got what it takes!
			return false;
		}
		return true;
	}

	// anything else - return false.
	log(DEBUG,"umode %c not known by any ruleset",umode);
	return false;
}

bool ModeParser::ProcessModuleUmode(char umode, userrec* source, void* dest, bool adding)
{
	userrec* s2;
	bool faked = false;
	if (!source)
	{
		s2 = new userrec;
		strlcpy(s2->nick,Config->ServerName,NICKMAX-1);
		*s2->modes = 'o';
		*(s2->modes+1) = 0;
		s2->fd = -1;
		source = s2;
		faked = true;
	}
	string_list p;
	p.clear();
	if (ModeDefined(umode,MT_CLIENT))
	{
		for (int i = 0; i <= MODCOUNT; i++)
		{
			if (modules[i]->OnExtendedMode(source,(void*)dest,umode,MT_CLIENT,adding,p))
			{
				log(DEBUG,"Module %s claims umode %c",Config->module_names[i].c_str(),umode);
				return true;
			}
		}
		log(DEBUG,"No module claims umode %c",umode);
		if (faked)
		{
			delete s2;
			source = NULL;
		}
		return false;
	}
	else
	{
		if (faked)
		{
			delete s2;
			source = NULL;
		}
		return false;
	}
}

void cmd_mode::Handle (char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr;
	userrec* dest;
	int can_change;
	int direction = 1;
	char outpars[MAXBUF];
	bool next_ok = true;

	dest = Find(parameters[0]);

	if (!user)
	{
		return;
	}

	if ((dest) && (pcnt == 1))
	{
		WriteServ(user->fd,"221 %s :+%s",dest->nick,dest->modes);
		return;
	}

	if ((dest) && (pcnt > 1))
	{
		std::string tidied = ServerInstance->ModeGrok->CompressModes(parameters[1],false);
		parameters[1] = (char*)tidied.c_str();

		char dmodes[MAXBUF];
		strlcpy(dmodes,dest->modes,MAXMODES);
		log(DEBUG,"pulled up dest user modes: %s",dmodes);

		can_change = 0;
		if (user != dest)
		{
			if ((strchr(user->modes,'o')) || (is_uline(user->server)))
			{
				can_change = 1;
			}
		}
		else
		{
			can_change = 1;
		}
		if (!can_change)
		{
			WriteServ(user->fd,"482 %s :Can't change mode for other users",user->nick);
			return;
		}
		
		outpars[0] = *parameters[1];
		outpars[1] = 0;
		direction = (*parameters[1] == '+');

		if ((*parameters[1] != '+') && (*parameters[1] != '-'))
			return;

		for (char* i = parameters[1]; *i; i++)
		{
			if (*i == ' ')
				continue;

			if ((i != parameters[1]) && (*i != '+') && (*i != '-'))
				next_ok = true;

			if (*i == '+')
			{
				if ((direction != 1) && (next_ok))
				{
					charlcat(outpars,'+',MAXBUF);
					next_ok = false;
				}	
				direction = 1;
			}
			else
			if (*i == '-')
			{
				if ((direction != 0) && (next_ok))
				{
					charlcat(outpars,'-',MAXBUF);
					next_ok = false;
				}
				direction = 0;
			}
			else
			{
				can_change = 0;
				if (strchr(user->modes,'o'))
				{
					can_change = 1;
				}
				else
				{
					if ((*i == 'i') || (*i == 'w') || (*i == 's') || (ServerInstance->ModeGrok->AllowedUmode(*i,user->modes,direction,false)))
					{
						can_change = 1;
					}
				}
				if (can_change)
				{
					if (direction == 1)
					{
						if ((!strchr(dmodes,*i)) && (ServerInstance->ModeGrok->AllowedUmode(*i,user->modes,true,false)))
						{
							if ((ServerInstance->ModeGrok->ProcessModuleUmode(*i, user, dest, direction)) || (*i == 'i') || (*i == 's') || (*i == 'w') || (*i == 'o'))
							{
								charlcat(dmodes,*i,MAXMODES);
								charlcat(outpars,*i,MAXMODES);
								if (*i == 'o')
								{
									FOREACH_MOD(I_OnGlobalOper,OnGlobalOper(dest));
								}
							}
						}
					}
					else
					{
						if ((ServerInstance->ModeGrok->AllowedUmode(*i,user->modes,false,false)) && (strchr(dmodes,*i)))
						{
							if ((ServerInstance->ModeGrok->ProcessModuleUmode(*i, user, dest, direction)) || (*i == 'i') || (*i == 's') || (*i == 'w') || (*i == 'o'))
							{
								charlcat(outpars,*i,MAXMODES);
								charremove(dmodes,*i);
								if (*i == 'o')
								{
									*dest->oper = 0;
									DeleteOper(dest);
								}
							}
						}
					}
				}
			}
		}
		if (*outpars)
		{
			char b[MAXBUF];
			char* z = b;

			for (char* i = outpars; *i;)
			{
				*z++ = *i++;
				if (((*i == '-') || (*i == '+')) && ((*(i+1) == '-') || (*(i+1) == '+')))
				{
					// someones playing silly buggers and trying
					// to put a +- or -+ into the line...
					i++;
				}
				if (!*(i+1))
				{
					// Someone's trying to make the last character in
					// the line be a + or - symbol.
					if ((*i == '-') || (*i == '+'))
					{
						i++;
					}
				}
			}
			*z = 0;

			if ((*b) && (!IS_SINGLE(b,'+')) && (!IS_SINGLE(b,'-')))
			{
				WriteTo(user, dest, "MODE %s :%s", dest->nick, b);
				FOREACH_MOD(I_OnMode,OnMode(user, dest, TYPE_USER, b));
			}

			log(DEBUG,"Stripped mode line");
			log(DEBUG,"Line dest is now %s",dmodes);
			strlcpy(dest->modes,dmodes,MAXMODES-1);

		}

		return;
	}
	
	Ptr = FindChan(parameters[0]);
	if (Ptr)
	{
		if (pcnt == 1)
		{
			/* just /modes #channel */
			WriteServ(user->fd,"324 %s %s +%s",user->nick, Ptr->name, chanmodes(Ptr,has_channel(user,Ptr)));
                        WriteServ(user->fd,"329 %s %s %d", user->nick, Ptr->name, Ptr->created);
			return;
		}
		else
		if (pcnt == 2)
		{
			char* mode = parameters[1];
			if (*mode == '+')
				mode++;
			int MOD_RESULT = 0;
                        FOREACH_RESULT(I_OnRawMode,OnRawMode(user, Ptr, *mode, "", false, 0));
                        if (!MOD_RESULT)
                        {
				if (*mode == 'b')
				{

					for (BanList::iterator i = Ptr->bans.begin(); i != Ptr->bans.end(); i++)
					{
						WriteServ(user->fd,"367 %s %s %s %s %d",user->nick, Ptr->name, i->data, i->set_by, i->set_time);
					}
					WriteServ(user->fd,"368 %s %s :End of channel ban list",user->nick, Ptr->name);
					return;
				}
				if ((ModeDefined(*mode,MT_CHANNEL)) && (ModeIsListMode(*mode,MT_CHANNEL)))
				{
					// list of items for an extmode
					log(DEBUG,"Calling OnSendList for all modules, list output for mode %c",*mode);
					FOREACH_MOD(I_OnSendList,OnSendList(user,Ptr,*mode));
					return;
				}
			}
		}

                if (((Ptr) && (!has_channel(user,Ptr))) && (!is_uline(user->server)) && (IS_LOCAL(user)))
                {
                        WriteServ(user->fd,"442 %s %s :You're not on that channel!",user->nick, Ptr->name);
                        return;
                }

		if (Ptr)
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,NULL,Ptr,AC_GENERAL_MODE));
			
			if (MOD_RESULT == ACR_DENY)
				return;
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((cstatus(user,Ptr) < STATUS_HOP) && (IS_LOCAL(user)))
				{
					WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, Ptr->name);
					return;
				}
			}

			ServerInstance->ModeGrok->ProcessModes(parameters,user,Ptr,cstatus(user,Ptr),pcnt,false,false,false);
		}
	}
	else
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
	}
}




void ModeParser::ServerMode(char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr;
	userrec* dest;
	int can_change;
	int direction = 1;
	char outpars[MAXBUF];
	bool next_ok = true;

	dest = Find(parameters[0]);
	
	// fix: ChroNiCk found this - we cant use this as debug if its null!
	if (dest)
	{
		log(DEBUG,"server_mode on %s",dest->nick);
	}

	if ((dest) && (pcnt > 1))
	{
                std::string tidied = ServerInstance->ModeGrok->CompressModes(parameters[1],false);
                parameters[1] = (char*)tidied.c_str();

		char dmodes[MAXBUF];
		strlcpy(dmodes,dest->modes,MAXBUF);

		outpars[0] = *parameters[1];
		outpars[1] = 0;
		direction = (*parameters[1] == '+');

		if ((*parameters[1] != '+') && (*parameters[1] != '-'))
			return;

		for (char* i = parameters[1]; *i; i++)
		{
                        if (*i == ' ')
                                continue;

			if ((i != parameters[1]) && (*i != '+') && (*i != '-'))
				next_ok = true;

			if (*i == '+')
			{
				if ((direction != 1) && (next_ok))
				{
					next_ok = false;
					charlcat(outpars,'+',MAXBUF);
				}
				direction = 1;
			}
			else
			if (*i == '-')
			{
				if ((direction != 0) && (next_ok))
				{
					next_ok = false;
					charlcat(outpars,'-',MAXBUF);
				}
				direction = 0;
			}
			else
			{
				log(DEBUG,"begin mode processing entry");
				can_change = 1;
				if (can_change)
				{
					if (direction == 1)
					{
						log(DEBUG,"umode %c being added",*i);
						if ((!strchr(dmodes,*i)) && (ServerInstance->ModeGrok->AllowedUmode(*i,user->modes,true,true)))
						{
							log(DEBUG,"umode %c is an allowed umode",*i);
							if ((ServerInstance->ModeGrok->ProcessModuleUmode(*i, user, dest, direction)) || (*i == 'i') || (*i == 's') || (*i == 'w') || (*i == 'o'))
							{
								charlcat(dmodes,*i,MAXMODES);
								charlcat(outpars,*i,MAXMODES);
							}
						}
					}
					else
					{
						// can only remove a mode they already have
						log(DEBUG,"umode %c being removed",*i);
						if ((ServerInstance->ModeGrok->AllowedUmode(*i,user->modes,false,true)) && (strchr(dmodes,*i)))
						{
							log(DEBUG,"umode %c is an allowed umode",*i);
							if ((ServerInstance->ModeGrok->ProcessModuleUmode(*i, user, dest, direction)) || (*i == 'i') || (*i == 's') || (*i == 'w') || (*i == 'o'))
							{
								charlcat(outpars,*i,MAXMODES);
								charremove(dmodes,*i);
							}
						}
					}
				}
			}
		}
                if (*outpars)
                {
                        char b[MAXBUF];
                        char* z = b;

                        for (char* i = outpars; *i;)
                        {
                                *z++ = *i++;
                                if (((*i == '-') || (*i == '+')) && ((*(i+1) == '-') || (*(i+1) == '+')))
                                {
                                        // someones playing silly buggers and trying
                                        // to put a +- or -+ into the line...
                                        i++;
                                }
                                if (!*(i+1))
                                {
                                        // Someone's trying to make the last character in
                                        // the line be a + or - symbol.
                                        if ((*i == '-') || (*i == '+'))
                                        {
                                                i++;
                                        }
                                }
                        }
                        *z = 0;

                        if ((*b) && (!IS_SINGLE(b,'+')) && (!IS_SINGLE(b,'-')))
                        {
                                WriteTo(user, dest, "MODE %s :%s", dest->nick, b);
                                FOREACH_MOD(I_OnMode,OnMode(user, dest, TYPE_USER, b));
                        }

                        log(DEBUG,"Stripped mode line");
                        log(DEBUG,"Line dest is now %s",dmodes);
                        strlcpy(dest->modes,dmodes,MAXMODES-1);
                                         
                }

		return;
	}
	
	Ptr = FindChan(parameters[0]);
	if (Ptr)
	{
		ServerInstance->ModeGrok->ProcessModes(parameters,user,Ptr,STATUS_OP,pcnt,true,false,false);
	}
	else
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
	}
}
