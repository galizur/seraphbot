-- commands/flip.lua - Simple coin flip with global cooldown
return {
    name = "flip",
    aliases = {"coin", "coinflip"},
    description = "Flip a coin",
    usage = "!flip",
    command_cooldown = 5,  -- 5 seconds between any uses of this command
    execute = function(ctx, args)
        local result = math.random(2) == 1 and "Heads" or "Tails"
        ctx:reply("🪙 " .. result .. "!")
    end
}
