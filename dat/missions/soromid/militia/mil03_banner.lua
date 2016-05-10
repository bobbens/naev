--[[
   
   This is the fourth mission of the Militia's campaign. The player has to murder a corrupt soromid executive
   stages : 0 way to mission's system
            1 battle
            2 way back
--]]

include "dat/scripts/proximity.lua"

lang = naev.lang()
if lang == "es" then
   else -- default english
   title = {}
   text = {}
   osd_msg = {}
   
   title[1] = "A traitor"
   text[1] = [["Hello, %s. I have bad news: the pirate lord of Terminus decided to send a fleet in order to join the troops from New Haven and to do a massive attack against us. We need to prepare to face them, but the problem is that Dza'ton, the military governor of %s will probably let the militia alone against that foe. That's why I plan to... kill him." As you start to ask if it's really a good idea to kill an executive of the Soromid army, Sun answers: "Look, this man is a traitor to Sorom, because of him, dozen of honest traders and pilots were left to a certain death. He is the symbol of what is corrupt in the Soromid system. He is as bad as the pirates you killed until now. What's more, I have a plan that will let us kill him without having any problem with the Soromid authorities. So, are you in?"]]

   title[2] = "The plan"
   text[2] = [["Ok, so, the plan is to pretend that the pirates murdered him. He is coming back from a seminar in Cerberus with his Odium right now. You will intercept and kill him with a pirate ship in %s. So, good luck."]]
	
   refusetitle = "I'sorry, I can't"
   refusetext = [["Ok, so goodbye," says Sun.]]

   title[3] = "Good job"
   text[3] = [[You land and join Jimmy Sun in the militia's headquarters. "Ok," he says, "this one won't annoy us anymore. Now, we can focus on the preparation of the final battle against the pirates. By the way, a generous contributor gave us 20 laser turrets to equip our ships. I think, you deserve one."]]
	
   title[4] = "You ran away!"
   text[4] = [[Your mission failed.]]

   title[5] = "The target ran away!"
   text[5] = [[Your mission failed.]]

   title[6] = "Nice try"
   text[6] = [[You recive a call from Jimmy Sun: "Hey, remember, you need to use a pirate ship. Actually, you're lucky because Dza'ton was delayed. Come back later with a pirate ship."]]

   sorcomm = {}
   sorcomm[1] = "Hey, that pirate is attacking the governor!"
   sorcomm[2] = "Hostile element in range: engage at will."

   -- Mission details
   misn_title = "False Banner Operation"
   misn_reward = "Maybe lots of missiles again?"
   misn_desc = "The militia needs you to kill a corrupt Soromid executive."
	
   -- OSD
	osd_title = "False Banner Operation"
   osd_msg[1] = "Fly to %s with a pirate ship"
   osd_msg[2] = "Find and destroy Dza'ton's Odium"
   osd_msg[3] = "Go back to %s"

   npc_desc = "Jimmy Sun"
   bar_desc = "Jimmy Sun looks like someone who needs help from an experienced pilot."
end

function create ()

   sysname = "Octantis"
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
	
   if tk.yesno(title[1], text[1]:format( player.name(), plaend )) then
      misn.accept()
      tk.msg(title[2], text[2]:format( missys ))
      
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
      clearMis(false)
   end
end

function enter()

   -- The player enters the system for the battle
   if system.cur() == missys then

      --Check if the player uses a pirate ship
      playership = player.pilot():ship():name()
      if playership == "Hyena" or playership == "Pirate Shark" or playership == "Pirate Vendetta"
         or playership == "Pirate Ancestor" or playership == "Pirate Phalanx" or playership == "Pirate Admonisher"
         or playership == "Pirate Rhino" or playership == "Pirate Kestrel" then

         stage = 1
         pilot.toggleSpawn(false)
         pilot.clear()
         hook.rm(jumpin)
         misn.markerRm( marker1 )
         misn.osdActive(2)

         source = system.get( "Corvus" )
         dzaton = pilot.add( "Soromid Odium", nil, source )[1]

         -- The Odium must try to avoid the player
         dzaton:memory("careful", true)
         dzaton:rename("Dza'ton")

         -- To avoid faction loss
         dzaton:setFaction( "Thugs" )
         dzaton:setHostile()
         dzaton:setHilight()

         dzaton:control()
         dzaton:hyperspace( ssys )

         escort = {}
         escort[1] = pilot.add( "Soromid Brigand", nil, source )[1]
         escort[2] = pilot.add( "Soromid Brigand", nil, source )[1]
         escort[1]:memory("angle", 100)
         escort[2]:memory("angle", 200)

         for _, j in ipairs(escort) do
            j:setFaction( "Thugs" )
            j:setHostile()
            j:control()
            j:follow( dzaton, true )
         end

         proxh = hook.timer(500, "proximity", {anchor = dzaton, radius = 1500, funcname = "combat"})
         atth = hook.pilot( dzaton, "attacked", "odiumAttacked" )
         jphook = hook.pilot( dzaton, "jump", "getAway" )
         fleehook = hook.jumpout( "playerFlee" )
         diehook = hook.pilot( dzaton, "death", "youWon" )

         else
         tk.msg(title[6], text[6])
      end

   end
end

function odimuAttacked( p, attacker )
   hook.rm( atth )
   if proxh ~= nil then
      hook.rm( proxh )
   end

   if attacked == player.pilot() then  -- Who else?
      combat()
   end
end

-- The target escaped
function getAway()
   tk.msg(title[5], text[5])
   clearMis( false )
end

-- The player escaped
function playerFlee()
   tk.msg(title[4], text[4])
   clearMis( false )
end

-- Release the pilots
function combat()
   hook.rm( proxh )
   if proxh ~= nil then
      hook.rm( atth )
   end

   dzaton:control( false )
   for i, j in ipairs(escort) do
      j:control( false )
      j:broadcast( sorcomm[i] )
   end
end

-- The target is dead
function youWon()
   misn.osdActive(3)
   marker2 = misn.markerAdd(ssys, "low")
   stage = 2
   landhook = hook.land( "endLand" )
   hook.rm( fleehook )
end

function endLand()
   -- Player lands at Dono and gets congratuled
   if planet.cur() == spla then
      hook.rm(landhook)
      tk.msg(title[3], text[3])
      player.addOutfit("Laser Turret MK3")
      clearMis( true )
   end
end

-- Cleans everything and closes the mission
function clearMis( status )

   -- Systematically remove hooks
   hooks = { jumpin, landhook, fleehook, proxh, atth, jphook, diehook }
   for _, j in ipairs( hooks ) do
      if j ~= nil then
         hook.rm(j)
      end
   end

   misn.osdDestroy()
   misn.finish(status)
end
