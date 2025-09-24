-- commands/limited.lua - Heavily restricted command example
return {
    name = "special",
    description = "Special limited command",
    usage = "!special",
    subscriber_only = true,
    user_cooldown = 60,      -- 1 minute per user
    command_cooldown = 10,   -- 10 seconds between any uses
    max_uses_per_stream = 10, -- Only 10 uses total per stream
    execute = function(ctx, args)
        ctx:reply("🎉 Special subscriber command activated by " .. ctx:getUser() .. "!")
    end
}
