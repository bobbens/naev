--[[
   
   This is the second mission of the Militia's campaign. The player has to kill a pirate
   stages : 0 the player goes to meeting point
            1 the battle begins
            2 The player killed the target and is on the way back
--]]

--Needed scripts
include("dat/scripts/pilot/pirate.lua")
include "dat/scripts/proximity.lua"

lang = naev.lang()
if lang == "es" then
   else -- default english
   title = {}
   text = {}
   osd_msg = {}
   
   title[1] = "Are you on the side of justice?"
   text[1] = [[As you decide to answer the call, a smiling face appears on your viewscreen. "Hello, I'm the commander Jimmy Sun, retired pilot of the soromid army. I belive you are %s, right. You are the first pilot I know who managed to escort a Mule in the north of Alteris."]]
	
   refusetitle = "I can't do that"
   refusetext = [["Well, so I will look for someone else... Have a nice day anyway."]]
   
   title[2] = "A bug"
   text[2] = [[You push the button to answer the call, but the equipment of the other pilot seems to be defective and the viewscreen becomes blue and you can read: "Someone is trying to call you". After a few seconds, the call ends and the pilot doesn't make any more attempts to communicate.]]
   
   title[3] = "Are you on the side of justice?"
   text[3] = [["Do you want to help the citizen of Dono, and all the traders who have hard time earning a living? Myself, and a few soromid trader and army pilots, are building an organization whose purpose is to fight piracy. I'd be very happy to see us in our ranks.
The pirate Lord %s, who leads New Haven is convinced that he is the best fighter pilot in the universe. He challenges every pilot who wants it to a dogfight disable duel around his planet. My plan is to challenge and to kill him during a dogfight.
Once he will be dead, the other pirates will fight against each other for supremacy and the honest citizen's life will be much better. Of course this operation is highly risky, and all of the members of my organization are volunteers, so you won't be paied. Are you still in?"]]

   title[4] = "Let's go"
   text[4] = [["It seems you're a real though guy!" says Sun, "So, you now have to go to %s, land on %s (you may need to bribe them, but they are not really expensive), say to everybody in the bar that you are a better fighter pilot than their chief, and then kill him and report back to %s in %s. Good luck."]]
	
   title[5] = "You ran away!"
   text[5] = [[Your mission failed.]]

   title[6] = "That was... Awesome!"
   text[6] = [[As you land, Jimmy Sun and a group of pilots is already waiting for you. "Oh man," he says, "You got him! That's wonderful. Now, everyone knows that being a pirate is way more dangerous than being an honest citizen."
   Sun then turns solemnly to the other pilots and says: "Today, we obtained a great victory, but the fight is not over. I declare the etablishment of the Free Citizen's Militia, witch goal is to make the pirates of New Haven understand that they have nothing to do on our trade routes! %s, if you want to help us again, just come back, we will maybe have somethong for you."]]
	
   title[7] = "I want to fight!"
   text[7] = [[You sit at the pirate's table and start yelling: "Hey, you, I've looked everywhere on this planet for a good fighter pilot, and I only saw weaklings. I'm sure, you are no exception! I'm starting to belive that everybody in New Haven is unable to shoot anything more than a trader' Llama. Actually, it's not surprising as your boss is %s. Any real pilot could disable this guy's ship eyes colsed!"
   The usual noise in the bar stop and the pirate says to you: "Looks like you are in trouble, %s will be informed that you want to fight with him, and he won't miss you, I can promise that!"]]

   title[8] = "You are so in trouble!"
   text[8] = [["So I guess you know how it works: missiles are forbitten and you have to use only a fighter. There will be some pilots around you who ensure that you respect the rules. First, you join jour mark, and then, the fight begins. The first ship that is disabled (that means your ship) has lost. If you attack the boss before he joined his mark, we'll destroy you. Understood?"]]

   dismisstitle = "You are dismissed"
   missiletext = "You aren't allowed to use missiles"
   fightertext = "You had to use a fighter"

   attackMsg = "Hey, you aren't supposed to do that!"
   player_lost = "Haw haw haw, I am invincible!"

   player_killed = "He killed the boss! After him!"

   -- Mission details
   misn_title = "The Execution"
   misn_reward = "The eternal gratitude of the citizen of %s"
   misn_desc = "A group of pilots decided to murder a pirate clan lord during a dogfight contest"
	
   -- OSD
	osd_title = "The Execution"
   osd_msg[1] = "Fly to the %s system with a fighter and land on %s"
   osd_msg[2] = "Kill %s"
   osd_msg[3] = "Escape and land on %s"

   npc_desc = "Roughneck pirate"
   bar_desc = "This pirate doesn't seems to want to be told what he has to do"

   --mark
   mark_name = "START"
end

function create ()
   sysname = "New Haven"
   missys = system.get(sysname)
   if not misn.claim(missys) then
      npc_picture = "unknown"
      tk.msg(title[2], text[2], npc_picture)
      misn.finish(false)
   end
   pirname = pirate_name()

   npc_picture = "neutral/thief2"
   tk.msg(title[1], text[1]:format(player.name()), npc_picture)
   if tk.yesno( title[3], text[3]:format(pirname) ) then
      accept()
   else
      tk.msg(refusetitle, refusetext)
      misn.finish(false)
   end
end

function accept()
   planame = "New Haven"
   mispla = planet.get(planame)
   safename = "Dono"
   safesys = "Wahri"
   missafe = planet.get(safename)
   syssafe = system.get(safesys)
   tk.msg(title[4], text[4]:format(planame, sysname, safename, safesys))

   misn.accept()
   
   misn.setTitle(misn_title)
   misn.setReward(misn_reward:format(safename))
   misn.setDesc(misn_desc)
    
   osd_msg[1] = osd_msg[1]:format(sysname, planame)
   osd_msg[2] = osd_msg[2]:format(pirname)
   osd_msg[3] = osd_msg[3]:format(safename)
   misn.osdCreate(osd_title, osd_msg)
   
   misn_marker1 = misn.markerAdd( missys, "low" )
   
   stage = 0
   landhook = hook.land("land")
end

function land()
   if stage == 1  then-- The player lands during the duel
      tk.msg(title[5], text[5])
      clearMis(false)

      elseif planet.cur() == mispla and stage == 0 then-- Land on New Haven to meet the pirates
      pirate = misn.npcAdd("beginbattle", npc_desc, "pirate/pirate3", bar_desc)

      elseif planet.cur() == missafe and stage == 2 then-- Land on Dono to get congratulated
      tk.msg(title[6], text[6]:format(player.name()))
      clearMis(true)
   end
end

function beginbattle()
   misn.markerRm(misn_marker1)
   --Chat with the pirate and take off
   tk.msg( title[7], text[7]:format(pirname, pirname) )
   tk.msg( title[8], text[8] )
   stage = 1
   takhook = hook.takeoff("epic_fight")
end

function epic_fight()

   --Launchers are forbidden
   listofoutfits = player.pilot():outfits()
   haslauncher = false
   for i, j in ipairs(listofoutfits) do
      if j:type() == "Launcher" then
         haslauncher = true
      end
   end

   if haslauncher == true then
      tk.msg(dismisstitle, missiletext)
      clearMis(false)
      elseif ship.class(pilot.ship(player.pilot())) ~= "Fighter" then
      tk.msg(dismisstitle, fightertext)
      clearMis(false)
   end

   pilot.clear()
   pilot.toggleSpawn(false)
   misn.osdActive(2)

   opponent = pilot.add( "Pirate Vendetta", nil, mispla )[1]
   opponent:rmOutfit("all")
   opponent:addOutfit("Ion Cannon", 6)

   opponent:rename(pirname)
   opponent:setHilight()
   opponent:setHostile()

   opponent:control()
   opponent:goto(mispla:pos() + vec2.new( 500,  750))

   --The security
   sec1 = pilot.add( "Pirate Hyena", nil, mispla )[1]
   sec2 = pilot.add( "Pirate Hyena", nil, mispla )[1]

   hooks = {}
   for i, k in ipairs({sec1, sec2}) do
      hooks[i] = hook.pilot(k, "attacked", "escort_attacked")
      k:control()
      k:memory("radius", 300) --Set the radius for the follow function
      k:memory("angle", 200)
   end

   --The escort follows the competitors
   sec1:follow(player.pilot(), true)
   sec2:follow(opponent, true)

   --Some hooks
   jumphook = hook.jumpout("jumpout")

   opdehook = hook.pilot( opponent, "death", "oppo_dead" )
   opjuhook = hook.pilot( opponent, "jump", "oppo_jump" )
   pldihook = hook.pilot( player.pilot(), "disable", "player_disabled" )
   attackhook = hook.pilot( opponent, "attacked", "oppo_attacked" )

   --Adding the starting mark
   start_pos = mispla:pos() + vec2.new( -500, -750)
   mark = system.mrkAdd( mark_name, start_pos )
   prox = hook.timer(500, "proximity", {location = start_pos, radius = 300, funcname = "assault"})

end

function escort_attacked(pilot,attacker) --Someone attacked the escort
   for i, j in ipairs(hooks) do
      hook.rm(j)
   end
   for i, k in ipairs({sec1, sec2}) do
      k:control(false)
   end
   if stage == 1 and attacker == player.pilot() then-- Player attacked the escort (that isn't very smart)
      opponent:control(false)
      opponent:comm(attackMsg)

      -- Some renforcements for the baddies
      p1 = pilot.add( "Pirate Hyena", nil, mispla ) [1]
      p2 = pilot.add( "Pirate Shark", nil, mispla ) [1]
      p3 = pilot.add( "Pirate Admonisher", nil, mispla ) [1]
      p1:setHostile()
      p2:setHostile()
      p3:setHostile()
      pilot.toggleSpawn(false, true)
   end
end

function jumpout()
   if stage == 1 then
      tk.msg(title[5], text[5])
      clearMis(false)
   end
end

function oppo_dead()  --The player killed the pirate
   player_wanted()
   stage = 2
   misn.osdActive(3)
   if sec1:exists() then
      sec1:broadcast( player_killed )
   end
   misn_marker2 = misn.markerAdd( syssafe, "low" )
end

function player_disabled()  --player has lost
   opponent:taskClear()
   opponent:land("New Haven")
   opponent:broadcast(player_lost)
   clearMis(false)
end

function oppo_boarded()  -- That was definitly not a good idea
   player_wanted()
end

function oppo_attacked()
   hook.rm(prox)
   hook.rm(attackhook)
   system.mrkRm(mark)
   opponent:comm(attackMsg)
   opponent:control(false)
   player_wanted()
end

function player_wanted()  -- For some reason, the player is wanted
   for i, j in ipairs(hooks) do
      hook.rm(j)
   end
   for i, k in ipairs({sec1, sec2}) do
      if k:exists() then
         k:setHostile()
         k:control(false)
      end
   end

   -- Some renforcements for the baddies
   p1 = pilot.add( "Pirate Hyena", nil, mispla ) [1]
   p2 = pilot.add( "Pirate Shark", nil, mispla ) [1]
   p3 = pilot.add( "Pirate Admonisher", nil, mispla ) [1]
   p1:setHostile()
   p2:setHostile()
   p3:setHostile()
   pilot.toggleSpawn(false, true)
end

function assault()
   opponent:attack(player.pilot())
   hook.rm(prox)
   hook.rm(attackhook)
   system.mrkRm(mark)
end

-- Cleans everything and closes the mission
function clearMis( status )

   -- Systematically remove hooks
   hooks2 = { prox, attackhook, opdehook, opjuhook, pldihook, takhook, landhook }
   for _, j in ipairs( hooks2 ) do
      if j ~= nil then
         hook.rm(j)
      end
   end
   if hooks ~= nil then
      for _, j in ipairs( hooks ) do
         hook.rm(j)
      end
   end

   misn.osdDestroy()
   misn.finish(status)
end
