--[[

	A player command to enable/ disable persistent teams at mapchange

]]


local usage = "#persist 0|1"

return function(cn, option)

	if not option then
		return false, usage
	elseif tonumber(option) == 1 then
		server.reassignteams = 0
		server.player_msg(cn,orange("reshuffle teams at mapchange disabled"))
	elseif tonumber(option) == 0 then
		server.reassignteams = 1
		server.player_msg(cn, orange("reshuffle teams at mapchange enabled"))
	else
		return false, usage
	end

end
