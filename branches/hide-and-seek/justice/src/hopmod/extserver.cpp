/*
    This file is included from and is a part of "fpsgame/server.cpp".
*/
#ifdef INCLUDE_EXTSERVER_CPP

int sv_text_hit_length = 0;
int sv_sayteam_hit_length = 0;
int sv_mapvote_hit_length = 0;
int sv_switchname_hit_length = 0;
int sv_switchteam_hit_length = 0;
int sv_kick_hit_length = 0;
int sv_remip_hit_length = 0;
int sv_newmap_hit_length = 0;
int sv_spec_hit_length = 0;

struct restore_state_header
{
    int gamemode;
    string mapname;
    
    int gamemillis;
    int gamelimit;
    
    int numscores;
    int numteamscores;
};

struct restore_teamscore
{
    string name;
    int score;
};

void crash_handler(int signal)
{
    unlink_script_pipe();
    cleanup_info_files();
    
    int fd = open("log/restore", O_CREAT | O_WRONLY | O_APPEND, S_IRWXU);
    
    if(fd != -1)
    {
        restore_state_header header;
        
        header.gamemode = gamemode;
        copystring(header.mapname, smapname);
        
        header.gamemillis = gamemillis;
        header.gamelimit = gamelimit;
        
        header.numscores = scores.length() + clients.length();
        
        bool write_teamscores = false;
        header.numteamscores = 0;
        
        //the last four game modes have fixed team names and have simple team state
        if(gamemode+3 >= NUMGAMEMODES - 4 && smode)
        {
            header.numteamscores = 2;
            write_teamscores = true;
        }
        
        if(write(fd, &header, sizeof(header)) == -1)
        {
            close(fd);
            unlink("log/restore");
            return;
        }
        
        if(write(fd, scores.buf, scores.length() * sizeof(savedscore)) == -1)
        {
            close(fd);
            unlink("log/restore");
            return;
        }
        
        
        int client_count_check = 0;
        
        loopv(clients)
        {
            savedscore score;
            
            copystring(score.name, clients[i]->name);
            score.ip = getclientip(clients[i]->clientnum);
            
            score.save(clients[i]->state);
            
            if(write(fd, &score, sizeof(score)) == -1)
            {
                close(fd);
                unlink("log/restore");
                return;
            }
            
            if(++client_count_check > 128) return; // data structure is corrupted
        }
        
        if(write_teamscores)
        {
            restore_teamscore team;
            
            copystring(team.name, "evil");
            team.score = smode->getteamscore("evil");
            
            if(write(fd, &team, sizeof(team)) == -1)
            {
                close(fd);
                unlink("log/restore");
                return;
            }
            
            copystring(team.name, "good");
            team.score = smode->getteamscore("good");
            
            if(write(fd, &team, sizeof(team)) == -1)
            {
                close(fd);
                unlink("log/restore");
                return;
            }
        }
        
        close(fd);
    }
    
    if(totalmillis > 10000) //attempt to restart
    {
        int maxfd = getdtablesize();
        for(int i = 3; i < maxfd; i++) close(i);
        
        if(fork() == 0)
        {
            setpgid(0,0);
            umask(0);
            
            sigset_t unblock_signal;
            sigemptyset(&unblock_signal);
            sigaddset(&unblock_signal, signal);
            sigprocmask(SIG_UNBLOCK, &unblock_signal, NULL);
            
            execv(prog_argv[0], prog_argv);
            exit(1);
        }
    }
}

void restore_server(const char * filename)
{
    int fd = open(filename, O_RDONLY, 0);
    if(fd == -1) return;
    
    struct stat file_info;
    if(stat(filename, &file_info) == 0 && file_info.st_size > 20000000)
    {
        std::cerr<<"Server state recovery failed! The restore file is abnormally large which indicates that the data was corrupted."<<std::endl;
        unlink(filename);
        return;
    }
    
    restore_state_header header;
    
    int readlen = read(fd, &header, sizeof(header));
    
    if(readlen == -1)
    {
        close(fd);
        return;
    }
    
    int mins = (header.gamelimit - header.gamemillis)/60000;
    changemap(header.mapname, header.gamemode, mins);
    
    gamemillis = header.gamemillis;
    gamelimit = header.gamelimit;
    
    for(int i = 0; i < header.numscores; i++)
    {
        savedscore playerscore;
        readlen = read(fd, &playerscore, sizeof(savedscore));
        if(readlen == -1)
        {
            close(fd);
            return;
        }
        scores.add(playerscore);
    }
    
    for(int i = 0; i< header.numteamscores && smode; i++)
    {
        restore_teamscore team;
        readlen = read(fd, &team, sizeof(team));
        if(readlen == -1)
        {
            close(fd);
            return;
        }
        smode->setteamscore(team.name, team.score);
    }
    
    close(fd);
    unlink(filename);
}

struct disconnect_info
{
    int cn;
    int code;
    std::string reason;
};

static int execute_disconnect(void * info_vptr)
{
    disconnect_info * info = reinterpret_cast<disconnect_info *>(info_vptr);
    clientinfo * ci = get_ci(info->cn);
    ci->disconnect_reason = info->reason;
    disconnect_client(info->cn, info->code);
    return 0;
}

void disconnect(int cn, int code, const std::string & reason)
{
    disconnect_info * info = new disconnect_info;
    info->cn = cn;
    info->code = code;
    info->reason = reason;
    
    sched_callback(&execute_disconnect, info);
}

void changetime(int remaining)
{
    gamelimit = gamemillis + remaining;
    if(remaining > 0) sendf(-1, 1, "ri2", N_TIMEUP, remaining / 1000);
    last_timeupdate = gamemillis;
    signal_timeupdate(get_minutes_left());
}

int get_minutes_left()
{
    return (gamemillis>=gamelimit ? 0 : (gamelimit - gamemillis + 60000 - 1)/60000);
}

void set_minutes_left(int mins)
{
    changetime(mins * 1000 * 60);
}

void player_msg(int cn,const char * text)
{
    get_ci(cn)->sendprivtext(text);
}

int player_id(int cn)
{
    clientinfo * ci = getinfo(cn);
    return (ci ? ci->playerid : -1);
}

int player_sessionid(int cn)
{
    clientinfo * ci = getinfo(cn);
    return (ci ? ci->sessionid : -1);
}

const char * player_name(int cn){return get_ci(cn)->name;}

void player_rename(int cn, const char * newname)
{
    char safenewname[MAXNAMELEN + 1];
    filtertext(safenewname, newname, false, MAXNAMELEN);
    if(!safenewname[0]) copystring(safenewname, "unnamed");
    
    clientinfo * ci = get_ci(cn);
    putuint(ci->messages, N_SWITCHNAME);
    sendstring(safenewname, ci->messages);
    
    vector<uchar> switchname_message;
    putuint(switchname_message, N_SWITCHNAME);
    sendstring(safenewname, switchname_message);
    
    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    putuint(p, N_CLIENT);
    putint(p, ci->clientnum);
    putint(p, switchname_message.length());
    p.put(switchname_message.getbuf(), switchname_message.length());
    sendpacket(ci->clientnum, 1, p.finalize(), -1);
    
    char oldname[MAXNAMELEN+1];
    copystring(oldname, ci->name, MAXNAMELEN+1);
    
    copystring(ci->name, safenewname, MAXNAMELEN+1);
    
    signal_rename(ci->clientnum, oldname, ci->name);
}

std::string player_displayname(int cn)
{
    clientinfo * ci = get_ci(cn);
    
    std::string output;
    output.reserve(MAXNAMELEN + 5);
    
    output = ci->name;
    
    bool is_bot = ci->state.aitype != AI_NONE;
    bool duplicate = false;
    
    if(!is_bot)
    {
        loopv(clients)
        {
            if(clients[i]->clientnum == cn) continue;
            if(!strcmp(clients[i]->name, ci->name))
            {
                duplicate = true;
                break;
            }
        }
    }
    
    if(is_bot || duplicate)
    {
        char open = (is_bot ? '[' : '(');
        char close = (is_bot ? ']' : ')');
        
        output += "\fs" MAGENTA " ";
        output += open;
        output += boost::lexical_cast<std::string>(cn);
        output += close;
        output += "\fr";
    }
    
    return output;
}

const char * player_team(int cn)
{
    if(!m_teammode) return "";
    return get_ci(cn)->team;
}

const char * player_ip(int cn)
{
    return get_ci(cn)->hostname();
}

unsigned long player_iplong(int cn)
{
    return ntohl(getclientip(get_ci(cn)->clientnum));
}

int player_status_code(int cn)
{
    return get_ci(cn)->state.state;
}

int player_ping(int cn)
{
    return get_ci(cn)->ping;
}

int player_ping_update(int cn)
{
    return get_ci(cn)->lastpingupdate;
}

int player_lag(int cn)
{
    return get_ci(cn)->lag;
}

int player_frags(int cn)
{
    clientinfo * ci = get_ci(cn);
    return ci->state.frags + ci->state.suicides + ci->state.teamkills;
}

int player_score(int cn)
{
    return get_ci(cn)->state.frags;
}

int player_deaths(int cn)
{
    return get_ci(cn)->state.deaths;
}

int player_suicides(int cn)
{
    return get_ci(cn)->state.suicides;
}

int player_teamkills(int cn)
{
    return get_ci(cn)->state.teamkills;
}

int player_damage(int cn)
{
    return get_ci(cn)->state.damage;
}

int player_damagewasted(int cn)
{
    clientinfo * ci = get_ci(cn);
    return ci->state.explosivedamage + ci->state.shotdamage - ci->state.damage;
}

int player_maxhealth(int cn)
{
    return get_ci(cn)->state.maxhealth;
}

int player_health(int cn)
{
    return get_ci(cn)->state.health;
}

int player_armour(int cn)
{
    return get_ci(cn)->state.armour;
}

int player_armour_type(int cn)
{
    return get_ci(cn)->state.armourtype;  
}

int player_gun(int cn)
{
    return get_ci(cn)->state.gunselect;
}

int player_hits(int cn)
{
    return get_ci(cn)->state.hits;
}

int player_misses(int cn)
{
    return get_ci(cn)->state.misses;
}

int player_shots(int cn)
{
    return get_ci(cn)->state.shots;
}

int player_accuracy(int cn)
{
    clientinfo * ci = get_ci(cn);
    int shots = ci->state.shots;
    int hits = shots - ci->state.misses;
    return static_cast<int>(roundf(static_cast<float>(hits)/std::max(shots,1)*100));
}

int player_privilege_code(int cn)
{
    return get_ci(cn)->privilege;
}

const char * player_privilege(int cn)
{
    switch(get_ci(cn)->privilege)
    {
        case PRIV_MASTER: return "master";
        case PRIV_ADMIN: return "admin";
        default: return "none";
    }
}

const char * player_status(int cn)
{
    switch(get_ci(cn)->state.state)
    {
        case CS_ALIVE: return "alive";
        case CS_DEAD: return "dead"; 
        case CS_SPAWNING: return "spawning"; 
        case CS_LAGGED: return "lagged"; 
        case CS_SPECTATOR: return "spectator";
        case CS_EDITING: return "editing"; 
        default: return "unknown";
    }
}

int player_connection_time(int cn)
{
    return (totalmillis - get_ci(cn)->connectmillis)/1000;
}

int player_timeplayed(int cn)
{
    clientinfo * ci = get_ci(cn);
    return (ci->state.timeplayed + (ci->state.state != CS_SPECTATOR ? (lastmillis - ci->state.lasttimeplayed) : 0))/1000;
}

int player_win(int cn)
{
    clientinfo * ci = get_ci(cn);
    
    if(!m_teammode)
    {
        loopv(clients)
        {
            if(clients[i] == ci || clients[i]->state.state == CS_SPECTATOR) continue;
            
            bool more_frags = clients[i]->state.frags > ci->state.frags;
            bool eq_frags = clients[i]->state.frags == ci->state.frags;
            
            bool less_deaths = clients[i]->state.deaths < ci->state.deaths;
            bool eq_deaths = clients[i]->state.deaths == ci->state.deaths;
            
            int p1_acc = player_accuracy(clients[i]->clientnum);
            int p2_acc = player_accuracy(cn);
            
            bool better_acc = p1_acc > p2_acc;
            bool eq_acc = p1_acc == p2_acc;
            
            bool lower_ping = clients[i]->ping < ci->ping;
            
            if( more_frags || (eq_frags && less_deaths) ||
                (eq_frags && eq_deaths && better_acc) || 
                (eq_frags && eq_deaths && eq_acc && lower_ping)) return false;            
        }
        return true;
    }
    else return team_win(ci->team);
}

void player_slay(int cn)
{
    clientinfo * ci = get_ci(cn);
    if(ci->state.state != CS_ALIVE) return;
    ci->state.state = CS_DEAD;
    sendf(-1, 1, "ri2", N_FORCEDEATH, cn);
}

bool player_changeteam(int cn,const char * newteam)
{
    clientinfo * ci = get_ci(cn);
    
    if(!m_teammode || (smode && !smode->canchangeteam(ci, ci->team, newteam)) ||
        signal_chteamrequest(cn, ci->team, newteam) == -1) return false;
    
    if(smode || ci->state.state==CS_ALIVE) suicide(ci);
    signal_reteam(ci->clientnum, ci->team, newteam);
    
    copystring(ci->team, newteam, MAXTEAMLEN+1);
    sendf(-1, 1, "riisi", N_SETTEAM, cn, newteam, 3);
    
    if(ci->state.aitype == AI_NONE) aiman::dorefresh = true;
    
    return true;
}

int player_rank(int cn){return get_ci(cn)->rank;}
bool player_isbot(int cn){return get_ci(cn)->state.aitype != AI_NONE;}

int player_mapcrc(int cn){return get_ci(cn)->mapcrc;}

void changemap(const char * map,const char * mode = "",int mins = -1)
{
    int gmode = (mode[0] ? modecode(mode) : gamemode);
    if(!m_mp(gmode)) gmode = gamemode;
    sendf(-1, 1, "risii", N_MAPCHANGE, map, gmode, 1);
    changemap(map,gmode,mins);
}

int getplayercount()
{
    return numclients(-1, false, true);
}

int getbotcount()
{
    return numclients(-1, true, false) - numclients();
}

int getspeccount()
{
    return getplayercount() - numclients();
}

void team_msg(const char * team,const char * msg)
{
    if(!m_teammode) return;
    loopv(clients)
    {
        clientinfo *t = clients[i];
        if(t->state.state==CS_SPECTATOR || t->state.aitype != AI_NONE || strcmp(t->team, team)) continue;
        t->sendprivtext(msg);
    }
}

void player_spec(int cn)
{
    clientinfo * ci = get_ci(cn);
    ci->allow_self_unspec = false;
    setspectator(ci, true);
}

void player_unspec(int cn)
{
    setspectator(get_ci(cn), false);
}

int player_bots(int cn)
{
    clientinfo * ci = get_ci(cn);
    return ci->bots.length();
}

int player_pos(lua_State * L)
{
    int cn = luaL_checkint(L,1);
    vec pos = get_ci(cn)->state.o;
    lua_pushnumber(L, pos.x);
    lua_pushnumber(L, pos.y);
    lua_pushnumber(L, pos.z);
    return 3;
}

std::vector<float> player_pos(int cn)
{
    vec pos = get_ci(cn)->state.o;
    std::vector<float> result(3);
    result[0] = pos.x;
    result[1] = pos.y;
    result[2] = pos.z;
    return result;
}

void cleanup_masterstate(clientinfo * master)
{
    int cn = master->clientnum;
    
    if(cn == mastermode_owner)
    {
        mastermode = MM_OPEN;
        mastermode_owner = -1;
        mastermode_mtime = totalmillis;
        allowedips.setsize(0);
    }
    
    if(gamepaused && cn == pausegame_owner) pausegame(false);
    
    if(master->state.state==CS_SPECTATOR) aiman::removeai(master);
}

void unsetmaster()
{
    if(currentmaster != -1)
    {
        clientinfo * master = getinfo(currentmaster);
        
        defformatstring(msg)("The server has revoked your %s privilege.", privname(master->privilege));
        master->sendprivtext(msg);
        
        int old_priv = master->privilege;
        master->privilege = 0;
        int oldmaster = currentmaster;
        currentmaster = -1;
        masterupdate = true;
        
        cleanup_masterstate(master);
        
        signal_privilege(oldmaster, old_priv, PRIV_NONE);
    }
}

void unset_player_privilege(int cn)
{
    if(currentmaster == cn)
    {
        unsetmaster();
        return;
    }
    
    clientinfo * ci = get_ci(cn);
    if(ci->privilege == PRIV_NONE) return;
    
    int old_priv = ci->privilege;
    ci->privilege = PRIV_NONE;
    sendf(ci->clientnum, 1, "ri3", N_CURRENTMASTER, ci->clientnum, PRIV_NONE);
    
    cleanup_masterstate(ci);
    
    signal_privilege(cn, old_priv, PRIV_NONE);
}

void set_player_privilege(int cn, int priv_code, bool public_priv = false)
{
    clientinfo * player = get_ci(cn);
    
    if(player->privilege == priv_code) return;
    if(priv_code == PRIV_NONE) unset_player_privilege(cn);
    
    if(cn == currentmaster && !public_priv)
    {
        currentmaster = -1;
        masterupdate = true;
    }
    
    int old_priv = player->privilege;
    player->privilege = priv_code;
    
    if(public_priv)
    {
        currentmaster = cn;
        masterupdate = true;
    }
    else
    {
        sendf(player->clientnum, 1, "ri3", N_CURRENTMASTER, player->clientnum, player->privilege);
    }
    
    const char * change = (old_priv < player->privilege ? "raised" : "lowered");
    defformatstring(msg)("The server has %s your privilege to %s.", change, privname(priv_code));
    player->sendprivtext(msg);
    
    signal_privilege(cn, old_priv, player->privilege);
}

bool set_player_master(int cn)
{
    set_player_privilege(cn, PRIV_MASTER, true);
    return true;
}

void set_player_admin(int cn)
{
    set_player_privilege(cn, PRIV_ADMIN, true);
}

void set_player_private_admin(int cn)
{
   set_player_privilege(cn, PRIV_ADMIN, false);
}

void set_player_private_master(int cn)
{
    set_player_privilege(cn, PRIV_MASTER, false);
}

void player_freeze(int cn)
{
    clientinfo * ci = get_ci(cn);
    sendf(ci->clientnum, 1, "rii", N_PAUSEGAME, 1);
}

void player_unfreeze(int cn)
{
    clientinfo * ci = get_ci(cn);
    sendf(ci->clientnum, 1, "rii", N_PAUSEGAME, 0);
}

static void execute_addbot(int skill)
{
    clientinfo * owner = aiman::addai(skill, -1);
    if(!owner) return;
    signal_addbot(-1, skill, owner->clientnum);
    return;
}

void addbot(int skill)
{
    get_main_io_service().post(boost::bind(&execute_addbot, skill));
}

static void execute_deletebot(int cn)
{
    clientinfo * ci = getinfo(cn);
    if(!ci) return;
    aiman::deleteai(ci);
    return;
}

void deletebot(int cn)
{
    if(get_ci(cn)->state.aitype == AI_NONE) 
        throw fungu::script::error(fungu::script::OPERATION_ERROR, boost::make_tuple(std::string("not a bot player")));
    get_main_io_service().post(boost::bind(&execute_deletebot, cn));
}

void enable_master_auth(bool enable)
{
    mastermask = (enable ? mastermask & ~MM_AUTOAPPROVE : mastermask | MM_AUTOAPPROVE);
}

bool using_master_auth()
{
    return !(mastermask & MM_AUTOAPPROVE);
}

void update_mastermask()
{
    bool autoapprove = mastermask & MM_AUTOAPPROVE;
    mastermask &= ~(1<<MM_VETO) & ~(1<<MM_LOCKED) & ~(1<<MM_PRIVATE) & ~MM_AUTOAPPROVE;
    mastermask |= (allow_mm_veto << MM_VETO);
    mastermask |= (allow_mm_locked << MM_LOCKED);
    mastermask |= (allow_mm_private << MM_PRIVATE);
    if(autoapprove) mastermask |= MM_AUTOAPPROVE;
}

const char * gamemodename()
{
    return modename(gamemode,"unknown");
}

namespace cubescript{
std::vector<int> make_client_list(bool (* clienttype)(clientinfo *))
{
    std::vector<int> result;
    result.reserve(clients.length());
    loopv(clients) if(clienttype(clients[i])) result.push_back(clients[i]->clientnum);
    return result;
}
} //namespace cubescript

namespace lua{
int make_client_list(lua_State * L,bool (* clienttype)(clientinfo *))
{
    lua_newtable(L);
    int count = 0;
    
    loopv(clients) if(clienttype(clients[i]))
    {
        lua_pushinteger(L,++count);
        lua_pushinteger(L,clients[i]->clientnum);
        lua_settable(L, -3);
    }
    
    return 1;
}
}//namespace lua

bool is_player(clientinfo * ci){return ci->state.state != CS_SPECTATOR && ci->state.aitype == AI_NONE;}
bool is_spectator(clientinfo * ci){return ci->state.state == CS_SPECTATOR; /* bots can't be spectators*/}
bool is_bot(clientinfo * ci){return ci->state.aitype != AI_NONE;}
bool is_any(clientinfo *){return true;}

std::vector<int> cs_player_list(){return cubescript::make_client_list(&is_player);}
int lua_player_list(lua_State * L){return lua::make_client_list(L, &is_player);}

std::vector<int> cs_spec_list(){return cubescript::make_client_list(&is_spectator);}
int lua_spec_list(lua_State * L){return lua::make_client_list(L, &is_spectator);}

std::vector<int> cs_bot_list(){return cubescript::make_client_list(&is_bot);}
int lua_bot_list(lua_State *L){return lua::make_client_list(L, &is_bot);}

std::vector<int> cs_client_list(){return cubescript::make_client_list(&is_any);}
int lua_client_list(lua_State * L){return lua::make_client_list(L, &is_any);}

std::vector<std::string> get_teams()
{
    std::set<std::string> teams;
    loopv(clients) teams.insert(clients[i]->team);
    std::vector<std::string> result;
    std::copy(teams.begin(),teams.end(),std::back_inserter(result));
    return result;
}

int lua_team_list(lua_State * L)
{
    lua_newtable(L);
    std::vector<std::string> teams = get_teams();
    int count = 0;
    for(std::vector<std::string>::iterator it = teams.begin(); it != teams.end(); ++it)
    {
        lua_pushinteger(L, ++count);
        lua_pushstring(L, it->c_str());
        lua_settable(L, -3);
    }
    return 1;
}

int get_team_score(const char * team)
{
    int score = 0;
    if(smode) return smode->getteamscore(team);
    else loopv(clients)
        if(clients[i]->state.state != CS_SPECTATOR && !strcmp(clients[i]->team,team))
            score += clients[i]->state.frags;
    return score;
}

std::vector<int> get_team_players(const char * team)
{
    std::vector<int> result;
    loopv(clients)
        if(clients[i]->state.state != CS_SPECTATOR && clients[i]->state.aitype == AI_NONE && !strcmp(clients[i]->team,team))
            result.push_back(clients[i]->clientnum);
    return result;
}

int lua_team_players(lua_State * L)
{
    std::vector<int> players = get_team_players(luaL_checkstring(L,1));
    lua_newtable(L);
    int count = 0;
    for(std::vector<int>::iterator it = players.begin(); it != players.end(); ++it)
    {
        lua_pushinteger(L, ++count);
        lua_pushinteger(L, *it);
        lua_settable(L, -3);
    }
    
    return 1;
}

int team_win(const char * team)
{
    std::vector<std::string> teams = get_teams();
    int score = get_team_score(team);
    for(std::vector<std::string>::iterator it = teams.begin(); it != teams.end(); ++it)
    {
        if(*it == team) continue;
        if(get_team_score(it->c_str()) > score) return false;
    }
    return true;
}

int team_draw(const char * team)
{
    std::vector<std::string> teams = get_teams();
    int score = get_team_score(team);
    for(std::vector<std::string>::iterator it = teams.begin(); it != teams.end(); ++it)
    {
        if(*it == team) continue;
        if(get_team_score(it->c_str()) != score) return false;
    }
    return true;
}

int team_size(const char * team)
{
    int count = 0;
    loopv(clients) if(strcmp(clients[i]->team, team)==0) count++;
    return count;
}

int recorddemo(const char * filename)
{
    if(demorecord) return demo_id;
    else return setupdemorecord(false, filename);
}

int lua_gamemodeinfo(lua_State * L)
{
    int gamemode_argument = gamemode;
    
    if(lua_gettop(L) > 0 && lua_type(L, 1) == LUA_TSTRING)
    {
        gamemode_argument = modecode(lua_tostring(L, 1));
        if(gamemode_argument == -1) return 0;
    }
    
    lua_newtable(L);
    
    lua_pushboolean(L, m_check(gamemode_argument, M_NOITEMS));
    lua_setfield(L, -2, "noitems");
    
    lua_pushboolean(L, m_check(gamemode_argument,  M_NOAMMO|M_NOITEMS));
    lua_setfield(L, -2, "noammo");
    
    lua_pushboolean(L, m_check(gamemode_argument, M_INSTA));
    lua_setfield(L, -2, "insta");
    
    lua_pushboolean(L, m_check(gamemode_argument, M_TACTICS));
    lua_setfield(L, -2, "tactics");
    
    lua_pushboolean(L, m_check(gamemode_argument, M_EFFICIENCY));
    lua_setfield(L, -2, "efficiency");
    
    lua_pushboolean(L, m_check(gamemode_argument,  M_CAPTURE));
    lua_setfield(L, -2, "capture");
    
    lua_pushboolean(L, m_check(gamemode_argument, M_CAPTURE | M_REGEN));
    lua_setfield(L, -2, "regencapture");
    
    lua_pushboolean(L, m_check(gamemode_argument, M_CTF));
    lua_setfield(L, -2, "ctf");
    
    lua_pushboolean(L, m_checkall(gamemode_argument, M_CTF | M_PROTECT));
    lua_setfield(L, -2, "protect");
    
    lua_pushboolean(L, m_checkall(gamemode_argument, M_CTF | M_HOLD));
    lua_setfield(L, -2, "hold");
    
    lua_pushboolean(L, m_check(gamemode_argument, M_TEAM));
    lua_setfield(L, -2, "teams");
    
    lua_pushboolean(L, m_check(gamemode_argument, M_OVERTIME));
    lua_setfield(L, -2, "overtime");
    
    lua_pushboolean(L, m_checknot(gamemode_argument, M_DEMO|M_EDIT|M_LOCAL));
    lua_setfield(L, -2, "timed");
    
    return 1;
}

bool selectnextgame()
{
    signal_setnextgame();
    if(next_gamemode[0] && next_mapname[0])
    {
        int next_gamemode_code = modecode(next_gamemode);
        if(m_mp(next_gamemode_code))
        {
            mapreload = false;
            sendf(-1, 1, "risii", N_MAPCHANGE, next_mapname, next_gamemode_code, 1);
            changemap(next_mapname, next_gamemode_code, next_gametime);
            next_gamemode[0] = '\0';
            next_mapname[0] = '\0';
            next_gametime = -1;
        }
        else
        {
            std::cerr<<next_gamemode<<" game mode is unrecognised."<<std::endl;
            sendf(-1, 1, "ri", N_MAPRELOAD);
        }
        return true;
    }else return false;
}

void sendauthchallenge(int cn, int id, const char * domain, const char * challenge)
{
    clientinfo * ci = get_ci(cn);
    sendf(ci->clientnum, 1, "risis", N_AUTHCHAL, domain, id, challenge);
}

void send_auth_request(int cn, const char * domain)
{
    clientinfo * ci = get_ci(cn);
    sendf(ci->clientnum, 1, "ris", N_REQAUTH, domain);
}

static bool compare_player_score(const std::pair<int,int> & x, const std::pair<int,int> & y)
{
    return x.first > y.first;
}

static void calc_player_ranks(const char * team)
{
    if(m_edit) return;
    
    if(m_teammode && !team)
    {
        std::vector<std::string> teams = get_teams();
        for(std::vector<std::string>::const_iterator it = teams.begin();
             it != teams.end(); it++) calc_player_ranks(it->c_str());
        return;
    }
    
    std::vector<std::pair<int,int> > players;
    players.reserve(clients.length());
    
    loopv(clients) 
        if(clients[i]->state.state != CS_SPECTATOR && (!team || !strcmp(clients[i]->team,team)))
            players.push_back(std::pair<int,int>(clients[i]->state.frags, i));
    
    std::sort(players.begin(), players.end(), compare_player_score);
    
    int rank = 0;
    for(std::vector<std::pair<int,int> >::const_iterator it = players.begin();
        it != players.end(); ++it)
    {
        rank++;
        if(it != players.begin() && it->first == (it-1)->first) rank--;
        clients[it->second]->rank = rank;
    }
}

void calc_player_ranks()
{
    return calc_player_ranks(NULL);
}

void script_set_mastermode(int value)
{
    mastermode = value;
    mastermode_owner = -1;
    mastermode_mtime = totalmillis;
    allowedips.setsize(0);
    if(mastermode >= MM_PRIVATE)
    {
        loopv(clients) allowedips.add(getclientip(clients[i]->clientnum));
    }
}

void add_allowed_ip(const char * hostname)
{
    ENetAddress addr;
    if(enet_address_set_host(&addr, hostname) != 0)
        throw fungu::script::error(fungu::script::OPERATION_ERROR, boost::make_tuple(std::string("invalid hostname given")));
    allowedips.add(addr.host);
}

void suicide(int cn)
{
    suicide(get_ci(cn));
}

bool compare_admin_password(const char * x)
{
    return !strcmp(x, adminpass);
}

void enable_setmaster_autoapprove(bool enable)
{
    mastermask = (!enable ? mastermask & ~MM_AUTOAPPROVE : mastermask | MM_AUTOAPPROVE);
}

bool get_setmaster_autoapprove()
{
    return mastermask & MM_AUTOAPPROVE;
}

bool send_item(int type, int recipient) 
{
    int ent_index = sents_type_index[type];
    if(interm > 0 || !sents.inrange(ent_index)) return false;
    clientinfo *ci = getinfo(recipient);
    if(!ci || (!ci->local && !ci->state.canpickup(sents[ent_index].type))) return false;
    sendf(-1, 1, "ri3", N_ITEMACC, ent_index, recipient);
    ci->state.pickup(sents[ent_index].type);
    return true;
}

class player_token
{   
public:
    player_token(clientinfo * ci)
    :m_cn(ci->clientnum), m_session_id(ci->sessionid)
    {
        
    }
    
    clientinfo * get_clientinfo()const
    {
        clientinfo * ci = getinfo(m_cn);
        if(!ci) return NULL;
        if(!ci || ci->sessionid != m_session_id) return NULL;
        return ci;
    }
private:
    int m_cn;
    int m_session_id;       
};

int deferred_respawn_request(void * arg)
{
    player_token * player = reinterpret_cast<player_token *>(arg);
    clientinfo * ci = player->get_clientinfo();
    delete player;
    if(!ci) return 0;
    try_respawn(ci, ci);
    return 0;
}

void try_respawn(clientinfo * ci, clientinfo * cq)
{
    if(!ci || !cq || cq->state.state!=CS_DEAD || cq->state.lastspawn>=0 || (smode && !smode->canspawn(cq))) return;
    if(!ci->clientmap[0] && !ci->mapcrc) 
    {
        ci->mapcrc = -1;
        checkmaps();
    }
    if(cq->state.lastdeath)
    {
        int delay = signal_respawnrequest(cq->clientnum, cq->state.lastdeath);
        
        if(delay > 0)
        {
            sched_callback(deferred_respawn_request, new player_token(cq), delay);
            return;
        }
        
        flushevents(cq, cq->state.lastdeath + DEATHMILLIS);
        cq->state.respawn();
    }
    cleartimedevents(cq);
    sendspawn(cq);
}

void no_spawn(int cn, int canspawn)// MOD
{
    clientinfo *ci = getinfo(cn);
    if (!ci) return;
    ci->no_spawn = canspawn;
}

int is_valid_cn(int cn)// MOD
{
    clientinfo *ci = getinfo(cn);
    if (!ci) return 0;
    return 1;
}

void spawn_player(int cn)// MOD
{
     clientinfo *ci = getinfo(cn);
     if (!ci) return;
     sendspawn(ci, true);
}

void editvar(int cn, const char *var, int value) 
{
    clientinfo *ci = getinfo(cn);
    if (!ci) return;
    sendf(cn, 1, "ri5si", N_CLIENT, cn, 100/*should be safe*/, N_EDITVAR, ID_VAR, var, value);
}

#endif