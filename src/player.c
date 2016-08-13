/*
 * See Licensing and Copyright notice in naev.h
 */

/**
 * @file player.c
 *
 * @brief Contains all the player related stuff.
 */


#include "player.h"

#include "naev.h"

#include <stdlib.h>

#include "nxml.h"
#include "pilot.h"
#include "log.h"
#include "opengl.h"
#include "font.h"
#include "ndata.h"
#include "space.h"
#include "rng.h"
#include "land.h"
#include "land_outfits.h"
#include "sound.h"
#include "economy.h"
#include "pause.h"
#include "menu.h"
#include "toolkit.h"
#include "dialogue.h"
#include "mission.h"
#include "nlua_misn.h"
#include "ntime.h"
#include "hook.h"
#include "map.h"
#include "map_overlay.h"
#include "nfile.h"
#include "spfx.h"
#include "unidiff.h"
#include "comm.h"
#include "intro.h"
#include "perlin.h"
#include "ai.h"
#include "music.h"
#include "gui.h"
#include "gui_omsg.h"
#include "nlua_var.h"
#include "escort.h"
#include "event.h"
#include "conf.h"
#include "nebula.h"
#include "equipment.h"
#include "camera.h"
#include "claim.h"
#include "player_gui.h"
#include "start.h"
#include "input.h"
#include "news.h"
#include "nstring.h"


/*
 * player stuff
 */
Player_t player; /**< Local player. */
static Ship* player_ship      = NULL; /**< Temporary ship to hold when naming it */
static credits_t player_creds = 0; /**< Temporary hack for when creating. */
static const char *player_message_noland = NULL; /**< No landing message (when PLAYER_NOLAND is set). */

/*
 * Licenses.
 */
static char **player_licenses = NULL; /**< Licenses player has. */
static int player_nlicenses   = 0; /**< Number of licenses player has. */


/*
 * player sounds.
 */
static int player_engine_group = -1; /**< Player engine sound group. */
static int player_hyper_group = -1; /**< Player hyperspace sound group. */
static int player_gui_group   = -1; /**< Player GUI sound group. */
int snd_target                = -1; /**< Sound when targeting. */
int snd_jump                  = -1; /**< Sound when can jump. */
int snd_nav                   = -1; /**< Sound when changing nav computer. */
int snd_hail                  = -1; /**< Sound when being hailed. */
/* Hyperspace sounds. */
int snd_hypPowUp              = -1; /**< Hyperspace power up sound. */
int snd_hypEng                = -1; /**< Hyperspace engine sound. */
int snd_hypPowDown            = -1; /**< Hyperspace power down sound. */
int snd_hypPowUpJump          = -1; /**< Hyperspace Power up to jump sound. */
int snd_hypJump               = -1; /**< Hyperspace jump sound. */
static int player_lastEngineSound = -1; /**< Last engine sound. */
static int player_hailCounter = 0; /**< Number of times to play the hail. */
static double player_hailTimer = 0.; /**< Timer for hailing. */


/*
 * player pilot stack - ships he has
 */
static PlayerShip_t* player_stack   = NULL;  /**< Stack of ships player has. */
static int player_nstack            = 0;     /**< Number of ships player has. */


/*
 * player outfit stack - outfits he has
 */
static PlayerOutfit_t *player_outfits  = NULL;  /**< Outfits player has. */
static int player_noutfits             = 0;     /**< Number of outfits player has. */
static int player_moutfits             = 0;     /**< Current allocated memory. */
#define OUTFIT_CHUNKSIZE               32       /**< Allocation chunk size. */


/*
 * player global properties
 */
/* used in input.c */
double player_left         = 0.; /**< Player left turn velocity from input. */
double player_right        = 0.; /**< Player right turn velocity from input. */
double player_acc          = 0.; /**< Accel velocity from input. */
/* for death and such */
static double player_timer = 0.; /**< For death and such. */


/*
 * unique mission stack.
 */
static int* missions_done  = NULL; /**< Saves position of completed missions. */
static int missions_mdone  = 0; /**< Memory size of completed missions. */
static int missions_ndone  = 0; /**< Number of completed missions. */


/*
 * unique event stack.
 */
static int* events_done  = NULL; /**< Saves position of completed events. */
static int events_mdone  = 0; /**< Memory size of completed events. */
static int events_ndone  = 0; /**< Number of completed events. */


/*
 * Extern stuff for player ships.
 */
extern Pilot** pilot_stack;
extern int pilot_nstack;


/*
 * map stuff for autonav
 */
extern int map_npath;


/*
 * prototypes
 */
/*
 * internal
 */
static void player_checkHail (void);
/* creation */
static void player_newSetup( int tutorial );
static int player_newMake (void);
static Pilot* player_newShipMake( const char* name );
/* sound */
static void player_initSound (void);
/* save/load */
static int player_saveEscorts( xmlTextWriterPtr writer );
static int player_saveShipSlot( xmlTextWriterPtr writer, PilotOutfitSlot *slot, int i );
static int player_saveShip( xmlTextWriterPtr writer,
      Pilot* ship, char* loc );
static Planet* player_parse( xmlNodePtr parent );
static int player_parseDoneMissions( xmlNodePtr parent );
static int player_parseDoneEvents( xmlNodePtr parent );
static int player_parseLicenses( xmlNodePtr parent );
static void player_parseShipSlot( xmlNodePtr node, Pilot *ship, PilotOutfitSlot *slot );
static int player_parseShip( xmlNodePtr parent, int is_player, char *planet );
static int player_parseEscorts( xmlNodePtr parent );
static void player_addOutfitToPilot( Pilot* pilot, Outfit* outfit, PilotOutfitSlot *s );
/* Misc. */
static int player_filterSuitablePlanet( Planet *p );
static void player_planetOutOfRangeMsg (void);
static int player_outfitCompare( const void *arg1, const void *arg2 );
static int player_thinkMouseFly(void);
static int preemption = 0; /* Hyperspace target/untarget preemption. */
/*
 * externed
 */
int player_save( xmlTextWriterPtr writer ); /* save.c */
Planet* player_load( xmlNodePtr parent ); /* save.c */


/**
 * @brief Initializes player stuff.
 */
int player_init (void)
{
   player_initSound();
   return 0;
}

/**
 * @brief Sets up a new player.
 */
static void player_newSetup( int tutorial )
{
   double x, y;

   /* Set up GUI. */
   gui_setDefaults();

   /* Setup sound */
   player_initSound();

   /* Clean up player stuff if we'll be recreating. */
   player_cleanup();

   /* For pretty background. */
   pilots_cleanAll();
   if (!tutorial) {
      space_init( start_system() );
      start_position( &x, &y );
   }
   else
      start_tutPosition( &x, &y );

   cam_setTargetPos( x, y, 0 );
   cam_setZoom( conf.zoom_far );

   /* Clear the init message for new game. */
   gui_clearMessages();
}


/**
 * @brief Creates a new tutorial player.
 *
 *   - Cleans up after old players.
 *   - Prompts for name.
 */
void player_newTutorial (void)
{
   int ret;
   double x, y;

   /* Set up new player. */
   player_newSetup( 1 );

   /* New name. */
   player.name = strdup( "John Doe" );

   /* Time. */
   ntime_set( start_date() );

   /* Welcome message - must be before space_init. */
   player_message( "\egWelcome to "APPNAME" Tutorial!" );
   player_message( "\eg v%s", naev_version(0) );

   /* Try to create the pilot, if fails reask for player name. */
   player_ship = ship_get( start_ship() );
   if (player_ship==NULL) {
      WARN("Ship not properly set by module.");
      return;
   }
   player_creds   = 0;
   player_newShipMake( "Star Voyager" );
   start_tutPosition( &x, &y );
   vect_cset( &player.p->solid->pos, x, y );
   vectnull( &player.p->solid->vel );
   player.p->solid->dir = RNGF() * 2.*M_PI;
   space_init( start_tutSystem() );

   /* Monies. */
   player.p->credits = start_credits();

   /* clear the map */
   map_clear();

   /* Start the economy. */
   economy_init();

   /* Start the news */
   news_init();

   /* Play music. */
   music_choose( "ambient" );

   /* Load the GUI. */
   gui_load( gui_pick() );

   /* It's the tutorial. */
   player_setFlag( PLAYER_TUTORIAL );

   /* Add the mission if found. */
   if (start_tutMission() != NULL) {
      ret = mission_start(start_tutMission(), NULL);
      if (ret < 0)
         WARN("Failed to run start tutorial mission '%s'.", start_tutMission());
   }

   /* Add the event if found. */
   if (!menu_isOpen(MENU_MAIN) && (start_tutEvent() != NULL)) {
      if (event_start( start_tutEvent(), NULL ))
         WARN("Failed to run start event '%s'.", start_tutEvent());
   }
}


/**
 * @brief Creates a new player.
 *
 *   - Cleans up after old players.
 *   - Prompts for name.
 *
 * @sa player_newMake
 */
void player_new (void)
{
   int r;

   /* Set up new player. */
   player_newSetup( 0 );

   /* Get the name. */
   player.name = dialogue_input( "Player Name", 2, 20,
         "Please write your name:" );

   /* Player cancelled dialogue. */
   if (player.name == NULL) {
      menu_main();
      return;
   }

   if (nfile_fileExists("%ssaves/%s.ns", nfile_dataPath(), player.name)) {
      r = dialogue_YesNo("Overwrite",
            "You already have a pilot named %s. Overwrite?",player.name);
      if (r==0) { /* no */
         player_new();
         return;
      }
   }

   if (player_newMake())
      return;

   /* Display the intro. */
   intro_display( "dat/intro", "intro" );

   /* Play music. */
   music_choose( "ambient" );

   /* Add the mission if found. */
   if (start_mission() != NULL) {
      if (mission_start(start_mission(), NULL) < 0)
         WARN("Failed to run start mission '%s'.", start_mission());
   }

   /* Add the event if found. */
   if (start_event() != NULL) {
      if (event_start( start_event(), NULL ))
         WARN("Failed to run start event '%s'.", start_event());
   }

   /* Run the load event trigger. */
   events_trigger( EVENT_TRIGGER_LOAD );

   /* Load the GUI. */
   gui_load( gui_pick() );
}


/**
 * @brief Actually creates a new player.
 *
 *    @return 0 on success.
 */
static int player_newMake (void)
{
   Ship *ship;
   const char *shipname;
   double x,y;

   /* Time. */
   ntime_set( start_date() );

   /* Welcome message - must be before space_init. */
   player_message( "\egWelcome to "APPNAME"!" );
   player_message( "\eg v%s", naev_version(0) );

   /* Try to create the pilot, if fails reask for player name. */
   ship = ship_get( start_ship() );
   shipname = start_shipname();
   if (ship==NULL) {
      WARN("Ship not properly set by module.");
      return -1;
   }
   /* Setting a default name in the XML prevents naming prompt. */
   if (player_newShip( ship, shipname, 0, (shipname==NULL) ? 0 : 1 ) == NULL) {
      player_new();
      return -1;
   }
   start_position( &x, &y );
   vect_cset( &player.p->solid->pos, x, y );
   vectnull( &player.p->solid->vel );
   player.p->solid->dir = RNGF() * 2.*M_PI;
   space_init( start_system() );

   /* Monies. */
   player.p->credits = start_credits();

   /* clear the map */
   map_clear();

   /* Start the economy. */
   economy_init();

   /* Start the news */
   news_init();

   return 0;
}


/**
 * @brief Creates a new ship for player.
 *
 *    @param ship New ship to get.
 *    @param def_name Default name to give it if cancelled.
 *    @param trade Whether or not to trade player's current ship with the new ship.
 *    @param noname Whether or not to let the player name it.
 *    @return Newly created pilot on success or NULL if dialogue was cancelled.
 *
 * @sa player_newShipMake
 */
Pilot* player_newShip( Ship* ship, const char *def_name,
      int trade, int noname )
{
   char *ship_name, *old_name;
   int i, len, w;
   Pilot *new_ship;

   /* temporary values while player doesn't exist */
   player_creds = (player.p != NULL) ? player.p->credits : 0;
   player_ship    = ship;
   if (!noname)
      ship_name      = dialogue_input( "Ship Name", 3, 20,
            "Please name your new ship:" );
   else
      ship_name      = NULL;

   /* Dialogue cancelled. */
   if (ship_name == NULL) {
      /* No default name, fail. */
      if (def_name == NULL)
         return NULL;

      /* Add default name. */
      i = 2;
      len = strlen(def_name)+10;
      ship_name = malloc( len );
      strncpy( ship_name, def_name, len );
      while (player_hasShip(ship_name)) {
         nsnprintf( ship_name, len, "%s %d", def_name, i );
         i++;
      }
   }

   /* Must not have same name. */
   if (player_hasShip(ship_name)) {
      dialogue_msg( "Name collision",
            "Please do not give the ship the same name as another of your ships.");
      return NULL;
   }

   new_ship = player_newShipMake( ship_name );

   /* Player is trading ship in. */
   if (trade) {
      if (player.p == NULL)
         ERR("Player ship isn't valid... This shouldn't happen!");
      old_name = player.p->name;
      player_swapShip( ship_name ); /* Move to the new ship. */
      player_rmShip( old_name );
   }

   free(ship_name);

   /* Update ship list if landed. */
   if (landed) {
      w = land_getWid( LAND_WINDOW_EQUIPMENT );
      equipment_regenLists( w, 0, 1 );
   }

   return new_ship;
}

/**
 * @brief Actually creates the new ship.
 */
static Pilot* player_newShipMake( const char* name )
{
   Vector2d vp, vv;
   PilotFlags flags;
   PlayerShip_t *ship;
   Pilot *new_pilot;
   double px, py, dir;
   unsigned int id;

   /* store the current ship if it exists */
   pilot_clearFlagsRaw( flags );
   pilot_setFlagRaw( flags, PILOT_PLAYER );

   /* in case we're respawning */
   player_rmFlag( PLAYER_CREATING );

   /* create the player */
   if (player.p == NULL) {
      /* Set position to defaults. */
      if (player.p != NULL) {
         px    = player.p->solid->pos.x;
         py    = player.p->solid->pos.y;
         dir   = player.p->solid->dir;
      }
      else {
         px    = 0.;
         py    = 0.;
         dir   = 0.;
      }
      vect_cset( &vp, px, py );
      vect_cset( &vv, 0., 0. );

      /* Create the player. */
      id = pilot_create( player_ship, name, faction_get("Player"), "player",
            dir, &vp, &vv, flags, -1 );
      cam_setTargetPilot( id, 0 );
      new_pilot = pilot_get( id );
   }
   else {
      /* Grow memory. */
      player_stack = realloc(player_stack, sizeof(PlayerShip_t)*(player_nstack+1));
      ship        = &player_stack[player_nstack];
      /* Create the ship. */
      ship->p     = pilot_createEmpty( player_ship, name, faction_get("Player"), "player", flags );
      ship->loc   = strdup( land_planet->name );
      player_nstack++;
      new_pilot   = ship->p;
   }

   if (player.p == NULL)
      ERR("Something seriously wonky went on, newly created player does not exist, bailing!");

   /* Add GUI. */
   player_guiAdd( player_ship->gui );

   /* money. */
   player.p->credits = player_creds;
   player_creds = 0;

   return new_pilot;
}


/**
 * @brief Swaps player's current ship with their ship named shipname.
 *
 *    @param shipname Ship to change to.
 */
void player_swapShip( char* shipname )
{
   int i, j;
   Pilot* ship;
   Vector2d v;
   double dir;

   for (i=0; i<player_nstack; i++) {
      if (strcmp(shipname,player_stack[i].p->name)!=0)
         continue;

      /* swap player and ship */
      ship = player_stack[i].p;

      /* move credits over */
      ship->credits = player.p->credits;

      /* move cargo over */
      pilot_cargoMove( ship, player.p );

      /* Store position. */
      v = player.p->solid->pos;
      dir = player.p->solid->dir;

      /* extra pass to calculate stats */
      pilot_calcStats( ship );
      pilot_calcStats( player.p );

      /* now swap the players */
      player_stack[i].p = player.p;
      for (j=0; j<pilot_nstack; j++) /* find pilot in stack to swap */
         if (pilot_stack[j] == player.p) {
            player.p         = ship;
            pilot_stack[j] = ship;
            break;
         }

      /* Copy position back. */
      player.p->solid->pos = v;
      player.p->solid->dir = dir;

      /* Fill the tank. */
      if (landed)
         land_refuel();

      /* Set some gui stuff. */
      gui_load( gui_pick() );

      /* Bind camera. */
      cam_setTargetPilot( player.p->id, 0 );
      return;
   }
   WARN( "Unable to swap player.p with ship '%s': ship does not exist!", shipname );
}


/**
 * @brief Calculates the price of one of the player's ships.
 *
 *    @param shipname Name of the ship.
 *    @return The price of the ship in credits.
 */
credits_t player_shipPrice( char* shipname )
{
   int i;
   Pilot *ship = NULL;

   if (strcmp(shipname,player.p->name)==0)
      ship = player.p;
   else {
      /* Find the ship. */
      for (i=0; i<player_nstack; i++) {
         if (strcmp(shipname,player_stack[i].p->name)==0) {
            ship = player_stack[i].p;
            break;
         }
      }
   }

   /* Not found. */
   if (ship == NULL) {
      WARN( "Unable to find price for player's ship '%s': ship does not exist!", shipname );
      return -1;
   }

   return pilot_worth( ship );
}


/**
 * @brief Removes one of the player's ships.
 *
 *    @param shipname Name of the ship to remove.
 */
void player_rmShip( char* shipname )
{
   int i, w;

   for (i=0; i<player_nstack; i++) {
      /* Not the ship we are looking for. */
      if (strcmp(shipname,player_stack[i].p->name)!=0)
         continue;

      /* Free player ship and location. */
      pilot_free(player_stack[i].p);
      free(player_stack[i].loc);

      /* Move memory to make adjacent. */
      memmove( player_stack+i, player_stack+i+1,
            sizeof(PlayerShip_t) * (player_nstack-i-1) );
      player_nstack--; /* Shrink stack. */
      /* Realloc memory to smaller size. */
      player_stack = realloc( player_stack,
            sizeof(PlayerShip_t) * (player_nstack) );
   }

   /* Update ship list if landed. */
   if (landed) {
      w = land_getWid( LAND_WINDOW_EQUIPMENT );
      equipment_regenLists( w, 0, 1 );
   }
}


/**
 * @brief Cleans up player stuff like player_stack.
 */
void player_cleanup (void)
{
   int i;

   /* Enable all input. */
   input_enableAll();

   /* Clean up other stuff. */
   diff_clear();
   var_cleanup();
   missions_cleanup();
   events_cleanup();
   space_clearKnown();
   land_cleanup();
   map_cleanup();

   /* Reset controls. */
   player_accelOver();
   player_left  = 0.;
   player_right = 0.;

   /* Clear player. */
   player_clear();

   /* Clear hail timer. */
   player_hailCounter   = 0;
   player_hailTimer     = 0.;

   /* Clear messages. */
   gui_clearMessages();

   /* Reset factions. */
   factions_reset();

   /* clean up name */
   if (player.name != NULL) {
      free(player.name);
      player.name = NULL;
   }

   /* Clean up gui. */
   gui_cleanup();
   player_guiCleanup();
   ovr_setOpen(0);

   /* clean up the stack */
   for (i=0; i<player_nstack; i++) {
      pilot_free(player_stack[i].p);
      free(player_stack[i].loc);
   }
   if (player_stack != NULL)
      free(player_stack);
   player_stack = NULL;
   /* nothing left */
   player_nstack = 0;

   /* Free outfits. */
   if (player_outfits != NULL)
      free(player_outfits);
   player_outfits  = NULL;
   player_noutfits = 0;
   player_moutfits = 0;

   /* Clean up missions */
   if (missions_done != NULL)
      free(missions_done);
   missions_done = NULL;
   missions_ndone = 0;
   missions_mdone = 0;

   /* Clean up events. */
   if (events_done != NULL)
      free(events_done);
   events_done = NULL;
   events_ndone = 0;
   events_mdone = 0;

   /* Clean up licenses. */
   if (player_nlicenses > 0) {
      for (i=0; i<player_nlicenses; i++)
         free(player_licenses[i]);
      free(player_licenses);
      player_licenses = NULL;
      player_nlicenses = 0;
   }

   /* Clear claims. */
   claim_clear();

   /* just in case purge the pilot stack */
   pilots_cleanAll();

   /* Reset some player stuff. */
   player_creds   = 0;
   player.crating = 0;
   if (player.gui != NULL)
      free( player.gui );
   player.gui = NULL;

   /* Clear omsg. */
   omsg_cleanup();

   /* Stop the sounds. */
   sound_stopAll();

   /* Reset time compression. */
   pause_setSpeed( 1.0 );

   /* Clean up. */
   memset( &player, 0, sizeof(Player_t) );
   player_setFlag(PLAYER_CREATING);
}


static int player_soundReserved = 0; /**< Has the player already reserved sound? */
/**
 * @brief Initializes the player sounds.
 */
static void player_initSound (void)
{
   if (player_soundReserved)
      return;

   /* Allocate channels. */
   player_engine_group  = sound_createGroup(1); /* Channel for engine noises. */
   player_gui_group     = sound_createGroup(4);
   player_hyper_group   = sound_createGroup(4);
   sound_speedGroup( player_gui_group, 0 ); /* Disable pitch shift. */
   player_soundReserved = 1;

   /* Get sounds. */
   snd_target           = sound_get("target");
   snd_jump             = sound_get("jump");
   snd_nav              = sound_get("nav");
   snd_hail             = sound_get("hail");
   snd_hypPowUp         = sound_get("hyperspace_powerup");
   snd_hypEng           = sound_get("hyperspace_engine");
   snd_hypPowDown       = sound_get("hyperspace_powerdown");
   snd_hypPowUpJump     = sound_get("hyperspace_powerupjump");
   snd_hypJump          = sound_get("hyperspace_jump");
}


/**
 * @brief Plays a GUI sound (unaffected by time accel).
 *
 *    @param sound ID of the sound to play.
 *    @param once Play only once?
 */
void player_soundPlayGUI( int sound, int once )
{
   sound_playGroup( player_gui_group, sound, once );
}


/**
 * @brief Plays a sound at the player.
 *
 *    @param sound ID of the sound to play.
 *    @param once Play only once?
 */
void player_soundPlay( int sound, int once )
{
   sound_playGroup( player_hyper_group, sound, once );
}


/**
 * @brief Stops playing player sounds.
 */
void player_soundStop (void)
{
   if (player_gui_group >= 0)
      sound_stopGroup( player_gui_group );
   if (player_engine_group >= 0)
      sound_stopGroup( player_engine_group );
   if (player_hyper_group >= 0)
      sound_stopGroup( player_hyper_group );

   /* No last engine sound. */
   player_lastEngineSound = -1;
}


/**
 * @brief Pauses the ship's sounds.
 */
void player_soundPause (void)
{
   if (player_engine_group >= 0)
      sound_pauseGroup(player_engine_group);
   if (player_hyper_group >= 0)
      sound_pauseGroup(player_hyper_group);
}


/**
 * @brief Resumes the ship's sounds.
 */
void player_soundResume (void)
{
   if (player_engine_group >= 0)
      sound_resumeGroup(player_engine_group);
   if (player_hyper_group >= 0)
      sound_resumeGroup(player_hyper_group);
}


/**
 * @brief Warps the player to the new position
 *
 *    @param x X value of the position to warp to.
 *    @param y Y value of the position to warp to.
 */
void player_warp( const double x, const double y )
{
   vect_cset( &player.p->solid->pos, x, y );
}


/**
 * @brief Clears the targets.
 */
void player_clear (void)
{
   if (player.p != NULL) {
      pilot_setTarget( player.p, player.p->id );
      gui_setTarget();
   }

   /* Clear the noland flag. */
   player_rmFlag( PLAYER_NOLAND );
}


static char* player_ratings[] = {
      "Harmless",
      "Mostly Harmless",
      "Smallfry",
      "Average",
      "Above Average",
      "Major",
      "Intimidating",
      "Fearsome",
      "Terrifying",
      "Unstoppable",
      "Godlike"
}; /**< Combat ratings. */
/**
 * @brief Gets the player's combat rating in a human-readable string.
 *
 *    @return The player's combat rating in a human readable string.
 */
const char* player_rating (void)
{
   if (player.crating == 0.) return player_ratings[0];
   else if (player.crating < 25.) return player_ratings[1];
   else if (player.crating < 50.) return player_ratings[2];
   else if (player.crating < 100.) return player_ratings[3];
   else if (player.crating < 200.) return player_ratings[4];
   else if (player.crating < 500.) return player_ratings[5];
   else if (player.crating < 1000.) return player_ratings[6];
   else if (player.crating < 2000.) return player_ratings[7];
   else if (player.crating < 5000.) return player_ratings[8];
   else if (player.crating < 10000.) return player_ratings[9];
   else return player_ratings[10];
}


/**
 * @brief Checks to see if the player has enough credits.
 *
 *    @param amount Amount of credits to check to see if the player has.
 *    @return 1 if the player has enough credits.
 */
int player_hasCredits( credits_t amount )
{
   return pilot_hasCredits( player.p, amount );
}


/**
 * @brief Modifies the amount of credits the player has.
 *
 *    @param amount Quantity to modify player's credits by.
 *    @return Amount of credits the player has.
 */
credits_t player_modCredits( credits_t amount )
{
   return pilot_modCredits( player.p, amount );
}


/**
 * @brief Renders the player
 */
void player_render( double dt )
{
   /*
    * Check to see if the death menu should pop up.
    */
   if (player_isFlag(PLAYER_DESTROYED)) {
      player_timer -= dt;
      if (!toolkit_isOpen() && !player_isFlag(PLAYER_CREATING) &&
            (player_timer < 0.))
         menu_death();
   }

   /*
    * Render the player.
    */
   if ((player.p != NULL) && !player_isFlag(PLAYER_CREATING) &&
         !pilot_isFlag( player.p, PILOT_INVISIBLE))
      pilot_render(player.p, dt);
}


/**
 * @brief Basically uses keyboard input instead of AI input. Used in pilot.c.
 *
 *    @param pplayer Player to think.
 */
void player_think( Pilot* pplayer, const double dt )
{
   (void) dt;
   Pilot *target;
   double turn;
   int facing, fired;

   /* last i heard, the dead don't think */
   if (pilot_isFlag(pplayer,PILOT_DEAD)) {
      /* no sense in accelerating or turning */
      pilot_setThrust( pplayer, 0. );
      pilot_setTurn( pplayer, 0. );
      return;
   }

   /* Under manual control is special. */
   if (pilot_isFlag( pplayer, PILOT_MANUAL_CONTROL )) {
      ai_think( pplayer, dt );
      return;
   }

   /* Not facing anything yet. */
   facing = 0;

   /* Autonav takes over normal controls. */
   if (player_isFlag(PLAYER_AUTONAV)) {
      player_thinkAutonav( pplayer, dt );

      /* Disable turning. */
      facing = 1;
   }

   /* Mouse-flying is enabled. */
   if (!facing && player_isFlag(PLAYER_MFLY))
      facing = player_thinkMouseFly();

   /* turning taken over by PLAYER_FACE */
   if (!facing && player_isFlag(PLAYER_FACE)) {
      /* Try to face pilot target. */
      if (player.p->target != PLAYER_ID) {
         target = pilot_get(player.p->target);
         if (target != NULL) {
            pilot_face( pplayer,
                  vect_angle( &player.p->solid->pos, &target->solid->pos ));

            /* Disable turning. */
            facing = 1;
         }
      }
      /* If not try to face planet target. */
      else if ((player.p->nav_planet != -1) && ((preemption == 0) || (player.p->nav_hyperspace == -1))) {
         pilot_face( pplayer,
               vect_angle( &player.p->solid->pos,
                  &cur_system->planets[ player.p->nav_planet ]->pos ));
         /* Disable turning. */
         facing = 1;
      }
      else if (player.p->nav_hyperspace != -1) {
         pilot_face( pplayer,
               vect_angle( &player.p->solid->pos,
                  &cur_system->jumps[ player.p->nav_hyperspace ].pos ));
         /* Disable turning. */
         facing = 1;
      }
   }

   /* turning taken over by PLAYER_REVERSE */
   if (player_isFlag(PLAYER_REVERSE)) {

      /* Check to see if already stopped. */
      /*
      if (VMOD(pplayer->solid->vel) < MIN_VEL_ERR)
         player_accel( 0. );

      else {
         d = pilot_face( pplayer, VANGLE(player.p->solid->vel) + M_PI );
         if ((player_acc < 1.) && (d < MAX_DIR_ERR))
            player_accel( 1. );
      }
      */

      /*
       * If the player has reverse thrusters, fire those.
       */
      if (player.p->stats.misc_reverse_thrust)
         player_accel( -PILOT_REVERSE_THRUST );
      else if (!facing){
         pilot_face( pplayer, VANGLE(player.p->solid->vel) + M_PI );
         /* Disable turning. */
         facing = 1;
      }
   }

   /* normal turning scheme */
   if (!facing) {
      turn = 0;
      if (player_isFlag(PLAYER_TURN_LEFT))
         turn -= player_left;
      if (player_isFlag(PLAYER_TURN_RIGHT))
         turn += player_right;
      turn = CLAMP( -1., 1., turn );
      pilot_setTurn( pplayer, -turn );
   }

   /*
    * Weapon shooting stuff
    */
   fired = 0;

   /* Primary weapon. */
   if (player_isFlag(PLAYER_PRIMARY)) {
      fired |= pilot_shoot( pplayer, 0 );
      player_setFlag(PLAYER_PRIMARY_L);
   }
   else if (player_isFlag(PLAYER_PRIMARY_L)) {
      pilot_shootStop( pplayer, 0 );
      player_rmFlag(PLAYER_PRIMARY_L);
   }
   /* Secondary weapon - we use PLAYER_SECONDARY_L to track last frame. */
   if (player_isFlag(PLAYER_SECONDARY)) { /* needs target */
      fired |= pilot_shoot( pplayer, 1 );
      player_setFlag(PLAYER_SECONDARY_L);
   }
   else if (player_isFlag(PLAYER_SECONDARY_L)) {
      pilot_shootStop( pplayer, 1 );
      player_rmFlag(PLAYER_SECONDARY_L);
   }

   if (fired) {
      player.autonav_timer = MAX( player.autonav_timer, 1. );
      player_autonavResetSpeed();
   }

   pilot_setThrust( pplayer, player_acc );
}


/**
 * @brief Player update function.
 *
 *    @param pplayer Player to update.
 *    @param dt Current deltatick.
 */
void player_update( Pilot *pplayer, const double dt )
{
   /* Update normally. */
   pilot_update( pplayer, dt );

   /* Update player.p specific stuff. */
   if (!player_isFlag(PLAYER_DESTROYED))
      player_updateSpecific( pplayer, dt );
}


/**
 * @brief Does a player specific update.
 *
 *    @param pplayer Player to update.
 *    @param dt Current deltatick.
 */
void player_updateSpecific( Pilot *pplayer, const double dt )
{
   int engsound;

   /* Calculate engine sound to use. */
   if (pilot_isFlag(pplayer, PILOT_AFTERBURNER))
      engsound = pplayer->afterburner->outfit->u.afb.sound;
   else if ((pplayer->solid->thrust > 1e-3) || (pplayer->solid->thrust < -1e-3)) {
      /* See if is in hyperspace. */
      if (pilot_isFlag(pplayer, PILOT_HYPERSPACE))
         engsound = snd_hypEng;
      else
         engsound = pplayer->ship->sound;
   }
   else
      engsound = -1;
   /* See if sound must change. */
   if (player_lastEngineSound != engsound) {
      sound_stopGroup( player_engine_group );
      if (engsound >= 0)
         sound_playGroup( player_engine_group, engsound, 0 );
   }
   player_lastEngineSound = engsound;

   /* Sound. */
   /*
    * Sound is now camera-specific and thus not player specific. A bit sad really.
   sound_updateListener( pplayer->solid->dir,
         pplayer->solid->pos.x, pplayer->solid->pos.y,
         pplayer->solid->vel.x, pplayer->solid->vel.y );
   */

   /* See if must play hail sound. */
   if (player_hailCounter > 0) {
      player_hailTimer -= dt;
      if (player_hailTimer < 0.) {
         player_soundPlayGUI( snd_hail, 1 );
         player_hailCounter--;
         player_hailTimer = 3.;
      }
   }
}


/*
 *    For use in keybindings
 */
/**
 * @brief Activates a player's weapon set.
 */
void player_weapSetPress( int id, int type, int repeat )
{
   if (repeat)
      return;

   if ((type > 0) && ((player.p == NULL) || toolkit_isOpen()))
      return;

   if (player.p == NULL)
      return;

   if (pilot_isFlag(player.p, PILOT_HYP_PREP) ||
         pilot_isFlag(player.p, PILOT_HYPERSPACE) ||
         pilot_isFlag(player.p, PILOT_LANDING) ||
         pilot_isFlag(player.p, PILOT_TAKEOFF))
      return;

   pilot_weapSetPress( player.p, id, type );
}


/**
 * @brief Aborts autonav and other states that take control of the ship.
 *
 *    @param reason Reason for aborting (see player.h)
 *    @param str String accompanying the reason.
 */
void player_restoreControl( int reason, char *str )
{
   if (player.p==NULL)
      return;

   if (reason != PINPUT_AUTONAV) {
      /* Autonav should be harder to abort when paused. */
      if (!paused || reason != PINPUT_MOVEMENT)
         player_autonavAbort(str);
   }

   if (reason != PINPUT_BRAKING) {
      pilot_rmFlag(player.p, PILOT_BRAKING);
      pilot_rmFlag(player.p, PILOT_COOLDOWN_BRAKE);
      if (pilot_isFlag(player.p, PILOT_COOLDOWN))
         pilot_cooldownEnd(player.p, str);
   }
}


/**
 * @brief Sets the player's target planet.
 *
 *    @param id Target planet or -1 if none should be selected.
 */
void player_targetPlanetSet( int id )
{
   int old;

   if (id >= cur_system->nplanets) {
      WARN("Trying to set player's planet target to invalid ID '%d'", id);
      return;
   }

   if ((player.p == NULL) || pilot_isFlag( player.p, PILOT_LANDING ))
      return;

   old = player.p->nav_planet;
   player.p->nav_planet = id;
   player_hyperspacePreempt((id < 0) ? 1 : 0);
   if (old != id) {
      player_rmFlag(PLAYER_LANDACK);
      if (id >= 0)
         player_soundPlayGUI(snd_nav, 1);
   }
   gui_forceBlink();
   gui_setNav();
}


/**
 * @brief Cycle through planet targets.
 */
void player_targetPlanet (void)
{
   int id, i;

   /* Not under manual control. */
   if (pilot_isFlag( player.p, PILOT_MANUAL_CONTROL ))
      return;

   /* Find next planet target. */
   for (id=player.p->nav_planet+1; id<cur_system->nplanets; id++)
      if (planet_isKnown( cur_system->planets[id] ))
         break;

   /* Try to select the lowest-indexed valid planet. */
   if (id >= cur_system->nplanets ) {
      id = -1;
      for (i=0; i<cur_system->nplanets; i++)
         if (planet_isKnown( cur_system->planets[i] )) {
            id = i;
            break;
         }
   }

   /* Untarget if out of range. */
   player_targetPlanetSet( id );
}


/**
 * @brief Try to land or target closest planet if no land target.
 */
void player_land (void)
{
   int i;
   int tp, silent;
   double td, d;
   Planet *planet;

   silent = 0; /* Whether to suppress the land ack noise. */

   if (landed) { /* player is already landed */
      takeoff(1);
      return;
   }

   /* Not under manual control or disabled. */
   if (pilot_isFlag( player.p, PILOT_MANUAL_CONTROL ) ||
         pilot_isDisabled(player.p))
      return;

   /* Already landing. */
   if ((pilot_isFlag( player.p, PILOT_LANDING) ||
         pilot_isFlag( player.p, PILOT_TAKEOFF)))
      return;

   /* Check if there are planets to land on. */
   if (cur_system->nplanets == 0) {
      player_messageRaw( "\erThere are no planets to land on." );
      return;
   }

   if (player.p->nav_planet == -1) { /* get nearest planet target */
      td = -1; /* temporary distance */
      tp = -1; /* temporary planet */
      for (i=0; i<cur_system->nplanets; i++) {
         planet = cur_system->planets[i];
         d = vect_dist(&player.p->solid->pos,&planet->pos);
         if (pilot_inRangePlanet( player.p, i ) &&
               planet_hasService(planet,PLANET_SERVICE_LAND) &&
               ((tp==-1) || ((td == -1) || (td > d)))) {
            tp = i;
            td = d;
         }
      }
      player_targetPlanetSet( tp );
      player_hyperspacePreempt(0);

      /* no landable planet */
      if (player.p->nav_planet < 0)
         return;

      silent = 1; /* Suppress further targeting noises. */
   }
   /*check if planet is in range*/
   else if (!pilot_inRangePlanet( player.p, player.p->nav_planet)) {
      player_planetOutOfRangeMsg();
      return;
   }

   if (player_isFlag(PLAYER_NOLAND)) {
      player_message( "\er%s", player_message_noland );
      return;
   }
   else if (pilot_isFlag( player.p, PILOT_NOLAND)) {
      player_message( "\erDocking stabilizers malfunctioning, cannot land." );
      return;
   }

   /* attempt to land at selected planet */
   planet = cur_system->planets[player.p->nav_planet];
   if (!planet_hasService(planet, PLANET_SERVICE_LAND)) {
      player_messageRaw( "\erYou can't land here." );
      return;
   }
   else if (!player_isFlag(PLAYER_LANDACK)) { /* no landing authorization */
      if (planet_hasService(planet,PLANET_SERVICE_INHABITED)) { /* Basic services */
         if (planet->can_land || (planet->land_override > 0))
            player_message( "\e%c%s>\e0 %s", planet_getColourChar(planet),
                  planet->name, planet->land_msg );
         else if (planet->bribed && (planet->land_override >= 0))
            player_message( "\e%c%s>\e0 %s", planet_getColourChar(planet),
                  planet->name, planet->bribe_ack_msg );
         else { /* Hostile */
            player_message( "\e%c%s>\e0 %s", planet_getColourChar(planet),
                  planet->name, planet->land_msg );
            return;
         }
      }
      else /* No shoes, no shirt, no lifeforms, no service. */
         player_message( "\epReady to land on %s.", planet->name );

      player_setFlag(PLAYER_LANDACK);
      if (!silent)
         player_soundPlayGUI(snd_nav, 1);

      return;
   }
   else if (vect_dist2(&player.p->solid->pos,&planet->pos) > pow2(planet->radius)) {
      player_message("\erYou are too far away to land on %s.", planet->name);
      return;
   }
   else if ((pow2(VX(player.p->solid->vel)) + pow2(VY(player.p->solid->vel))) >
         (double)pow2(MAX_HYPERSPACE_VEL)) {
      player_message("\erYou are going too fast to land on %s.", planet->name);
      return;
   }

   /* Abort autonav. */
   player_restoreControl(0, NULL);

   /* Stop afterburning. */
   pilot_afterburnOver( player.p );
   /* Stop accelerating. */
   player_accelOver();

   /* Stop all on outfits. */
   if (pilot_outfitOffAll( player.p ) > 0)
      pilot_calcStats( player.p );

   /* Start landing. */
   player_soundPause();
   player.p->ptimer = PILOT_LANDING_DELAY;
   pilot_setFlag( player.p, PILOT_LANDING );
   pilot_setThrust( player.p, 0. );
   pilot_setTurn( player.p, 0. );
}


/**
 * @brief Revokes landing authorization if the player's reputation is too low.
 */
void player_checkLandAck( void )
{
   Planet *p;

   /* No authorization to revoke. */
   if ((player.p == NULL) || !player_isFlag(PLAYER_LANDACK))
      return;

   /* Avoid a potential crash if PLAYER_LANDACK is set inappropriately. */
   if (player.p->nav_planet < 0) {
      WARN("Player has landing permission, but no valid planet targeted.");
      return;
   }

   p = cur_system->planets[ player.p->nav_planet ];

   /* Player can still land. */
   if (p->can_land || (p->land_override > 0) || p->bribed)
      return;

   player_rmFlag(PLAYER_LANDACK);
   player_message( "\e%c%s>\e0 Landing permission revoked.",
         planet_getColourChar(p), p->name );
}


/**
 * @brief Checks whether the player's ship is able to takeoff.
 */
int player_canTakeoff(void)
{
   return !pilot_checkSpaceworthy(player.p);
}


/**
 * @brief Sets the no land message.
 *
 *    @brief str Message to set when the player is not allowed to land temporarily.
 */
void player_nolandMsg( const char *str )
{
   if (str != NULL)
      player_message_noland = str;
   else
      player_message_noland = "You are not allowed to land at this moment.";
}


/**
 * @brief Sets the player's hyperspace target.
 *
 *    @param id ID of the hyperspace target.
 */
void player_targetHyperspaceSet( int id )
{
   int old;

   if (id >= cur_system->njumps) {
      WARN("Trying to set player's hyperspace target to invalid ID '%d'", id);
      return;
   }

   if (pilot_isFlag(player.p, PILOT_HYP_PREP) ||
         pilot_isFlag(player.p, PILOT_HYP_BEGIN) ||
         pilot_isFlag(player.p, PILOT_HYPERSPACE))
      return;


   old = player.p->nav_hyperspace;
   player.p->nav_hyperspace = id;
   player_hyperspacePreempt((id < 0) ? 0 : 1);
   if ((old != id) && (id >= 0))
      player_soundPlayGUI(snd_nav,1);
   gui_setNav();
}


/**
 * @brief Gets a hyperspace target.
 */
void player_targetHyperspace (void)
{
   int id, i;

   /* Not under manual control. */
   if (pilot_isFlag( player.p, PILOT_MANUAL_CONTROL ))
      return;

   map_clear(); /* clear the current map path */

   for (id=player.p->nav_hyperspace+1; id<cur_system->njumps; id++)
      if (jp_isKnown( &cur_system->jumps[id]))
         break;

   /* Try to find the lowest-indexed valid jump. */
   if (id >= cur_system->njumps) {
      id = -1;
      for (i=0; i<cur_system->njumps; i++)
         if (jp_isUsable( &cur_system->jumps[i])) {
            id = i;
            break;
         }
   }

   player_targetHyperspaceSet( id );

   /* Map gets special treatment if open. */
   if (id == -1)
      map_select( NULL , 0);
   else
      map_select( cur_system->jumps[ id ].target, 0 );
}


/**
 * @brief Enables or disables jump points preempting planets in autoface and target clearing.
 *
 *    @param preempt Boolean; 1 preempts planet target.
 */
void player_hyperspacePreempt( int preempt )
{
   preemption = preempt;
}


/**
 * @brief Returns whether the jump point target should preempt the planet target.
 *
 *    @return Boolean; 1 preempts planet target.
 */
int player_getHypPreempt(void)
{
   return preemption;
}


/**
 * @brief Starts the hail sounds and aborts autoNav
 */
void player_hailStart (void)
{
   player_hailCounter = 5;

   /* Abort autonav. */
   player_messageRaw("\erReceiving hail!");
   player_autonavResetSpeed();
   player.autonav_timer = MAX( player.autonav_timer, 10. );
}


/**
 * @brief Actually attempts to jump in hyperspace.
 *
 *    @return 1 if actually started a jump, 0 otherwise.
 */
int player_jump (void)
{
   int i, j;
   double dist, mindist;

   /* Must have a jump target and not be already jumping. */
   if (pilot_isFlag(player.p, PILOT_HYPERSPACE))
      return 0;

   /* Not under manual control or disabled. */
   if (pilot_isFlag( player.p, PILOT_MANUAL_CONTROL ) ||
         pilot_isDisabled(player.p))
      return 0;

   /* Select nearest jump if not target. */
   if (player.p->nav_hyperspace == -1) {
      j        = -1;
      mindist  = INFINITY;
      for (i=0; i<cur_system->njumps; i++) {
         dist = vect_dist2( &player.p->solid->pos, &cur_system->jumps[i].pos );
         if (dist < mindist && jp_isUsable(&cur_system->jumps[i])) {
            mindist  = dist;
            j        = i;
         }
      }
      if (j  < 0)
         return 0;

      player.p->nav_hyperspace = j;
      player_soundPlayGUI(snd_nav,1);
      map_select( cur_system->jumps[player.p->nav_hyperspace].target, 0 );
      gui_setNav();

      /* Only follow through if within range. */
      if (mindist > pow2( cur_system->jumps[j].radius ))
         return 0;
   }

   /* Already jumping, so we break jump. */
   if (pilot_isFlag(player.p, PILOT_HYP_PREP)) {
      pilot_hyperspaceAbort(player.p);
      player_message("\erAborting hyperspace sequence.");
      return 0;
   }

   /* Try to hyperspace. */
   i = space_hyperspace(player.p);
   if (i == -1)
      player_message("\erYou are too far from a jump point to initiate hyperspace.");
   else if (i == -2)
      player_message("\erHyperspace drive is offline.");
   else if (i == -3)
      player_message("\erYou do not have enough fuel to hyperspace jump.");
   else {
      player_message("\epPreparing for hyperspace.");
      /* Stop acceleration noise. */
      player_accelOver();
      /* Stop possible shooting. */
      pilot_shootStop( player.p, 0 );
      pilot_shootStop( player.p, 1 );
      return 1;
   }
   return 0;
}

/**
 * @brief Player actually broke hyperspace (entering new system).
 */
void player_brokeHyperspace (void)
{
   ntime_t t;
   StarSystem *sys;
   JumpPoint *jp;

   /* First run jump hook. */
   hooks_run( "jumpout" );

   /* Prevent targeted planet # from carrying over. */
   gui_setNav();
   gui_setTarget();
   player_targetPlanetSet( -1 );

   /* calculates the time it takes, call before space_init */
   t  = pilot_hyperspaceDelay( player.p );
   ntime_inc( t );

   /* Save old system. */
   sys = cur_system;

   /* Free old graphics. */
   space_gfxUnload( sys );

   /* enter the new system */
   jp = &cur_system->jumps[player.p->nav_hyperspace];
   space_init( jp->target->name );

   /* set position, the pilot_update will handle lowering vel */
   space_calcJumpInPos( cur_system, sys, &player.p->solid->pos, &player.p->solid->vel, &player.p->solid->dir );
   cam_setTargetPilot( player.p->id, 0 );

   /* reduce fuel */
   player.p->fuel -= player.p->fuel_consumption;

   /* stop hyperspace */
   pilot_rmFlag( player.p, PILOT_HYPERSPACE );
   pilot_rmFlag( player.p, PILOT_HYP_BEGIN );
   pilot_rmFlag( player.p, PILOT_HYP_BRAKE );
   pilot_rmFlag( player.p, PILOT_HYP_PREP );

   /* Update the map */
   map_jump();

   /* Add the escorts. */
   player_addEscorts();

   /* Disable autonavigation if arrived. */
   if (player_isFlag(PLAYER_AUTONAV)) {
      if (player.p->nav_hyperspace == -1) {
         player_message( "\epAutonav arrived at the %s system.", cur_system->name);
         player_autonavEnd();
      }
      else {
         player_message( "\epAutonav continuing until destination (%d jump%s left).",
               map_npath, (map_npath==1) ? "" : "s" );
      }
   }

   /* Safe since this is run in the player hook section. */
   hooks_run( "jumpin" );
   hooks_run( "enter" );
   events_trigger( EVENT_TRIGGER_ENTER );
   missions_run( MIS_AVAIL_SPACE, -1, NULL, NULL );

   /* Player sound. */
   player_soundPlay( snd_hypJump, 1 );
}


/**
 * @brief Start accelerating.
 *
 *    @param acc How much thrust should be applied of maximum (0 - 1).
 */
void player_accel( double acc )
{
   if ((player.p == NULL) || pilot_isFlag(player.p, PILOT_HYP_PREP) ||
         pilot_isFlag(player.p, PILOT_HYPERSPACE))
      return;


   player_acc = acc;
   if (toolkit_isOpen() || paused)
      player_soundPause();
}


/**
 * @brief Done accelerating.
 */
void player_accelOver (void)
{
   player_acc = 0.;
}


/**
 * @brief Sets the player's target.
 *
 *    @param id Target to set for the player.
 */
void player_targetSet( unsigned int id )
{
   unsigned int old;
   old = player.p->target;
   pilot_setTarget( player.p, id );
   if ((old != id) && (player.p->target != PLAYER_ID)) {
      gui_forceBlink();
      player_soundPlayGUI( snd_target, 1 );
   }
   gui_setTarget();
}


/**
 * @brief Targets the nearest hostile enemy to the player.
 *
 * @note This function largely duplicates pilot_getNearestEnemy, because the
 *       player's hostility with AIs is more nuanced than AI vs AI.
 */
void player_targetHostile (void)
{
   unsigned int tp;
   int i;
   double d, td;

   tp = PLAYER_ID;
   d  = 0;
   for (i=0; i<pilot_nstack; i++) {
      /* Don't get if is bribed. */
      if (pilot_isFlag(pilot_stack[i],PILOT_BRIBED))
         continue;

      /* Shouldn't be disabled. */
      if (pilot_isDisabled(pilot_stack[i]))
         continue;

      /* Must be a valid target. */
      if (!pilot_validTarget( player.p, pilot_stack[i] ))
         continue;

      /* Normal unbribed check. */
      if (pilot_isHostile(pilot_stack[i])) {
         td = vect_dist2(&pilot_stack[i]->solid->pos, &player.p->solid->pos);
         if ((tp==PLAYER_ID) || (td < d)) {
            d  = td;
            tp = pilot_stack[i]->id;
         }
      }
   }

   player_targetSet( tp );
}


/**
 * @brief Cycles to next target.
 *
 *    @param mode Mode to target. 0 is normal, 1 is hostiles.
 */
void player_targetNext( int mode )
{
   player_targetSet( pilot_getNextID(player.p->target, mode) );
}


/**
 * @brief Cycles to previous target.
 *
 *    @param mode Mode to target. 0 is normal, 1 is hostiles.
 */
void player_targetPrev( int mode )
{
   player_targetSet( pilot_getPrevID(player.p->target, mode) );
}


/**
 * @brief Clears the player's ship, planet or hyperspace target, in that order.
 */
void player_targetClear (void)
{
   gui_forceBlink();
   if (player.p->target == PLAYER_ID && (preemption == 1 || player.p->nav_planet == -1)
         && !pilot_isFlag(player.p, PILOT_HYP_PREP)) {
      player.p->nav_hyperspace = -1;
      player_hyperspacePreempt(0);
      map_clear();
   }
   else if (player.p->target == PLAYER_ID)
      player_targetPlanetSet( -1 );
   else
      player_targetSet( PLAYER_ID );
   gui_setNav();
}


/**
 * @brief Targets the pilot.
 *
 *    @param prev 1 if is cycling backwards.
 */
void player_targetEscort( int prev )
{
   int i;

   /* Check if current target is an escort. */
   for (i=0; i<player.p->nescorts; i++) {
      if (player.p->target == player.p->escorts[i].id) {

         /* Cycle targets. */
         if (prev)
            pilot_setTarget( player.p, (i > 0) ?
                  player.p->escorts[i-1].id : player.p->id );
         else
            pilot_setTarget( player.p, (i < player.p->nescorts-1) ?
                  player.p->escorts[i+1].id : player.p->id );

         break;
      }
   }

   /* Not found in loop. */
   if (i >= player.p->nescorts) {

      /* Check to see if he actually has escorts. */
      if (player.p->nescorts > 0) {

         /* Cycle forward or backwards. */
         if (prev)
            pilot_setTarget( player.p, player.p->escorts[player.p->nescorts-1].id );
         else
            pilot_setTarget( player.p, player.p->escorts[0].id );
      }
      else
         pilot_setTarget( player.p, player.p->id );
   }


   if (player.p->target != PLAYER_ID) {
      gui_forceBlink();
      player_soundPlayGUI( snd_target, 1 );
   }
   gui_setTarget();
}



/**
 * @brief Player targets nearest pilot.
 */
void player_targetNearest (void)
{
   unsigned int t, dt;
   double d;

   d = pilot_getNearestPos( player.p, &dt, player.p->solid->pos.x,
         player.p->solid->pos.y, 1 );
   t = dt;

   /* Disabled ships are typically only valid if within 500 px of the player. */
   if ((d > 250000) && (pilot_isDisabled( pilot_get(dt) ))) {
      t = pilot_getNearestPilot(player.p);
      /* Try to target a disabled ship if there are no active ships in range. */
      if (t == PLAYER_ID)
         t = dt;
   }

   player_targetSet( t );
}


static int screenshot_cur = 0; /**< Current screenshot at. */
/**
 * @brief Takes a screenshot.
 */
void player_screenshot (void)
{
   char filename[PATH_MAX];

   if (nfile_dirMakeExist("%s", nfile_dataPath()) < 0 || nfile_dirMakeExist("%sscreenshots", nfile_dataPath()) < 0) {
      WARN("Aborting screenshot");
      return;
   }

   /* Try to find current screenshots. */
   for ( ; screenshot_cur < 1000; screenshot_cur++) {
      nsnprintf( filename, PATH_MAX, "%sscreenshots/screenshot%03d.png",
            nfile_dataPath(), screenshot_cur );
      if (!nfile_fileExists( filename ))
         break;
   }

   if (screenshot_cur >= 999) { /* in case the crap system breaks :) */
      WARN("You have reached the maximum amount of screenshots [999]");
      return;
   }

   /* now proceed to take the screenshot */
   DEBUG( "Taking screenshot [%03d]...", screenshot_cur );
   gl_screenshot(filename);
}


/**
 * @brief Checks to see if player is still being hailed and clears hail counters
 *        if he isn't.
 */
static void player_checkHail (void)
{
   int i;
   Pilot *p;

   /* See if a pilot is hailing. */
   for (i=0; i<pilot_nstack; i++) {
      p = pilot_stack[i];

      /* Must be hailing. */
      if (pilot_isFlag(p, PILOT_HAILING))
         return;
   }

   /* Clear hail timer. */
   player_hailCounter   = 0;
   player_hailTimer     = 0.;
}


/**
 * @brief Displays an out of range message for the player's currently selected planet.
 */
static void player_planetOutOfRangeMsg (void)
{
   player_message( "\er%s is out of comm range, unable to contact.",
         cur_system->planets[player.p->nav_planet]->name );
}


/**
 * @brief Opens communication with the player's target.
 */
void player_hail (void)
{
   /* Not under manual control or disabled. */
   if (pilot_isFlag( player.p, PILOT_MANUAL_CONTROL ) ||
         pilot_isDisabled(player.p))
      return;

   if (player.p->target != player.p->id)
      comm_openPilot(player.p->target);
   else if(player.p->nav_planet != -1) {
      if (pilot_inRangePlanet( player.p, player.p->nav_planet ))
         comm_openPlanet( cur_system->planets[ player.p->nav_planet ] );
      else
         player_planetOutOfRangeMsg();
   }
   else
      player_message("\erNo target selected to hail.");

   /* Clear hails if none found. */
   player_checkHail();
}


/**
 * @brief Opens communication with the player's planet target.
 */
void player_hailPlanet (void)
{
   /* Not under manual control. */
   if (pilot_isFlag( player.p, PILOT_MANUAL_CONTROL ))
      return;

   if (player.p->nav_planet != -1) {
      if (pilot_inRangePlanet( player.p, player.p->nav_planet ))
         comm_openPlanet( cur_system->planets[ player.p->nav_planet ] );
      else
         player_planetOutOfRangeMsg();
   }
   else
      player_message("\erNo target selected to hail.");
}


/**
 * @brief Automatically tries to hail a pilot that hailed the player.
 */
void player_autohail (void)
{
   int i;
   Pilot *p;

   /* Not under manual control or disabled. */
   if (pilot_isFlag( player.p, PILOT_MANUAL_CONTROL ) ||
         pilot_isDisabled(player.p))
      return;

   /* Find pilot to autohail. */
   for (i=0; i<pilot_nstack; i++) {
      p = pilot_stack[i];

      /* Must be hailing. */
      if (pilot_isFlag(p, PILOT_HAILING))
         break;
   }

   /* Not found any. */
   if (i >= pilot_nstack) {
      player_message("\erYou haven't been hailed by any pilots.");
      return;
   }

   /* Try to hail. */
   pilot_setTarget( player.p, p->id );
   gui_setTarget();
   player_hail();

   /* Clear hails if none found. */
   player_checkHail();
}


/**
 * @brief Toggles mouse flying.
 */
void player_toggleMouseFly(void)
{
   if (!player_isFlag(PLAYER_MFLY)) {
      input_mouseShow();
      player_message("\epMouse flying enabled.");
      player_setFlag(PLAYER_MFLY);
   }
   else {
      input_mouseHide();
      player_rmFlag(PLAYER_MFLY);
      player_message("\erMouse flying disabled.");

      if (conf.mouse_thrust)
         player_accelOver();
   }
}


/**
 * @brief Starts braking or active cooldown.
 */
void player_brake(void)
{
   int stopped;

   if (pilot_isFlag(player.p, PILOT_TAKEOFF))
      return;

   /* Not under manual control or disabled. */
   if (pilot_isFlag( player.p, PILOT_MANUAL_CONTROL ) ||
         pilot_isDisabled(player.p))
      return;

   stopped = pilot_isStopped(player.p);
   if (stopped && !pilot_isFlag(player.p, PILOT_COOLDOWN))
      pilot_cooldown(player.p);
   else if (pilot_isFlag(player.p, PILOT_BRAKING))
      pilot_setFlag(player.p, PILOT_COOLDOWN_BRAKE);
   else if (!stopped)
      pilot_setFlag(player.p, PILOT_BRAKING);
}


/**
 * @brief Handles mouse flying based on cursor position.
 *
 *    @return 1 if cursor is outside the dead zone, 0 if it isn't.
 */
static int player_thinkMouseFly(void)
{
   double px, py, r, x, y, acc;

   px = player.p->solid->pos.x;
   py = player.p->solid->pos.y;
   gl_screenToGameCoords( &x, &y, player.mousex, player.mousey );
   r = sqrt(pow2(x-px) + pow2(y-py));
   if (r > 50.) { /* Ignore mouse input within a 50 px radius of the centre. */
      pilot_face(player.p, atan2( y - py, x - px));
      if (conf.mouse_thrust) { /* Only alter thrust if option is enabled. */
         acc = CLAMP(0., 1., (r - 100) / 200.);
         acc = 3 * pow2(acc) - 2 * pow(acc, 3);
         /* Only accelerate when within 180 degrees of the intended direction. */
         if (ABS(angle_diff(atan2( y - py, x - px), player.p->solid->dir)) < M_PI_2 )
            player_accel(acc);
         else
            player_accel(0.);
      }
      return 1;
   }
   else
      return 0;
}


/**
 * @brief Player got pwned.
 */
void player_dead (void)
{
   /* Explode at normal speed. */
   pause_setSpeed(1.);

   gui_cleanup();

   /* Close the overlay. */
   ovr_setOpen(0);
}


/**
 * @brief Player blew up in a fireball.
 */
void player_destroyed (void)
{
   if (player_isFlag(PLAYER_DESTROYED))
      return;

   /* Mark as destroyed. */
   player_setFlag(PLAYER_DESTROYED);

   /* Set timer for death menu. */
   player_timer = 5.;

   /* Stop sounds. */
   player_soundStop();

   /* Stop autonav */
   player_autonavEnd();

   /* Reset time compression when player dies. */
   pause_setSpeed( 1. );
}


/**
 * @brief PlayerShip_t compare function for qsort().
 */
static int player_shipsCompare( const void *arg1, const void *arg2 )
{
   PlayerShip_t *ps1, *ps2;
   credits_t p1, p2;

   /* Get the arguments. */
   ps1 = (PlayerShip_t*) arg1;
   ps2 = (PlayerShip_t*) arg2;

   /* Get prices. */
   p1 = pilot_worth( ps1->p );
   p2 = pilot_worth( ps2->p );

   /* Compare price INVERSELY */
   if (p1 < p2)
      return +1;
   else if (p1 > p2)
      return -1;

   /* In case of tie sort by name so they don't flip or something. */
   return strcmp( ps1->p->name, ps2->p->name );
}


/**
 * @brief Returns a buffer with all the player's ships names.
 *
 *    @param sships Fills sships with player_nships ship names.
 *    @param tships Fills sships with player_nships ship target textures.
 *    @return Freshly allocated array with allocated ship names.
 *    @return The number of ships the player has.
 */
int player_ships( char** sships, glTexture** tships )
{
   int i;

   if (player_nstack == 0)
      return 0;

   /* Sort. */
   qsort( player_stack, player_nstack, sizeof(PlayerShip_t), player_shipsCompare );

   /* Create the struct. */
   for (i=0; i < player_nstack; i++) {
      sships[i] = strdup(player_stack[i].p->name);
      tships[i] = player_stack[i].p->ship->gfx_store;
   }

   return player_nstack;
}


/**
 * @brief Gets all of the player's ships.
 *
 *    @param[out] Number of star systems gotten.
 *    @return The player's ships.
 */
const PlayerShip_t* player_getShipStack( int *n )
{
   *n = player_nstack;
   return player_stack;
}


/**
 * @brief Gets the amount of ships player has in storage.
 *
 *    @return The number of ships the player has.
 */
int player_nships (void)
{
   return player_nstack;
}


/**
 * @brief Sees if player has a ship of a name.
 *
 *    @param shipname Nome of the ship to get.
 *    @return 1 if ship exists.
 */
int player_hasShip( char* shipname )
{
   int i;

   /* Check current ship. */
   if ((player.p != NULL) && (strcmp(player.p->name,shipname)==0))
      return 1;

   /* Check stocked ships. */
   for (i=0; i < player_nstack; i++)
      if (strcmp(player_stack[i].p->name, shipname)==0)
         return 1;
   return 0;
}


/**
 * @brief Gets a specific ship.
 *
 *    @param shipname Nome of the ship to get.
 *    @return The ship matching name.
 */
Pilot* player_getShip( char* shipname )
{
   int i;

   if ((player.p != NULL) && (strcmp(shipname,player.p->name)==0))
      return player.p;

   for (i=0; i < player_nstack; i++)
      if (strcmp(player_stack[i].p->name, shipname)==0)
         return player_stack[i].p;

   WARN("Player ship '%s' not found in stack", shipname);
   return NULL;
}


/**
 * @brief Gets where a specific ship is.
 *
 *    @param shipname Ship to check where it is.
 *    @return The location of the ship.
 */
char* player_getLoc( char* shipname )
{
   int i;

   if (strcmp(player.p->name,shipname)==0)
      return land_planet->name;

   for (i=0; i < player_nstack; i++)
      if (strcmp(player_stack[i].p->name, shipname)==0)
         return player_stack[i].loc;

   WARN("Player ship '%s' not found in stack", shipname);
   return NULL;
}


/**
 * @brief Sets the location of a specific ship.
 *
 *    @param shipname Name of the ship to change location of.
 *    @param loc Location of the ship to change to.
 */
void player_setLoc( char* shipname, char* loc )
{
   int i;

   for (i=0; i < player_nstack; i++) {
      if (strcmp(player_stack[i].p->name, shipname)==0) {
         free(player_stack[i].loc);
         player_stack[i].loc = strdup(loc);
         return;
      }
   }

   WARN("Player ship '%s' not found in stack", shipname);
}


/**
 * @brief Gets how many of the outfit the player owns.
 *
 *    @param outfitname Outfit to check how many the player owns.
 *    @return The number of outfits matching outfitname owned.
 */
int player_outfitOwned( const Outfit* o )
{
   int i;

   /* Special case map. */
   if ((outfit_isMap(o) && map_isMapped(o)) ||
         (outfit_isLocalMap(o) && localmap_isMapped(o)))
      return 1;

   /* Special case license. */
   if (outfit_isLicense(o) &&
         player_hasLicense(o->name))
      return 1;

   /* Special case GUI. */
   if (outfit_isGUI(o) &&
         player_guiCheck(o->u.gui.gui))
      return 1;

   /* Try to find it. */
   for (i=0; i<player_noutfits; i++)
      if (player_outfits[i].o == o)
         return player_outfits[i].q;

   return 0;
}


/**
 * @brief qsort() compare function for PlayerOutfit_t sorting.
 */
static int player_outfitCompare( const void *arg1, const void *arg2 )
{
   PlayerOutfit_t *po1, *po2;

   /* Get type. */
   po1 = (PlayerOutfit_t*) arg1;
   po2 = (PlayerOutfit_t*) arg2;

   /* Compare. */
   return outfit_compareTech( &po1->o, &po2->o );
}


/**
 * @brief Returns the player's outfits.
 *
 *    @param[out] n Number of distinct outfits (not total quantity).
 *    @return Outfits the player owns.
 */
const PlayerOutfit_t* player_getOutfits( int *n )
{
	*n = player_noutfits;
	return (const PlayerOutfit_t*) player_outfits;
}


/**
 * @brief Prepares two arrays for displaying in an image array.
 *
 *    @param[out] outfits Outfits the player owns.
 *    @param[out] toutfits Optional store textures for the image array.
 *    @param[in] filter Function to filter which outfits to get.
 *    @param[in] name Name fragment that each outfit must contain.
 *    @return Number of outfits.
 */
int player_getOutfitsFiltered( Outfit **outfits, glTexture*** toutfits, int* ntoutfits,
		int(*filter)( const Outfit *o ), char *name )
{
	int i;

	if (player_noutfits == 0)
		return 0;

	/* We'll sort. */
	qsort( player_outfits, player_noutfits,
			sizeof(PlayerOutfit_t), player_outfitCompare );

	for (i=0; i<player_noutfits; i++)
		outfits[i] = (Outfit*)player_outfits[i].o;

	return outfits_filter( outfits, toutfits, ntoutfits, player_noutfits, filter, name );
}


/**
 * @brief Gets the amount of different outfits in the player outfit stack.
 *
 *    @return Amount of different outfits.
 */
int player_numOutfits (void)
{
   return player_noutfits;
}


/**
 * @brief Adds an outfit to the player outfit stack.
 *
 *    @param o Outfit to add.
 *    @param quantity Amount to add.
 *    @return Amount added.
 */
int player_addOutfit( const Outfit *o, int quantity )
{
   int i;

   /* Sanity check. */
   if (quantity == 0)
      return 0;

   /* special case if it's a map */
   if (outfit_isMap(o)) {
      map_map(o);
      return 1; /* Success. */
   }
   else if (outfit_isLocalMap(o)) {
      localmap_map(o);
      return 1;
   }
   /* special case if it's an outfit */
   else if (outfit_isGUI(o)) {
      player_guiAdd(o->u.gui.gui);
      return 1; /* Success. */
   }
   /* special case if it's a license. */
   else if (outfit_isLicense(o)) {
      player_addLicense(o->name);
      return 1; /* Success. */
   }

   /* Try to find it. */
   for (i=0; i<player_noutfits; i++) {
      if (player_outfits[i].o == o) {
         player_outfits[i].q  += quantity;
         return quantity;
      }
   }

   /* Allocate if needed. */
   player_noutfits++;
   if (player_noutfits > player_moutfits) {
      if (player_moutfits == 0)
         player_moutfits = OUTFIT_CHUNKSIZE;
      else
         player_moutfits *= 2;
      player_outfits   = realloc( player_outfits,
            sizeof(PlayerOutfit_t) * player_moutfits );
   }

   /* Add the outfit. */
   player_outfits[player_noutfits-1].o = o;
   player_outfits[player_noutfits-1].q = quantity;
   return quantity;
}


/**
 * @brief Remove an outfit from the player's outfit stack.
 *
 *    @param o Outfit to remove.
 *    @param quantity Amount to remove.
 *    @return Amount removed.
 */
int player_rmOutfit( const Outfit *o, int quantity )
{
   int i, q;

   /* Try to find it. */
   for (i=0; i<player_noutfits; i++) {
      if (player_outfits[i].o == o) {
         /* See how many to remove. */
         q = MIN( player_outfits[i].q, quantity );
         player_outfits[i].q -= q;

         /* See if must remove element. */
         if (player_outfits[i].q <= 0) {
            player_noutfits--;
            memmove( &player_outfits[i], &player_outfits[i+1],
                  sizeof(PlayerOutfit_t) * (player_noutfits-i) );
         }

         /* Return removed outfits. */
         return q;
      }
   }

   /* Nothing removed. */
   return 0;
}


/**
 * @brief Marks a mission as completed.
 *
 *    @param id ID of the mission to mark as completed.
 */
void player_missionFinished( int id )
{
   /* Make sure not already marked. */
   if (player_missionAlreadyDone(id))
      return;

   /* Mark as done. */
   missions_ndone++;
   if (missions_ndone > missions_mdone) { /* need to grow */
      missions_mdone += 25;
      missions_done = realloc( missions_done, sizeof(int) * missions_mdone);
   }
   missions_done[ missions_ndone-1 ] = id;
}


/**
 * @brief Checks to see if player has already completed a mission.
 *
 *    @param id ID of the mission to see if player has completed.
 *    @return 1 if player has completed the mission, 0 otherwise.
 */
int player_missionAlreadyDone( int id )
{
   int i;
   for (i=0; i<missions_ndone; i++)
      if (missions_done[i] == id)
         return 1;
   return 0;
}


/**
 * @brief Marks a event as completed.
 *
 *    @param id ID of the event to mark as completed.
 */
void player_eventFinished( int id )
{
   /* Make sure not already done. */
   if (player_eventAlreadyDone(id))
      return;

   /* Add to done. */
   events_ndone++;
   if (events_ndone > events_mdone) { /* need to grow */
      events_mdone += 25;
      events_done = realloc( events_done, sizeof(int) * events_mdone);
   }
   events_done[ events_ndone-1 ] = id;
}


/**
 * @brief Checks to see if player has already completed a event.
 *
 *    @param id ID of the event to see if player has completed.
 *    @return 1 if player has completed the event, 0 otherwise.
 */
int player_eventAlreadyDone( int id )
{
   int i;
   for (i=0; i<events_ndone; i++)
      if (events_done[i] == id)
         return 1;
   return 0;
}


/**
 * @brief Checks to see if player has license.
 *
 *    @param license License to check to see if the player has.
 *    @return 1 if has license (or none needed), 0 if doesn't.
 */
int player_hasLicense( char *license )
{
   int i;
   if (!license) /* Null input. */
      return 1;

   for (i=0; i<player_nlicenses; i++)
      if (strcmp(license, player_licenses[i])==0)
         return 1;

   return 0;
}


/**
 * @brief Gives the player a license.
 *
 *    @brief license License to give the player.
 */
void player_addLicense( char *license )
{
   /* Player already has license. */
   if (player_hasLicense(license))
      return;

   /* Add the license. */
   player_nlicenses++;
   player_licenses = realloc( player_licenses, sizeof(char*)*player_nlicenses );
   player_licenses[player_nlicenses-1] = strdup(license);
}


/**
 * @brief Gets the player's licenses.
 *
 *    @param nlicenses Amount of licenses the player has.
 *    @return Name of the licenses he has.
 */
char **player_getLicenses( int *nlicenses )
{
   *nlicenses = player_nlicenses;
   return player_licenses;
}


/**
 * @brief Runs hooks for the player.
 */
void player_runHooks (void)
{
   if (player_isFlag( PLAYER_HOOK_HYPER )) {
      player_brokeHyperspace();
      player_rmFlag( PLAYER_HOOK_HYPER );
   }
   if (player_isFlag( PLAYER_HOOK_JUMPIN)) {
      hooks_run( "jumpin" );
      hooks_run( "enter" );
      events_trigger( EVENT_TRIGGER_ENTER );
      missions_run( MIS_AVAIL_SPACE, -1, NULL, NULL );
      player_rmFlag( PLAYER_HOOK_JUMPIN );
   }
   if (player_isFlag( PLAYER_HOOK_LAND )) {
      land( cur_system->planets[ player.p->nav_planet ], 0 );
      player_rmFlag( PLAYER_HOOK_LAND );
   }
}


/**
 * @brief Clears escorts to make sure deployment is sane.
 */
void player_clearEscorts (void)
{
   int i;

   for (i=0; i<player.p->noutfits; i++) {
      if (player.p->outfits[i]->outfit == NULL)
         continue;

      if (outfit_isFighterBay(player.p->outfits[i]->outfit))
         player.p->outfits[i]->u.ammo.deployed = 0;
   }
}


/**
 * @brief Adds the player's escorts.
 *
 *    @return 0 on success.
 */
int player_addEscorts (void)
{
   int i, j;
   double a;
   Vector2d v;
   unsigned int e;
   Outfit *o;
   int q;

   /* Clear escorts first. */
   player_clearEscorts();

   for (i=0; i<player.p->nescorts; i++) {
      a = RNGF() * 2 * M_PI;
      vect_cset( &v, player.p->solid->pos.x + 50.*cos(a),
            player.p->solid->pos.y + 50.*sin(a) );
      e = escort_create( player.p, player.p->escorts[i].ship,
            &v, &player.p->solid->vel, player.p->solid->dir,
            player.p->escorts[i].type, 0 );
      player.p->escorts[i].id = e; /* Important to update ID. */

      /* Update outfit if needed. */
      if (player.p->escorts[i].type != ESCORT_TYPE_BAY)
         continue;

      for (j=0; j<player.p->noutfits; j++) {
         /* Must have outfit. */
         if (player.p->outfits[j]->outfit == NULL)
            continue;

         /* Must be fighter bay. */
         if (!outfit_isFighterBay(player.p->outfits[j]->outfit))
            continue;

         /* Ship must match. */
         o = outfit_ammo(player.p->outfits[j]->outfit);
         if (!outfit_isFighter(o) ||
               (strcmp(player.p->escorts[i].ship,o->u.fig.ship)!=0))
            continue;

         /* Must not have all deployed. */
         q = player.p->outfits[j]->u.ammo.deployed + player.p->outfits[j]->u.ammo.quantity;
         if (q >= outfit_amount(player.p->outfits[j]->outfit))
            continue;

         /* Mark as deployed. */
         player.p->outfits[j]->u.ammo.deployed += 1;
         break;
      }
      if (j >= player.p->noutfits)
         WARN("Unable to mark escort as deployed");
   }

   return 0;
}


/**
 * @brief Saves the player's escorts.
 */
static int player_saveEscorts( xmlTextWriterPtr writer )
{
   int i;

   for (i=0; i<player.p->nescorts; i++) {
      xmlw_startElem(writer, "escort");
      xmlw_attr(writer,"type","bay"); /**< @todo other types. */
      xmlw_str(writer, "%s", player.p->escorts[i].ship);
      xmlw_endElem(writer); /* "escort" */
   }

   return 0;
}


/**
 * @brief Save the freaking player in a freaking xmlfile.
 *
 *    @param writer xml Writer to use.
 *    @return 0 on success.
 */
int player_save( xmlTextWriterPtr writer )
{
   char **guis;
   int i, n;
   MissionData *m;
   const char *ev;
   int scu, stp, stu;
   double rem;

   xmlw_startElem(writer,"player");

   /* Standard player details. */
   xmlw_attr(writer,"name","%s",player.name);
   xmlw_elem(writer,"rating","%f",player.crating);
   xmlw_elem(writer,"credits","%"CREDITS_PRI,player.p->credits);
   if (player.gui != NULL)
      xmlw_elem(writer,"gui","%s",player.gui);
   xmlw_elem(writer,"guiOverride","%d",player.guiOverride);
   xmlw_elem(writer,"mapOverlay","%d",ovr_isOpen());

   /* Time. */
   xmlw_startElem(writer,"time");
   ntime_getR( &scu, &stp, &stu, &rem );
   xmlw_elem(writer,"SCU","%d", scu);
   xmlw_elem(writer,"STP","%d", stp);
   xmlw_elem(writer,"STU","%d", stu);
   xmlw_elem(writer,"Remainder","%lf", rem);
   xmlw_endElem(writer); /* "time" */

   /* Current ship. */
   xmlw_elem(writer,"location","%s",land_planet->name);
   player_saveShip( writer, player.p, NULL ); /* current ship */

   /* Ships. */
   xmlw_startElem(writer,"ships");
   for (i=0; i<player_nstack; i++)
      player_saveShip( writer, player_stack[i].p, player_stack[i].loc );
   xmlw_endElem(writer); /* "ships" */

   /* GUIs. */
   xmlw_startElem(writer,"guis");
   guis = player_guiList( &n );
   for (i=0; i<n; i++)
      xmlw_elem(writer,"gui","%s",guis[i]);
   xmlw_endElem(writer); /* "guis" */

   /* Outfits. */
   xmlw_startElem(writer,"outfits");
   for (i=0; i<player_noutfits; i++) {
      xmlw_startElem(writer,"outfit");
      xmlw_attr(writer,"quantity","%d",player_outfits[i].q);
      xmlw_str(writer,"%s",player_outfits[i].o->name);
      xmlw_endElem(writer); /* "outfit" */
   }
   xmlw_endElem(writer); /* "outfits" */

   /* Licenses. */
   xmlw_startElem(writer,"licenses");
   for (i=0; i<player_nlicenses; i++)
      xmlw_elem(writer,"license","%s",player_licenses[i]);
   xmlw_endElem(writer); /* "licenses" */

   xmlw_endElem(writer); /* "player" */

   /* Mission the player has done. */
   xmlw_startElem(writer,"missions_done");
   for (i=0; i<missions_ndone; i++) {
      m = mission_get(missions_done[i]);
      if (m != NULL) /* In case mission name changes between versions */
         xmlw_elem(writer,"done","%s",m->name);
   }
   xmlw_endElem(writer); /* "missions_done" */

   /* Events the player has done. */
   xmlw_startElem(writer,"events_done");
   for (i=0; i<events_ndone; i++) {
      ev = event_dataName(events_done[i]);
      if (ev != NULL) /* In case mission name changes between versions */
         xmlw_elem(writer,"done","%s",ev);
   }
   xmlw_endElem(writer); /* "events_done" */

   /* Escorts. */
   xmlw_startElem(writer, "escorts");
   player_saveEscorts(writer);
   xmlw_endElem(writer); /* "escorts" */

   return 0;
}

/**
 * @brief Saves an outfit slot.
 */
static int player_saveShipSlot( xmlTextWriterPtr writer, PilotOutfitSlot *slot, int i )
{
   Outfit *o;
   o = slot->outfit;
   xmlw_startElem(writer,"outfit");
   xmlw_attr(writer,"slot","%d",i);
   if ((outfit_ammo(o) != NULL) &&
         (slot->u.ammo.outfit != NULL)) {
      xmlw_attr(writer,"ammo","%s",slot->u.ammo.outfit->name);
      xmlw_attr(writer,"quantity","%d", slot->u.ammo.quantity);
   }
   xmlw_str(writer,"%s",o->name);
   xmlw_endElem(writer); /* "outfit" */

   return 0;
}


/**
 * @brief Saves a ship.
 *
 *    @param writer XML writer.
 *    @param ship Ship to save.
 *    @param loc Location of the ship.
 *    @return 0 on success.
 */
static int player_saveShip( xmlTextWriterPtr writer,
      Pilot* ship, char* loc )
{
   int i, j, k, n;
   int found;
   const char *name;
   PilotWeaponSetOutfit *weaps;

   xmlw_startElem(writer,"ship");
   xmlw_attr(writer,"name","%s",ship->name);
   xmlw_attr(writer,"model","%s",ship->ship->name);

   if (loc != NULL)
      xmlw_elem(writer,"location","%s",loc);

   /* save the fuel */
   xmlw_elem(writer,"fuel","%f",ship->fuel);

   /* save the outfits */
   xmlw_startElem(writer,"outfits_structure");
   for (i=0; i<ship->outfit_nstructure; i++) {
      if (ship->outfit_structure[i].outfit==NULL)
         continue;
      player_saveShipSlot( writer, &ship->outfit_structure[i], i );
   }
   xmlw_endElem(writer); /* "outfits_structure" */
   xmlw_startElem(writer,"outfits_utility");
   for (i=0; i<ship->outfit_nutility; i++) {
      if (ship->outfit_utility[i].outfit==NULL)
         continue;
      player_saveShipSlot( writer, &ship->outfit_utility[i], i );
   }
   xmlw_endElem(writer); /* "outfits_utility" */
   xmlw_startElem(writer,"outfits_weapon");
   for (i=0; i<ship->outfit_nweapon; i++) {
      if (ship->outfit_weapon[i].outfit==NULL)
         continue;
      player_saveShipSlot( writer, &ship->outfit_weapon[i], i );
   }
   xmlw_endElem(writer); /* "outfits_weapon" */

   /* save the commodities */
   xmlw_startElem(writer,"commodities");
   for (i=0; i<ship->ncommodities; i++) {
      /* Remove cargo with id and no mission. */
      if (ship->commodities[i].id > 0) {
         found = 0;
         for (j=0; j<MISSION_MAX; j++) {
            /* Only check active missions. */
            if (player_missions[j]->id > 0) {
               /* Now check if it's in the cargo list. */
               for (k=0; k<player_missions[j]->ncargo; k++) {
                  /* See if it matches a cargo. */
                  if (player_missions[j]->cargo[k] == ship->commodities[i].id) {
                     found = 1;
                     break;
                  }
               }
            }
            if (found)
               break;
         }

         if (!found) {
            WARN("Found mission cargo without associated mission.");
            WARN("Please reload save game to remove the dead cargo.");
            continue;
         }
      }

      xmlw_startElem(writer,"commodity");

      xmlw_attr(writer,"quantity","%d",ship->commodities[i].quantity);
      if (ship->commodities[i].id > 0)
         xmlw_attr(writer,"id","%d",ship->commodities[i].id);
      xmlw_str(writer,"%s",ship->commodities[i].commodity->name);

      xmlw_endElem(writer); /* commodity */
   }
   xmlw_endElem(writer); /* "commodities" */

   xmlw_startElem(writer,"weaponsets");
   xmlw_attr(writer,"autoweap","%d",ship->autoweap);
   xmlw_attr(writer,"active_set","%d",ship->active_set);
   for (i=0; i<PILOT_WEAPON_SETS; i++) {
      weaps = pilot_weapSetList( ship, i, &n );
      xmlw_startElem(writer,"weaponset");
      /* Inrange isn't handled by autoweap for the player. */
      xmlw_attr(writer,"inrange","%d",pilot_weapSetInrangeCheck(ship,i));
      xmlw_attr(writer,"id","%d",i);
      if (!ship->autoweap) {
         name = pilot_weapSetName(ship,i);
         if (name != NULL)
            xmlw_attr(writer,"name","%s",name);
         xmlw_attr(writer,"type","%d",pilot_weapSetTypeCheck(ship,i));
         for (j=0; j<n;j++) {
            xmlw_startElem(writer,"weapon");
            xmlw_attr(writer,"level","%d",weaps[j].level);
            xmlw_str(writer,"%d",weaps[j].slot->id);
            xmlw_endElem(writer); /* "weapon" */
         }
      }
      xmlw_endElem(writer); /* "weaponset" */
   }
   xmlw_endElem(writer); /* "weaponsets" */

   xmlw_endElem(writer); /* "ship" */

   return 0;
}

/**
 * @brief Loads the player stuff.
 *
 *    @param parent Node where the player stuff is to be found.
 *    @return 0 on success.
 */
Planet* player_load( xmlNodePtr parent )
{
   xmlNodePtr node;
   Planet *pnt;

   /* some cleaning up */
   memset( &player, 0, sizeof(Player_t) );
   pnt = NULL;
   map_cleanup();

   node = parent->xmlChildrenNode;
   do {
      if (xml_isNode(node,"player"))
         pnt = player_parse( node );
      else if (xml_isNode(node,"missions_done"))
         player_parseDoneMissions( node );
      else if (xml_isNode(node,"events_done"))
         player_parseDoneEvents( node );
      else if (xml_isNode(node,"escorts"))
         player_parseEscorts(node);
   } while (xml_nextNode(node));

   return pnt;
}


/**
 * @brief Parses the player node.
 *
 *    @param parent The player node.
 *    @return Planet to start on on success.
 */
static Planet* player_parse( xmlNodePtr parent )
{
   char *planet, *found, *str;
   unsigned int services;
   Planet *pnt;
   xmlNodePtr node, cur;
   int q;
   Outfit *o;
   int i, map_overlay;
   StarSystem *sys;
   double a, r;
   Pilot *old_ship;
   PilotFlags flags;
   int scu, stp, stu, time_set;
   double rem;

   xmlr_attr(parent,"name",player.name);

   /* Make sure player.p is NULL. */
   player.p = NULL;
   pnt = NULL;

   /* Sane defaults. */
   planet      = NULL;
   time_set    = 0;
   map_overlay = 0;

   /* Must get planet first. */
   node = parent->xmlChildrenNode;
   do {
      xmlr_str(node,"location",planet);
   } while (xml_nextNode(node));

   /* Parse rest. */
   node = parent->xmlChildrenNode;
   do {

      /* global stuff */
      xmlr_float(node,"rating",player.crating);
      xmlr_ulong(node,"credits",player_creds);
      xmlr_strd(node,"gui",player.gui);
      xmlr_int(node,"guiOverride",player.guiOverride);
      xmlr_int(node,"mapOverlay",map_overlay);
      ovr_setOpen(map_overlay);

      /* Time. */
      if (xml_isNode(node,"time")) {
         cur = node->xmlChildrenNode;
         scu = stp = stu = -1;
         rem = -1.;
         do {
            xmlr_int(cur,"SCU",scu);
            xmlr_int(cur,"STP",stp);
            xmlr_int(cur,"STU",stu);
            xmlr_float(cur,"Remainder",rem);
         } while (xml_nextNode(cur));
         if ((scu < 0) || (stp < 0) || (stu < 0) || (rem<0.))
            WARN("Malformed time in save game!");
         ntime_setR( scu, stp, stu, rem );
         if ((scu >= 0) || (stp >= 0) || (stu >= 0))
            time_set = 1;
      }

      if (xml_isNode(node,"ship"))
         player_parseShip(node, 1, planet);

      /* Parse ships. */
      else if (xml_isNode(node,"ships")) {
         cur = node->xmlChildrenNode;
         do {
            if (xml_isNode(cur,"ship"))
               player_parseShip(cur, 0, planet);
         } while (xml_nextNode(cur));
      }

      /* Parse GUIs. */
      else if (xml_isNode(node,"guis")) {
         cur = node->xmlChildrenNode;
         do {
            if (xml_isNode(cur,"gui"))
               player_guiAdd( xml_get(cur) );
         } while (xml_nextNode(cur));
      }

      /* Parse outfits. */
      else if (xml_isNode(node,"outfits")) {
         cur = node->xmlChildrenNode;
         do {
            if (xml_isNode(cur,"outfit")) {
               o = outfit_get( xml_get(cur) );
               if (o == NULL) {
                  WARN("Outfit '%s' was saved but does not exist!", xml_get(cur));
                  continue;
               }

               xmlr_attr( cur, "quantity", str );
               if (str != NULL) {
                  q = atof(str);
                  free(str);
               }
               else {
                  WARN("Outfit '%s' was saved without quantity!", o->name);
                  continue;
               }

               player_addOutfit( o, q );
            }
         } while (xml_nextNode(cur));
      }

      /* Parse licenses. */
      else if (xml_isNode(node,"licenses"))
         player_parseLicenses(node);

   } while (xml_nextNode(node));

   /* Handle cases where ship is missing. */
   if (player.p == NULL) {
      pilot_clearFlagsRaw( flags );
      pilot_setFlagRaw( flags, PILOT_PLAYER );
      pilot_setFlagRaw( flags, PILOT_NO_OUTFITS );
      WARN("Player ship does not exist!");

      if (player_nstack == 0) {
         WARN("Player has no other ships, giving starting ship.");
         pilot_create( ship_get(start_ship()), "MIA",
               faction_get("Player"), "player", 0., NULL, NULL, flags, -1 );
      }
      else {

         /* Just give player.p a random ship in the stack. */
         old_ship = player_stack[player_nstack-1].p;
         pilot_create( old_ship->ship, old_ship->name,
               faction_get("Player"), "player", 0., NULL, NULL, flags, -1 );
         player_rmShip( old_ship->name );
         WARN("Giving player ship '%s'.", player.p->name );
      }
   }

   /* Check. */
   if (player.p == NULL) {
      ERR("Something went horribly wrong, player does not exist after load...");
      return NULL;
   }

   /* set global thingies */
   player.p->credits = player_creds;
   if (!time_set) {
      WARN("Save has no time information, setting to start information.");
      ntime_set( start_date() );
   }

   /* set player in system */
   pnt = planet_get( planet );
   /* Get random planet if it's NULL. */
   if ((pnt == NULL) || (planet_getSystem(planet) == NULL) ||
         !planet_hasService(pnt, PLANET_SERVICE_LAND)) {
      WARN("Player starts out in non-existent or invalid planet '%s',"
            "trying to find a suitable one instead.",
            planet );

      /* Find a landable, inhabited planet that's in a system, offers refueling
       * and meets the following additional criteria:
       *
       *    0: Shipyard, outfitter, non-hostile
       *    1: Outfitter, non-hostile
       *    2: None
       *
       * If no planet meeting the current criteria can be found, the next
       * set of criteria is tried until none remain.
       */
      found = NULL;
      for (i=0; i<3; i++) {
         services = PLANET_SERVICE_LAND | PLANET_SERVICE_INHABITED |
               PLANET_SERVICE_REFUEL;

         if (i == 0)
            services |= PLANET_SERVICE_SHIPYARD;

         if (i != 2)
            services |= PLANET_SERVICE_OUTFITS;

         found = space_getRndPlanet( 1, services,
               (i != 2) ? player_filterSuitablePlanet : NULL );
         if (found != NULL)
            break;

         WARN("Could not find a planet satisfying criteria %d.", i);
      }

      if (found == NULL) {
         WARN("Could not find a suitable planet. Choosing a random planet.");
         found = space_getRndPlanet(0, 0, NULL); /* This should never, ever fail. */
      }
      pnt = planet_get( found );
   }
   sys = system_get( planet_getSystem( pnt->name ) );
   space_gfxLoad( sys );
   a = RNGF() * 2.*M_PI;
   r = RNGF() * pnt->radius * 0.8;
   player_warp( pnt->pos.x + r*cos(a), pnt->pos.y + r*sin(a) );
   player.p->solid->dir = RNG(0,359) * M_PI/180.;

   /* initialize the system */
   space_init( sys->name );
   map_clear(); /* sets the map up */

   /* initialize the sound */
   player_initSound();

   return pnt;
}


/**
 * @brief Filter function for space_getRndPlanet
 *
 *    @param p Planet.
 *    @return Whether the planet is suitable for teleporting to.
 */
static int player_filterSuitablePlanet( Planet *p )
{
   return !areEnemies(p->faction, FACTION_PLAYER);
}


/**
 * @brief Parses player's done missions.
 *
 *    @param parent Node of the missions.
 *    @return 0 on success.
 */
static int player_parseDoneMissions( xmlNodePtr parent )
{
   xmlNodePtr node;
   int id;

   node = parent->xmlChildrenNode;

   do {
      if (xml_isNode(node,"done")) {
         id = mission_getID( xml_get(node) );
         if (id < 0)
            DEBUG("Mission '%s' doesn't seem to exist anymore, removing from save.",
                  xml_get(node));
         else
            player_missionFinished( id );
      }
   } while (xml_nextNode(node));

   return 0;
}


/**
 * @brief Parses player's done missions.
 *
 *    @param parent Node of the missions.
 *    @return 0 on success.
 */
static int player_parseDoneEvents( xmlNodePtr parent )
{
   xmlNodePtr node;
   int id;

   node = parent->xmlChildrenNode;

   do {
      if (xml_isNode(node,"done")) {
         id = event_dataID( xml_get(node) );
         if (id < 0)
            DEBUG("Event '%s' doesn't seem to exist anymore, removing from save.",
                  xml_get(node));
         else
            player_eventFinished( id );
      }
   } while (xml_nextNode(node));

   return 0;
}


/**
 * @brief Parses player's licenses.
 *
 *    @param parent Node of the licenses.
 *    @return 0 on success.
 */
static int player_parseLicenses( xmlNodePtr parent )
{
   xmlNodePtr node;

   node = parent->xmlChildrenNode;

   do {
      if (xml_isNode(node,"license"))
         player_addLicense( xml_get(node) );
   } while (xml_nextNode(node));

   return 0;
}


/**
 * @brief Parses the escorts from the escort node.
 *
 *    @param parent "escorts" node to parse.
 *    @return 0 on success.
 */
static int player_parseEscorts( xmlNodePtr parent )
{
   xmlNodePtr node;
   char *buf, *ship;
   EscortType_t type;

   node = parent->xmlChildrenNode;

   do {
      if (xml_isNode(node,"escort")) {
         xmlr_attr( node, "type", buf );
         if (strcmp(buf,"bay")==0)
            type = ESCORT_TYPE_BAY;
         else {
            WARN("Escort has invalid type '%s'.", buf);
            type = ESCORT_TYPE_NULL;
         }
         free(buf);

         ship = xml_get(node);

         /* Add escort to the list. */
         escort_addList( player.p, ship, type, 0 );
      }
   } while (xml_nextNode(node));

   return 0;
}


/**
 * @brief Adds outfit to pilot if it can.
 */
static void player_addOutfitToPilot( Pilot* pilot, Outfit* outfit, PilotOutfitSlot *s )
{
   int ret;

   if (!outfit_fitsSlot( outfit, &s->sslot->slot )) {
      DEBUG( "Outfit '%s' does not fit designated slot on player's pilot '%s', adding to stock.",
            outfit->name, pilot->name );
      player_addOutfit( outfit, 1 );
      return;
   }

   ret = pilot_addOutfitRaw( pilot, outfit, s );
   if (ret != 0) {
      DEBUG("Outfit '%s' does not fit on player's pilot '%s', adding to stock.",
            outfit->name, pilot->name);
      player_addOutfit( outfit, 1 );
      return;
   }

   /* Update stats. */
   pilot_calcStats( pilot );
}


/**
 * @brief Parses a ship outfit slot.
 */
static void player_parseShipSlot( xmlNodePtr node, Pilot *ship, PilotOutfitSlot *slot )
{
   Outfit *o, *ammo;
   char *buf;
   int q;

   char *name = xml_get(node);
   if (name == NULL) {
      WARN("Empty ship slot node found, skipping.");
      return;
   }

   /* Add the outfit. */
   o = outfit_get( name );
   if (o==NULL)
      return;
   player_addOutfitToPilot( ship, o, slot );

   /* Doesn't have ammo. */
   if (outfit_ammo(o)==NULL)
      return;

   /* See if has ammo. */
   xmlr_attr(node,"ammo",buf);
   if (buf == NULL)
      return;

   /* Get the ammo. */
   ammo = outfit_get(buf);
   free(buf);
   if (ammo==NULL)
      return;

   /* See if has quantity. */
   xmlr_attr(node,"quantity",buf);
   if (buf == NULL)
      return;

   /* Get quantity. */
   q = atoi(buf);
   free(buf);

   /* Add ammo. */
   pilot_addAmmo( ship, slot, ammo, q );
}


/**
 * @brief Parses a player's ship.
 *
 *    @param parent Node of the ship.
 *    @param is_player Is it the ship the player is currently in?
 *    @param planet Default planet in case ship location not found.
 *    @return 0 on success.
 */
static int player_parseShip( xmlNodePtr parent, int is_player, char *planet )
{
   char *name, *model, *loc, *q, *id;
   int i, n;
   double fuel;
   Ship *ship_parsed;
   Pilot* ship;
   xmlNodePtr node, cur, ccur;
   int quantity;
   Outfit *o;
   int ret;
   /*const char *str;*/
   Commodity *com;
   PilotFlags flags;
   unsigned int pid;
   int autoweap, level, weapid, active_set;

   xmlr_attr(parent,"name",name);
   xmlr_attr(parent,"model",model);

   /* Sane defaults. */
   loc = NULL;
   pilot_clearFlagsRaw( flags );
   pilot_setFlagRaw( flags, PILOT_PLAYER );
   pilot_setFlagRaw( flags, PILOT_NO_OUTFITS );

   /* Get the ship. */
   ship_parsed = ship_get(model);
   if (ship_parsed == NULL) {
      WARN("Player ship '%s' not found!", model);

      /* Clean up. */
      free(name);
      free(model);

      return -1;
   }

   /* Add GUI if applicable. */
   player_guiAdd( ship_parsed->gui );

   /* player is currently on this ship */
   if (is_player != 0) {
      pid = pilot_create( ship_parsed, name, faction_get("Player"), "player", 0., NULL, NULL, flags, -1 );
      ship = player.p;
      cam_setTargetPilot( pid, 0 );
   }
   else
      ship = pilot_createEmpty( ship_parsed, name, faction_get("Player"), "player", flags );

   /* Ship should not have default outfits. */
   for (i=0; i<ship->noutfits; i++)
      pilot_rmOutfitRaw( ship, ship->outfits[i] );

   /* Clean up. */
   free(name);
   free(model);

   /* Defaults. */
   fuel     = -1;
   autoweap = 1;

   /* Start parsing. */
   node = parent->xmlChildrenNode;
   do {
      /* Get location. */
      if (is_player == 0)
         xmlr_str(node,"location",loc);

      /* get fuel */
      xmlr_float(node,"fuel",fuel);

      /* New outfit loading. */
      if (xml_isNode(node,"outfits_structure") || xml_isNode(node,"outfits_low")) { /** @todo remove legacy layer for 0.6.0 */
         cur = node->xmlChildrenNode;
         do { /* load each outfit */
            if (xml_isNode(cur,"outfit")) {
               xmlr_attr(cur,"slot",q);
               n = -1;
               if (q != NULL) {
                  n = atoi(q);
                  free(q);
               }
               if ((n<0) || (n >= ship->outfit_nstructure)) {
                  WARN("Outfit slot out of range, not adding.");
                  continue;
               }
               player_parseShipSlot( cur, ship, &ship->outfit_structure[n] );
            }
         } while (xml_nextNode(cur));
      }
      else if (xml_isNode(node,"outfits_utility") || xml_isNode(node,"outfits_medium")) { /** @todo remove legacy layer for 0.6.0 */
         cur = node->xmlChildrenNode;
         do { /* load each outfit */
            if (xml_isNode(cur,"outfit")) {
               xmlr_attr(cur,"slot",q);
               n = -1;
               if (q != NULL) {
                  n = atoi(q);
                  free(q);
               }
               if ((n<0) || (n >= ship->outfit_nutility)) {
                  WARN("Outfit slot out of range, not adding.");
                  continue;
               }
               player_parseShipSlot( cur, ship, &ship->outfit_utility[n] );
            }
         } while (xml_nextNode(cur));
      }
      else if (xml_isNode(node,"outfits_weapon") || xml_isNode(node,"outfits_high")) { /** @todo remove legacy layer for 0.6.0 */
         cur = node->xmlChildrenNode;
         do { /* load each outfit */
            if (xml_isNode(cur,"outfit")) {
               xmlr_attr(cur,"slot",q);
               n = -1;
               if (q != NULL) {
                  n = atoi(q);
                  free(q);
               }
               if ((n<0) || (n >= ship->outfit_nweapon)) {
                  WARN("Outfit slot out of range, not adding.");
                  continue;
               }
               player_parseShipSlot( cur, ship, &ship->outfit_weapon[n] );
            }
         } while (xml_nextNode(cur));
      }
      else if (xml_isNode(node,"commodities")) {
         cur = node->xmlChildrenNode;
         do {
            if (xml_isNode(cur,"commodity")) {
               xmlr_attr(cur,"quantity",q);
               xmlr_attr(cur,"id",id);
               quantity = atoi(q);
               i = (id==NULL) ? 0 : atoi(id);
               free(q);
               if (id != NULL)
                  free(id);

               /* Get the commodity. */
               com = commodity_get(xml_get(cur));
               if (com == NULL) {
                  WARN("Unknown commodity '%s' detected, removing.", xml_get(cur));
                  continue;
               }

               /* actually add the cargo with id hack
                * Note that the player's cargo_free is ignored here.
                */
               pilot_cargoAddRaw( ship, com, quantity, 0 );
               if (i != 0)
                  ship->commodities[ ship->ncommodities-1 ].id = i;
            }
         } while (xml_nextNode(cur));
      }
   } while (xml_nextNode(node));

   /* Update stats. */
   pilot_calcStats( ship );

   /* Test for sanity. */
   if (fuel >= 0)
      ship->fuel = MIN(ship->fuel_max, fuel);
   if ((is_player == 0) && (planet_get(loc)==NULL))
      loc = planet;
   /* ships can now be non-spaceworthy on save
    * str = pilot_checkSpaceworthy( ship ); */
   if (!pilot_slotsCheckSanity( ship )) {
      DEBUG("Player ship '%s' failed slot sanity check , removing all outfits and adding to stock.",
            ship->name );
      /* Remove all outfits. */
      for (i=0; i<ship->noutfits; i++) {
         o = ship->outfits[i]->outfit;
         ret = pilot_rmOutfitRaw( ship, ship->outfits[i] );
         if (ret==0)
            player_addOutfit( o, 1 );
      }
      pilot_calcStats( ship );
   }

   /* add it to the stack if it's not what the player is in */
   if (is_player == 0) {
      player_stack = realloc(player_stack, sizeof(PlayerShip_t)*(player_nstack+1));
      player_stack[player_nstack].p    = ship;
      player_stack[player_nstack].loc  = (loc!=NULL) ? strdup(loc) : strdup("Uknown");
      player_nstack++;
   }

   /* Sets inrange by default if weapon sets are missing. */
   for (i=0; i<PILOT_WEAPON_SETS; i++)
      pilot_weapSetInrange( ship, i, WEAPSET_INRANGE_PLAYER_DEF );

   /* Second pass for weapon sets. */
   node = parent->xmlChildrenNode;
   do {
      if (!xml_isNode(node,"weaponsets"))
         continue;

      /* Check for autoweap. */
      xmlr_attr(node,"autoweap",id);
      if (id != NULL) {
         autoweap = atoi(id);
         free(id);
      }

      /* Load the last weaponset the player used on this ship. */
      xmlr_attr(node,"active_set",id);
      if (id != NULL) {
         active_set = atoi(id);
         free(id);
      }
      else {
         /* set active_set to invalid. will be dealt with later */
         active_set = -1;
      }

      /* Parse weapon sets. */
      cur = node->xmlChildrenNode;
      do { /* Load each weapon set. */
         xml_onlyNodes(cur);
         if (!xml_isNode(cur,"weaponset")) {
            WARN("Player ship '%s' has unknown node '%s' in 'weaponsets' (expected 'weaponset').",
                  ship->name, cur->name);
            continue;
         }

         /* Get id. */
         xmlr_attr(cur,"id",id);
         if (id == NULL) {
            WARN("Player ship '%s' missing 'id' tag for weapon set.",ship->name);
            continue;
         }
         i = atoi(id);
         free(id);
         if ((i < 0) || (i >= PILOT_WEAPON_SETS)) {
            WARN("Player ship '%s' has invalid weapon set id '%d' [max %d].",
                  ship->name, i, PILOT_WEAPON_SETS-1 );
            continue;
         }

         /* Set inrange mode. */
         xmlr_attr(cur,"inrange",id);
         if (id == NULL)
            pilot_weapSetInrange( ship, i, WEAPSET_INRANGE_PLAYER_DEF );
         else {
            pilot_weapSetInrange( ship, i, atoi(id) );
            free(id);
         }

         if (autoweap) /* Autoweap handles everything except inrange. */
            continue;

         /* Set type mode. */
         xmlr_attr(cur,"type",id);
         if (id == NULL) {
            WARN("Player ship '%s' missing 'type' tag for weapon set.",ship->name);
            continue;
         }
         pilot_weapSetType( ship, i, atoi(id) );
         free(id);

         /* Parse individual weapons. */
         ccur = cur->xmlChildrenNode;
         do {
            /* Only nodes. */
            xml_onlyNodes(ccur);

            /* Only weapon nodes. */
            if (!xml_isNode(ccur,"weapon")) {
               WARN("Player ship '%s' has unknown 'weaponset' child node '%s' (expected 'weapon').",
                     ship->name, ccur->name );
               continue;
            }

            /* Get level. */
            xmlr_attr(ccur,"level",id);
            if (id == NULL) {
               WARN("Player ship '%s' missing 'level' tag for weapon set weapon.", ship->name);
               continue;
            }
            level = atoi(id);
            free(id);
            weapid = xml_getInt(ccur);
            if ((weapid < 0) || (weapid >= ship->noutfits)) {
               WARN("Player ship '%s' has invalid weapon id %d [max %d].",
                     ship->name, weapid, ship->noutfits-1 );
               continue;
            }

            /* Add the weapon set. */
            pilot_weapSetAdd( ship, i, ship->outfits[weapid], level );

         } while (xml_nextNode(ccur));
      } while (xml_nextNode(cur));
   } while (xml_nextNode(node));

   /* Set up autoweap if necessary. */
   ship->autoweap = autoweap;
   if (autoweap)
      pilot_weaponAuto( ship );
   pilot_weaponSane( ship );
   if (active_set >= 0 && active_set < PILOT_WEAPON_SETS)
      ship->active_set = active_set;
   else
      pilot_weaponSetDefault( ship );

   return 0;
}


