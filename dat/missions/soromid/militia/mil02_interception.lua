--[[
   
   This is the third mission of the Militia's campaign. The player has to intercept a pirate raid
   stages : 0 way to mission's system
            1 battle
            2 way back
--]]

include "fleethelper.lua"
include "fleet_form.lua"

lang = naev.lang()
if lang == "es" then
   else -- default english
   title = {}
   text = {}
   osd_msg = {}
   
   title[1] = "We need you once again"
   text[1] = [[You sit at Sun's table and ask how the pirate cleaning is going on. "Terribly, he answers: in spite of a few successes for the militia, the pirates didn't break up after the death of their lord. They are right now uniting under the flag of the former second in command on New Haven, and he decided to send an expedition here. They are planning to use fast ships in order to avoid the militia's fighters, to land on %s and to kill sympathizers of the militia."]]

   title[2] = "We need you once again"
   text[2] = [[You ask why the Soromid army can't protect the people of %s. "The military governor of %s is a corrupt man: the pirates give him a smooth amount of credits every time they don't want to see Soromid fighters on their way. Anyway, we need to stop the pirate squadron. Do you want to lead our boys?"]]
	
   refusetitle = "I'sorry, I can't"
   refusetext = [["Ok, so goodbye," says Sun.]]

   title[3] = "Let's shoot them!"
   text[3] = [["I had no doubt, you were going to help us. So, jump in %s and join the patrol I already sent, they will follow you. One of our agents put a transmitter on the pirate leader, so you will see him right after he jumped in. There are two courrier ships with them. The goal is to destroy those ships. You can let the other run away if you want, but please don't let the courrier jump in %s."]]
	
   title[4] = "You ran away!"
   text[4] = [[Your mission failed.]]

   title[5] = "Well done!"
   text[5] = [[After landing, you go to the militia's headquarters and meet Jimmy Sun "Nice to see that you are still alive, %s" he says. Hey, I have a gift for you: I recently got a grant from the space trader's guild, and I bought thousands of fury missiles to equip the boys. I can give you some."]]

   title[6] = "Job is done"
   text[6] = "It seems, all the transport ships were destroyed or went away: good job!"

   title[7] = "What a mess!"
   text[7] = "A pirate transport ship made it to %s: the militia is beaten."

   mil_broad = "Let's go, boys!"

   -- Mission details
   misn_title = "The Interception"
   misn_reward = "Safer lanes"
   misn_desc = "You were asked to counter a pirate raid on %s"
	
   -- OSD
	osd_title = "The Interception"
   osd_msg[1] = "Fly to the %s system and join the patrol."
   osd_msg[2] = "Destroy the pirate transport ships"
   osd_msg[3] = "Go back to %s"

   npc_desc = "Jimmy Sun"
   bar_desc = "This is Jimmy Sun, the chief of the Militia of Free Citizen."
end

function create ()

   sysname = "Daled"
   missys = system.get(sysname)

   if not misn.claim(missys) then
      misn.finish(false)
   end
   misn.setNPC(npc_desc, "neutral/thief2")
   misn.setDesc(bar_desc)

end

function accept()
   
   --Change here to change the planet and the system
   sysend = "Wahri"
   plaend = "Dono"
   ssys = system.get(sysend)
   spla = planet.get(plaend)
	
   tk.msg(title[1], text[1]:format(plaend))
   if tk.yesno(title[2], text[2]:format( plaend, planed )) then
      misn.accept()
      tk.msg(title[3], text[3]:format( missys, sysend ))
      
      misn.setTitle(misn_title)
      misn.setReward(misn_reward)
      misn.setDesc( misn_desc:format( sysend ) )
      osd_msg[1] = osd_msg[1]:format( missys )
      osd_msg[3] = osd_msg[3]:format( sysend )
      osd = misn.osdCreate(osd_title, osd_msg)
      misn.osdActive(1)

      jumpin = hook.enter("enter")
      stage = 0
      marker1 = misn.markerAdd(missys, "low")

      else
      tk.msg(refusetitle, refusetext)
      misn.finish(false)
   end
end

function enter()

   -- The player enters the system for the battle
   if system.cur() == missys then

      -- We need that in fleet form in the enemy spotting routine
      stand = faction.playerStanding( faction.get("Pirate") )
      if stand >= 0 then
         faction.setPlayerStanding( faction.get("Pirate"), -1 )
      end

      stage = 1
      pilot.toggleSpawn(false)
      pilot.clear()
      hook.rm(jumpin)
      misn.markerRm( marker1 )
      misn.osdActive(2)
      pirhook = hook.timer(5000, "piratesIn")
      militiaIn()
   end
end

function piratesIn() -- Spawn some pirates
   source_system = system.get("Haven")
   attackers = addShips({"Pirate Hyena", "Pirate Shark"}, nil, source_system, 3)
   attackers[7] = pilot.addRaw("Quicksilver", "pirate",source_system, "Pirate")[1]
   attackers[8] = pilot.addRaw("Quicksilver", "pirate",source_system, "Pirate")[1]

   attackers[7]:setHilight()
   attackers[8]:setHilight()

   quick1dh = hook.pilot ( attackers[7], "death", "quickd" )
   quick2dh = hook.pilot ( attackers[8], "death", "quickd" )
   quick1jh = hook.pilot ( attackers[7], "jump", "quickj" )
   quick2jh = hook.pilot ( attackers[8], "jump", "quickj" )
   number = 2  -- number of Quicksilvers left to kill

   for i, j in ipairs(attackers) do
      j:rename("Pirate Raider")
      j:setHostile()
   end

   atFleet = Forma:new(attackers, "wedge", 2000)
   atFleet:setTask( "hyperspace", ssys )
   atFleet.fleader:rename("Pirate Leader")
   atFleet.fleader:setVisplayer()
   atFleet.fleader:setHilight()
end

function militiaIn() -- Spawn some militia ships
   position = jump.get(system.cur(), ssys):pos()
   
   ships = {"Lancelot", "Shark", "Vendetta", "Soromid Brigand"}
   defenders = {}

   for i=1,5 do
      ship = ships[ rnd.rnd( #ships-1 ) + 1 ]
      pos = position + vec2.new(rnd.rnd(1000),rnd.rnd(1000))
      defenders[i] = pilot.addRaw(ship, "soromid",pos, "Militia")[1]
      defenders[i]:rename("Militia patrol")
   end

   defenders[1]:broadcast(mil_broad)

   defenders[6] = player.pilot()
   deFleet = Forma:new( defenders, "wedge", 4000, player.pilot() )

   for i=1,5 do
      -- I don't want the ships to be from the player's faction
      defenders[i]:setFaction("Militia")
   end
end

-- A Quicksilver died
function quickd()
   number = number-1
   if number == 0 then
      sysClear()
   end
end

-- A Quicksilver jumped away
function quickj( p, jp )
   -- A Quicksilver jumped to the protected system : that's bad
   if jp:dest() == ssys then
      tk.msg(title[7], text[7]:format( sysend ))
      var.push( "milDead", true ) -- The militia is dead
      clearMis(true)

      else
      quickd()
   end
end

-- All the Quicksilvers are destroyed
function sysClear()
   tk.msg(title[6], text[6])
   misn.osdActive(3)
   jumph = hook.jumpout("jumpOut")
   stage = 2
   marker2 = misn.markerAdd(ssys, "low")
end

-- Player jumps out from the battle system
function jumpOut()

   if stage == 1 then-- The battle isn't over
      tk.msg(title[4], text[4]:format( sysend ))
      var.push( "milDead", true ) -- The militia is dead
      clearMis(true)
   end

   if stage == 2 then-- The transports are dead
      if deFleet ~= nil then
         deFleet:disband()  -- Remove the fleets
      end
      if atFleet ~= nil then
         atFleet:disband()
      end

      hook.rm(jumph)
      landh = hook.land("endLand")
      -- If he was friend with the pirates, let's give him his standing back
      -- With a small penalty
      if stand >= 0 then
         faction.setPlayerStanding( faction.get("Pirate"), stand-10 )
      end
   end
end

function endLand()
   -- Player lands at Dono and gets congratuled
   if planet.cur() == spla then
      hook.rm(landh)
      tk.msg(title[5], text[5]:format( player.name() ))
      player.addOutfit("Fury Missile", 50)
      clearMis( true )
   end
end

-- Cleans everything and closes the mission
function clearMis( status )

   -- Systematically remove hooks
   hooks = { landh, jumph, quick1dh, quick2dh, quick1jh, quick2jh, jumpin }
   for _, j in ipairs( hooks ) do
      if j ~= nil then
         hook.rm(j)
      end
   end

   misn.osdDestroy()
   misn.finish(status)
end
