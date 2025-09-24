-- commands/modcmd.lua - Moderator-only command
return {
    name = "modcmd",
    description = "Moderator only command",
    usage = "!modcmd",
    mod_only = true,
    execute = function(ctx, args)
        ctx:reply("This is a moderator-only command executed by " .. ctx:getUser())
    end
}
