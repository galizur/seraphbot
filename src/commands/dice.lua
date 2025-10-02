-- commands/dice.lua - Enhanced with usage limit
return {
    name = "dice",
    aliases = {"roll", "d6"},
    description = "Roll dice",
    usage = "!dice [sides]",
    user_cooldown = 15,
    max_uses_per_stream = 50,  -- Only 50 uses per stream
    execute = function(ctx, args)
        local sides = 6
        if #args > 0 then
            local input = tonumber(args[1])
            if input and input > 1 and input <= 100 then
                sides = input
            end
        end

        local result = math.random(sides)
        ctx:reply("🎲 " .. ctx:getUser() .. " rolled a " .. result .. " (1-" .. sides .. ")")
    end
}
