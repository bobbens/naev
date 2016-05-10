--[[
   
   This is the fifth mission of the Militia's campaign. The player has to look for a pirate fleet
   stages : 0 player isn't in system
            1 player is in system, but didn't see the fleet
            2 way back
--]]

include "fleethelper.lua"
include "fleet_form.lua"
include "dat/scripts/proximity.lua"

lang = naev.lang()
if lang == "es" then
   else -- default english
   title = {}
   text = {}
   osd_msg = {}
   
   title[1] = "They are coming"
   text[1] = [["Nice to see you again, %s. You know what? I managed to become the new military governor of %s. I am now preparing to defend the system against the imminent pirate attack. I need someone to scout a few systems for me right now. Can you do it?"]]

   title[2] = "The mission"
   text[2] = [["I already saied that the pirates from Terminus are coming in order to help their brothers from New Haven. The problem is that I can't beat them right now if they manage to meet. I need to know how many time we still have to prepare the fleet. Go to %s, %s, %s, %s and %s and come back when you have seen the pirate fleet. Please be careful: the pirates have set up lots of patrols in those systems in order to intercept our scouts."]]
	
   refusetitle = "I have something else to do"
   refusetext = [["Well, come back if you change your mind."]]

   title[3] = "End of mission"
   text[3] = [["Hey, %s, it's me, Jimmy Sun. Check my new ship! Krain industries sold it to me for the halth of it's price! We are currently doing a combat exercise with the Soromid fleet. I'll have a look at the data you gathered. Meet me at the bar in %s just after that, we are totally ready for the final combat. By the way, Let me give you a reactor to equip your ship."]]

   title[4] = "The pirate fleet"
   text[4] = [[Suddently, your mass detection device emits a signal. You look at the sensor's screen and see them: a handful cruisers surrounded by destroyers and corvettes. These ships mean buisness. For now, they are refuelling, but they will soon start up again. It's time for you to report back to the militia's headquarters.]]

   -- Mission details
   misn_title = "Hide and Seek"
   misn_reward = "Probably some random item"
   misn_desc = "You were asked to locate a pirate fleet."
	
   -- OSD
	osd_title = "Hide and Seek"
   osd_msg[1] = "Inspect the marked systems"
   osd_msg[2] = "Go back to %s"

   npc_desc = "Jimmy Sun"
   bar_desc = [[People around him start to call him "your Lordship" that's weired!]]
end

function create ()

   sysname = {}
   missys = {}

   sysname[1] = "Haven"
   sysname[2] = "Andres"
   sysname[3] = "Rockbed"
   sysname[4] = "Sikh"
   sysname[5] = "Acheron"

   -- Pick the actual system with the pirate fleet 
   -- (don't pick Haven because it's too close imho)
   thesysn = sysname[ rnd.rnd( 2, #sysname ) ]
   thesys = system.get( thesysn )

   for i, j in ipairs(sysname) do
      missys[i] = system.get(j)
   end
   if not misn.claim(thesys) then
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
	
   if tk.yesno(title[1], text[1]:format( player.name(), plaend )) then
      misn.accept()
      tk.msg(title[2], text[2]:format( sysname[1], sysname[2], 
         sysname[3], sysname[4], sysname[5] ))
      
      misn.setTitle(misn_title)
      misn.setReward(misn_reward)
      misn.setDesc( misn_desc:format( sysend ) )

      marker1 = misn.markerAdd(missys[1], "low")
      marker2 = misn.markerAdd(missys[2], "low")
      marker3 = misn.markerAdd(missys[3], "low")
      marker4 = misn.markerAdd(missys[4], "low")
      marker5 = misn.markerAdd(missys[5], "low")

      osd_msg[2] = osd_msg[2]:format( plaend )
      osd = misn.osdCreate(osd_title, osd_msg)
      misn.osdActive(1)

      jumpin = hook.enter("enter")
      stage = 0

      else
      tk.msg(refusetitle, refusetext)
      misn.finish(false)
   end
end

function enter()

   -- Player is in the good system
   if system.cur() == thesys and stage == 0 then

      stage = 1
      misn.markerRm( marker1 )
      misn.markerRm( marker2 )
      misn.markerRm( marker3 )
      misn.markerRm( marker4 )
      misn.markerRm( marker5 )

      pilot.toggleSpawn(false)
      pilot.clear()

      patrol1 = {}
      position = jump.get( system.cur(), previous_sys ):pos()
      for i = 1, 3 do
         pos1 = position + vec2.new(rnd.rnd(1000),rnd.rnd(1000))
         patrol1[i] = pilot.add( "Pirate Hyena", nil, pos )[1]
      end
      squadron1 = Forma:new( patrol1, "wedge", 1500 )

      patrol2 = addShips({"Pirate Hyena"}, nil, vec2.new(0,0), 3)
      squadron2 = Forma:new( patrol2, "wedge", 1500 )

      fuel = {}
      idlehook = {}
      for i = 1, 5 do
         fuel[i] = pilot.addRaw( "Mule", "pirate", vec2.new(0,0) , "Pirate" )[1]
         fuel[i]:control()
         angle = rnd.rnd() * 360
         pos = vec2.newP( 1000, angle )
         fuel[i]:goto( pos )
         idlehook[i] = hook.pilot( fuel[i], "idle", "idle" )
      end

      theBoss    = pilot.add( "Pirate Kestrel", "dummy", vec2.new(0,0) )[1]

      coreFleet = {}
      for i = 1, 3 do
         coreFleet[i] = pilot.add( "Pirate Hyena", nil, vec2.new(0,0) )[1]
      end
      for i = 4, 9 do
         coreFleet[i] = pilot.add( "Pirate Shark", nil, vec2.new(0,0) )[1]
      end
      for i = 10, 13 do
         coreFleet[i] = pilot.add( "Pirate Vendetta", nil, vec2.new(0,0) )[1]
      end
      for i = 14, 18 do
         coreFleet[i] = pilot.add( "Pirate Ancestor", nil, vec2.new(0,0) )[1]
      end
      for i = 19, 20 do
         coreFleet[i] = pilot.add( "Pirate Phalanx", nil, vec2.new(0,0) )[1]
      end
      for i = 21, 23 do
         coreFleet[i] = pilot.add( "Pirate Admonisher", nil, vec2.new(0,0) )[1]
      end
      for i = 24, 27 do
         coreFleet[i] = pilot.add( "Pirate Rhino", nil, vec2.new(0,0) )[1]
      end
      for i = 28, 29 do
         coreFleet[i] = pilot.add( "Pirate Kestrel", nil, vec2.new(0,0) )[1]
      end
      coreFleet[30] = theBoss

      piratefl = Forma:new( coreFleet, "circle", 3000, theBoss )

      trySpot()
      juphook = hook.jumpout( "goesAway" )
      
   end

   -- Player completed the mission
   if system.cur() == ssys and stage == 2 then

      pilot.toggleSpawn(false)
      pilot.clear()

      patrol1 = {}
      patrol1 = addShips({"Militia Lancelot"}, nil, vec2.new(3000,2000), 5)
      squadron1 = Forma:new( patrol1, "wedge", 1500 )

      patrol2 = addShips({"Militia Ancestor"}, nil, vec2.new(-1000,500), 3)
      squadron2 = Forma:new( patrol2, "wedge", 1500 )

      jim = pilot.addRaw( "Kestrel","dummy", vec2.new(100,-500), "Militia" )[1]

      coreFleet = {}
      for i = 1, 7 do
         coreFleet[i] = pilot.add( "Militia Lancelot", nil, vec2.new(0,0) )[1]
      end
      for i = 8, 10 do
         coreFleet[i] = pilot.add( "Militia Ancestor", nil, vec2.new(0,0) )[1]
      end
      for i = 11, 14 do
         coreFleet[i] = pilot.add( "Militia Admonisher", nil, vec2.new(0,0) )[1]
      end
      coreFleet[15] = jim
      jim:rename("The Slayer")

      milfleet = Forma:new( coreFleet, "wall", 3000, jim )

      secFleet = {}
      for i = 1, 4 do
         secFleet[i] = pilot.add( "Soromid Reaver", nil, vec2.new(0,0) )[1]
      end
      for i = 5, 7 do
         secFleet[i] = pilot.add( "Soromid Odium", nil, vec2.new(0,0) )[1]
      end
      for i = 7, 8 do
         secFleet[i] = pilot.add( "Soromid Nyx", nil, vec2.new(0,0) )[1]
      end
      secFleet[9] = pilot.add( "Soromid Ira", nil, vec2.new(100,-800) )[1]

      for i, j in ipairs(secFleet) do
         j:setFaction( "Militia" )
      end

      sorfleet = Forma:new( secFleet, "wall", 3000 )

      thook = hook.timer(500, "proximity", {anchor = jim, radius = 5000, funcname = "hailme"})
   end

   previous_sys = system.cur()   -- This variable makes it possible to make pilots jump after the player

end

-- Player lives the system: let's clean the fleets
function goesAway ()
   if piratefl ~= nil then
      piratefl:disband()
   end
   if squadron1 ~= nil then
      squadron1:disband()
   end
   if squadron2 ~= nil then
      squadron2:disband()
   end
end

-- A fuel ship is idle
function idle( pilot )
   angle = rnd.rnd() * 360
   pos = vec2.newP( 1000, angle )
   pilot:goto( pos )
end

-- Hooked function to detect the fact that the player sees the fleet
-- Let's pick for example a random Rhino
function trySpot()
   detected, scanned = player.pilot():inrange( coreFleet[26] )
   if scanned then
      tk.msg(title[4], text[4])
      stage = 2
      marker = misn.markerAdd(ssys, "low")
      misn.osdActive(1)

      else -- re-try in 1 second
      hook.timer( 1000, "trySpot" )
   end
end

-- Sun hails the player
function hailme()
   jim:hailPlayer()
   hook.rm( thook )
   hook.pilot(jim, "hail", "answer")
end

-- Time for chat
function answer()
   player.commClose()
   tk.msg(title[3], text[3]:format( player.name(), plaend ))
   leaveh1 = hook.land("landp")
   leaveh2 = hook.jumpout("landp")
end

-- ends the mission when the player leaves the system
function landp()
   if milfleet ~= nil then
      milfleet:disband()
   end
   if sorfleet ~= nil then
   sorfleet:disband()
   end
   if squadron1 ~= nil then
   squadron1:disband()
   end
   if squadron2 ~= nil then
   squadron2:disband()
   end

   player.addOutfit("Reactor Class III")
   clearMis( true )
end

-- Cleans everything and closes the mission
function clearMis( status )

   -- Systematically remove hooks
   hooks = { leaveh1, leaveh2, thook, juphook, jumpin }
   for _, j in ipairs( hooks ) do
      if j ~= nil then
         hook.rm(j)
      end
   end
   if idlehook ~= nil then
      for _, j in ipairs( idlehook ) do
         hook.rm(j)
      end
   end

   misn.osdDestroy()
   misn.finish(status)
end
