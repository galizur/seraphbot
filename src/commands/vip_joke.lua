-- commands/vip_joke.lua - VIP/Sub only command with specific users allowed
return {
    name = "vipjoke",
    description = "Tell a VIP joke",
    usage = "!vipjoke",
    vip_only = true,
    allowed_users = {"specificfriend", "anotherfriend"},  -- These users can use it even if not VIP
    command_cooldown = 30,
    execute = function(ctx, args)
        local jokes = {
            "Why do VIPs never get lost? Because they always have priority directions!",
            "What did the VIP say to the regular viewer? 'Thanks for the support!'",
            "Why are VIPs like good code? They're both well-structured and valuable!"
        }

        local joke = jokes[math.random(#jokes)]
        ctx:reply("🌟 " .. joke)
    end
}
