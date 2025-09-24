-- commands/hello.lua - Basic command with cooldown
return {
    name = "hello",
    description = "Greet a user",
    usage = "!hello [username]",
    user_cooldown = 10,  -- 10 second cooldown per user
    execute = function(ctx, args)
        if #args > 0 then
            ctx:reply("Hello " .. args[1] .. "!")
        else
            ctx:reply("Hello " .. ctx:getUser() .. "!")
        end
    end
}
