--[[
   
   This is the sixth mission of the Militia's campaign, and the last of the first chapter.
   The player has to lead a militia's fleet in a battle against the pirates
   stages : 0 player goes to battle
            1 battle
            2 way back
--]]

include "fleethelper.lua"
include "fleet_form.lua"
include("dat/scripts/numstring.lua")

lang = naev.lang()
if lang == "es" then
   else -- default english
   title = {}
   text = {}
   osd_msg = {}
   
   title[1] = "Time has come to hit"
   text[1] = [[As you approach, the officers around Jimmy Sun stand up. The atmosphere is tense, and you can feel that the militiamen have a stomach ache. "The guerrilla is over," says Sun, "It's time to beat them once for all. %s, are you ready?"]]

   title[2] = "Time has come to hit"
   text[2] = [["I will lead the half of our fleet, %s will have the second half under his responsability. The Soromid regular army will assist us, but don't expect them to be a very big help: for some reason, the Soromid central admiralty wants to keep their best ships as reserve. So, %s, you will go to %s, use the secret jump we discovered recently, and join the feet on %s.
	After that, we will converge on the planet New Haven, and destroy the fleet. We don't have the needed land forces to actually destroy their bases on New Haven, so we will only destroy every pirate ship. If my computations are exact, the renforcement fleet of Terminus will jump in during the battle. So after having beaten the pirates of New Haven, we will take care of them. Let's go!"]]
	
   refusetitle = "No thanks"
   refusetext = [["Oh, that's annoying," says Sun, "tell me when you are availible..."]]

   title[3] = "We won!"
   text[3] = [[As you join the headquarters of the militia, lots of pilots are already back from New Haven. All of them seem happy and relived. A small sign on the wall reads: "In memory of our deads comrades", but no one seems to want to think about the dead pilots. You see Jimmy Sun: "Hey, %s, nice to see you. Guess what, we have a money overflow, let me give you the modest sum of %s M credits."]]

   title[4] = "Shame on you!"
   text[4] = [[You let your comrades alone in the battle!]]

   victorymess = "The pirate fleet is destroyed: fly back to %s"
   defeatmess  = "The Slayer, capship of the militia, was destroyed: the battle is lost!"
   jimcomm     = "This is it, boys, kill them all!"
   sorcomm     = "Soromid starfleet leader to all units: prepare to engage the hostile forces."

   -- Mission details
   misn_title = "The Battle Of New Haven"
   misn_reward = "An opportunuity to use your cruiser"
   misn_desc = "You lead a part of the militia's fleet in the final fight!"
	
   -- OSD
	osd_title = "The Battle Of New Haven"
   osd_msg[1] = "Go to %s"
   osd_msg[2] = "Use the jump to %s"
   osd_msg[3] = "Destroy the pirate Fleet"
   osd_msg[4] = "Go back to %s"

   npc_desc = "Jimmy Sun"
   bar_desc = [[Sun looks focused on his goal: destroying the pirate fleet.]]
end

function create ()

   sysname = "New Haven"
   missys = system.get(sysname)

   if not misn.claim(missys) then
      misn.finish(false)
   end

   misn.setNPC(npc_desc, "neutral/thief2")
   misn.setDesc(bar_desc)

end

function accept()
   
   sysend = "Wahri"
   plaend = "Dono"
   ssys = system.get(sysend)
   spla = planet.get(plaend)
   waysys = "Forlat"
   wsys = system.get(waysys)
	
   if tk.yesno(title[1], text[1]:format( player.name() )) then
      misn.accept()
      tk.msg(title[2], text[2]:format( player.name(),  player.name(), waysys, sysname))
      
      misn.setTitle(misn_title)
      misn.setReward(misn_reward)
      misn.setDesc( misn_desc )

      marker1 = misn.markerAdd( wsys, "low" )

      osd_msg[1] = osd_msg[1]:format( waysys )
      osd_msg[2] = osd_msg[2]:format( sysname )
      osd_msg[4] = osd_msg[4]:format( plaend )

      osd = misn.osdCreate(osd_title, osd_msg)
      misn.osdActive(1)

      jump.setKnown( waysys, sysname ) -- The secret Jump

      jumpin = hook.enter("enter")
      stage = 0

      else
      tk.msg(refusetitle, refusetext)
      misn.finish(false)
   end
end

function enter()
   -- Player jumps in Forlat
   if system.cur() == wsys and stage == 0 then
      misn.markerRm( marker1 )
      marker2 = misn.markerAdd( missys, "high" )
      misn.osdActive(2)
   end

   -- Player jumps in New Haven
   if system.cur() == missys and stage == 0 then
      misn.markerRm( marker2 )
      pilot.toggleSpawn(false)
      pilot.clear()
      misn.osdActive(3)
      stage = 1

      -- We need that in fleet form in the enemy spotting routine
      -- But this time, the player doesn't regain his standing afterwards
      stand = faction.playerStanding( faction.get("Pirate") )
      if stand >= 0 then
         faction.setPlayerStanding( faction.get("Pirate"), -1 )
      end
      
      -- This makes the battle more reliable
      player.pilot():setVisible()

      militiaIn() -- spawn the fleets
      piratesIn()
      thook = hook.timer( 100000, "piratesIn2" )

      leaveh1 = hook.land("landp")
      leaveh2 = hook.jumpout("landp")

   end
end

-- The player runs away
function landp()
end

-- Spaws the militia fleets
function militiaIn()

   position = jump.get( system.cur(), wsys ):pos()

   coreFleet = {}
   for i = 1, 3 do
      pos1 = position + vec2.new(rnd.rnd(1000),rnd.rnd(1000))
      coreFleet[i] = pilot.addRaw( "Shark", "pirate", pos1, "Militia" )[1]
   end
   for i = 4, 7 do
      pos1 = position + vec2.new(rnd.rnd(1000),rnd.rnd(1000))
      coreFleet[i] = pilot.add( "Militia Lancelot", nil, pos1 )[1]
   end
   for i = 8, 10 do
      pos1 = position + vec2.new(rnd.rnd(1000),rnd.rnd(1000))
      coreFleet[i] = pilot.addRaw( "Vendetta", "pirate", pos1, "Militia" )[1]
   end
   for i = 11, 14 do
      coreFleet[i] = pilot.add( "Militia Ancestor", nil, pos1 )[1]
   end
   for i = 15, 18 do
      pos1 = position + vec2.new(rnd.rnd(1000),rnd.rnd(1000))
      coreFleet[i] = pilot.add( "Militia Admonisher", nil, pos1 )[1]
   end

   jim = pilot.addRaw( "Kestrel", "pirate", position + vec2.new(-1500,1500), "Empire" )[1]
   jim:setFaction("Militia")  -- Jim spawns as an imperial in order to become better equipment
   coreFleet[19] = jim
   jim:rename("The Slayer")

   for i, j in ipairs(coreFleet) do
      j:setVisplayer()
      j:memory("norun", true)
   end

   milfleet = Forma:new( coreFleet, "wedge", 3000, jim )

   jim:broadcast( jimcomm )

   secFleet = {}
   for i = 1, 4 do
      pos1 = position + vec2.new(rnd.rnd(1000),rnd.rnd(1000))
      secFleet[i] = pilot.add( "Soromid Brigand", nil, pos1 )[1]
   end
   for i = 5, 8 do
      pos1 = position + vec2.new(rnd.rnd(1000),rnd.rnd(1000))
      secFleet[i] = pilot.add( "Soromid Reaver", nil, pos1  )[1]
   end
   for i = 9, 12 do
      pos1 = position + vec2.new(rnd.rnd(1000),rnd.rnd(1000))
      secFleet[i] = pilot.add( "Soromid Marauder", nil, pos1  )[1]
   end
   for i = 13, 15 do
      pos1 = position + vec2.new(rnd.rnd(1000),rnd.rnd(1000))
      secFleet[i] = pilot.add( "Soromid Odium", nil, pos1  )[1]
   end
   for i = 16, 18 do
      pos1 = position + vec2.new(rnd.rnd(1000),rnd.rnd(1000))
      secFleet[i] = pilot.add( "Soromid Nyx", nil, pos1  )[1]
   end
   secFleet[19] = pilot.add( "Soromid Ira", nil, position + vec2.new(-1000,1000) )[1]

   for i, j in ipairs(secFleet) do
      j:setFaction( "Militia" )
   end

   sorfleet = Forma:new( secFleet, "wedge", 3000 )

   for i, j in ipairs(secFleet) do
      j:setVisplayer()
      j:memory("norun", true)
   end

   jim:setHilight()
   secFleet[19]:setHilight()

   losthook = hook.pilot( jim, "death", "JimDead" )

   -- In order to be sure that everybody stay together
   milfleet:setTask ( "follow", player.pilot() )
   sorfleet:setTask ( "follow", player.pilot() )

   secFleet[19]:broadcast( sorcomm )

   thiFleet = {}
   for i = 1, 2 do
      pos1 = position + vec2.new(rnd.rnd(1000),rnd.rnd(1000))
      thiFleet[i] = pilot.addRaw( "Shark", "pirate", pos1, "Militia" )[1]
   end
   for i = 3, 6 do
      pos1 = position + vec2.new(rnd.rnd(1000),rnd.rnd(1000))
      thiFleet[i] = pilot.add( "Militia Lancelot", nil, pos1 )[1]
   end
   for i = 7, 8 do
      pos1 = position + vec2.new(rnd.rnd(1000),rnd.rnd(1000))
      thiFleet[i] = pilot.addRaw( "Vendetta", "pirate", pos1, "Militia" )[1]
   end
   for i = 9, 11 do
      thiFleet[i] = pilot.add( "Militia Ancestor", nil, pos1 )[1]
   end
   for i = 12, 13 do
      pos1 = position + vec2.new(rnd.rnd(1000),rnd.rnd(1000))
      thiFleet[i] = pilot.add( "Militia Admonisher", nil, pos1 )[1]
   end
   thiFleet[14] = player.pilot()

   plafleet = Forma:new( thiFleet, "wedge", 3000, player.pilot() )

   for i, j in ipairs(thiFleet) do
      if j ~= player.pilot() then
         j:setVisplayer()
         j:memory("norun", true)
      end
   end

end

function piratesIn()

   position0 = jump.get( system.cur(), wsys ):pos()
   position = position0/2  -- They will start not too far away
   theBoss = pilot.add( "Pirate Kestrel", nil, position )[1]

   pFleet = {}
   for i = 1, 7 do
      pFleet[i] = pilot.add( "Pirate Hyena", nil, position )[1]
   end
   for i = 8, 13 do
      pFleet[i] = pilot.add( "Pirate Shark", nil, position )[1]
   end
   for i = 14, 17 do
      pFleet[i] = pilot.add( "Pirate Vendetta", nil, position )[1]
   end
   for i = 18, 22 do
      pFleet[i] = pilot.add( "Pirate Ancestor", nil, position )[1]
   end
   for i = 23, 25 do
      pFleet[i] = pilot.add( "Pirate Phalanx", nil, position )[1]
   end
   for i = 26, 28 do
      pFleet[i] = pilot.add( "Pirate Admonisher", nil, position )[1]
   end
   for i = 29, 32 do
      pFleet[i] = pilot.add( "Pirate Rhino", nil, position )[1]
   end
   for i = 33, 34 do
      pFleet[i] = pilot.add( "Pirate Kestrel", nil, position )[1]
   end
   pFleet[35] = theBoss
   theBoss:setVisible()

   piratefl1 = Forma:new( pFleet, "wedge", 3000, theBoss )
   piratefl1:setTask ( "attack", player.pilot() )

end

function piratesIn2()

   source = jump.get( system.cur(), system.get("Haven") ):pos()

   theBoss2 = pilot.add( "Pirate Kestrel", nil, source )[1]

   pFleet2 = {}
   for i = 1, 9 do
      pFleet2[i] = pilot.add( "Pirate Hyena", nil, source )[1]
   end
   for i = 10, 15 do
      pFleet2[i] = pilot.add( "Pirate Shark", nil, source )[1]
   end
   for i = 16, 19 do
      pFleet2[i] = pilot.add( "Pirate Vendetta", nil, source )[1]
   end
   for i = 20, 24 do
      pFleet2[i] = pilot.add( "Pirate Ancestor", nil, source )[1]
   end
   for i = 25, 26 do
      pFleet2[i] = pilot.add( "Pirate Phalanx", nil, source )[1]
   end
   for i = 27, 29 do
      pFleet2[i] = pilot.add( "Pirate Admonisher", nil, source )[1]
   end
   for i = 29, 32 do
      pFleet2[i] = pilot.add( "Pirate Rhino", nil, source )[1]
   end
   for i = 33, 34 do
      pFleet2[i] = pilot.add( "Pirate Kestrel", nil, source )[1]
   end

   pFleet2[35] = theBoss2
   theBoss2:setVisible()

   piratefl2 = Forma:new( pFleet2, "wedge", 3000, theBoss2 )
   piratefl2:setTask ( "attack", player.pilot() )

   -- From now, the battle can be won
   tryWon()

end

-- Hooked function to detect the fact that the pirate fleet is destroyed
function tryWon()
   pirates = pilot.get( { faction.get("Pirate") } )
   if #pirates == 0 then
      player.msg( victorymess:format(plaend) )
      stage = 2
      marker = misn.markerAdd(ssys, "low")
      misn.osdActive(4)

      hook.rm(leaveh2)
      hook.rm(leaveh1)

      landhook = hook.land("playerBack")
      leavehook = hook.jumpout("goesAway")

      else -- re-try in 1 second
      hook.timer( 1000, "tryWon" )
   end
end

-- Player is back and wants the reward
function playerBack()
   if planet.cur() == spla then
      money = 2 -- 2M credits
      tk.msg(title[3], text[3]:format( player.name(),  money ))
      player.pay( money*1000000 )
      clearMis( true )
   end
end

-- Player goes away during the battle
function landp()
   tk.msg(title[4], text[4])
   var.push( "milDead", true ) -- The militia is dead
   clearMis( true )
end

-- Player lives the system: let's clean the fleets
function goesAway ()
   hook.rm(leavehook)
   if sorfleet ~= nil then
      sorfleet:disband()
   end
   if milfleet ~= nil then
      milfleet:disband()
   end
   if thifleet ~= nil then
      thifleet:disband()
   end
end

-- Jimmy's ship got destroyed
function JimDead()
   player.msg( defeatmess )
   var.push( "milDead", true ) -- The militia is dead
   clearMis( true )
end

-- Cleans everything and closes the mission
function clearMis( status )

   -- Systematically remove hooks
   hooks = { leavehook, landhook, leaveh1, leaveh2, losthook, thook, jumpin }
   for _, j in ipairs( hooks ) do
      if j ~= nil then
         hook.rm(j)
      end
   end

   misn.osdDestroy()
   misn.finish(status)
end
