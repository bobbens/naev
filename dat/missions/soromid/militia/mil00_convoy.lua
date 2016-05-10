--[[
   
   This is the first mission of the Militia's campaign. The player has to escort a merchant
   stages : 0 the mule hasn't jumped yet
            1 the pilot needs to follow the mule
--]]

include("dat/scripts/numstring.lua")

lang = naev.lang()
if lang == "es" then
   else -- default english
   title = {}
   text = {}
   osd_msg = {}
   
   title[1] = "Looking for an escort"
   text[1] = [[As you sit at her table, the woman starts to talk : "I'm an honest trader and I have an important delivery to %s. I don't have enough time to use the common trade route: I need to cross %s and %s. That's why I'm looking for a combat capable pilot to go with me. I can pay you %s credits. Are you going to do it ?"]]
	
   refusetitle = "No thank you"
   refusetext = [["Ok, " she says, obviously disappointed, "I guess I have to look for someone else..."]]
   
   title[2] = "Let's go"
   text[2] = [["I knew, there were still people who don't fear the pirates!"]]
   
   title[3] = "That was a nice trip"
   text[3] = [["We have done it, %s", says the merchant. "Take these credits as a reward. I'll tell people that there are still pilots who are courageous enough to stand up against the pirates." She hands you a credit chip and goes to the docks, where workers are already unloading her ship.]]
	
   title[4] = "You ran away!"
   title[5] = "The mule got destroyed"
   text[6] = "The mule got boarded"
   text[4] = [[Your mission failed.]]

   -- Mission details
   misn_title = "The Convoy"
   misn_reward = "%s credits"
   misn_desc = "You are escorting a trader."
   
   -- NPC
   npc_desc = "A woman"
   bar_desc = "Maybe she has a suitable job for you."
	
   -- OSD
	osd_title = "The Convoy"
   osd_msg = "Protect the mule."
end

function create ()
	-- No system claim   
   misn.setNPC(npc_desc, "neutral/miner2")
   misn.setDesc(bar_desc)
end

function accept()
   
   --Change here to change the planet and the system
   sysname = "Dune"
   planame = "Arrakis"
   crossys0 = "Daled"
   crossys1 = "Haven"
   crossys2 = "Gamel"
   missys = system.get(sysname)
   mispla = planet.get(planame)
   ssys = system.get("Wahri")
   source = planet.get("Dono")
   
   crossed = 0
   reward = 70000
	
   if tk.yesno(title[1], text[1]:format(sysname,crossys1,crossys2,numstring(reward))) then
      misn.accept()
      tk.msg(title[2], text[2])
      
      misn.setTitle(misn_title)
      misn.setReward(misn_reward:format(numstring(reward)))
      misn.setDesc(misn_desc)
      osd = misn.osdCreate(osd_title, {osd_msg})
      misn.osdActive(1)
      tocross = {ssys, system.get(crossys0), system.get(crossys1), system.get(crossys2), missys}

      player_jump = hook.enter("enter")

      else
      tk.msg(refusetitle, refusetext)
      misn.finish(false)
   end
end

function vip_jumped() -- The mule jumped : waiting for the player to do the same
   stage = 1
end
function vip_landed() -- The mule landed : waiting for the player to do the same
   stage = 1
end
function vip_dead() -- The mule died
   tk.msg(title[4], text[5])
   clearMis(false)
end
function vip_board() -- The mule was boarded
   tk.msg(title[4], text[6])
   clearMis(false)
end

function enter()

   -- Remove some old hooks
   hooks = { player_jump, vip_jump, dehook, bohook }
   for _, j in ipairs( hooks ) do
      if j ~= nil then
         hook.rm(j)
      end
   end

   -- Recall the list here in case the player loads the game
   -- (lists aren't stored if you save)
   tocross = {ssys, system.get(crossys0), system.get(crossys1), system.get(crossys2), missys}

   if stage == 0 then -- The player didn't wait for the mule
      tk.msg(title[4], text[4])
      clearMis(false)

      else
      stage = 0
      if crossed == 0 then
         vip = pilot.add("Trader Mule", nil, source)[1]
      else
         vip = pilot.add("Trader Mule", nil, tocross[crossed])[1]
      end

      vip:setHilight()
      vip:control()

		dehook = hook.pilot(vip, "death", "vip_dead")
		bohook = hook.pilot(vip, "board", "vip_board")

      player_jump = hook.enter("enter")

      if crossed < 4 then -- There are still systems to cross
         vip:hyperspace( tocross[crossed+2] )
         vip_jump = hook.pilot(vip, "jump", "vip_jumped")

         else --Arrived in Dune
         vip:land( mispla )
         vip_land = hook.pilot(vip, "land", "vip_landed")
         endHook = hook.land("endOfMission")
      end
      crossed = crossed + 1

   end
end

function endOfMission()
   tk.msg(title[3], text[3]:format(player:name()))
   player.pay(reward)
   clearMis(true)
end

-- Cleans everything and closes the mission
function clearMis( status )

   -- Systematically remove hooks
   hooks = { player_jump, endHook, vip_land, vip_jump, dehook, bohook }
   for _, j in ipairs( hooks ) do
      if j ~= nil then
         hook.rm(j)
      end
   end

   misn.osdDestroy()
   misn.finish(status)
end
