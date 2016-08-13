/*
 * See Licensing and Copyright notice in naev.h
 */

/**
 * @file outfit.c
 *
 * @brief Handles all the ship outfit specifics.
 *
 * These outfits allow you to modify ships or make them more powerful and are
 *  a fundamental part of the game.
 */


#include "outfit.h"

#include "naev.h"

#include <math.h>
#include "nstring.h"
#include <stdlib.h>

#include "nxml.h"
#include "SDL_thread.h"

#include "log.h"
#include "ndata.h"
#include "nfile.h"
#include "spfx.h"
#include "array.h"
#include "ship.h"
#include "conf.h"
#include "pilot_heat.h"
#include "nstring.h"
#include "pilot.h"
#include "damagetype.h"
#include "slots.h"
#include "mapData.h"


#define outfit_setProp(o,p)      ((o)->properties |= p) /**< Checks outfit property. */

#define XML_OUTFIT_TAG     "outfit"    /**< XML section identifier. */

#define OUTFIT_SHORTDESC_MAX  256 /**< Max length of the short description of the outfit. */


/*
 * the stack
 */
static Outfit* outfit_stack = NULL; /**< Stack of outfits. */


/*
 * Prototypes
 */
/* misc */
static OutfitType outfit_strToOutfitType( char *buf );
static int outfit_setDefaultSize( Outfit *o );
static void outfit_launcherDesc( Outfit* o );
static int outfit_compareNames( const void *name1, const void *name2 );
/* parsing */
static int outfit_loadDir( char *dir );
static int outfit_parseDamage( Damage *dmg, xmlNodePtr node );
static int outfit_parse( Outfit* temp, const char* file );
static void outfit_parseSBolt( Outfit* temp, const xmlNodePtr parent );
static void outfit_parseSBeam( Outfit* temp, const xmlNodePtr parent );
static void outfit_parseSLauncher( Outfit* temp, const xmlNodePtr parent );
static void outfit_parseSAmmo( Outfit* temp, const xmlNodePtr parent );
static void outfit_parseSMod( Outfit* temp, const xmlNodePtr parent );
static void outfit_parseSAfterburner( Outfit* temp, const xmlNodePtr parent );
static void outfit_parseSJammer( Outfit *temp, const xmlNodePtr parent );
static void outfit_parseSFighterBay( Outfit *temp, const xmlNodePtr parent );
static void outfit_parseSFighter( Outfit *temp, const xmlNodePtr parent );
static void outfit_parseSMap( Outfit *temp, const xmlNodePtr parent );
static void outfit_parseSLocalMap( Outfit *temp, const xmlNodePtr parent );
static void outfit_parseSGUI( Outfit *temp, const xmlNodePtr parent );
static void outfit_parseSLicense( Outfit *temp, const xmlNodePtr parent );


/**
 * @brief Gets an outfit by name.
 *
 *    @param name Name to match.
 *    @return Outfit matching name or NULL if not found.
 */
Outfit* outfit_get( const char* name )
{
   int i;

   for (i=0; i<array_size(outfit_stack); i++)
      if (strcmp(name,outfit_stack[i].name)==0)
         return &outfit_stack[i];

   WARN("Outfit '%s' not found in stack.", name);
   return NULL;
}


/**
 * @brief Gets an outfit by name without warning on no-find.
 *
 *    @param name Name to match.
 *    @return Outfit matching name or NULL if not found.
 */
Outfit* outfit_getW( const char* name )
{
   int i;
   for (i=0; i<array_size(outfit_stack); i++)
      if (strcmp(name,outfit_stack[i].name)==0)
         return &outfit_stack[i];
   return NULL;
}


/**
 * @brief Gets all the outfits.
 */
Outfit* outfit_getAll( int *n )
{
   *n = array_size(outfit_stack);
   return outfit_stack;
}


/**
 * @brief Checks to see if an outfit exists matching name (case insensitive).
 */
const char *outfit_existsCase( const char* name )
{
   int i;
   for (i=0; i<array_size(outfit_stack); i++)
      if (strcasecmp(name,outfit_stack[i].name)==0)
         return outfit_stack[i].name;
   return NULL;
}


/**
 * @brief Does a fuzzy search of all the outfits.
 */
char **outfit_searchFuzzyCase( const char* name, int *n )
{
   int i, len, nstack;
   char **names;

   /* Overallocate to maximum. */
   nstack = array_size(outfit_stack);
   names = malloc( sizeof(char*) * nstack );

   /* Do fuzzy search. */
   len = 0;
   for (i=0; i<nstack; i++) {
      if (nstrcasestr( outfit_stack[i].name, name ) != NULL) {
         names[len] = outfit_stack[i].name;
         len++;
      }
   }

   /* Free if empty. */
   if (len == 0) {
      free(names);
      names = NULL;
   }

   *n = len;
   return names;
}


/**
 * @brief Function meant for use with C89, C99 algorithm qsort().
 *
 *    @param outfit1 First argument to compare.
 *    @param outfit2 Second argument to compare.
 *    @return -1 if first argument is inferior, +1 if it's superior, 0 if ties.
 */
int outfit_compareTech( const void *outfit1, const void *outfit2 )
{
   int ret;
   const Outfit *o1, *o2;

   /* Get outfits. */
   o1 = * (const Outfit**) outfit1;
   o2 = * (const Outfit**) outfit2;

   /* Compare slot type. */
   if (o1->slot.type < o2->slot.type)
      return +1;
   else if (o1->slot.type > o2->slot.type)
      return -1;

   /* Compare intrinsic types. */
   if (o1->type < o2->type)
      return -1;
   else if (o1->type > o2->type)
      return +1;

   /* Compare named types. */
   if ((o1->typename == NULL) && (o2->typename != NULL))
      return -1;
   else if ((o1->typename != NULL) && (o2->typename == NULL))
      return +1;
   else if ((o1->typename != NULL) && (o2->typename != NULL)) {
      ret = strcmp( o1->typename, o2->typename );
      if (ret != 0)
         return ret;
   }

   /* Compare slot sizes. */
   if (o1->slot.size < o2->slot.size)
      return +1;
   else if (o1->slot.size > o2->slot.size)
      return -1;

   /* Compare sort priority. */
   if (o1->priority < o2->priority)
      return +1;
   else if (o1->priority > o2->priority)
      return -1;

   /* Compare price. */
   if (o1->price < o2->price)
      return +1;
   else if (o1->price > o2->price)
      return -1;

   /* It turns out they're the same. */
   return strcmp( o1->name, o2->name );
}


/**
 * @brief Gets the name of the slot type of an outfit.
 *
 *    @param o Outfit to get slot type of.
 *    @return The human readable name of the slot type.
 */
const char *outfit_slotName( const Outfit* o )
{
   switch (o->slot.type) {
      case OUTFIT_SLOT_NULL:
         return "NULL";
      case OUTFIT_SLOT_NA:
         return "NA";
      case OUTFIT_SLOT_STRUCTURE:
         return "Structure";
      case OUTFIT_SLOT_UTILITY:
         return "Utility";
      case OUTFIT_SLOT_WEAPON:
         return "Weapon";
      default:
         return "Unknown";
   }
}


/**
 * @brief Gets the name of the slot size of an outfit.
 *
 *    @param o Outfit to get slot size of.
 *    @return The human readable name of the slot size.
 */
const char *outfit_slotSize( const Outfit* o )
{
   switch( o->slot.size) {
      case OUTFIT_SLOT_SIZE_NA:
         return "NA";
      case OUTFIT_SLOT_SIZE_LIGHT:
         return "Small";
      case OUTFIT_SLOT_SIZE_MEDIUM:
         return "Medium";
      case OUTFIT_SLOT_SIZE_HEAVY:
         return "Large";
      default:
         return "Unknown";
   }
}

const char *slotSize( const OutfitSlotSize o )
{
   switch( o ) {
      case OUTFIT_SLOT_SIZE_NA:
         return "NA";
      case OUTFIT_SLOT_SIZE_LIGHT:
         return "Small";
      case OUTFIT_SLOT_SIZE_MEDIUM:
         return "Medium";
      case OUTFIT_SLOT_SIZE_HEAVY:
         return "Large";
      default:
         return "Unknown";
   }
}



/**
 * @brief Gets the slot size colour for an outfit slot.
 *
 *    @param os Outfit slot to get the slot size colour of.
 *    @return The slot size colour of the outfit slot.
 */
const glColour *outfit_slotSizeColour( const OutfitSlot* os )
{
   if (os->size == OUTFIT_SLOT_SIZE_HEAVY)
      return &cFontBlue;
   else if (os->size == OUTFIT_SLOT_SIZE_MEDIUM)
      return &cFontGreen;
   else if (os->size == OUTFIT_SLOT_SIZE_LIGHT)
      return &cFontYellow;
   return NULL;
}


/**
 * @brief Gets the outfit slot size from a human readable string.
 *
 *    @param s String representing an outfit slot size.
 *    @return Outfit slot size matching string.
 */
OutfitSlotSize outfit_toSlotSize( const char *s )
{
   if (s == NULL) {
      /*WARN( "(NULL) outfit slot size" );*/
      return OUTFIT_SLOT_SIZE_NA;
   }

   if (strcasecmp(s,"Large")==0)
      return OUTFIT_SLOT_SIZE_HEAVY;
   else if (strcasecmp(s,"Medium")==0)
      return OUTFIT_SLOT_SIZE_MEDIUM;
   else if (strcasecmp(s,"Small")==0)
      return OUTFIT_SLOT_SIZE_LIGHT;

   WARN("'%s' does not match any outfit slot sizes.", s);
   return OUTFIT_SLOT_SIZE_NA;
}


/**
 * @brief Sets the outfit slot size from default outfit properties.
 */
static int outfit_setDefaultSize( Outfit *o )
{
   if (o->mass <= 10.)
      o->slot.size = OUTFIT_SLOT_SIZE_LIGHT;
   else if (o->mass <= 30.)
      o->slot.size = OUTFIT_SLOT_SIZE_MEDIUM;
   else
      o->slot.size = OUTFIT_SLOT_SIZE_HEAVY;
   WARN("Outfit '%s' has implicit slot size, setting to '%s'.",o->name,outfit_slotSize(o));
   return 0;
}

/**
 * @brief Checks if outfit is an active outfit.
 *    @param o Outfit to check.
 *    @return 1 if o is active.
 */
int outfit_isActive( const Outfit* o )
{
   if (outfit_isForward(o) || outfit_isTurret(o) || outfit_isLauncher(o) || outfit_isFighterBay(o))
      return 1;
   if (outfit_isMod(o) && o->u.mod.active)
      return 1;
   if (outfit_isJammer(o))
      return 1;
   if (outfit_isAfterburner(o))
      return 1;
   return 0;
}


/**
 * @brief Checks if outfit is a fixed mounted weapon.
 *    @param o Outfit to check.
 *    @return 1 if o is a weapon (beam/bolt).
 */
int outfit_isForward( const Outfit* o )
{
   return ( (o->type==OUTFIT_TYPE_BOLT)      ||
         (o->type==OUTFIT_TYPE_BEAM) );
}
/**
 * @brief Checks if outfit is bolt type weapon.
 *    @param o Outfit to check.
 *    @return 1 if o is a bolt type weapon.
 */
int outfit_isBolt( const Outfit* o )
{
   return ( (o->type==OUTFIT_TYPE_BOLT)      ||
         (o->type==OUTFIT_TYPE_TURRET_BOLT) );
}
/**
 * @brief Checks if outfit is a beam type weapon.
 *    @param o Outfit to check.
 *    @return 1 if o is a beam type weapon.
 */
int outfit_isBeam( const Outfit* o )
{
   return ( (o->type==OUTFIT_TYPE_BEAM)      ||
         (o->type==OUTFIT_TYPE_TURRET_BEAM) );
}
/**
 * @brief Checks if outfit is a weapon launcher.
 *    @param o Outfit to check.
 *    @return 1 if o is a weapon launcher.
 */
int outfit_isLauncher( const Outfit* o )
{
   return ( (o->type==OUTFIT_TYPE_LAUNCHER) ||
         (o->type==OUTFIT_TYPE_TURRET_LAUNCHER) );
}
/**
 * @brief Checks if outfit is ammo for a launcher.
 *    @param o Outfit to check.
 *    @return 1 if o is ammo.
 */
int outfit_isAmmo( const Outfit* o )
{
   return (o->type==OUTFIT_TYPE_AMMO);
}
/**
 * @brief Checks if outfit is a seeking weapon.
 *    @param o Outfit to check.
 *    @return 1 if o is a seeking weapon.
 */
int outfit_isSeeker( const Outfit* o )
{
   if (((o->type==OUTFIT_TYPE_AMMO) || (o->type==OUTFIT_TYPE_TURRET_LAUNCHER) ||
            (o->type==OUTFIT_TYPE_LAUNCHER)) &&
         (o->u.amm.ai > 0))
      return 1;
   return 0;
}
/**
 * @brief Checks if outfit is a turret class weapon.
 *    @param o Outfit to check.
 *    @return 1 if o is a turret class weapon.
 */
int outfit_isTurret( const Outfit* o )
{
   return ( (o->type==OUTFIT_TYPE_TURRET_BOLT)  ||
         (o->type==OUTFIT_TYPE_TURRET_BEAM)     ||
         (o->type==OUTFIT_TYPE_TURRET_LAUNCHER) );
}
/**
 * @brief Checks if outfit is a ship modification.
 *    @param o Outfit to check.
 *    @return 1 if o is a ship modification.
 */
int outfit_isMod( const Outfit* o )
{
   return (o->type==OUTFIT_TYPE_MODIFICATION);
}
/**
 * @brief Checks if outfit is an afterburner.
 *    @param o Outfit to check.
 *    @return 1 if o is an afterburner.
 */
int outfit_isAfterburner( const Outfit* o )
{
   return (o->type==OUTFIT_TYPE_AFTERBURNER);
}
/**
 * @brief Checks if outfit is a missile jammer.
 *    @param o Outfit to check.
 *    @return 1 if o is a jammer.
 */
int outfit_isJammer( const Outfit* o )
{
   return (o->type==OUTFIT_TYPE_JAMMER);
}
/**
 * @brief Checks if outfit is a fighter bay.
 *    @param o Outfit to check.
 *    @return 1 if o is a jammer.
 */
int outfit_isFighterBay( const Outfit* o )
{
   return (o->type==OUTFIT_TYPE_FIGHTER_BAY);
}
/**
 * @brief Checks if outfit is a fighter.
 *    @param o Outfit to check.
 *    @return 1 if o is a Fighter.
 */
int outfit_isFighter( const Outfit* o )
{
   return (o->type==OUTFIT_TYPE_FIGHTER);
}
/**
 * @brief Checks if outfit is a space map.
 *    @param o Outfit to check.
 *    @return 1 if o is a map.
 */
int outfit_isMap( const Outfit* o )
{
   return (o->type==OUTFIT_TYPE_MAP);
}
/**
 * @brief Checks if outfit is a local space map.
 *    @param o Outfit to check.
 *    @return 1 if o is a map.
 */
int outfit_isLocalMap( const Outfit* o )
{
   return (o->type==OUTFIT_TYPE_LOCALMAP);
}
/**
 * @brief Checks if outfit is a license.
 *    @param o Outfit to check.
 *    @return 1 if o is a license.
 */
int outfit_isLicense( const Outfit* o )
{
   return (o->type==OUTFIT_TYPE_LICENSE);
}
/**
 * @brief Checks if outfit is a GUI.
 *    @param o Outfit to check.
 *    @return 1 if o is a GUI.
 */
int outfit_isGUI( const Outfit* o )
{
   return (o->type==OUTFIT_TYPE_GUI);
}


/**
 * @brief Checks if outfit has the secondary flag set.
 *    @param o Outfit to check.
 *    @return 1 if o is a secondary weapon.
 */
int outfit_isSecondary( const Outfit* o )
{
   return (o->properties & OUTFIT_PROP_WEAP_SECONDARY);
}


/**
 * @brief Gets the outfit's graphic effect.
 *    @param o Outfit to get information from.
 */
glTexture* outfit_gfx( const Outfit* o )
{
   if (outfit_isBolt(o)) return o->u.blt.gfx_space;
   else if (outfit_isBeam(o)) return o->u.bem.gfx;
   else if (outfit_isAmmo(o)) return o->u.amm.gfx_space;
   return NULL;
}
/**
 * @brief Gets the outfit's sound effect.
 *    @param o Outfit to get information from.
 */
int outfit_spfxArmour( const Outfit* o )
{
   if (outfit_isBolt(o)) return o->u.blt.spfx_armour;
   else if (outfit_isBeam(o)) return o->u.bem.spfx_armour;
   else if (outfit_isAmmo(o)) return o->u.amm.spfx_armour;
   return -1;
}
/**
 * @brief Gets the outfit's sound effect.
 *    @param o Outfit to get information from.
 */
int outfit_spfxShield( const Outfit* o )
{
   if (outfit_isBolt(o)) return o->u.blt.spfx_shield;
   else if (outfit_isBeam(o)) return o->u.bem.spfx_shield;
   else if (outfit_isAmmo(o)) return o->u.amm.spfx_shield;
   return -1;
}
/**
 * @brief Gets the outfit's damage.
 *    @param o Outfit to get information from.
 */
const Damage *outfit_damage( const Outfit* o )
{
   if (outfit_isBolt(o)) return &o->u.blt.dmg;
   else if (outfit_isBeam(o)) return &o->u.bem.dmg;
   else if (outfit_isAmmo(o)) return &o->u.amm.dmg;
   return NULL;
}
/**
 * @brief Gets the outfit's delay.
 *    @param o Outfit to get information from.
 */
double outfit_delay( const Outfit* o )
{
   if (outfit_isBolt(o)) return o->u.blt.delay;
   else if (outfit_isBeam(o)) return o->u.bem.delay;
   else if (outfit_isLauncher(o)) return o->u.lau.delay;
   else if (outfit_isFighterBay(o)) return o->u.bay.delay;
   return -1;
}
/**
 * @brief Gets the outfit's ammo.
 *    @param o Outfit to get information from.
 */
Outfit* outfit_ammo( const Outfit* o )
{
   if (outfit_isLauncher(o)) return o->u.lau.ammo;
   else if (outfit_isFighterBay(o)) return o->u.bay.ammo;
   return NULL;
}
/**
 * @brief Gets the amount an outfit can hold.
 *    @param o Outfit to get information from.
 */
int outfit_amount( const Outfit* o )
{
   if (outfit_isLauncher(o)) return o->u.lau.amount;
   else if (outfit_isFighterBay(o)) return o->u.bay.amount;
   return -1;
}

/**
 * @brief Gets the outfit's energy usage.
 *    @param o Outfit to get information from.
 */
double outfit_energy( const Outfit* o )
{
   if (outfit_isBolt(o)) return o->u.blt.energy;
   else if (outfit_isBeam(o)) return o->u.bem.energy;
   else if (outfit_isAmmo(o)) return o->u.amm.energy;
   return -1.;
}
/**
 * @brief Gets the outfit's heat generation.
 *    @param o Outfit to get information from.
 */
double outfit_heat( const Outfit* o )
{
   if (outfit_isBolt(o)) return o->u.blt.heat;
   else if (outfit_isAfterburner(o)) return o->u.afb.heat;
   else if (outfit_isBeam(o)) return o->u.bem.heat;
   return -1;
}
/**
 * @brief Gets the outfit's cpu usage.
 *    @param o Outfit to get information from.
 */
double outfit_cpu( const Outfit* o )
{
   return o->cpu;
}
/**
 * @brief Gets the outfit's range.
 *    @param o Outfit to get information from.
 */
double outfit_range( const Outfit* o )
{
   Outfit *amm;
   double at;

   if (outfit_isBolt(o)) return o->u.blt.falloff + (o->u.blt.range - o->u.blt.falloff)/2.;
   else if (outfit_isBeam(o)) return o->u.bem.range;
   else if (outfit_isAmmo(o)) {
      if (o->u.amm.thrust) {
         at = o->u.amm.speed / o->u.amm.thrust;
         if (at < o->u.amm.duration)
            return o->u.amm.speed * (o->u.amm.duration - at / 2.);

         /* Maximum speed will never be reached. */
         return pow2(o->u.amm.duration) * o->u.amm.thrust / 2.;
      }

      return o->u.amm.speed * o->u.amm.duration;
   }
   else if (outfit_isLauncher(o)) {
      amm = outfit_ammo(o);
      if (amm != NULL)
         return outfit_range(amm);
   }
   else if (outfit_isFighterBay(o))
      return INFINITY;
   return -1.;
}
/**
 * @brief Gets the outfit's speed.
 *    @param o Outfit to get information from.
 *    @return Outfit's speed.
 */
double outfit_speed( const Outfit* o )
{
   Outfit *amm;
   double t;
   if (outfit_isBolt(o)) return o->u.blt.speed;
   else if (outfit_isAmmo(o)) {
      if (o->u.amm.thrust == 0)
         return o->u.amm.speed;
      else {     /*Gets the average speed*/
         t = o->u.amm.speed / o->u.amm.thrust; /*time to reach max speed*/
         if (t < o->u.amm.duration)
            return (o->u.amm.thrust * t*t/2 + 
                  o->u.amm.speed*(o->u.amm.duration-t))/o->u.amm.duration;
         else return o->u.amm.thrust * o->u.amm.duration/2;
      }
   }
   else if (outfit_isLauncher(o)) {
      amm = outfit_ammo(o);
      if (amm != NULL)
         return outfit_speed(amm);
   }
   return -1.;
}
/**
 * @brief Gets the outfit's animation spin.
 *    @param o Outfit to get information from.
 *    @return Outfit's animation spin.
 */
double outfit_spin( const Outfit* o )
{
   if (outfit_isBolt(o)) return o->u.blt.spin;
   else if (outfit_isAmmo(o)) return o->u.amm.spin;
   return -1.;
}
/**
 * @brief Gets the outfit's sound.
 *    @param o Outfit to get sound from.
 *    @return Outfit's sound.
 */
int outfit_sound( const Outfit* o )
{
   if (outfit_isBolt(o)) return o->u.blt.sound;
   else if (outfit_isAmmo(o)) return o->u.amm.sound;
   return -1.;
}
/**
 * @brief Gets the outfit's hit sound.
 *    @param o Outfit to get hit sound from.
 *    @return Outfit's hit sound.
 */
int outfit_soundHit( const Outfit* o )
{
   if (outfit_isBolt(o)) return o->u.blt.sound_hit;
   else if (outfit_isAmmo(o)) return o->u.amm.sound_hit;
   return -1.;
}
/**
 * @brief Gets the outfit's duration.
 *    @param o Outfit to get the duration of.
 *    @return Outfit's duration.
 */
double outfit_duration( const Outfit* o )
{
   Outfit *amm;
   if (outfit_isMod(o)) { if (o->u.mod.active) return o->u.mod.duration; }
   else if (outfit_isJammer(o)) return INFINITY;
   else if (outfit_isAfterburner(o)) return INFINITY;
   else if (outfit_isBolt(o)) return (o->u.blt.range / o->u.blt.speed);
   else if (outfit_isBeam(o)) return o->u.bem.duration;
   else if (outfit_isAmmo(o)) return o->u.amm.duration;
   else if (outfit_isLauncher(o)) {
      amm = outfit_ammo(o);
      if (amm != NULL)
         return outfit_duration(amm);
   }
   else if (outfit_isFighterBay(o)) return INFINITY;
   return -1.;
}
/**
 * @brief Gets the outfit's cooldown.
 *    @param o Outfit to get the cooldown of.
 *    @return Outfit's cooldown.
 */
double outfit_cooldown( const Outfit* o )
{
   if (outfit_isMod(o)) { if (o->u.mod.active) return o->u.mod.cooldown; }
   else if (outfit_isJammer(o)) return 0.;
   else if (outfit_isAfterburner(o)) return 0.;
   return -1.;
}


/**
 * @brief Gets the outfit's specific type.
 *
 *    @param o Outfit to get specific type from.
 *    @return The specific type in human readable form.
 */
const char* outfit_getType( const Outfit* o )
{
   const char* outfit_typename[] = {
         "NULL",
         "Bolt Cannon",
         "Beam Cannon",
         "Bolt Turret",
         "Beam Turret",
         "Launcher",
         "Ammunition",
         "Turret Launcher",
         "Ship Modification",
         "Afterburner",
         "Jammer",
         "Fighter Bay",
         "Fighter",
         "Star Map",
         "Local Map",
         "GUI",
         "License"
   };

   /* Name override. */
   if (o->typename != NULL)
      return o->typename;
   return outfit_typename[o->type];
}


/**
 * @brief Gets the outfit's broad type.
 *
 *    @param o Outfit to get the type of.
 *    @return The outfit's broad type in human readable form.
 */
const char* outfit_getTypeBroad( const Outfit* o )
{
   if (outfit_isBolt(o))            return "Bolt Weapon";
   else if (outfit_isBeam(o))       return "Beam Weapon";
   else if (outfit_isLauncher(o))   return "Launcher";
   else if (outfit_isAmmo(o))       return "Ammo";
   else if (outfit_isTurret(o))     return "Turret";
   else if (outfit_isMod(o))        return "Modification";
   else if (outfit_isAfterburner(o)) return "Afterburner";
   else if (outfit_isJammer(o))     return "Jammer";
   else if (outfit_isFighterBay(o)) return "Fighter Bay";
   else if (outfit_isFighter(o))    return "Fighter";
   else if (outfit_isMap(o))        return "Map";
   else if (outfit_isLocalMap(o))   return "Local Map";
   else if (outfit_isGUI(o))        return "GUI";
   else if (outfit_isLicense(o))    return "License";
   else                             return "Unknown";
}


/**
 * @brief Gets a human-readable string describing an ammo outfit's AI.
 *    @param o Ammo outfit.
 *    @return Name of the outfit's AI.
 */
const char* outfit_getAmmoAI( const Outfit *o )
{
   const char *ai_type[] = {
      "Dumb",
      "Seek",
      "Smart"
   };

   if (!outfit_isAmmo(o)) {
      WARN("Outfit '%s' is not an ammo outfit", o->name);
      return NULL;
   }

   return ai_type[o->u.amm.ai];
}


/**
 * @brief Checks to see if an outfit fits a slot.
 *
 *    @param o Outfit to see if fits in a slot.
 *    @param s Slot to see if outfit fits in.
 *    @return 1 if outfit fits the slot, 0 otherwise.
 */
int outfit_fitsSlot( const Outfit* o, const OutfitSlot* s )
{
   const OutfitSlot *os;
   os = &o->slot;

   /* Outfit must have valid slot type. */
   if ((os->type == OUTFIT_SLOT_NULL) ||
      (os->type == OUTFIT_SLOT_NA))
      return 0;

   /* Outfit type must match outfit slot. */
   if (os->type != s->type)
      return 0;

   /* Must match slot property. */
   if (os->spid != 0)
      if (s->spid != os->spid)
         return 0;

   /* Exclusive only match property. */
   if (s->exclusive)
      if (s->spid != os->spid)
         return 0;

   /* Must have valid slot size. */
   if (os->size == OUTFIT_SLOT_SIZE_NA)
      return 0;

   /* It doesn't fit. */
   if (os->size > s->size)
      return 0;

   /* It meets all criteria. */
   return 1;
}


/**
 * @brief Checks to see if an outfit fits a slot type (ignoring size).
 *
 *    @param o Outfit to see if fits in a slot.
 *    @param s Slot to see if outfit fits in.
 *    @return 1 if outfit fits the slot, 0 otherwise.
 */
int outfit_fitsSlotType( const Outfit* o, const OutfitSlot* s )
{
   const OutfitSlot *os;
   os = &o->slot;

   /* Outfit must have valid slot type. */
   if ((os->type == OUTFIT_SLOT_NULL) ||
      (os->type == OUTFIT_SLOT_NA))
      return 0;

   /* Outfit type must match outfit slot. */
   if (os->type != s->type)
      return 0;

   /* It meets all criteria. */
   return 1;
}


/**
 * @brief Frees an outfit slot.
 *
 *    @param s Slot to free.
 */
void outfit_freeSlot( OutfitSlot* s )
{
   (void) s;
}


#define O_CMP(s,t) \
if (strcasecmp(buf,(s))==0) return t /**< Define to help with outfit_strToOutfitType. */
/**
 * @brief Gets the outfit type from a human readable string.
 *
 *    @param buf String to extract outfit type from.
 *    @return Outfit type stored in buf.
 */
static OutfitType outfit_strToOutfitType( char *buf )
{
   O_CMP("bolt",           OUTFIT_TYPE_BOLT);
   O_CMP("beam",           OUTFIT_TYPE_BEAM);
   O_CMP("turret bolt",    OUTFIT_TYPE_TURRET_BOLT);
   O_CMP("turret beam",    OUTFIT_TYPE_TURRET_BEAM);
   O_CMP("launcher",       OUTFIT_TYPE_LAUNCHER);
   O_CMP("ammo",           OUTFIT_TYPE_AMMO);
   O_CMP("turret launcher",OUTFIT_TYPE_TURRET_LAUNCHER);
   O_CMP("modification",   OUTFIT_TYPE_MODIFICATION);
   O_CMP("afterburner",    OUTFIT_TYPE_AFTERBURNER);
   O_CMP("fighter bay",    OUTFIT_TYPE_FIGHTER_BAY);
   O_CMP("fighter",        OUTFIT_TYPE_FIGHTER);
   O_CMP("jammer",         OUTFIT_TYPE_JAMMER);
   O_CMP("map",            OUTFIT_TYPE_MAP);
   O_CMP("localmap",       OUTFIT_TYPE_LOCALMAP);
   O_CMP("license",        OUTFIT_TYPE_LICENSE);
   O_CMP("gui",            OUTFIT_TYPE_GUI);

   WARN("Invalid outfit type: '%s'",buf);
   return  OUTFIT_TYPE_NULL;
}
#undef O_CMP


/**
 * @brief Parses a damage node.
 *
 * Example damage node would be:
 * @code
 * <damage type="kinetic">10</damage>
 * @endcode
 *
 *    @param[out] dtype Stores the damage type here.
 *    @param[out] dmg Stores the damage here.
 *    @param[in] node Node to parse damage from.
 *    @return 0 on success.
 */
static int outfit_parseDamage( Damage *dmg, xmlNodePtr node )
{
   char *buf;
   xmlNodePtr cur;

   /* Defaults. */
   dmg->type         = dtype_get("normal");
   dmg->damage       = 0.;
   dmg->penetration  = 0.;
   dmg->disable      = 0.;

   cur = node->xmlChildrenNode;
   do {
      xml_onlyNodes( cur );

      /* Core properties. */
      xmlr_float( cur, "penetrate", dmg->penetration );
      xmlr_float( cur, "physical",  dmg->damage );
      xmlr_float( cur, "disable",   dmg->disable );

      /* Get type */
      if (xml_isNode(cur,"type")) {
         buf         = xml_get( cur );
         dmg->type   = dtype_get(buf);
         if (dmg->type < 0) { /* Case damage type in outfit.xml that isn't in damagetype.xml */
            dmg->type = 0;
            WARN("Unknown damage type '%s'", buf);
         }
         continue;
      }
      WARN("Damage has unknown node '%s'", cur->name);
   } while (xml_nextNode(cur));

   /* Normalize. */
   dmg->penetration /= 100.;

   return 0;
}


/**
 * @brief Parses the specific area for a bolt weapon and loads it into Outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSBolt( Outfit* temp, const xmlNodePtr parent )
{
   xmlNodePtr node;
   char *buf;
   double C, area;
   int l;

   /* Defaults */
   temp->u.blt.spfx_armour    = -1;
   temp->u.blt.spfx_shield    = -1;
   temp->u.blt.sound          = -1;
   temp->u.blt.sound_hit      = -1;
   temp->u.blt.falloff        = -1.;
   temp->u.blt.ew_lockon      = 1.;

   node = parent->xmlChildrenNode;
   do { /* load all the data */
      xml_onlyNodes(node);
      xmlr_float(node,"speed",temp->u.blt.speed);
      xmlr_float(node,"delay",temp->u.blt.delay);
      xmlr_float(node,"ew_lockon",temp->u.blt.ew_lockon);
      xmlr_float(node,"energy",temp->u.blt.energy);
      xmlr_float(node,"heatup",temp->u.blt.heatup);
      xmlr_float(node,"track",temp->u.blt.track);
      xmlr_float(node,"swivel",temp->u.blt.swivel);
      if (xml_isNode(node,"range")) {
         buf = xml_nodeProp(node,"blowup");
         if (buf != NULL) {
            if (strcmp(buf,"armour")==0)
               outfit_setProp(temp, OUTFIT_PROP_WEAP_BLOWUP_SHIELD);
            else if (strcmp(buf,"shield")==0)
               outfit_setProp(temp, OUTFIT_PROP_WEAP_BLOWUP_ARMOUR);
            else
               WARN("Outfit '%s' has invalid blowup property: '%s'",
                     temp->name, buf );
            free(buf);
         }
         temp->u.blt.range = xml_getFloat(node);
         continue;
      }
      xmlr_float(node,"falloff",temp->u.blt.falloff);

      /* Graphics. */
      if (xml_isNode(node,"gfx")) {
         temp->u.blt.gfx_space = xml_parseTexture( node,
               OUTFIT_GFX_PATH"space/%s.png", 6, 6,
               OPENGL_TEX_MAPTRANS | OPENGL_TEX_MIPMAPS );
         xmlr_attr(node, "spin", buf);
         if (buf != NULL) {
            outfit_setProp( temp, OUTFIT_PROP_WEAP_SPIN );
            temp->u.blt.spin = atof( buf );
            free(buf);
         }
         continue;
      }
      if (xml_isNode(node,"gfx_end")) {
         if (!conf.interpolate)
            continue;
         temp->u.blt.gfx_end = xml_parseTexture( node,
               OUTFIT_GFX_PATH"space/%s.png", 6, 6,
               OPENGL_TEX_MAPTRANS | OPENGL_TEX_MIPMAPS );
         continue;
      }

      /* Special effects. */
      if (xml_isNode(node,"spfx_shield")) {
         temp->u.blt.spfx_shield = spfx_get(xml_get(node));
         continue;
      }
      if (xml_isNode(node,"spfx_armour")) {
         temp->u.blt.spfx_armour = spfx_get(xml_get(node));
         continue;
      }

      /* Misc. */
      if (xml_isNode(node,"sound")) {
         temp->u.blt.sound = sound_get( xml_get(node) );
         continue;
      }
      if (xml_isNode(node,"sound_hit")) {
         temp->u.blt.sound_hit = sound_get( xml_get(node) );
         continue;
      }
      if (xml_isNode(node,"damage")) {
         outfit_parseDamage( &temp->u.blt.dmg, node );
         continue;
      }
      WARN("Outfit '%s' has unknown node '%s'",temp->name, node->name);
   } while (xml_nextNode(node));

   /* If not defined assume maximum. */
   if (temp->u.blt.falloff < 0.)
      temp->u.blt.falloff = temp->u.blt.range;

   /* Post processing. */
   temp->u.blt.swivel  *= M_PI/180.;
   if (outfit_isTurret(temp))
      temp->u.blt.swivel = M_PI;
   /*
    *         dT Mthermal - Qweap
    * Hweap = ----------------------
    *                tweap
    */
   C = pilot_heatCalcOutfitC(temp);
   area = pilot_heatCalcOutfitArea(temp);
   temp->u.blt.heat     = ((800.-CONST_SPACE_STAR_TEMP)*C +
            STEEL_HEAT_CONDUCTIVITY * ((800-CONST_SPACE_STAR_TEMP) * area)) /
         temp->u.blt.heatup * temp->u.blt.delay;

   /* Set default outfit size if necessary. */
   if (temp->slot.size == OUTFIT_SLOT_SIZE_NA)
      outfit_setDefaultSize( temp );

   /* Set short description. */
   temp->desc_short = malloc( OUTFIT_SHORTDESC_MAX );
   l = nsnprintf( temp->desc_short, OUTFIT_SHORTDESC_MAX,
         "%s [%s]\n"
         "%.0f CPU\n"
         "%.0f%% Penetration\n"
         "%.2f DPS [%.0f Damage]\n",
         outfit_getType(temp), dtype_damageTypeToStr(temp->u.blt.dmg.type),
         temp->cpu,
         temp->u.blt.dmg.penetration*100.,
         1./temp->u.blt.delay * temp->u.blt.dmg.damage, temp->u.blt.dmg.damage );
   if (temp->u.blt.dmg.disable > 0.) {
      l += nsnprintf( &temp->desc_short[l], OUTFIT_SHORTDESC_MAX-l,
         "%.2f Disable/s [%.0f Disable]\n",
         1./temp->u.blt.delay * temp->u.blt.dmg.disable, temp->u.blt.dmg.disable );
   }
   l += nsnprintf( &temp->desc_short[l], OUTFIT_SHORTDESC_MAX-l,
         "%.1f Shots Per Second\n"
         "%.1f EPS [%.0f Energy]\n"
         "%.0f Range\n"
         "%.1f second heat up",
         1./temp->u.blt.delay,
         1./temp->u.blt.delay * temp->u.blt.energy, temp->u.blt.energy,
         temp->u.blt.range,
         temp->u.blt.heatup);
   if (!outfit_isTurret(temp)) {
      l += nsnprintf( &temp->desc_short[l], OUTFIT_SHORTDESC_MAX-l,
         "\n%.1f degree swivel",
         temp->u.blt.swivel*180./M_PI );
   }


#define MELEMENT(o,s) \
if (o) WARN("Outfit '%s' missing/invalid '"s"' element", temp->name) /**< Define to help check for data errors. */
   MELEMENT(temp->u.blt.gfx_space==NULL,"gfx");
   MELEMENT(temp->u.blt.spfx_shield==-1,"spfx_shield");
   MELEMENT(temp->u.blt.spfx_armour==-1,"spfx_armour");
   MELEMENT((sound_disabled!=0) && (temp->u.blt.sound<0),"sound");
   MELEMENT(temp->mass==0.,"mass");
   MELEMENT(temp->u.blt.delay==0,"delay");
   MELEMENT(temp->u.blt.speed==0,"speed");
   MELEMENT(temp->u.blt.range==0,"range");
   MELEMENT(temp->u.blt.dmg.damage==0,"damage");
   MELEMENT(temp->u.blt.energy==0.,"energy");
   MELEMENT(temp->cpu==0.,"cpu");
   MELEMENT(temp->u.blt.falloff > temp->u.blt.range,"falloff");
   MELEMENT(temp->u.blt.heatup==0.,"heatup");
   MELEMENT(((temp->u.blt.swivel > 0.) || outfit_isTurret(temp)) && (temp->u.blt.track==0.),"track");
#undef MELEMENT
}


/**
 * @brief Parses the beam weapon specifics of an outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSBeam( Outfit* temp, const xmlNodePtr parent )
{
   int l;
   xmlNodePtr node;
   double C, area;
   char *prop;

   /* Defaults. */
   temp->u.bem.spfx_armour = -1;
   temp->u.bem.spfx_shield = -1;
   temp->u.bem.sound_warmup = -1;
   temp->u.bem.sound = -1;
   temp->u.bem.sound_off = -1;

   node = parent->xmlChildrenNode;
   do { /* load all the data */
      xml_onlyNodes(node);
      xmlr_float(node,"range",temp->u.bem.range);
      xmlr_float(node,"turn",temp->u.bem.turn);
      xmlr_float(node,"energy",temp->u.bem.energy);
      xmlr_float(node,"delay",temp->u.bem.delay);
      xmlr_float(node,"warmup",temp->u.bem.warmup);
      xmlr_float(node,"heatup",temp->u.bem.heatup);

      if (xml_isNode(node, "duration")) {
         prop = xml_nodeProp(node, "min");
         if (prop != NULL) {
            temp->u.bem.min_duration = atof(prop);
            free(prop);
         }
         temp->u.bem.duration = xml_getFloat(node);
         continue;
      }

      if (xml_isNode(node,"damage")) {
         outfit_parseDamage( &temp->u.bem.dmg, node );
         continue;
      }

      /* Graphic stuff. */
      if (xml_isNode(node,"gfx")) {
         temp->u.bem.gfx = xml_parseTexture( node,
               OUTFIT_GFX_PATH"space/%s.png", 1, 1, OPENGL_TEX_MIPMAPS );
         continue;
      }
      if (xml_isNode(node,"spfx_armour")) {
         temp->u.bem.spfx_armour = spfx_get(xml_get(node));
         continue;
      }
      if (xml_isNode(node,"spfx_shield")) {
         temp->u.bem.spfx_shield = spfx_get(xml_get(node));
         continue;
      }

      /* Sound stuff. */
      if (xml_isNode(node,"sound_warmup")) {
         temp->u.bem.sound_warmup = sound_get( xml_get(node) );
         continue;
      }
      if (xml_isNode(node,"sound")) {
         temp->u.bem.sound = sound_get( xml_get(node) );
         continue;
      }
      if (xml_isNode(node,"sound_off")) {
         temp->u.bem.sound_off = sound_get( xml_get(node) );
         continue;
      }
      WARN("Outfit '%s' has unknown node '%s'",temp->name, node->name);
   } while (xml_nextNode(node));

   /* Post processing. */
   temp->u.bem.turn     *= M_PI/180.; /* Convert to rad/s. */
   C = pilot_heatCalcOutfitC(temp);
   area = pilot_heatCalcOutfitArea(temp);
   temp->u.bem.heat     = ((800.-CONST_SPACE_STAR_TEMP)*C +
            STEEL_HEAT_CONDUCTIVITY * ((800-CONST_SPACE_STAR_TEMP) * area)) /
         temp->u.bem.heatup;

   /* Set default outfit size if necessary. */
   if (temp->slot.size == OUTFIT_SLOT_SIZE_NA)
      outfit_setDefaultSize( temp );

   /* Set short description. */
   temp->desc_short = malloc( OUTFIT_SHORTDESC_MAX );
   l = nsnprintf( temp->desc_short, OUTFIT_SHORTDESC_MAX,
         "%s\n"
         "%.0f CPU\n"
         "%.0f%% Penetration\n"
         "%.2f DPS [%s]\n",
         outfit_getType(temp),
         temp->cpu,
         temp->u.bem.dmg.penetration*100.,
         temp->u.bem.dmg.damage, dtype_damageTypeToStr(temp->u.bem.dmg.type) );
   if (temp->u.blt.dmg.disable > 0.) {
      l += nsnprintf( &temp->desc_short[l], OUTFIT_SHORTDESC_MAX-l,
         "%.0f Disable/s\n",
         temp->u.bem.dmg.disable );
   }
   l += nsnprintf( &temp->desc_short[l], OUTFIT_SHORTDESC_MAX-l,
         "%.1f EPS\n"
         "%.1f Duration %.1f Cooldown\n"
         "%.0f Range\n"
         "%.1f second heat up",
         temp->u.bem.energy,
         temp->u.bem.duration, temp->u.bem.delay,
         temp->u.bem.range,
         temp->u.bem.heatup);

#define MELEMENT(o,s) \
if (o) WARN("Outfit '%s' missing/invalid '"s"' element", temp->name) /**< Define to help check for data errors. */
   MELEMENT(temp->u.bem.gfx==NULL,"gfx");
   MELEMENT(temp->u.bem.spfx_shield==-1,"spfx_shield");
   MELEMENT(temp->u.bem.spfx_armour==-1,"spfx_armour");
   MELEMENT((sound_disabled!=0) && (temp->u.bem.warmup > 0.) && (temp->u.bem.sound<0),"sound_warmup");
   MELEMENT((sound_disabled!=0) && (temp->u.bem.sound<0),"sound");
   MELEMENT((sound_disabled!=0) && (temp->u.bem.sound_off<0),"sound_off");
   MELEMENT(temp->u.bem.delay==0,"delay");
   MELEMENT(temp->u.bem.duration==0,"duration");
   MELEMENT(temp->u.bem.min_duration < 0,"duration");
   MELEMENT(temp->u.bem.range==0,"range");
   MELEMENT((temp->type!=OUTFIT_TYPE_BEAM) && (temp->u.bem.turn==0),"turn");
   MELEMENT(temp->u.bem.energy==0.,"energy");
   MELEMENT(temp->cpu==0.,"cpu");
   MELEMENT(temp->u.bem.dmg.damage==0,"damage");
   MELEMENT(temp->u.bem.heatup==0.,"heatup");
#undef MELEMENT
}


/**
 * @brief Parses the specific area for a launcher and loads it into Outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSLauncher( Outfit* temp, const xmlNodePtr parent )
{
   xmlNodePtr node;

   node  = parent->xmlChildrenNode;
   do { /* load all the data */
      xml_onlyNodes(node);
      xmlr_float(node,"delay",temp->u.lau.delay);
      xmlr_strd(node,"ammo",temp->u.lau.ammo_name);
      xmlr_int(node,"amount",temp->u.lau.amount);
      xmlr_float(node,"ew_target",temp->u.lau.ew_target);
      xmlr_float(node,"lockon",temp->u.lau.lockon);
      if (!outfit_isTurret(temp))
         xmlr_float(node,"arc",temp->u.lau.arc);
      WARN("Outfit '%s' has unknown node '%s'",temp->name, node->name);
   } while (xml_nextNode(node));

   /* Post processing. */
   temp->u.lau.arc *= M_PI/180.;
   temp->u.lau.ew_target2 = pow2( temp->u.lau.ew_target );

   /* Set default outfit size if necessary. */
   if (temp->slot.size == OUTFIT_SLOT_SIZE_NA)
      outfit_setDefaultSize( temp );

#define MELEMENT(o,s) \
if (o) WARN("Outfit '%s' missing '"s"' element", temp->name) /**< Define to help check for data errors. */
   MELEMENT(temp->u.lau.ammo_name==NULL,"ammo");
   MELEMENT(temp->u.lau.delay==0.,"delay");
   MELEMENT(temp->cpu==0.,"cpu");
   MELEMENT(temp->u.lau.amount==0.,"amount");
#undef MELEMENT
}


/**
 * @brief Parses the specific area for a weapon and loads it into Outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSAmmo( Outfit* temp, const xmlNodePtr parent )
{
   xmlNodePtr node;
   char *buf;
   int l;

   node = parent->xmlChildrenNode;

   /* Defaults. */
   temp->slot.type         = OUTFIT_SLOT_NA;
   temp->slot.size         = OUTFIT_SLOT_SIZE_NA;
   temp->u.amm.spfx_armour = -1;
   temp->u.amm.spfx_shield = -1;
   temp->u.amm.sound       = -1;
   temp->u.amm.sound_hit   = -1;
   temp->u.amm.ai          = -1;

   do { /* load all the data */
      xml_onlyNodes(node);
      /* Basic */
      if (xml_isNode(node,"duration")) {
         buf = xml_nodeProp(node,"blowup");
         if (buf != NULL) {
            if (strcmp(buf,"armour")==0)
               outfit_setProp(temp, OUTFIT_PROP_WEAP_BLOWUP_SHIELD);
            else if (strcmp(buf,"shield")==0)
               outfit_setProp(temp, OUTFIT_PROP_WEAP_BLOWUP_ARMOUR);
            else
               WARN("Outfit '%s' has invalid blowup property: '%s'",
                     temp->name, buf );
            free(buf);
         }
         temp->u.amm.duration = xml_getFloat(node);
         continue;
      }
      xmlr_float(node,"resist",temp->u.amm.resist);
      /* Movement */
      xmlr_float(node,"thrust",temp->u.amm.thrust);
      xmlr_float(node,"turn",temp->u.amm.turn);
      xmlr_float(node,"speed",temp->u.amm.speed);
      xmlr_float(node,"energy",temp->u.amm.energy);
      if (xml_isNode(node,"gfx")) {
         temp->u.amm.gfx_space = xml_parseTexture( node,
               OUTFIT_GFX_PATH"space/%s.png", 6, 6,
               OPENGL_TEX_MAPTRANS | OPENGL_TEX_MIPMAPS );
         xmlr_attr(node, "spin", buf);
         if (buf != NULL) {
            outfit_setProp( temp, OUTFIT_PROP_WEAP_SPIN );
            temp->u.amm.spin = atof( buf );
            free(buf);
         }
         continue;
      }
      if (xml_isNode(node,"spfx_armour")) {
         temp->u.amm.spfx_armour = spfx_get(xml_get(node));
         continue;
      }
      if (xml_isNode(node,"spfx_shield")) {
         temp->u.amm.spfx_shield = spfx_get(xml_get(node));
         continue;
      }
      if (xml_isNode(node,"sound")) {
         temp->u.amm.sound = sound_get( xml_get(node) );
         continue;
      }
      if (xml_isNode(node,"sound_hit")) {
         temp->u.amm.sound_hit = sound_get( xml_get(node) );
         continue;
      }
      if (xml_isNode(node,"damage")) {
         outfit_parseDamage( &temp->u.amm.dmg, node );
         continue;
      }
      if (xml_isNode(node,"ai")) {
         buf = xml_get(node);
         if (buf != NULL) {

            if (strcmp(buf,"dumb")==0)
               temp->u.amm.ai = AMMO_AI_DUMB;
            else if (strcmp(buf,"seek")==0)
               temp->u.amm.ai = AMMO_AI_SEEK;
            else if (strcmp(buf,"smart")==0)
               temp->u.amm.ai = AMMO_AI_SMART;
            else
               WARN("Ammo '%s' has unknown ai type '%s'.", temp->name, buf);
         }
         continue;
      }
      WARN("Outfit '%s' has unknown node '%s'",temp->name, node->name);
   } while (xml_nextNode(node));

   /* Post-processing */
   temp->u.amm.resist /= 100.; /* Set it in per one */
   temp->u.amm.turn   *= M_PI/180.; /* Convert to rad/s. */

   /* Set short description. */
   temp->desc_short = malloc( OUTFIT_SHORTDESC_MAX );
   l = nsnprintf( temp->desc_short, OUTFIT_SHORTDESC_MAX,
         "%s\n"
         "%.0f%% Penetration\n"
         "%.0f Damage [%s]\n",
         outfit_getType(temp),
         temp->u.amm.dmg.penetration*100.,
         temp->u.amm.dmg.damage, dtype_damageTypeToStr(temp->u.amm.dmg.type) );
   if (temp->u.blt.dmg.disable > 0.) {
      l += nsnprintf( &temp->desc_short[l], OUTFIT_SHORTDESC_MAX-l,
         "%.0f Disable\n",
         temp->u.amm.dmg.disable );
   }
   l += nsnprintf( &temp->desc_short[l], OUTFIT_SHORTDESC_MAX-l,
         "%.0f Energy\n"
         "%.0f Maximum Speed\n"
         "%.0f%% Jam resistance\n"
         "%.1f duration",
         temp->u.amm.energy,
         temp->u.amm.speed,
         temp->u.amm.resist,
         temp->u.amm.duration );

#define MELEMENT(o,s) \
if (o) WARN("Outfit '%s' missing/invalid '"s"' element", temp->name) /**< Define to help check for data errors. */
   MELEMENT(temp->mass==0.,"mass");
   MELEMENT(temp->u.amm.gfx_space==NULL,"gfx");
   MELEMENT(temp->u.amm.spfx_shield==-1,"spfx_shield");
   MELEMENT(temp->u.amm.spfx_armour==-1,"spfx_armour");
   MELEMENT((sound_disabled!=0) && (temp->u.amm.sound<0),"sound");
   /* MELEMENT(temp->u.amm.thrust==0,"thrust"); */
   /* Dumb missiles don't need everything */
   if (outfit_isSeeker(temp)) {
      MELEMENT(temp->u.amm.turn==0,"turn");
   }
   MELEMENT(temp->u.amm.speed==0,"speed");
   MELEMENT(temp->u.amm.duration==0,"duration");
   MELEMENT(temp->u.amm.dmg.damage==0,"damage");
   /*MELEMENT(temp->u.amm.energy==0.,"energy");*/
   MELEMENT(temp->cpu!=0.,"cpu");
#undef MELEMENT
}


/**
 * @brief Parses the modification tidbits of the outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSMod( Outfit* temp, const xmlNodePtr parent )
{
   int i;
   xmlNodePtr node;
   ShipStatList *ll;
   char *buf;
   node = parent->children;

   do { /* load all the data */
      xml_onlyNodes(node);
      if (xml_isNode(node,"active")) {
         xmlr_attr(node, "cooldown", buf);
         if (buf != NULL) {
            temp->u.mod.cooldown = atof( buf );
            free(buf);
         }
         temp->u.mod.duration = xml_getFloat(node);
         temp->u.mod.active   = 1;
         continue;
      }
      /* movement */
      xmlr_float(node,"thrust",temp->u.mod.thrust);
      xmlr_float(node,"turn",temp->u.mod.turn);
      xmlr_float(node,"speed",temp->u.mod.speed);
      /* health */
      xmlr_float(node,"armour",temp->u.mod.armour);
      xmlr_float(node,"shield",temp->u.mod.shield);
      xmlr_float(node,"energy",temp->u.mod.energy);
      xmlr_float(node,"fuel",temp->u.mod.fuel);
      xmlr_float(node,"armour_regen", temp->u.mod.armour_regen );
      xmlr_float(node,"shield_regen", temp->u.mod.shield_regen );
      xmlr_float(node,"energy_regen", temp->u.mod.energy_regen );
      xmlr_float(node,"energy_loss", temp->u.mod.energy_loss );
      xmlr_float(node,"absorb", temp->u.mod.absorb );
      /* misc */
      xmlr_float(node,"cargo",temp->u.mod.cargo);
      xmlr_float(node,"crew_rel", temp->u.mod.crew_rel);
      xmlr_float(node,"mass_rel",temp->u.mod.mass_rel);
      /* Stats. */
      ll = ss_listFromXML( node );
      if (ll != NULL) {
         ll->next          = temp->u.mod.stats;
         temp->u.mod.stats = ll;
         continue;
      }

      WARN("Outfit '%s' has unknown node '%s'",temp->name, node->name);
   } while (xml_nextNode(node));

   /* Set default outfit size if necessary. */
   if (temp->slot.size == OUTFIT_SLOT_SIZE_NA)
      outfit_setDefaultSize( temp );

   /* Set short description. */
   temp->desc_short = malloc( OUTFIT_SHORTDESC_MAX );
   i = nsnprintf( temp->desc_short, OUTFIT_SHORTDESC_MAX,
         "%s"
         "%s",
         outfit_getType(temp),
         (temp->u.mod.active) ? "\n\erActivated Outfit\e0" : "" );

#define DESC_ADD(x, s, n, c) \
if ((x) != 0.) \
   i += nsnprintf( &temp->desc_short[i], OUTFIT_SHORTDESC_MAX-i, \
         "\n\e%c%+."n"f "s"\e0", c, x )
#define DESC_ADD0(x, s)    DESC_ADD( x, s, "0", ((x)>0)?'D':'r' )
#define DESC_ADD1(x, s)    DESC_ADD( x, s, "1", ((x)>0)?'D':'r' )
   DESC_ADD0( temp->cpu, "CPU" );
   DESC_ADD0( temp->u.mod.thrust, "Thrust" );
   DESC_ADD0( temp->u.mod.turn, "Turn Rate" );
   DESC_ADD0( temp->u.mod.speed, "Maximum Speed" );
   DESC_ADD0( temp->u.mod.armour, "Armour" );
   DESC_ADD0( temp->u.mod.shield, "Shield" );
   DESC_ADD0( temp->u.mod.energy, "Energy" );
   DESC_ADD0( temp->u.mod.fuel, "Fuel" );
   DESC_ADD1( temp->u.mod.armour_regen, "Armour Per Second" );
   DESC_ADD1( temp->u.mod.shield_regen, "Shield Per Second" );
   DESC_ADD1( temp->u.mod.energy_regen, "Energy Per Second" );
   DESC_ADD0( temp->u.mod.absorb, "Absorption" );
   DESC_ADD0( temp->u.mod.cargo, "Cargo" );
   DESC_ADD0( temp->u.mod.crew_rel, "%% Crew" );
   DESC_ADD0( temp->u.mod.mass_rel, "%% Mass" );
#undef DESC_ADD1
#undef DESC_ADD0
#undef DESC_ADD
   /*i +=*/ ss_statsListDesc( temp->u.mod.stats,
         &temp->desc_short[i], OUTFIT_SHORTDESC_MAX-i, 1 );

   /* More processing. */
   temp->u.mod.turn       *= M_PI / 180.;
   temp->u.mod.absorb     /= 100.;
   temp->u.mod.turn_rel   /= 100.;
   temp->u.mod.speed_rel  /= 100.;
   temp->u.mod.armour_rel /= 100.;
   temp->u.mod.shield_rel /= 100.;
   temp->u.mod.energy_rel /= 100.;
   temp->u.mod.mass_rel   /= 100.;
   temp->u.mod.crew_rel   /= 100.;
}


/**
 * @brief Parses the afterburner tidbits of the outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSAfterburner( Outfit* temp, const xmlNodePtr parent )
{
   xmlNodePtr node;
   node = parent->children;
   double C, area;

   /* Defaults. */
   temp->u.afb.sound = -1;
   temp->u.afb.sound_on = -1;
   temp->u.afb.sound_off = -1;

   /* must be >= 1. */
   temp->u.afb.thrust = 1.;
   temp->u.afb.speed  = 1.;

   do { /* parse the data */
      xml_onlyNodes(node);
      xmlr_float(node,"rumble",temp->u.afb.rumble);
      if (xml_isNode(node,"sound_on")) {
         temp->u.afb.sound_on = sound_get( xml_get(node) );
         continue;
      }
      if (xml_isNode(node,"sound")) {
         temp->u.afb.sound = sound_get( xml_get(node) );
         continue;
      }
      if (xml_isNode(node,"sound_off")) {
         temp->u.afb.sound_off = sound_get( xml_get(node) );
         continue;
      }
      xmlr_float(node,"thrust",temp->u.afb.thrust);
      xmlr_float(node,"speed",temp->u.afb.speed);
      xmlr_float(node,"energy",temp->u.afb.energy);
      xmlr_float(node,"mass_limit",temp->u.afb.mass_limit);
      xmlr_float(node,"heatup",temp->u.afb.heatup);
      xmlr_float(node,"heat_cap",temp->u.afb.heat_cap);
      xmlr_float(node,"heat_base",temp->u.afb.heat_base);
      WARN("Outfit '%s' has unknown node '%s'",temp->name, node->name);
   } while (xml_nextNode(node));

   /* Set short description. */
   temp->desc_short = malloc( OUTFIT_SHORTDESC_MAX );
   nsnprintf( temp->desc_short, OUTFIT_SHORTDESC_MAX,
         "%s\n"
         "\erActivated Outfit\e0\n"
         "%.0f CPU\n"
         "Only one can be equipped\n"
         "%.0f Maximum Effective Mass\n"
         "%.0f%% Thrust\n"
         "%.0f%% Maximum Speed\n"
         "%.1f EPS\n"
         "%.1f Rumble",
         outfit_getType(temp),
         temp->cpu,
         temp->u.afb.mass_limit,
         temp->u.afb.thrust + 100.,
         temp->u.afb.speed + 100.,
         temp->u.afb.energy,
         temp->u.afb.rumble );

   /* Post processing. */
   temp->u.afb.thrust /= 100.;
   temp->u.afb.speed  /= 100.;
   C = pilot_heatCalcOutfitC(temp);
   area = pilot_heatCalcOutfitArea(temp);
   temp->u.afb.heat    = ((800.-CONST_SPACE_STAR_TEMP)*C +
            STEEL_HEAT_CONDUCTIVITY * ((800-CONST_SPACE_STAR_TEMP) * area)) /
         temp->u.afb.heatup;

   /* Set default outfit size if necessary. */
   if (temp->slot.size == OUTFIT_SLOT_SIZE_NA)
      outfit_setDefaultSize( temp );

#define MELEMENT(o,s) \
if (o) WARN("Outfit '%s' missing/invalid '"s"' element", temp->name) /**< Define to help check for data errors. */
   MELEMENT(temp->u.afb.thrust==0.,"thrust");
   MELEMENT(temp->u.afb.speed==0.,"speed");
   MELEMENT(temp->u.afb.energy==0.,"energy");
   MELEMENT(temp->cpu==0.,"cpu");
   MELEMENT(temp->u.afb.mass_limit==0.,"mass_limit");
   MELEMENT(temp->u.afb.heatup==0.,"heatup");
#undef MELEMENT
}

/**
 * @brief Parses the fighter bay tidbits of the outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSFighterBay( Outfit *temp, const xmlNodePtr parent )
{
   xmlNodePtr node;
   node = parent->children;

   do {
      xml_onlyNodes(node);
      xmlr_int(node,"delay",temp->u.bay.delay);
      xmlr_strd(node,"ammo",temp->u.bay.ammo_name);
      xmlr_int(node,"amount",temp->u.bay.amount);
      WARN("Outfit '%s' has unknown node '%s'",temp->name, node->name);
   } while (xml_nextNode(node));

   /* Set default outfit size if necessary. */
   if (temp->slot.size == OUTFIT_SLOT_SIZE_NA)
      outfit_setDefaultSize( temp );

   /* Set short description. */
   temp->desc_short = malloc( OUTFIT_SHORTDESC_MAX );
   nsnprintf( temp->desc_short, OUTFIT_SHORTDESC_MAX,
         "%s\n"
         "%.0f CPU\n"
         "%.1f Launches Per Second\n"
         "Holds %d %s",
         outfit_getType(temp),
         temp->cpu,
         1./temp->u.bay.delay,
         temp->u.bay.amount, temp->u.bay.ammo_name );

#define MELEMENT(o,s) \
if (o) WARN("Outfit '%s' missing/invalid '"s"' element", temp->name) /**< Define to help check for data errors. */
   MELEMENT(temp->u.bay.delay==0,"delay");
   MELEMENT(temp->cpu==0.,"cpu");
   MELEMENT(temp->u.bay.ammo_name==NULL,"ammo");
   MELEMENT(temp->u.bay.amount==0,"amount");
#undef MELEMENT
}

/**
 * @brief Parses the fighter tidbits of the outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSFighter( Outfit *temp, const xmlNodePtr parent )
{
   xmlNodePtr node;
   node = parent->children;

   temp->slot.type         = OUTFIT_SLOT_NA;
   temp->slot.size         = OUTFIT_SLOT_SIZE_NA;

   do {
      xml_onlyNodes(node);
      xmlr_strd(node,"ship",temp->u.fig.ship);
      WARN("Outfit '%s' has unknown node '%s'",temp->name, node->name);
   } while (xml_nextNode(node));

   /* Set short description. */
   temp->desc_short = malloc( OUTFIT_SHORTDESC_MAX );
   nsnprintf( temp->desc_short, OUTFIT_SHORTDESC_MAX,
         "%s",
         outfit_getType(temp) );

#define MELEMENT(o,s) \
if (o) WARN("Outfit '%s' missing/invalid '"s"' element", temp->name)
/**< Define to help check for data errors. */
   MELEMENT(temp->u.fig.ship==NULL,"ship");
   MELEMENT(temp->cpu!=0.,"cpu");
#undef MELEMENT
}

/**
 * @brief Parses the map tidbits of the outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSMap( Outfit *temp, const xmlNodePtr parent )
{
   int i, j;
   xmlNodePtr node, cur;
   void *buf;
   StarSystem *sys, *system_stack;
   Planet *asset;
   JumpPoint *jump;
   int nsys;

   node = parent->children;

   temp->slot.type         = OUTFIT_SLOT_NA;
   temp->slot.size         = OUTFIT_SLOT_SIZE_NA;

   temp->u.map->systems = array_create(StarSystem*);
   temp->u.map->assets  = array_create(Planet*);
   temp->u.map->jumps   = array_create(JumpPoint*);

   do {
      xml_onlyNodes(node);

      if (xml_isNode(node,"sys")) {
         buf = xml_nodeProp(node,"name");
         if ((buf != NULL) && ((sys = system_get(buf)) != NULL)) {
            free(buf);
            array_grow( &temp->u.map->systems ) = sys;

            cur = node->children;

            do {
               xml_onlyNodes(cur);

               if (xml_isNode(cur,"asset")) {
                  buf = xml_get(cur);
                  if ((buf != NULL) && ((asset = planet_get(buf)) != NULL))
                     array_grow( &temp->u.map->assets ) = asset;
                  else
                     WARN("Map '%s' has invalid asset '%s'", temp->name, buf);
               }
               else if (xml_isNode(cur,"jump")) {
                  buf = xml_get(cur);
                  if ((buf != NULL) && ((jump = jump_get(xml_get(cur),
                        temp->u.map->systems[array_size(temp->u.map->systems)-1] )) != NULL))
                     array_grow( &temp->u.map->jumps ) = jump;
                  else
                     WARN("Map '%s' has invalid jump point '%s'", temp->name, buf);
               }
               else
                  WARN("Outfit '%s' has unknown node '%s'",temp->name, cur->name);
            } while (xml_nextNode(cur));
         }
         else
            WARN("Map '%s' has invalid system '%s'", temp->name, buf);
      }
      else if (xml_isNode(node,"short_desc")) {
         temp->desc_short = malloc( OUTFIT_SHORTDESC_MAX );
         nsnprintf( temp->desc_short, OUTFIT_SHORTDESC_MAX, "%s", xml_get(node) );
      }
      else if (xml_isNode(node,"all")) { /* Add everything to the map */
         system_stack = system_getAll(&nsys);
         for (i=0;i<nsys;i++) {
            array_grow( &temp->u.map->systems ) = &system_stack[i];
            for (j=0;j<system_stack[i].nplanets;j++)
               array_grow( &temp->u.map->assets ) = system_stack[i].planets[j];
            for (j=0;j<system_stack[i].njumps;j++)
               array_grow( &temp->u.map->jumps ) = &system_stack[i].jumps[j];
         }
      }
      else
         WARN("Outfit '%s' has unknown node '%s'",temp->name, node->name);
   } while (xml_nextNode(node));

   array_shrink( &temp->u.map->systems );
   array_shrink( &temp->u.map->assets  );
   array_shrink( &temp->u.map->jumps   );

   if (temp->desc_short == NULL) {
      /* Set short description based on type. */
      temp->desc_short = malloc( OUTFIT_SHORTDESC_MAX );
      nsnprintf( temp->desc_short, OUTFIT_SHORTDESC_MAX,
            "%s", outfit_getType(temp) );
   }


#define MELEMENT(o,s) \
if (o) WARN("Outfit '%s' missing/invalid '"s"' element", temp->name)
/**< Define to help check for data errors. */
   MELEMENT(temp->mass!=0.,"cpu");
   MELEMENT(temp->cpu!=0.,"cpu");
#undef MELEMENT
}


/**
 * @brief Parses the map tidbits of the outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSLocalMap( Outfit *temp, const xmlNodePtr parent )
{
   xmlNodePtr node;
   node = parent->children;

   temp->slot.type         = OUTFIT_SLOT_NA;
   temp->slot.size         = OUTFIT_SLOT_SIZE_NA;

   do {
      xml_onlyNodes(node);
      xmlr_float(node,"asset_detect",temp->u.lmap.asset_detect);
      xmlr_float(node,"jump_detect",temp->u.lmap.jump_detect);
      WARN("Outfit '%s' has unknown node '%s'",temp->name, node->name);
   } while (xml_nextNode(node));

   temp->u.lmap.asset_detect = pow2( temp->u.lmap.asset_detect );
   temp->u.lmap.jump_detect  = pow2( temp->u.lmap.jump_detect );

   /* Set short description. */
   temp->desc_short = malloc( OUTFIT_SHORTDESC_MAX );
   nsnprintf( temp->desc_short, OUTFIT_SHORTDESC_MAX,
         "%s",
         outfit_getType(temp) );

#define MELEMENT(o,s) \
if (o) WARN("Outfit '%s' missing/invalid '"s"' element", temp->name)
/**< Define to help check for data errors. */
   MELEMENT(temp->mass!=0.,"cpu");
   MELEMENT(temp->cpu!=0.,"cpu");
#undef MELEMENT
}


/**
 * @brief Parses the GUI tidbits of the outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSGUI( Outfit *temp, const xmlNodePtr parent )
{
   xmlNodePtr node;

   temp->slot.type         = OUTFIT_SLOT_NA;
   temp->slot.size         = OUTFIT_SLOT_SIZE_NA;

   node = parent->children;

   do {
      xml_onlyNodes(node);
      xmlr_strd(node,"gui",temp->u.gui.gui);
      WARN("Outfit '%s' has unknown node '%s'",temp->name, node->name);
   } while (xml_nextNode(node));

   /* Set short description. */
   temp->desc_short = malloc( OUTFIT_SHORTDESC_MAX );
   nsnprintf( temp->desc_short, OUTFIT_SHORTDESC_MAX,
         "GUI (Graphical User Interface)" );

#define MELEMENT(o,s) \
if (o) WARN("Outfit '%s' missing/invalid '"s"' element", temp->name)
/**< Define to help check for data errors. */
   MELEMENT(temp->u.gui.gui==NULL,"gui");
   MELEMENT(temp->mass!=0.,"cpu");
   MELEMENT(temp->cpu!=0.,"cpu");
#undef MELEMENT
}


/**
 * @brief Parses the license tidbits of the outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSLicense( Outfit *temp, const xmlNodePtr parent )
{
   temp->slot.type         = OUTFIT_SLOT_NA;
   temp->slot.size         = OUTFIT_SLOT_SIZE_NA;

   xmlNodePtr node;
   node = parent->children;

   do {
      xml_onlyNodes(node);
      WARN("Outfit '%s' has unknown node '%s'",temp->name, node->name);
   } while (xml_nextNode(node));

   /* Set short description. */
   temp->desc_short = malloc( OUTFIT_SHORTDESC_MAX );
   nsnprintf( temp->desc_short, OUTFIT_SHORTDESC_MAX,
         "%s",
         outfit_getType(temp) );

#define MELEMENT(o,s) \
if (o) WARN("Outfit '%s' missing/invalid '"s"' element", temp->name)
/**< Define to help check for data errors. */
   MELEMENT(temp->mass!=0.,"cpu");
   MELEMENT(temp->cpu!=0.,"cpu");
#undef MELEMENT
}


/**
 * @brief Parses the jammer tidbits of the outfit.
 *
 *    @param temp Outfit to finish loading.
 *    @param parent Outfit's parent node.
 */
static void outfit_parseSJammer( Outfit *temp, const xmlNodePtr parent )
{
   xmlNodePtr node;
   node = parent->children;

   do {
      xml_onlyNodes(node);
      xmlr_float(node,"energy",temp->u.jam.energy);
      xmlr_float(node,"range",temp->u.jam.range);
      xmlr_float(node,"power",temp->u.jam.power);
      WARN("Outfit '%s' has unknown node '%s'",temp->name, node->name);
   } while (xml_nextNode(node));

   /* Set default outfit size if necessary. */
   if (temp->slot.size == OUTFIT_SLOT_SIZE_NA)
      outfit_setDefaultSize( temp );
   temp->u.jam.energy = -temp->u.jam.energy;

   /* Set short description. */
   temp->desc_short = malloc( OUTFIT_SHORTDESC_MAX );
   nsnprintf( temp->desc_short, OUTFIT_SHORTDESC_MAX,
         "%s\n"
         "\erActivated Outfit\e0\n"
         "%.0f CPU\n"
         "Only one can be equipped\n"
         "%.0f Range\n"
         "%.0f%% Power\n"
         "%.1f EPS",
         outfit_getType(temp),
         temp->cpu,
         temp->u.jam.range,
         temp->u.jam.power,
         temp->u.jam.energy );

   temp->u.jam.power  /= 100.; /* Put in per one, instead of percent */
   temp->u.jam.range2  = pow2( temp->u.jam.range ); /**< We square it already. */

#define MELEMENT(o,s) \
if (o) WARN("Outfit '%s' missing/invalid '"s"' element", temp->name) /**< Define to help check for data errors. */
   MELEMENT(temp->u.jam.range==0.,"range");
   MELEMENT(temp->u.jam.power==0.,"power");
   MELEMENT(temp->cpu==0.,"cpu");
#undef MELEMENT
}


/**
 * @brief Parses and returns Outfit from parent node.

 *    @param temp Outfit to load into.
 *    @param parent Parent node to parse outfit from.
 *    @return 0 on success.
 */
static int outfit_parse( Outfit* temp, const char* file )
{
   xmlNodePtr cur, node, parent;
   char *prop;
   const char *cprop;
   int group;
   uint32_t bufsize;
   char *buf = ndata_read( file, &bufsize );

   xmlDocPtr doc = xmlParseMemory( buf, bufsize );

   parent = doc->xmlChildrenNode; /* first system node */
   if (parent == NULL) {
      ERR("Malformed '"OUTFIT_DATA_PATH"' file: does not contain elements");
      return -1;
   }

   /* Clear data. */
   memset( temp, 0, sizeof(Outfit) );

   temp->name = xml_nodeProp(parent,"name"); /* already mallocs */
   if (temp->name == NULL)
      WARN("Outfit in "OUTFIT_DATA_PATH" has invalid or no name");

   node = parent->xmlChildrenNode;

   do { /* load all the data */

      /* Only handle nodes. */
      xml_onlyNodes(node);

      if (xml_isNode(node,"general")) {
         cur = node->children;
         do {
            xml_onlyNodes(cur);
            xmlr_strd(cur,"license",temp->license);
            xmlr_float(cur,"mass",temp->mass);
            xmlr_float(cur,"cpu",temp->cpu);
            xmlr_long(cur,"price",temp->price);
            xmlr_strd(cur,"limit",temp->limit);
            xmlr_strd(cur,"description",temp->description);
            xmlr_strd(cur,"typename",temp->typename);
            xmlr_int(cur,"priority",temp->priority);
            xmlr_int(cur,"level",temp->level);
            xmlr_strd(cur,"logo",temp->logo);
            if (xml_isNode(cur,"gfx_store")) {
               temp->gfx_store = xml_parseTexture( cur,
                     OUTFIT_GFX_PATH"store/%s.png", 1, 1, OPENGL_TEX_MIPMAPS );
               continue;
            }
            else if (xml_isNode(cur,"slot")) {
               cprop = xml_get(cur);
               if (cprop == NULL)
                  WARN("Outfit '%s' has an slot type invalid.", temp->name);
               else if (strcmp(cprop,"structure") == 0)
                  temp->slot.type = OUTFIT_SLOT_STRUCTURE;
               else if (strcmp(cprop,"utility") == 0)
                  temp->slot.type = OUTFIT_SLOT_UTILITY;
               else if (strcmp(cprop,"weapon") == 0)
                  temp->slot.type = OUTFIT_SLOT_WEAPON;
               else
                  WARN("Outfit '%s' has unknown slot type '%s'.", temp->name, cprop);

               /* Property. */
               xmlr_attr( cur, "prop", prop );
               if (prop != NULL)
                  temp->slot.spid = sp_get( prop );
               free( prop );
               continue;
            }
            else if (xml_isNode(cur,"size")) {
               temp->slot.size = outfit_toSlotSize( xml_get(cur) );
               continue;
            }
            WARN("Outfit '%s' has unknown general node '%s'",temp->name, cur->name);
         } while (xml_nextNode(cur));
         continue;
      }

      if (xml_isNode(node,"specific")) { /* has to be processed separately */

         /* get the type */
         prop = xml_nodeProp(node,"type");
         if (prop == NULL)
            ERR("Outfit '%s' element 'specific' missing property 'type'",temp->name);
         temp->type = outfit_strToOutfitType(prop);
         free(prop);

         /* is secondary weapon? */
         prop = xml_nodeProp(node,"secondary");
         if (prop != NULL) {
            if ((int)atoi(prop))
               outfit_setProp(temp, OUTFIT_PROP_WEAP_SECONDARY);
            free(prop);
         }

         /* Check for manually-defined group. */
         prop = xml_nodeProp(node, "group");
         if (prop != NULL) {
            group = atoi(prop);
            if (group > PILOT_WEAPON_SETS || group < 1) {
               WARN("Outfit '%s' has group '%d', should be in the 1-%d range",
                     temp->name, group, PILOT_WEAPON_SETS);
            }

            temp->group = CLAMP(0, 9, group - 1);
            free(prop);
         }

         if (temp->type==OUTFIT_TYPE_NULL)
            WARN("Outfit '%s' is of type NONE", temp->name);
         else if (outfit_isBolt(temp))
            outfit_parseSBolt( temp, node );
         else if (outfit_isBeam(temp))
            outfit_parseSBeam( temp, node );
         else if (outfit_isLauncher(temp))
            outfit_parseSLauncher( temp, node );
         else if (outfit_isAmmo(temp))
            outfit_parseSAmmo( temp, node );
         else if (outfit_isMod(temp))
            outfit_parseSMod( temp, node );
         else if (outfit_isAfterburner(temp))
            outfit_parseSAfterburner( temp, node );
         else if (outfit_isJammer(temp))
            outfit_parseSJammer( temp, node );
         else if (outfit_isFighterBay(temp))
            outfit_parseSFighterBay( temp, node );
         else if (outfit_isFighter(temp))
            outfit_parseSFighter( temp, node );
         else if (outfit_isMap(temp)) {
            temp->u.map = malloc( sizeof(OutfitMapData_t) ); /**< deal with maps after the universe is loaded */
            temp->slot.type         = OUTFIT_SLOT_NA;
            temp->slot.size         = OUTFIT_SLOT_SIZE_NA;
         }
         else if (outfit_isLocalMap(temp))
            outfit_parseSLocalMap( temp, node );
         else if (outfit_isGUI(temp))
            outfit_parseSGUI( temp, node );
         else if (outfit_isLicense(temp))
            outfit_parseSLicense( temp, node );

         continue;
      }
      WARN("Outfit '%s' has unknown node '%s'",temp->name, node->name);
   } while (xml_nextNode(node));

#define MELEMENT(o,s) \
if (o) WARN("Outfit '%s' missing/invalid '"s"' element", temp->name) /**< Define to help check for data errors. */
   MELEMENT(temp->name==NULL,"name");
   MELEMENT(temp->slot.type==OUTFIT_SLOT_NULL,"slot");
   MELEMENT((temp->slot.type!=OUTFIT_SLOT_NA) && (temp->slot.size==OUTFIT_SLOT_SIZE_NA),"size");
   MELEMENT(temp->gfx_store==NULL,"gfx_store");
   /*MELEMENT(temp->mass==0,"mass"); Not really needed */
   MELEMENT(temp->type==0,"type");
   /*MELEMENT(temp->price==0,"price");*/
   MELEMENT(temp->description==NULL,"description");
   //MELEMENT(temp->level==0,"level");
#undef MELEMENT

   xmlFreeDoc(doc);

   if (temp->gfx_store) {

	   int nlayers=1;

	   if (temp->slot.type != OUTFIT_SLOT_NA) {
		   nlayers+=2;
	   }

	   if (temp->level > 0) {
		   nlayers++;
	   }

	   if (temp->logo) {
		   nlayers++;
	   }

	   int layerpos=0;

	   temp->gfx_store_layers=malloc(sizeof(glTexture*)*nlayers);
	   temp->gfx_store_layers[layerpos]=temp->gfx_store;
	   layerpos++;
	   if (temp->slot.type != OUTFIT_SLOT_NA) {
		   if (temp->slot.spid>0) {
			   nsnprintf( buf, bufsize, OUTFIT_GFX_PATH"store/layers/slot_special_%s.png",  sp_display(temp->slot.spid));
			   temp->gfx_store_layers[layerpos]=gl_newImage( buf, OPENGL_TEX_MIPMAPS );
		   } else if (temp->slot.type == OUTFIT_SLOT_UTILITY) {
			   temp->gfx_store_layers[layerpos]=gl_newImage( OUTFIT_GFX_PATH"store/layers/slot_utility.png", OPENGL_TEX_MIPMAPS );
		   } else if (temp->slot.type == OUTFIT_SLOT_STRUCTURE) {
			   temp->gfx_store_layers[layerpos]=gl_newImage( OUTFIT_GFX_PATH"store/layers/slot_structure.png", OPENGL_TEX_MIPMAPS );
		   } else if (temp->slot.type == OUTFIT_SLOT_WEAPON) {
			   temp->gfx_store_layers[layerpos]=gl_newImage( OUTFIT_GFX_PATH"store/layers/slot_weapon.png", OPENGL_TEX_MIPMAPS );
		   }

		   layerpos++;

		   if (temp->slot.size == OUTFIT_SLOT_SIZE_HEAVY)
			   temp->gfx_store_layers[layerpos]=gl_newImage( OUTFIT_GFX_PATH"store/layers/slot_heavy.png", OPENGL_TEX_MIPMAPS );
		  else if (temp->slot.size == OUTFIT_SLOT_SIZE_MEDIUM)
			  temp->gfx_store_layers[layerpos]=gl_newImage( OUTFIT_GFX_PATH"store/layers/slot_medium.png", OPENGL_TEX_MIPMAPS );
		  else if (temp->slot.size == OUTFIT_SLOT_SIZE_LIGHT)
			  temp->gfx_store_layers[layerpos]=gl_newImage( OUTFIT_GFX_PATH"store/layers/slot_light.png", OPENGL_TEX_MIPMAPS );

		   layerpos++;
	   }

	   if (temp->level > 0) {
		   nsnprintf( buf, bufsize, OUTFIT_GFX_PATH"store/layers/level_%d.png", temp->level);
		   temp->gfx_store_layers[layerpos]=gl_newImage( buf, OPENGL_TEX_MIPMAPS );
		   layerpos++;
	   }

	   if (temp->logo) {
		   nsnprintf( buf, bufsize, OUTFIT_GFX_PATH"store/layers/logo_%s.png", temp->logo);
		   temp->gfx_store_layers[layerpos]=gl_newImage( buf, OPENGL_TEX_MIPMAPS );
		   layerpos++;
	   }

	   temp->gfx_store_nlayers=nlayers;
   }

   free(buf);

   return 0;
}


/**
 * @brief Loads all the files in a directory.
 *
 *    @param dir Directory to load files from.
 *    @return 0 on success.
 */
static int outfit_loadDir( char *dir )
{
   uint32_t nfiles;
   char **outfit_files;
   int i;

   outfit_files = ndata_listRecursive( dir, &nfiles );
   for (i=0; i<(int)nfiles; i++) {
      outfit_parse( &array_grow(&outfit_stack), outfit_files[i] );
      free( outfit_files[i] );
   }
   free( outfit_files );

   /* Reduce size. */
   array_shrink( &outfit_stack );

   return 0;
}

/**
 * @brief Loads all the outfits.
 *
 *    @return 0 on success.
 */
int outfit_load (void)
{
   int i, noutfits;
   Outfit *o;

   /* First pass, loads up ammunition. */
   outfit_stack = array_create(Outfit);
   outfit_loadDir( OUTFIT_DATA_PATH );
   array_shrink(&outfit_stack);
   noutfits = array_size(outfit_stack);

   /* Second pass, sets up ammunition relationships. */
   for (i=0; i<noutfits; i++) {
      o = &outfit_stack[i];
      if (outfit_isLauncher(&outfit_stack[i])) {
         o->u.lau.ammo = outfit_get( o->u.lau.ammo_name );
         if (outfit_isSeeker(o) && /* Smart seekers. */
               (o->u.lau.ammo->u.amm.ai)) {
            if (o->u.lau.ew_target == 0.)
               WARN("Outfit '%s' missing/invalid 'ew_target' element", o->name);
            if (o->u.lau.lockon == 0.)
               WARN("Outfit '%s' missing/invalid 'lockon' element", o->name);
            if (!outfit_isTurret(o) && (o->u.lau.arc == 0.))
               WARN("Outfit '%s' missing/invalid 'arc' element", o->name);
         }

         outfit_launcherDesc(o);
      }
      else if (outfit_isFighterBay(&outfit_stack[i]))
         o->u.bay.ammo = outfit_get( o->u.bay.ammo_name );
   }

#ifdef DEBUGGING
   char **outfit_names = malloc( noutfits * sizeof(char*) );
   int start;

   for (i=0; i<noutfits; i++)
      outfit_names[i] = outfit_stack[i].name;

   qsort( outfit_names, noutfits, sizeof(char*) , outfit_compareNames );
   for (i=0; i<(noutfits - 1); i++) {
      start = i;
      while (strcmp(outfit_names[i], outfit_names[i+1]) == 0)
         i++;

      if (i == start)
         continue;

      WARN("Name collision! %d outfits are named '%s'", i+1 - start,
            outfit_names[start]);
   }
   free(outfit_names);
#endif

   DEBUG("Loaded %d Outfit%s", noutfits, (noutfits == 1) ? "" : "s" );

   return 0;
}


/**
 * @brief qsort compare function for names.
 */
static int outfit_compareNames( const void *name1, const void *name2 )
{
   const char *n1, *n2;

   n1 = *(const char**) name1;
   n2 = *(const char**) name2;

   return strcmp(n1, n2);
}


/**
 * @brief Parses all the maps.
 *
 */
int outfit_mapParse (void)
{
   int i, len;
   Outfit *o;
   uint32_t bufsize, nfiles;
   char *buf;
   xmlNodePtr node, cur;
   xmlDocPtr doc;
   char **map_files;
   char *file, *n;

   map_files = ndata_list( MAP_DATA_PATH, &nfiles );
   for (i=0; i<(int)nfiles; i++) {

      len  = strlen(MAP_DATA_PATH)+strlen(map_files[i])+2;
      file = malloc( len );
      nsnprintf( file, len, "%s%s", MAP_DATA_PATH, map_files[i] );

      buf = ndata_read( file, &bufsize );
      doc = xmlParseMemory( buf, bufsize );

      node = doc->xmlChildrenNode; /* first system node */
      if (node == NULL) {
         WARN("Malformed '"OUTFIT_DATA_PATH"' file: does not contain elements");
         free(file);
         xmlFreeDoc(doc);
         free(buf);
         return -1;
      }

      n = xml_nodeProp( node,"name" );
      o = outfit_get( n );
      free(n);
      if (!outfit_isMap(o)) { /* If its not a map, we don't care. */
         free(file);
         xmlFreeDoc(doc);
         free(buf);
         continue;
      }

      cur = node->xmlChildrenNode;
      do { /* load all the data */
         /* Only handle nodes. */
         xml_onlyNodes(cur);

         if (xml_isNode(cur,"specific"))
            outfit_parseSMap(o, cur);

      } while (xml_nextNode(cur));

      /* Clean up. */
      free(file);
      xmlFreeDoc(doc);
      free(buf);
   }

   /* Clean up. */
   for (i=0; i<(int)nfiles; i++)
      free( map_files[i] );
   free( map_files );

   return 0;
}


/**
 * @brief Generates short descs for launchers, including ammo info.
 *
 *    @param o Launcher.
 */
static void outfit_launcherDesc( Outfit* o )
{
   int l;
   Outfit *a; /* Launcher's ammo. */

   if (o->desc_short != NULL) {
      WARN("Outfit '%s' already has a short description", o->name);
      return;
   }

   a = o->u.lau.ammo;

   o->desc_short = malloc( OUTFIT_SHORTDESC_MAX );
   l = nsnprintf( o->desc_short, OUTFIT_SHORTDESC_MAX,
         "%s [%s]\n"
         "%.0f CPU\n"
         "%.0f%% Penetration\n"
         "%.2f DPS [%.0f Damage]\n",
         outfit_getType(o), dtype_damageTypeToStr(a->u.amm.dmg.type),
         o->cpu,
         a->u.amm.dmg.penetration * 100.,
         1. / o->u.lau.delay * a->u.amm.dmg.damage, a->u.amm.dmg.damage );

   if (a->u.amm.dmg.disable > 0.)
      l += nsnprintf( &o->desc_short[l], OUTFIT_SHORTDESC_MAX - l,
            "%.2f Disable/s [%.0f Disable]\n",
            1. / o->u.lau.delay * a->u.amm.dmg.disable, a->u.amm.dmg.disable );

   l += nsnprintf( &o->desc_short[l], OUTFIT_SHORTDESC_MAX - l,
         "%.1f Shots Per Second\n"
         "Holds %d %s",
         1. / o->u.lau.delay,
         o->u.lau.amount, o->u.lau.ammo_name );
}


/**
 * @brief Frees the outfit stack.
 */
void outfit_free (void)
{
   int i;
   Outfit *o;
   for (i=0; i < array_size(outfit_stack); i++) {
      o = &outfit_stack[i];

      /* free graphics */
      if (outfit_gfx(&outfit_stack[i]))
         gl_freeTexture(outfit_gfx(&outfit_stack[i]));

      /* Free slot. */
      outfit_freeSlot( &outfit_stack[i].slot );

      /* Type specific. */
      if (outfit_isBolt(o) && o->u.blt.gfx_end)
         gl_freeTexture(o->u.blt.gfx_end);
      if (outfit_isLauncher(o) && o->u.lau.ammo_name)
         free(o->u.lau.ammo_name);
      if (outfit_isFighterBay(o) && o->u.bay.ammo_name)
         free(o->u.bay.ammo_name);
      if (outfit_isFighter(o) && o->u.fig.ship)
         free(o->u.fig.ship);
      if (outfit_isGUI(o) && o->u.gui.gui)
         free(o->u.gui.gui);
      if (o->type == OUTFIT_TYPE_MODIFICATION)
         ss_free( o->u.mod.stats );
      if (outfit_isMap(o)) {
         array_free( o->u.map->systems );
         array_free( o->u.map->assets );
         array_free( o->u.map->jumps );
         free( o->u.map );
      }

      if (o->gfx_store_layers)
          	  free(o->gfx_store_layers);

      /* strings */
      free(o->typename);
      free(o->description);
      free(o->limit);
      free(o->desc_short);
      free(o->license);
      free(o->name);
      if (o->gfx_store)
         gl_freeTexture(o->gfx_store);
      if (o->logo)
         free(o->logo);
   }

   array_free(outfit_stack);
}

