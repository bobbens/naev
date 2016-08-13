/*
 * See Licensing and Copyright notice in naev.h
 */

/**
 * @file land_trade.c
 *
 * @brief Handles the Trading Center at land.
 */


#include "land_trade.h"

#include "naev.h"
#include "ndata.h"

#include <stdlib.h>
#include <stdio.h>
#include "nstring.h"
#include <math.h>
#include <assert.h>
#include "log.h"
#include "hook.h"
#include "player.h"
#include "space.h"
#include "toolkit.h"
#include "tk/toolkit_priv.h"
#include "dialogue.h"
#include "map_find.h"
#include "land_shipyard.h"

/*
 * Quantity to buy on one click
*/
static int commodity_mod = 10;


/**
 * @brief Opens the local market window.
 */
void commodity_exchange_open( unsigned int wid )
{
   int i, ngoods;
   char **goods;
   glTexture **tgoods;
   int w, h;
   int iw, ih;

  
   /* Mark as generated. */
   land_tabGenerate(LAND_WINDOW_COMMODITY);

   /* Get window dimensions. */
   window_dimWindow( wid, &w, &h );

   /* Calculate image array dimensions. */
   /* Window size minus right column size minus space on left and right */
   iw = w-LAND_BUTTON_WIDTH-3*20;
   ih = h - 60;

   /* buttons */
   window_addButtonKey( wid, -20, 20,
         LAND_BUTTON_WIDTH, LAND_BUTTON_HEIGHT, "btnCommodityClose",
         "Take Off", land_buttonTakeoff, SDLK_t );
   window_addButtonKey( wid, -40-((LAND_BUTTON_WIDTH-20)/2), 20*2 + LAND_BUTTON_HEIGHT,
         (LAND_BUTTON_WIDTH-20)/2, LAND_BUTTON_HEIGHT, "btnCommodityBuy",
         "Buy", commodity_buy, SDLK_b );
   window_addButtonKey( wid, -20, 20*2 + LAND_BUTTON_HEIGHT,
         (LAND_BUTTON_WIDTH-20)/2, LAND_BUTTON_HEIGHT, "btnCommoditySell",
         "Sell", commodity_sell, SDLK_s );

      /* cust draws the modifier : # of tons one click buys or sells */
   window_addCust( wid, -40-((LAND_BUTTON_WIDTH-20)/2), 60+ 2*LAND_BUTTON_HEIGHT,
         (LAND_BUTTON_WIDTH-20)/2, LAND_BUTTON_HEIGHT, "cstMod", 0, commodity_renderMod, NULL, NULL );

   /* store gfx */
   window_addRect( wid, 20+iw+20+(LAND_BUTTON_WIDTH-128)/2, -40,
         128, 128, "rctStore", &cBlack, 0 );
   window_addImage( wid, 20+iw+20+(LAND_BUTTON_WIDTH-128)/2, -40,
         128, 128, "imgStore", NULL, 1 );

   /* text */
   window_addText( wid, -20, -190, LAND_BUTTON_WIDTH, 80, 0,
         "txtSInfo", &gl_smallFont, &cDConsole,
         "You have:\n"
         "Market Price:\n"
         "\n"
         "Free Space:\n"
         "Money:\n" );
   window_addText( wid, -20, -190, LAND_BUTTON_WIDTH/2, 80, 0,
         "txtDInfo", &gl_smallFont, &cBlack, NULL );
   window_addText( wid, -40, -290, LAND_BUTTON_WIDTH-20,
         h-140-LAND_BUTTON_HEIGHT, 0,
         "txtDesc", &gl_smallFont, &cBlack, NULL );

   /* goods list */
   if (land_planet->ncommodities > 0) {
      ngoods = land_planet->ncommodities;
      goods = malloc(sizeof(char*) * ngoods);
      tgoods    = malloc(sizeof(glTexture*) * ngoods);
      for (i=0; i<ngoods; i++) {
         goods[i] = strdup(land_planet->commodities[i]->name);
         tgoods[i] = land_planet->commodities[i]->gfx_store;
      }
   }
   else {
      goods    = malloc( sizeof(char*) );
      goods[0] = strdup("None");
      tgoods    = malloc(sizeof(glTexture*));
      tgoods[0] = NULL;
      ngoods   = 1;
   }

   /* set up the goods to buy/sell */
   window_addImageArray( wid, 20, 20,
         iw, ih, "iarTrade", 64, 64,
         tgoods, goods, ngoods, commodity_update, commodity_update );

   /* Set default keyboard focuse to the list */
   window_setFocus( wid , "iarTrade" );
 }


/**
 * @brief Updates the commodity window.
 *    @param wid Window to update.
 *    @param str Unused.
 */
void commodity_update( unsigned int wid, char* str )
{
   (void)str;
   char buf[PATH_MAX];
   char *comname;
   Commodity *com;
   
   comname = toolkit_getImageArray( wid, "iarTrade" );
   if ((comname==NULL) || (strcmp( comname, "None" )==0)) {
      nsnprintf( buf, PATH_MAX,
         "NA Tons\n"
         "NA Cr./Ton\n"
         "\n"
         "%d Tons\n"
         "%"CREDITS_PRI" Cr.\n",
         pilot_cargoFree(player.p),
         pilot_modCredits(player.p, 0) );
      window_modifyText( wid, "txtDInfo", buf );
      window_modifyText( wid, "txtDesc", "No commodities available." );
      window_disableButton( wid, "btnCommodityBuy" );
      window_disableButton( wid, "btnCommoditySell" );
      return;
   }
   com = commodity_get( comname );

   /* modify image */
   window_modifyImage( wid, "imgStore", com->gfx_store, 128, 128 );

   /* modify text */
   nsnprintf( buf, PATH_MAX,
         "%d Tons\n"
         "%"CREDITS_PRI" Cr./Ton\n"
         "\n"
         "%d Tons\n"
         "%"CREDITS_PRI" Cr.\n",
         pilot_cargoOwned( player.p, comname ),
         planet_commodityPrice( land_planet, com ),
         pilot_cargoFree(player.p),
         pilot_modCredits(player.p, 0) );
   window_modifyText( wid, "txtDInfo", buf );
   nsnprintf( buf, PATH_MAX,
         "%s\n"
         "\n"
         "%s",
         comname,
         com->description);
   window_modifyText( wid, "txtDesc", buf );

   /* Button enabling/disabling */
   if (commodity_canBuy( comname ))
      window_enableButton( wid, "btnCommodityBuy" );
   else
      window_disableButtonSoft( wid, "btnCommodityBuy" );

   if (commodity_canSell( comname ))
      window_enableButton( wid, "btnCommoditySell" );
   else
      window_disableButtonSoft( wid, "btnCommoditySell" );
}


int commodity_canBuy( char *name )
{
   int failure;
   unsigned int q, price;
   Commodity *com;
   char buf[ECON_CRED_STRLEN];

   failure = 0;
   q = commodity_getMod();
   com = commodity_get( name );
   price = planet_commodityPrice( land_planet, com ) * q;

   if (!player_hasCredits( price )) {
      credits2str( buf, price - player.p->credits, 2 );
      land_errDialogueBuild("You need %s more credits.", buf );
      failure = 1;
   }
   if (pilot_cargoFree(player.p) <= 0) {
      land_errDialogueBuild("No cargo space available!");
      failure = 1;
   }

   return !failure;
}


int commodity_canSell( char *name )
{
   int failure;

   failure = 0;

   if (pilot_cargoOwned( player.p, name ) == 0) {
      land_errDialogueBuild("You can't sell something you don't have!");
      failure = 1;
   }

   return !failure;
}


/**
 * @brief Buys the selected commodity.
 *    @param wid Window buying from.
 *    @param str Unused.
 */
void commodity_buy( unsigned int wid, char* str )
{
   (void)str;
   char *comname;
   Commodity *com;
   unsigned int q;
   credits_t price;
   HookParam hparam[3];
   
   /* Get selected. */
   q     = commodity_getMod();
   comname = toolkit_getImageArray( wid, "iarTrade" );
   com   = commodity_get( comname );
   price = planet_commodityPrice( land_planet, com );

   /* Check stuff. */
   if (land_errDialogue( comname, "buyCommodity" ))
      return;

   /* Make the buy. */
   q = pilot_cargoAdd( player.p, com, q, 0 );
   price *= q;
   player_modCredits( -price );
   commodity_update(wid, NULL);

   /* Run hooks. */
   hparam[0].type    = HOOK_PARAM_STRING;
   hparam[0].u.str   = comname;
   hparam[1].type    = HOOK_PARAM_NUMBER;
   hparam[1].u.num   = q;
   hparam[2].type    = HOOK_PARAM_SENTINEL;
   hooks_runParam( "comm_buy", hparam );
   if (land_takeoff)
      takeoff(1);
}
/**
 * @brief Attempts to sell a commodity.
 *    @param wid Window selling commodity from.
 *    @param str Unused.
 */
void commodity_sell( unsigned int wid, char* str )
{
   (void)str;
   char *comname;
   Commodity *com;
   unsigned int q;
   credits_t price;
   HookParam hparam[3];
   
   /* Get parameters. */
   q     = commodity_getMod();
   comname = toolkit_getImageArray( wid, "iarTrade" );
   com   = commodity_get( comname );
   price = planet_commodityPrice( land_planet, com );

   /* Check stuff. */
   if (land_errDialogue( comname, "sellCommodity" ))
      return;

   /* Remove commodity. */
   q = pilot_cargoRm( player.p, com, q );
   price = price * (credits_t)q;
   player_modCredits( price );
   commodity_update(wid, NULL);

   /* Run hooks. */
   hparam[0].type    = HOOK_PARAM_STRING;
   hparam[0].u.str   = comname;
   hparam[1].type    = HOOK_PARAM_NUMBER;
   hparam[1].u.num   = q;
   hparam[2].type    = HOOK_PARAM_SENTINEL;
   hooks_runParam( "comm_sell", hparam );
   if (land_takeoff)
      takeoff(1);
}

/**
 * @brief Gets the current modifier status.
 *    @return The amount modifier when buying or selling commodities.
 */
int commodity_getMod (void)
{
   SDLMod mods;
   int q;

   mods = SDL_GetModState();
   q = 10;
   if (mods & (KMOD_LCTRL | KMOD_RCTRL))
      q *= 5;
   if (mods & (KMOD_LSHIFT | KMOD_RSHIFT))
      q *= 10;

   return q;
}
/**
 * @brief Renders the commodity buying modifier.
 *    @param bx Base X position to render at.
 *    @param by Base Y position to render at.
 *    @param w Width to render at.
 *    @param h Height to render at.
 */
void commodity_renderMod( double bx, double by, double w, double h, void *data )
{
   (void) data;
   (void) h;
   int q;
   char buf[8];

   q = commodity_getMod();
   if (q != commodity_mod) {
      commodity_update( land_getWid(LAND_WINDOW_COMMODITY), NULL );
      commodity_mod = q;
   }
   nsnprintf( buf, 8, "%dx", q );
   gl_printMid( &gl_smallFont, w, bx, by, &cBlack, buf );
}
