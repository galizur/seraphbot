-- commands/sfx.lua
return {
    name = "sfx",
    aliases = {"sound", "audio", "play"},
    description = "Play sound effects",
    usage = "!sfx <soundname> | !sfx list [page] | !sfx random",
    user_cooldown = 3,
    command_cooldown = 1,

    execute = function(ctx, args)
        local sound_dir = "sounds"

        -- Handle no arguments - show quick usage
        if #args == 0 then
            ctx:reply("Usage: !sfx <sound> | !sfx list | !sfx random")
            return
        end

        local command = args[1]:lower()

        -- List available sounds with pagination
        if command == "list" then
            local page = 1
            if #args > 1 then
                page = tonumber(args[2]) or 1
            end

            local sounds = ctx:listAudioFiles(sound_dir)
            if #sounds == 0 then
                ctx:reply("No sounds found in directory: " .. sound_dir)
                return
            end

            -- Pagination logic - stay under 500 characters, don't cut words
            local response = "Available sounds: "
            local sounds_added = 0
            local start_index = (page - 1) * 15 + 1 -- Rough estimate, we'll adjust dynamically
            local current_length = string.len(response)
            local page_sounds = {}

            for i = start_index, #sounds do
                local sound_name = sounds[i]
                local test_addition = sound_name
                if sounds_added > 0 then
                    test_addition = ", " .. sound_name
                end

                -- Check if adding this sound would exceed limit
                if current_length + string.len(test_addition) > 450 then -- Leave room for page info
                    break
                end

                table.insert(page_sounds, sound_name)
                current_length = current_length + string.len(test_addition)
                sounds_added = sounds_added + 1
            end

            if #page_sounds == 0 then
                ctx:reply("Page " .. page .. " is empty. Try a lower page number.")
                return
            end

            response = response .. table.concat(page_sounds, ", ")

            -- Add page info
            local total_pages = math.ceil(#sounds / sounds_added)
            if total_pages > 1 then
                response = response .. " (Page " .. page .. "/" .. total_pages .. ")"
            end

            ctx:reply(response)
            return
        end

        -- Random sound selection
        if command == "random" then
            local sounds = ctx:listAudioFiles(sound_dir)
            if #sounds == 0 then
                ctx:reply("No sounds available for random selection")
                return
            end

            local random_sound = sounds[math.random(#sounds)]
            local filepath = ctx:findAudioFile(random_sound, sound_dir)

            if filepath ~= "" then
                   local volume = 70 -- Default volume
                   if #args > 1 then
                       volume = tonumber(args[2]) or 70
                       volume = math.max(0, math.min(100, volume)) -- Clamp 0-100
                   end

                   if ctx:playSound(filepath, volume) then
                       ctx:reply("Random sound: " .. random_sound)
                   else
                       ctx:reply("Failed to play random sound: " .. random_sound)
                   end
            else
                ctx:reply("Could not find random sound file")
            end
            return
        end

        -- Play specific sound
        local sound_name = command
        local volume = 70 -- Default volume

        -- Check if second argument is volume
        if #args > 1 then
            local vol_arg = tonumber(args[2])
            if vol_arg then
                volume = math.max(0, math.min(100, vol_arg)) -- Clamp 0-100
            end
        end

        local filepath = ctx:findAudioFile(sound_name, sound_dir)

        if filepath == "" then
            ctx:reply("Sound '" .. sound_name .. "' not found. Try !sfx list")
            return
        end

        if ctx:playSound(filepath, volume) then
            -- Optional: Confirm sound played
            ctx:reply(sound_name)
        else
            ctx:reply("Failed to play sound: " .. sound_name)
        end

    end
}
