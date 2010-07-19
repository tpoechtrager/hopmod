-- (c) 2010 by |DM|Thomas & |noVI:pure_ascii

hide_and_seek = false
local hah_active_player = -1
local hah_seek_team = ""
local hah_next_player = -1
local hah_actor_frags = 0
local MAP_LOADED_ALL = false
local PLAYER_ACTIVE = { }
local hah_active_player_can_spawn = false
local GAME_ACTIVE = false
local caught_players = { }
local server_change_team_request = { } 
local can_vote = true
local has_time = 12 -- 12 minutes
local last_mapvote = server.uptime
local player_stats = { }
local player_waitlist = { }
local last_warn = { }

local function reset_on_mapchange()
    hah_actor_frags = 0
    MAP_LOADED_ALL = false
    PLAYER_ACTIVE = { }
    hah_active_player_can_spawn = false
    GAME_ACTIVE = false
    PLAYER_SWITCHED = false
    caught_players = { }
    server_change_team_request = { } 
    can_vote = false
    player_stats = { }
    last_warn = { }
end

local function setteam(cn)
    local jointeam = "hide"
    
    if caught_players[cn] ~= nil or hah_active_player == cn then
        return hah_seek_team
    end
    
    if server.player_team(cn) ~= jointeam then    
        server_change_team_request[cn] = true 
        server.changeteam(cn, jointeam)
    end
    
    return jointeam
end

local function relocate_players()
    if server.is_valid_cn(hah_active_player) then
        hah_seek_team = server.player_team(hah_active_player)
    end
    for i, cn in ipairs(server.players()) do
        setteam(cn)
    end 
end

local function relocate_vars(set_seek)
    if server.is_valid_cn(hah_next_player) == 0 then
           	if #server.players() == 0 then
			server.msg("failed to get seek player.")
			return
		end
		while server.is_valid_cn(hah_next_player) == 0 do
			for i, cn_ in ipairs(server.players()) do
				if math.random(0, 1) == 1 then
					hah_next_player = cn_
				end
			end           
		end
				
			  
		server.msg(green() .. "Set random Player as Next Seek Player: " .. server.player_name(hah_next_player))
		
    end
	
	if seet_seek ~= nil then
		if set_seek == true then
			return 
		end
	end
    
    hah_active_player = hah_next_player 
        
    server.no_spawn(hah_active_player, 1)
    server.player_slay(hah_active_player)
    server_change_team_request[hah_active_player] = true 
    server.changeteam(hah_active_player, "seek")
    
    for i, cn_ in ipairs(server.players()) do
        if server.player_status(cn_) == "dead" and cn_ ~= hah_active_player then
            server.no_spawn(cn_, 0)
            server.player_slay(cn_)
            server.spawn_player(cn_)
        end
    end   
    
    relocate_players()
end

local function check_game(check_only)

	local hide = 0
	local seek = 0

	for i, cn_ in ipairs(server.players()) do
            if server.player_team(cn_) == "hide" then
                hide = hide + 1  
            end
            if server.player_team(cn_) == "seek" then
                seek = seek + 1  
            end
        end

	if hide == 0 or seek == 0 then
		if not check_only then
			server.msg(green() .. "No players left, game ended.")
		end
		server.changetime(0)
		return false
	end

	return true
end

server.event_handler("spectator", function(cn, val)
	if hide_and_seek == false then return end
	if val == 0 then
		setteam(cn)
	else
		check_game()
	end
end)

server.event_handler("connect", function(cn)
	if hide_and_seek == false then return end
	server.spec(cn)

	local str = "\f3PLEASE READ: "..orange().."This server is currently running the Hide and Seek mode, make sure you have the current map and understand how the mode works, before you are asking to be unspecced! thank you! [NOTE: THIS IS _NOT_ A OFFICIAL GAMEMODE]"

	server.sleep(3000, function() server.player_msg(cn, str) end)
	server.sleep(5000, function() server.player_msg(cn, str) end)
	server.sleep(7000, function() server.player_msg(cn, str) end)

end)

server.event_handler("damage", function(client, actor)
    if hide_and_seek == false then return end
    if client == actor then
        return -1
    end
    if server.player_team(actor) == server.player_team(client) and actor ~= client then
	local set = false
	if last_team_fire_warn[actor] == nil then 
		last_warn[actor] = server.uptime
		set = true
	end
	if (server.uptime - last_warn[actor]) > 10000 or set then
		server.player_msg(actor, red() .. "You can't frag your teammates in this mode!")
		last_warn[actor] = server.uptime
	end
        return -1
    end
    if server.player_team(actor) ~= hah_seek_team and actor ~= hah_active_player and client ~= actor then
	local set = false
	if last_team_fire_warn[actor] == nil then 
		last_warn[actor] = server.uptime
		set = true
	end
	if (server.uptime - last_warn[actor]) > 10000 or set then
		server.player_msg(actor, red() .. "You are not allowed to attack the seek Player!!!")
		last_warn[actor] = server.uptime
	end 
        return -1       
    end
end)

server.event_handler("frag", function(client, actor)
    if hide_and_seek == false then return end
    if hah_active_player == actor and client ~= actor then
        hah_actor_frags = hah_actor_frags + 1
        if hah_actor_frags == 1 then
            server.msg(red() .. string.upper("Next seek player set: \f1") .. server.player_name(client))
            hah_next_player = client
        end
    end
    if client ~= actor then
        if hah_seek_team == server.player_team(actor) then
            local count = 0
            for i, cn_ in ipairs(server.players()) do
                if hah_seek_team ~= server.player_team(cn_) then
                    count = count + 1  
                end
            end
            count = count - 1
	    local str = "Players"
	    if count == 1 then
		str = "Player"
	    end
            server.msg(orange() .. server.player_name(actor) .. " got " .. server.player_name(client) .. " - " .. count .. " " .. str .. " left!")
	  
	    player_stats[actor] = (player_stats[actor] or 0) + 1
		
            server_change_team_request[client] = true 
            server.changeteam(client, hah_seek_team)

            caught_players[client] = true
            if count == 0 then
                server.changetime(0)
            end
        end
    end
end)

server.event_handler("suicide", function(cn)
    if hide_and_seek == false then return end
    if server_change_team_request[cn] == true then return end
    if hah_seek_team ~= server.player_team(cn) then
        hah_actor_frags = hah_actor_frags + 1
        if hah_actor_frags == 1 then
            server.msg(blue() .. "Next seek player set: " .. server.player_name(cn))
            hah_next_player = cn
        end
        local count = 0
        for i, cn_ in ipairs(server.clients()) do
            if server.player_status(cn_) ~= "spectator" and hah_seek_team ~= server.player_team(cn_) then
                count = count + 1  
            end
        end
        count = count - 1
        server.msg(green() .. server.player_name(cn) .. " suicided and became a seeker - " .. count .. " Players left!")
        server_change_team_request[cn] = true 
        server.changeteam(cn, hah_seek_team)
        caught_players[cn] = true
        if count == 0 then
            server.changetime(0)
        end
    end
end)

server.event_handler("intermission", function(actor, client)
    if hide_and_seek == false then return end


    table.sort(player_stats, function(a, b) return a > b end)

    local best_msg = blue() .. "Best Seekers:"
    local count = 0

    for k, v in pairs(player_stats) do 
	count = count + 1
	best_msg = best_msg .. " " .. red() .. server.player_name(k) .. "(" .. v .. ")" 
	if count < #player_stats then
		best_msg = best_msg .. blue() .. ","
	end
    end  

    if count > 0 then
	    server.msg(best_msg)
    end

    for i, cn in ipairs(server.clients()) do
	server.no_spawn(cn, 1)
    end

       
    server.intermission = server.gamemillis + 10000
    
    local starttime = round((server.intermission - server.gamemillis))
    
    server.sleep((starttime - 10), function() -- riscy, but should work :P
	if (server.uptime - last_mapvote) > 10000 then -- check if master/admin changed map
       		server.changemap(server.map)
	end
    end)
end)

ALREADY_DONE = false

server.event_handler("mapchange", function()
	if hide_and_seek == false then return end
    	server.msg(green() .. "waiting for clients to load the map...")
	
	for k, v in pairs(player_waitlist) do 
		if player_waitlist[k] ~= nil then -- to be sure
			if server.is_valid_cn(k) then
				server.unspec(k)
				server.sleep(500, function() 
					server.msg(blue() .. "Unspecced: " .. red() .. server.player_name(k))
				end)
			end
			player_waitlist[k] = nil
		end
	end


	for i, cn in ipairs(server.players()) do
		server.player_slay(cn)
		server.no_spawn(cn, 1)
	end

	reset_on_mapchange() 
end)

server.event_handler("spectator", function(cn, join)
    if hide_and_seek == false then return end
    if join == 0 then
        relocate_players()
    else
    end
end)


server.event_handler("reteam", function(cn, old, new)
    if hide_and_seek == false then return end
    if server_change_team_request[cn] == true then
        server_change_team_request[cn] = false
        return
    end
    server_change_team_request[cn] = true 
    server.changeteam(cn, old)
    server.player_msg(cn, red() .. "You can't switch the team!")
end)

server.event_handler("mapvote", function(cn, map, mode)
	if hide_and_seek == false then return end
	if server.player_priv(cn) ~= "master" and server.player_priv(cn) ~= "admin" then
		server.player_msg(cn, red() .. "Only Master/Admin can set a Map!")
		return -1
	else	
		if can_vote == false then
			server.player_msg(cn, red() .. "Please wait until the seek player spawned for a new mapvote!")
			return -1
		else
			--if mode ~= "teamplay" then
			--	server.player_msg(cn, red() .. "Hide and Seek can only be played in Teamplay-Mode!")
			--	return -1
			--end
		end
	end
	last_mapvote = server.uptime
end)



server.event_handler("maploaded", function(cn)
    if hide_and_seek == false then return end

    if GAME_ACTIVE == true then
        return
    end
    
    PLAYER_ACTIVE[cn] = true
    local canstart = true

    for _, cn_ in ipairs(server.players()) do -- wait for clients to load the map ....
	if PLAYER_ACTIVE[cn_] == nil then
	canstart = false 
	end
    end
    
    if canstart then
        GAME_ACTIVE = true
       
        
        server.msg(red() .. "Maploading finished!")

		relocate_vars()

		for i, cn_ in ipairs(server.players()) do -- (players): no spawn + force spawn
			if cn_ ~= hah_active_player then
				server.no_spawn(cn_, 0)
				server.spawn_player(cn_)
			end
		end		

		server.sleep(0, function()
			if server.player_status(hah_active_player) == "spectator" then
				relocate_vars(true)
			end
			
			if check_game(true) then -- check if seeker isnt disconnected ...
				server.msg(green() .. "Go and hide, the seek player will spawn in 10 seconds!")	
			else
				return
			end	
			
			if server.player_status(hah_next_player) == "spectator" then
				 server.msg("seek fail")
				 relocate_vars(true)
				 hah_active_player = hah_next_player
			end
			
			server.sleep(5000, function()
				if check_game(true) then 
					server.msg(blue() .. "5 seconds, run!") 
				end
			end)
			server.sleep(7000, function()
				if check_game(true) then 
					server.msg(red() .. "3 seconds...")
				end 
			end)
			server.sleep(8000, function()
				if check_game(true) then 
					server.msg(red() .. "2 seconds...") 
				end
			end)
			server.sleep(9000, function()
				if check_game(true) then 
					server.msg(red() .. "1 second....") 
				end
				stop_kill_event = true
			end)
			server.sleep(10000, function()
				if check_game(true) then 
					server.no_spawn(hah_active_player, 0) -- allow seek player to spawn
					server.spawn_player(hah_active_player) -- spawn seek player
					server.msg(yellow() .. "Seek Player spawned!")
					can_vote = true
					server.changetime(1000*60*has_time) -- reset time
				end
			end)
		end)
    end
end)

server.event_handler("disconnect", function(cn, reason)
    if not hide_and_seek then return end
    check_game()
    if server.playercount == 0 then
	server.mastermode = 0
	hide_and_seek = false
    end
end)


function server.playercmd_has(cn, enable)
    enable = tonumber(enable)
    if server.player_priv(cn) ~= "master" and server.player_priv(cn) ~= "admin" then
	server.player_msg(cn, red() .. "You need master/admin to enable/disable hide and seek!")
	return
    end
    if enable == 1 then
	server.broadcast_mapmodified = false
        hide_and_seek = true
	--server.HIDE_AND_SEEK = 1 -- enable this function if you want to get banned from masterserver, changes weapon ammo amount
        server.mastermode = 2
        server.msg("mastermode is now locked (2)")
        server.msg(green() .. "Hide and Seek Mode enabled!")
	can_vote = true
    else
	server.broadcast_mapmodified = true
	--server.HIDE_AND_SEEK = 0
        server.msg(blue() .. "Hide and Seek Mode disabled!")
        server.mastermode = 0
        hide_and_seek = false
    end
end

function server.playercmd_add(cn, cnadd)
	if not hide_and_seek then return end
 	if server.player_priv(cn) ~= "master" and server.player_priv(cn) ~= "admin" then
		server.player_msg(cn, red() .. "You need master/admin for this command!")
		return
   	end
	cnadd = tonumber(cnadd)
	if server.valid_cn(cnadd) then
		if server.player_status(cnadd) ~= "spectator" then
			server.player_msg(cn, red() .. server.player_name(cnadd) .. " isnt a spectator, you cant add this player.")
			return
		else
			server.msg(red() .. server.player_name(cn) .. blue() .. " added " .. red() .. server.player_name(cnadd) ..  blue() .. " to the waitlist!")
			server.player_msg(cnadd, blue () .. "You will be unspecced automaticly after this game by the server!")
			server.player_msg(cnadd, blue () .. "You will be unspecced automaticly after this game by the server!")

			player_waitlist[cnadd] = true
		end
	end
end
