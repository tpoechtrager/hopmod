local tk_limit = server.teamkill_limit_flag_carrier
local ban_time = server.teamkill_bantime

local event = {}

local is_active = false


event.dropflag = server.event_handler_object("dropflag", function(cn)

	server.player_vars(cn).has_flag = nil

end)


event.takeflag = server.event_handler_object("takeflag", function(cn)

	server.player_vars(cn).has_flag = true

end)


event.teamkill = server.event_handler_object("teamkill", function(acn, tcn)

	if is_active == true and server.player_vars(tcn).has_flag then
		local tks_actor = (server.player_vars(acn).tks_flag_carrier or 0) + 1

		if tks_actor > tk_limit then
			server.kick(acn,ban_time,"server","teamkilling")
		else
			server.player_vars(acn).tks_flag_carrier = tks_actor
		end
	end

end)


event.mapchange = server.event_handler_object("mapchange", function(map, mode)

	if mode == "insta ctf" or mode == "ctf" or mode == "insta protect" or mode == "protect" then
		is_active = true
	else
		is_active = false
	end

	for p in server.aplayers() do
		p:vars().has_flag = nil
	end

end)

local gmode = server.gamemode
if gmode == "insta ctf" or gmode == "ctf" or gmode == "insta protect" or gmode == "protect" then
	is_active = true
end

local function unload()

	is_active = false

	event = {}

    for p in server.aplayers() do
        p:vars().has_flag = nil
    end

end


return {unload = unload}