--[[
-- In this event, a pilot proposes to the player to do the execution mission
-- From the militia string
--]]

function create ()
   if not evt.claim(system.cur()) then
      evt.finish()
   end

   gawain = pilot.add("Civilian Gawain", nil, true)[1]

   hook.pilot(gawain, "jump", "finish")
   hook.pilot(gawain, "death", "finish")
   hook.land("finish")
   hook.jumpout("finish")
    
   hailhook = hook.timer( 2000, "hailPlayer" );
end

-- Make the ship hail the player
function hailPlayer()
   gawain:hailPlayer()
   hook.pilot(gawain, "hail", "hail")
end

-- Triggered when the player hails the ship
function hail()
   naev.missionStart("The Execution")
   player.commClose()
   evt.finish(true)
end

function finish()
   hook.rm(hailhook)
   evt.finish()
end
