/*
 * See Licensing and Copyright notice in naev.h
 */

/**
 * @file land_outfits.c
 *
 * @brief Handles all the landing menus and actions.
 */


#include "land_outfits.h"

#include "naev.h"

#include <stdlib.h>
#include <stdio.h>
#include "nstring.h"
#include <math.h>

#include "log.h"
#include "map.h"
#include "equipment.h"
#include "outfit.h"
#include "player.h"
#include "player_gui.h"
#include "slots.h"
#include "space.h"
#include "toolkit.h"
#include "dialogue.h"
#include "map_find.h"


#define  OUTFITS_IAR    "iarOutfits"
#define  OUTFITS_TAB    "tabOutfits"
#define  OUTFITS_FILTER "inpFilterOutfits"
#define  OUTFITS_NTABS  6


static iar_data_t *iar_data = NULL; /**< Stored image array positions. */

/* Modifier for buying and selling quantity. */
static int outfits_mod = 1;


/*
 * Helper functions.
 */
static void outfits_getSize( unsigned int wid, int *w, int *h,
      int *iw, int *ih, int *bw, int *bh );
static void outfits_buy( unsigned int wid, char* str );
static void outfits_sell( unsigned int wid, char* str );
static int outfits_getMod (void);
static void outfits_renderMod( double bx, double by, double w, double h, void *data );
static void outfits_rmouse( unsigned int wid, char* widget_name );
static void outfits_find( unsigned int wid, char* str );
static credits_t outfit_getPrice( Outfit *outfit );
static void outfits_genList( unsigned int wid );
static void outfits_changeTab( unsigned int wid, char *wgt, int old, int tab );


/**
 * @brief Gets the size of the outfits window.
 */
static void outfits_getSize( unsigned int wid, int *w, int *h,
      int *iw, int *ih, int *bw, int *bh )
{
   int padding;

   /* Get window dimensions. */
   window_dimWindow( wid, w, h );

   /* Calculate image array dimensions. */
   if (iw != NULL)
      *iw = 310 + (*w-800);
   if (ih != NULL)
      *ih = *h - 60;

   /* Left padding + per-button padding * nbuttons */
   padding = 40 + 20 * 4;

   /* Calculate button dimensions. */
   if (bw != NULL)
      *bw = (*w - (iw!=NULL?*iw:0) - padding) / 4;
   if (bh != NULL)
      *bh = LAND_BUTTON_HEIGHT;
}



/**
 * @brief Opens the outfit exchange center window.
 */
void outfits_open( unsigned int wid )
{
   int w, h, iw, ih, bw, bh, off;

   /* Mark as generated. */
   land_tabGenerate(LAND_WINDOW_OUTFITS);

   /* Get dimensions. */
   outfits_getSize( wid, &w, &h, &iw, &ih, &bw, &bh );

   /* Initialize stored positions. */
   if (iar_data == NULL)
      iar_data = calloc( OUTFITS_NTABS, sizeof(iar_data_t) );
   else
      memset( iar_data, 0, sizeof(iar_data_t) * OUTFITS_NTABS );

   /* will allow buying from keyboard */
   window_setAccept( wid, outfits_buy );

   /* buttons */
   window_addButtonKey( wid, off = -20, 20,
         bw, bh, "btnCloseOutfits",
         "Take Off", land_buttonTakeoff, SDLK_t );
   window_addButtonKey( wid, off -= 20+bw, 20,
         bw, bh, "btnSellOutfit",
         "Sell", outfits_sell, SDLK_s );
   window_addButtonKey( wid, off -= 20+bw, 20,
         bw, bh, "btnBuyOutfit",
         "Buy", outfits_buy, SDLK_b );
   window_addButtonKey( wid, off -= 20+bw, 20,
         bw, bh, "btnFindOutfits",
         "Find Outfits", outfits_find, SDLK_f );

   /* fancy 128x128 image */
   window_addRect( wid, 19 + iw + 20, -50, 128, 129, "rctImage", &cBlack, 0 );
   window_addImageLayered( wid, 20 + iw + 20, -50-128, 0, 0, "imgOutfit", NULL, 0, 1 );

   /* cust draws the modifier */
   window_addCust( wid, -40-bw, 60+2*bh,
         bw, bh, "cstMod", 0, outfits_renderMod, NULL, NULL );

   /* the descriptive text */
   window_addText( wid, 20 + iw + 20 + 128 + 20, -60,
         280, 160, 0, "txtOutfitName", &gl_defFont, &cBlack, NULL );
   window_addText( wid, 20 + iw + 20 + 128 + 20, -60 - gl_defFont.h - 20,
         280, 160, 0, "txtDescShort", &gl_smallFont, &cBlack, NULL );
   window_addText( wid, 20 + iw + 20, -60-128-10,
         60, 160, 0, "txtSDesc", &gl_smallFont, &cDConsole,
         "Owned:\n"
         "\n"
         "Slot:\n"
         "Size:\n"
         "Mass:\n"
         "\n"
         "Price:\n"
         "Money:\n"
         "License:\n" );
   window_addText( wid, 20 + iw + 20 + 60, -60-128-10,
         250, 160, 0, "txtDDesc", &gl_smallFont, &cBlack, NULL );
   window_addText( wid, 20 + iw + 20, -60-128-10-160,
         w-(iw+80), 180, 0, "txtDescription",
         &gl_smallFont, NULL, NULL );

   /* Create the image array. */
   outfits_genList( wid );

   /* Set default keyboard focus to the list */
   window_setFocus( wid , OUTFITS_IAR );
}


/**
 * @brief Regenerates the outfit list.
 *
 *   @param wid Window to generate the list on.
 *   @param str Unused.
 */
void outfits_regenList( unsigned int wid, char *str )
{
   (void) str;
   int tab;
   char *focused;

   /* Must exist. */
   if(land_getWid( LAND_WINDOW_OUTFITS ) == 0)
      return;

   /* Save focus. */
   focused = strdup(window_getFocus(wid));

   /* Save positions. */
   tab = window_tabWinGetActive( wid, OUTFITS_TAB );
   toolkit_saveImageLayeredArrayData( wid, OUTFITS_IAR, &iar_data[tab] );
   window_destroyWidget( wid, OUTFITS_IAR );

   outfits_genList( wid );

   /* Restore positions. */
   toolkit_setImageLayeredArrayPos(    wid, OUTFITS_IAR, iar_data[tab].pos );
   toolkit_setImageLayeredArrayOffset( wid, OUTFITS_IAR, iar_data[tab].offset );
   outfits_update( wid, NULL );

   /* Restore focus. */
   window_setFocus( wid, focused );
   free(focused);
}


static int outfit_filterWeapon( const Outfit *o )
{ return ((o->slot.type == OUTFIT_SLOT_WEAPON) && !sp_required( o->slot.spid )); }

static int outfit_filterUtility( const Outfit *o )
{ return ((o->slot.type == OUTFIT_SLOT_UTILITY) && !sp_required( o->slot.spid )); }

static int outfit_filterStructure( const Outfit *o )
{ return ((o->slot.type == OUTFIT_SLOT_STRUCTURE) && !sp_required( o->slot.spid )); }

static int outfit_filterCore( const Outfit *o )
{ return sp_required( o->slot.spid ); }

static int outfit_filterOther( const Outfit *o )
{
   return (!sp_required( o->slot.spid ) && ((o->slot.type == OUTFIT_SLOT_NULL)
         || (o->slot.type == OUTFIT_SLOT_NA)));
}

/**
 * @brief Generates the outfit list.
 *
 *    @param wid Window to generate the list on.
 */
static void outfits_genList( unsigned int wid )
{
   int (*tabfilters[])( const Outfit *o ) = {
      NULL,
      outfit_filterWeapon,
      outfit_filterUtility,
      outfit_filterStructure,
      outfit_filterCore,
      outfit_filterOther
   };
   const char *tabnames[] = {
      "All", "\eb W ", "\eg U ", "\ep S ", "\eRCore", "Other"
   };

   int i, active, owned, len;
   int fx, fy, fw, fh, barw; /* Input filter. */
   Outfit **outfits;
   char **soutfits, **quantity;
   //char **slottype;
   glTexture ***toutfits;
   int *ntoutfits;
   int noutfits, moutfits;
   int w, h, iw, ih;
   //glColour *bg, blend;
   //const glColour *c;
   //const char *slotname;
   char *filtertext;

   /* Get dimensions. */
   outfits_getSize( wid, &w, &h, &iw, &ih, NULL, NULL );

   /* Create tabbed window. */
   if (!widget_exists( wid, OUTFITS_TAB )) {
      window_addTabbedWindow( wid, 20, 20 + ih - 30, iw, 30,
         OUTFITS_TAB, OUTFITS_NTABS, tabnames, 1 );

      barw = window_tabWinGetBarWidth( wid, OUTFITS_TAB );
      fw = CLAMP(0, 150, iw - barw - 30);
      fh = 20;

      fx = iw - fw;
      fy = ih - (30 - fh) / 2; /* Centered relative to 30 px tab bar */

      /* Only create the filter widget if it will be a reasonable size. */
      if (iw >= 30) {
         window_addInput( wid, fx, fy, fw, fh, OUTFITS_FILTER, 32, 1, &gl_smallFont );
         window_setInputCallback( wid, OUTFITS_FILTER, outfits_regenList );
      }
   }

   window_tabWinOnChange( wid, OUTFITS_TAB, outfits_changeTab );
   active = window_tabWinGetActive( wid, OUTFITS_TAB );

   /* Widget must not already exist. */
   if (widget_exists( wid, OUTFITS_IAR ))
      return;

   filtertext = NULL;
   if (widget_exists( wid, OUTFITS_FILTER )) {
      filtertext = window_getInput( wid, OUTFITS_FILTER );
      if (strlen(filtertext) == 0)
         filtertext = NULL;
   }

   /* set up the outfits to buy/sell */
   outfits = tech_getOutfit( land_planet->tech, &noutfits );

   moutfits = MAX( 1, noutfits );
   soutfits = malloc( moutfits * sizeof(char*) );
   toutfits = malloc( moutfits * sizeof(glTexture**) );
   ntoutfits = malloc( moutfits * sizeof(int) );

   noutfits = outfits_filter( outfits, toutfits, ntoutfits, noutfits,
         tabfilters[active], filtertext );

   if (noutfits <= 0) { /* No outfits */
      soutfits[0] = strdup("None");
      toutfits[0] = NULL;
      toutfits[0] = malloc( sizeof(glTexture*) );
      toutfits[0][0] = NULL;
      ntoutfits[0] = 0;
      noutfits    = 1;
   }
   else {
      /* Create the outfit arrays. */
      quantity = malloc(sizeof(char*)*noutfits);
      //bg       = malloc(sizeof(glColour)*noutfits);
      //slottype = malloc(sizeof(char*)*noutfits );
      for (i=0; i<noutfits; i++) {
         soutfits[i] = strdup( outfits[i]->name );

         /* Background colour. Replaced with layers */
         /*
         c = outfit_slotSizeColour( &outfits[i]->slot );
         if (c == NULL)
            c = &cBlack;
         col_blend( &blend, c, &cGrey70, 0.4 );
         bg[i] = blend;
         */

         /* Quantity. */
         owned = player_outfitOwned(outfits[i]);
         len = owned / 10 + 4;
         if (owned >= 1) {
            quantity[i] = malloc( len );
            nsnprintf( quantity[i], len, "%d", owned );
         }
         else
            quantity[i] = NULL;


         /* Get slot name. Removed in LN (replaced with layers) */
         /*
         slotname = outfit_slotName(outfits[i]);
         if ((strcmp(slotname,"NA") != 0) && (strcmp(slotname,"NULL") != 0)) {
            slottype[i]    = malloc( 2 );
            slottype[i][0] = outfit_slotName(outfits[i])[0];
            slottype[i][1] = '\0';
         }
         else
            slottype[i] = NULL;
           */
      }
   }

   /* Clean up. */
   free(outfits);

   window_addImageLayeredArray( wid, 20, 20,
         iw, ih - 31, OUTFITS_IAR, 96, 96,
         toutfits, ntoutfits, soutfits, noutfits, outfits_update, outfits_rmouse );

   /* write the outfits stuff */
   outfits_update( wid, NULL );

   if (noutfits > 0 && (strcmp(soutfits[0], "None") != 0)) {
      toolkit_setImageLayeredArrayQuantity( wid, OUTFITS_IAR, quantity );
      //Removed in LN
      //toolkit_setImageLayeredArraySlotType( wid, OUTFITS_IAR, slottype );
      //toolkit_setImageLayeredArrayBackground( wid, OUTFITS_IAR, bg );
   }
}


/**
 * @brief Updates the outfits in the outfit window.
 *    @param wid Window to update the outfits in.
 *    @param str Unused.
 */
void outfits_update( unsigned int wid, char* str )
{
   (void)str;
   char *outfitname;
   Outfit* outfit;
   char buf[PATH_MAX], buf2[ECON_CRED_STRLEN], buf3[ECON_CRED_STRLEN], buf4[PATH_MAX];
   double th;
   int iw, ih;
   int w, h;

   /* Get dimensions. */
   outfits_getSize( wid, &w, &h, &iw, &ih, NULL, NULL );

   /* Get and set parameters. */
   outfitname = toolkit_getImageLayeredArray( wid, OUTFITS_IAR );
   if (strcmp(outfitname,"None")==0) { /* No outfits */
      window_modifyImageLayered( wid, "imgOutfit", NULL, 0, 0, 0 );
      window_disableButton( wid, "btnBuyOutfit" );
      window_disableButton( wid, "btnSellOutfit" );
      nsnprintf( buf, PATH_MAX,
            "NA\n"
            "\n"
            "NA\n"
            "NA\n"
            "NA\n"
            "\n"
            "NA\n"
            "NA\n"
            "NA\n" );
      window_modifyText( wid, "txtDDesc", buf );
      window_modifyText( wid, "txtOutfitName", "None" );
      window_modifyText( wid, "txtDescShort", NULL );
      window_modifyText( wid, "txtDescription", NULL );
      /* Reposition. */
      th = 128;
      window_moveWidget( wid, "txtSDesc", 40+iw+20, -60-th-20 );
      window_moveWidget( wid, "txtDDesc", 40+iw+20+60, -60-th-20 );
      window_moveWidget( wid, "txtDescription", 20+iw+40, -240 );
      return;
   }

   outfit = outfit_get( outfitname );

   /* new image */
   window_modifyImageLayered( wid, "imgOutfit", outfit->gfx_store_layers, outfit->gfx_store_nlayers, 0, 0 );

   if (outfit_canBuy(outfitname, land_planet) > 0)
      window_enableButton( wid, "btnBuyOutfit" );
   else
      window_disableButtonSoft( wid, "btnBuyOutfit" );

   /* gray out sell button */
   if (outfit_canSell(outfitname) > 0)
      window_enableButton( wid, "btnSellOutfit" );
   else
      window_disableButtonSoft( wid, "btnSellOutfit" );

   /* new text */
   window_modifyText( wid, "txtDescription", outfit->description );
   price2str( buf2, outfit_getPrice(outfit), player.p->credits, 2 );
   credits2str( buf3, player.p->credits, 2 );

   if (outfit->license == NULL)
      strncpy( buf4, "None", sizeof(buf4) );
   else if (player_hasLicense( outfit->license ))
      strncpy( buf4, outfit->license, sizeof(buf4) );
   else
      nsnprintf( buf4, sizeof(buf4), "\er%s\e0", outfit->license );

   nsnprintf( buf, PATH_MAX,
         "%d\n"
         "\n"
         "%s\n"
         "%s\n"
         "%.0f tons\n"
         "\n"
         "%s credits\n"
         "%s credits\n"
         "%s\n",
         player_outfitOwned(outfit),
         outfit_slotName(outfit),
         outfit_slotSize(outfit),
         outfit->mass,
         buf2,
         buf3,
         buf4 );
   window_modifyText( wid, "txtDDesc", buf );
   window_modifyText( wid, "txtOutfitName", outfit->name );
   window_modifyText( wid, "txtDescShort", outfit->desc_short );
   th = MAX( 128, gl_printHeightRaw( &gl_smallFont, 280, outfit->desc_short ) );
   window_moveWidget( wid, "txtSDesc", 40+iw+20, -60-th-20 );
   window_moveWidget( wid, "txtDDesc", 40+iw+20+60, -60-th-20 );
   th += gl_printHeightRaw( &gl_smallFont, 250, buf );
   window_moveWidget( wid, "txtDescription", 20+iw+40, -60-th-20 );
}


/**
 * @brief Updates the outfitter and equipment outfit image arrays.
 */
void outfits_updateEquipmentOutfits( void )
{
   int ew, ow;

   if (landed && land_doneLoading()) {
      if (planet_hasService(land_planet, PLANET_SERVICE_OUTFITS)) {
         ow = land_getWid( LAND_WINDOW_OUTFITS );
         outfits_regenList( ow, NULL );
      }
      else if (!planet_hasService(land_planet, PLANET_SERVICE_SHIPYARD))
         return;
      ew = land_getWid( LAND_WINDOW_EQUIPMENT );
      equipment_addAmmo();
      equipment_regenLists( ew, 1, 0 );
   }
}


/**
 * @brief Ensures the tab's selected item is reflected in the ship slot list
 *
 *    @param wid Unused.
 *    @param wgt Unused.
 *    @param tab Tab changed to.
 */
static void outfits_changeTab( unsigned int wid, char *wgt, int old, int tab )
{
   (void) wid;
   (void) wgt;
   int pos;
   double offset;

   toolkit_saveImageLayeredArrayData( wid, OUTFITS_IAR, &iar_data[old] );

   /* Store the currently-saved positions for the new tab. */
   pos    = iar_data[tab].pos;
   offset = iar_data[tab].offset;

   /* Resetting the input will cause the outfit list to be regenerated. */
   if (widget_exists(wid, OUTFITS_FILTER))
      window_setInput(wid, OUTFITS_FILTER, NULL);
   else
      outfits_regenList( wid, NULL );

   /* Set positions for the new tab. This is necessary because the stored
    * position for the new tab may have exceeded the size of the old tab,
    * resulting in it being clipped. */
   toolkit_setImageLayeredArrayPos(    wid, OUTFITS_IAR, pos );
   toolkit_setImageLayeredArrayOffset( wid, OUTFITS_IAR, offset );

   /* Focus the outfit image array. */
   window_setFocus( wid, OUTFITS_IAR );
}


/**
 * @brief Applies a filter function and string to a list of outfits.
 *
 *    @param outfits Array of outfits to filter.
 *    @param[out] toutfits Optional array of arrays of layers of outfit textures to generate.
 *    @param n Number of layers of each outfits in the array.
 *    @param n Number of outfits in the array.
 *    @param filter Filter function to run on each outfit.
 *    @param name Name fragment that each outfit name must contain.
 *    @return Number of outfits.
 */
int outfits_filter( Outfit **outfits, glTexture ***toutfits, int *ntoutfits, int n,
      int(*filter)( const Outfit *o ), char *name )
{
   int i, j;

   j = 0;
   for (i=0; i<n; i++) {
      if ((filter != NULL) && !filter(outfits[i]))
         continue;

      if ((name != NULL) && (nstrcasestr( outfits[i]->name, name ) == NULL))
         continue;

      /* Shift matches downward. */
      outfits[j] = outfits[i];
      if (toutfits != NULL) {
         toutfits[j] = outfits[i]->gfx_store_layers;
         ntoutfits[j] = outfits[i]->gfx_store_nlayers;
      }

      j++;
   }

   return j;
}


/**
 * @brief Starts the map find with outfit search selected.
 *    @param wid Window buying outfit from.
 *    @param str Unused.
 */
static void outfits_find( unsigned int wid, char* str )
{
   (void) str;
   map_inputFindType(wid, "outfit");
}


/**
 * @brief Returns the price of an outfit (subject to quantity modifier)
 */
static credits_t outfit_getPrice( Outfit *outfit )
{
   unsigned int q;
   credits_t price;

   q = outfits_getMod();
   price = outfit->price * q;

   return price;
}

/**
 * @brief Checks to see if the player can buy the outfit.
 *    @param outfit Outfit to buy.
 */
int outfit_canBuy( char *name, Planet *planet )
{
   int failure;
   credits_t price;
   Outfit *outfit;
   char buf[ECON_CRED_STRLEN];

   failure = 0;
   outfit  = outfit_get(name);
   price   = outfit_getPrice(outfit);

   /* Map already mapped */
   if ((outfit_isMap(outfit) && map_isMapped(outfit)) ||
         (outfit_isLocalMap(outfit) && localmap_isMapped(outfit))) {
      land_errDialogueBuild( "You already know of everything this map contains." );
      return 0;
   }
   /* GUI already owned */
   if (outfit_isGUI(outfit) && player_guiCheck(outfit->u.gui.gui)) {
      land_errDialogueBuild( "You already own this GUI." );
      return 0;
   }
   /* Already has license. */
   if (outfit_isLicense(outfit) && player_hasLicense(outfit->name)) {
      land_errDialogueBuild( "You already have this license." );
      return 0;
   }
   /* not enough $$ */
   if (!player_hasCredits(price)) {
      credits2str( buf, price - player.p->credits, 2 );
      land_errDialogueBuild( "You need %s more credits.", buf);
      failure = 1;
   }
   /* Needs license. */
   if ((!player_hasLicense(outfit->license)) &&
         ((planet == NULL) || (!planet_isBlackMarket(planet)))) {
      land_errDialogueBuild( "You need the '%s' license to buy this outfit.",
               outfit->license );
      failure = 1;
   }

   return !failure;
}


/**
 * @brief Player right-clicks on an outfit.
 *    @param wid Window player is buying ship from.
 *    @param widget_name Name of the window. (unused)
 *    @param shipname Name of the ship the player wants to buy. (unused)
 */
static void outfits_rmouse( unsigned int wid, char* widget_name )
{
    outfits_buy( wid, widget_name );
}

/**
 * @brief Attempts to buy the outfit that is selected.
 *    @param wid Window buying outfit from.
 *    @param str Unused.
 */
static void outfits_buy( unsigned int wid, char* str )
{
   (void) str;
   char *outfitname;
   Outfit* outfit;
   int q;

   outfitname = toolkit_getImageLayeredArray( wid, OUTFITS_IAR );
   if (strcmp(outfitname, "None") == 0)
      return;

   outfit = outfit_get( outfitname );

   q = outfits_getMod();

   /* can buy the outfit? */
   if (land_errDialogue( outfitname, "buyOutfit" ))
      return;

   /* Actually buy the outfit. */
   player_modCredits( -outfit->price * player_addOutfit( outfit, q ) );
   outfits_updateEquipmentOutfits();
}
/**
 * @brief Checks to see if the player can sell the selected outfit.
 *    @param outfit Outfit to try to sell.
 */
int outfit_canSell( char *name )
{
   int failure;
   Outfit *outfit;

   failure = 0;
   outfit = outfit_get(name);

   /* Map check. */
   if (outfit_isMap(outfit) || outfit_isLocalMap(outfit)) {
      land_errDialogueBuild("You can't sell a map.");
      failure = 1;
   }

   /* GUI check. */
   if (outfit_isGUI(outfit)) {
      land_errDialogueBuild("You can't sell a GUI.");
      failure = 1;
   }

   /* License check. */
   if (outfit_isLicense(outfit)) {
      land_errDialogueBuild("You can't sell a license.");
      failure = 1;
   }

   /* has no outfits to sell */
   if (player_outfitOwned(outfit) <= 0) {
      land_errDialogueBuild( "You can't sell something you don't have!" );
      failure = 1;
   }

   return !failure;
}
/**
 * @brief Attempts to sell the selected outfit the player has.
 *    @param wid Window selling outfits from.
 *    @param str Unused.
 */
static void outfits_sell( unsigned int wid, char* str )
{
   (void)str;
   char *outfitname;
   Outfit* outfit;
   int q;

   outfitname  = toolkit_getImageLayeredArray( wid, OUTFITS_IAR );
   if (strcmp(outfitname, "None") == 0)
      return;

   outfit      = outfit_get( outfitname );

   q = outfits_getMod();

   /* Check various failure conditions. */
   if (land_errDialogue( outfitname, "sellOutfit" ))
      return;

   player_modCredits( outfit->price * player_rmOutfit( outfit, q ) );
   outfits_updateEquipmentOutfits();
}
/**
 * @brief Gets the current modifier status.
 *    @return The amount modifier when buying or selling outfits.
 */
static int outfits_getMod (void)
{
   SDLMod mods;
   int q;

   mods = SDL_GetModState();
   q = 1;
   if (mods & (KMOD_LCTRL | KMOD_RCTRL))
      q *= 5;
   if (mods & (KMOD_LSHIFT | KMOD_RSHIFT))
      q *= 10;

   return q;
}
/**
 * @brief Renders the outfit buying modifier.
 *    @param bx Base X position to render at.
 *    @param by Base Y position to render at.
 *    @param w Width to render at.
 *    @param h Height to render at.
 */
static void outfits_renderMod( double bx, double by, double w, double h, void *data )
{
   (void) data;
   (void) h;
   int q;
   char buf[8];

   q = outfits_getMod();
   if (q != outfits_mod) {
      outfits_updateEquipmentOutfits();
      outfits_mod = q;
   }
   if (q==1) return; /* Ignore no modifier. */

   nsnprintf( buf, 8, "%dx", q );
   gl_printMid( &gl_smallFont, w, bx, by, &cBlack, buf );
}


/**
 * @brief Cleans up outfit globals.
 */
void outfits_cleanup( void )
{
   /* Free stored positions. */
   if (iar_data != NULL) {
      free(iar_data);
      iar_data = NULL;
   }
}
