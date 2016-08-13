/*
 * See Licensing and Copyright notice in naev.h
 */

/**
 * @file equipment.c
 *
 * @brief Handles equipping ships.
 */


#include "equipment.h"

#include "naev.h"

#include <stdlib.h>
#include <stdio.h>
#include "nstring.h"
#include <math.h>

#include "log.h"
#include "land.h"
#include "toolkit.h"
#include "dialogue.h"
#include "player.h"
#include "mission.h"
#include "ntime.h"
#include "opengl_vbo.h"
#include "conf.h"
#include "gui.h"
#include "land_outfits.h"
#include "info.h"
#include "shipstats.h"
#include "slots.h"
#include "map.h"
#include "ndata.h"
#include "tk/toolkit_priv.h" /* Yes, I'm a bad person, abstractions be damned! */


/*
 * Image array names.
 */
#define  EQUIPMENT_SHIPS      "iarAvailShips"
#define  EQUIPMENT_OUTFIT_TAB "tabOutfits"
#define  EQUIPMENT_OUTFITS    "iarAvailOutfits"
#define  EQUIPMENT_FILTER     "inpFilterOutfits"
#define  OUTFIT_TABS          5


/* global/main window */
#define BUTTON_WIDTH    200 /**< Default button width. */
#define BUTTON_HEIGHT   40 /**< Default button height. */

#define SHIP_ALT_MAX    512 /**< Maximum ship alt text. */


/*
 * equipment stuff
 */
static CstSlotWidget eq_wgt; /**< Equipment widget. */
static double equipment_dir      = 0.; /**< Equipment dir. */
static unsigned int equipment_lastick = 0; /**< Last tick. */
static gl_vbo *equipment_vbo     = NULL; /**< The VBO. */
static unsigned int equipment_wid   = 0; /**< Global wid. */
static iar_data_t *iar_data = NULL; /**< Stored image array positions. */

/* Slot textures */
static glTexture *equip_ico_yes = NULL; /* Green circle */
static glTexture *equip_ico_no  = NULL; /* Red circle with slash */

/*
 * prototypes
 */
/* Creation. */
static void equipment_getDim( unsigned int wid, int *w, int *h,
      int *sw, int *sh, int *ow, int *oh,
      int *ew, int *eh,
      int *cw, int *ch, int *bw, int *bh );
static void equipment_genShipList( unsigned int wid );
static void equipment_genOutfitList( unsigned int wid );
/* Widget. */
static void equipment_genLists( unsigned int wid );
static void equipment_renderColumn( double x, double y, double w, double h,
      int n, PilotOutfitSlot *lst, const char *txt,
      int selected, Outfit *o, Pilot *p, CstSlotWidget *wgt );
static void equipment_renderSlots( double bx, double by, double bw, double bh, void *data );
static void equipment_renderMisc( double bx, double by, double bw, double bh, void *data );
static void equipment_renderOverlayColumn( double x, double y, double w, double h,
      int n, PilotOutfitSlot *lst, int mover, CstSlotWidget *wgt );
static void equipment_renderOverlaySlots( double bx, double by, double bw, double bh,
      void *data );
static void equipment_renderShip( double bx, double by,
      double bw, double bh, double x, double y, Pilot *p );
static int equipment_mouseInColumn( double y, double h, int n, double my );
static int equipment_mouseSlots( unsigned int wid, SDL_Event* event,
      double x, double y, double w, double h, void *data );
/* Misc. */
static char eq_qCol( double cur, double base, int inv );
static int equipment_swapSlot( unsigned int wid, Pilot *p, PilotOutfitSlot *slot );
static void equipment_sellShip( unsigned int wid, char* str );
static void equipment_renameShip( unsigned int wid, char *str );
static void equipment_transChangeShip( unsigned int wid, char* str );
static void equipment_changeShip( unsigned int wid );
static void equipment_transportShip( unsigned int wid );
static void equipment_unequipShip( unsigned int wid, char* str );
static credits_t equipment_transportPrice( char *shipname );
static void equipment_filterOutfits( unsigned int wid, char *str );
static void equipment_rightClickOutfits( unsigned int wid, char* str );
static void equipment_changeTab( unsigned int wid, char *wgt, int old, int tab );


/**
 * @brief Handles right-click on unequipped outfit.
 *    @param wid Window to update.
 *    @param str Widget name. Must be EQUIPMENT_OUTFITS.
 *    @param clicked_outfit Name of the outfit the user right-clicked on.
 */
void equipment_rightClickOutfits( unsigned int wid, char* str )
{
   (void)str;
   Outfit* o;
   int i;
   int outfit_n;
   PilotOutfitSlot* slots;
   Pilot *p;
   const char* clicked_outfit = toolkit_getImageLayeredArray( wid, EQUIPMENT_OUTFITS );

   /* Did the user click on background? */
   if (clicked_outfit == NULL)
      return;

   /* Can't do anything with None. */
   if (strcmp(clicked_outfit,"None")==0)
      return;

   /* Try to get the outfit. */
   o = outfit_get(clicked_outfit);
   if (o == NULL)
      return;

   /* Figure out which slot this stuff fits into */
   switch (o->slot.type) {
      case OUTFIT_SLOT_STRUCTURE:
         outfit_n = eq_wgt.selected->outfit_nstructure;
         slots    = eq_wgt.selected->outfit_structure;
         break;
      case OUTFIT_SLOT_UTILITY:
         outfit_n = eq_wgt.selected->outfit_nutility;
         slots    = eq_wgt.selected->outfit_utility;
         break;
      case OUTFIT_SLOT_WEAPON:
         outfit_n = eq_wgt.selected->outfit_nweapon;
         slots    = eq_wgt.selected->outfit_weapon;
         break;
      default:
         return;
   }

   /* Loop through outfit slots of the right type, try to find an empty one */
   for (i=0; i<outfit_n; i++) {

      /* Slot full. */
      if (slots[i].outfit != NULL)
         continue;

      /* Must fit the slot. */
      if (!outfit_fitsSlot( o, &slots[i].sslot->slot))
         continue;

      /* Bingo! */
      eq_wgt.outfit  = o;
      p              = eq_wgt.selected;
      equipment_swapSlot( equipment_wid, p, &slots[i] );
      return;
   }
}

/**
 * @brief Gets the window dimensions.
 */
static void equipment_getDim( unsigned int wid, int *w, int *h,
      int *sw, int *sh, int *ow, int *oh,
      int *ew, int *eh,
      int *cw, int *ch, int *bw, int *bh )
{
   int ssw, ssh;
   /* Get window dimensions. */
   window_dimWindow( wid, w, h );

   /* Calculate image array dimensions. */
   ssw = 200 + (*w-800);
   ssh = (*h - 100);
   if (sw != NULL)
      *sw = ssw;
   if (sh != NULL)
      *sh = (1*ssh)/3;
   if (ow != NULL)
      *ow = ssw;
   if (oh != NULL)
      *oh = (2*ssh)/3;

   /* Calculate slot widget. */
   if (ew != NULL)
      *ew = 180.;
   if (eh != NULL)
      *eh = *h - 100;

   /* Calculate custom widget. */
   if (cw != NULL)
      *cw = *w - 20 - (sw!=NULL?*sw:0) - 20 - (ew!=NULL?*ew:0) - 20.;
   if (ch != NULL)
      *ch = *h - 100;

   /* Calculate button dimensions. */
   if (bw != NULL)
      *bw = (*w - 20 - (sw!=NULL?*sw:0) - 40 - 20 - 60) / 5;
   if (bh != NULL)
      *bh = BUTTON_HEIGHT;
}

/**
 * @brief Opens the player's equipment window.
 */
void equipment_open( unsigned int wid )
{
   int i;
   int w, h;
   int sw, sh;
   int ow, oh;
   int bw, bh;
   int ew, eh;
   int cw, ch;
   int x, y;
   GLfloat colour[4*4];
   const char *buf;

   /* Mark as generated. */
   land_tabGenerate(LAND_WINDOW_EQUIPMENT);

   /* Set global WID. */
   equipment_wid = wid;

   /* Create the vbo if necessary. */
   if (equipment_vbo == NULL) {
      equipment_vbo = gl_vboCreateStream( (sizeof(GLshort)*2 + sizeof(GLfloat)*4)*4, NULL );
      for (i=0; i<4; i++) {
         colour[i*4+0] = cRadar_player.r;
         colour[i*4+1] = cRadar_player.g;
         colour[i*4+2] = cRadar_player.b;
         colour[i*4+3] = cRadar_player.a;
      }
      gl_vboSubData( equipment_vbo, sizeof(GLshort)*2*4, sizeof(colour), colour );
   }

   /* Get dimensions. */
   equipment_getDim( wid, &w, &h, &sw, &sh, &ow, &oh,
         &ew, &eh, &cw, &ch, &bw, &bh );

   /* Initialize stored positions. */
   if (iar_data == NULL)
      iar_data = calloc( OUTFIT_TABS, sizeof(iar_data_t) );
   else
      memset( iar_data, 0, sizeof(iar_data_t) * OUTFIT_TABS );

   /* Sane defaults. */
   equipment_lastick    = SDL_GetTicks();
   equipment_dir        = 0.;
   eq_wgt.selected      = NULL;

   /* Icons */
   if (equip_ico_yes == NULL)
      equip_ico_yes = gl_newImage( GUI_GFX_PATH"yes.png", 0);
   if (equip_ico_no == NULL)
      equip_ico_no  = gl_newImage( GUI_GFX_PATH"no.png", 0);

   /* Add ammo. */
   equipment_addAmmo();

   /* buttons */
   window_addButtonKey( wid, -20, 20,
         bw, bh, "btnCloseEquipment",
         "Take Off", land_buttonTakeoff, SDLK_t );
   window_addButtonKey( wid, -20 - (15+bw), 20,
         bw, bh, "btnRenameShip",
         "Rename", equipment_renameShip, SDLK_r );
   window_addButtonKey( wid, -20 - (15+bw)*2, 20,
         bw, bh, "btnSellShip",
         "Sell Ship", equipment_sellShip, SDLK_s );
   window_addButtonKey( wid, -20 - (15+bw)*3, 20,
         bw, bh, "btnChangeShip",
         "Swap Ship", equipment_transChangeShip, SDLK_p );
   window_addButtonKey( wid, -20 - (15+bw)*4, 20,
         bw, bh, "btnUnequipShip",
         "Unequip", equipment_unequipShip, SDLK_u );

   /* text */
   buf = "Name:\n"
      "Model:\n"
      "Class:\n"
      "Value:\n"
      "\n"
      "Mass:\n"
      "Jump Time:\n"
      "Thrust:\n"
      "Speed:\n"
      "Turn:\n"
      "\n"
      "Absorption:\n"
      "Shield:\n"
      "Armour:\n"
      "Energy:\n"
      "Cargo Space:\n"
      "Fuel:\n"
      "\n"
      "Transportation:\n"
      "Location:\n"
      "Ship Status:";
   x = 20 + sw + 20 + 180 + 20 + 30;
   y = -190;
   window_addText( wid, x, y,
         100, h+y, 0, "txtSDesc", &gl_smallFont, &cDConsole, buf );
   x += 100;
   window_addText( wid, x, y,
         w - x - 20, h+y, 0, "txtDDesc", &gl_smallFont, &cBlack, NULL );

   /* Generate lists. */
   window_addText( wid, 30, -20,
         130, 200, 0, "txtShipTitle", &gl_defFont, &cBlack, "Available Ships" );
   window_addText( wid, 30, -40-sh-20,
         130, 200, 0, "txtOutfitTitle", &gl_defFont, &cBlack, "Available Outfits" );
   equipment_genLists( wid );

   /* Separator. */
   window_addRect( wid, 20 + sw + 20, -40, 2, h-60, "rctDivider", &cGrey50, 0 );

   /* Slot widget. */
   equipment_slotWidget( wid, 20 + sw + 40, -40, ew, eh, &eq_wgt );
   eq_wgt.canmodify = 1;

   /* Custom widget (ship information). */
   window_addCust( wid, 20 + sw + 40 + ew + 20, -40, cw, ch, "cstMisc", 0,
         equipment_renderMisc, NULL, NULL );

   /* Focus the ships image array. */
   window_setFocus( wid , EQUIPMENT_SHIPS );
}


/**
 * @brief Creates the slot widget and initializes it.
 *
 *    @param x X position to put it at.
 *    @param y Y position to put it at.
 *    @param w Width.
 *    @param h Height;
 *    @param data Dataset to use.
 */
void equipment_slotWidget( unsigned int wid,
      double x, double y, double w, double h,
      CstSlotWidget *data )
{
   /* Initialize data. */
   memset( data, 0, sizeof(CstSlotWidget) );
   data->slot        = -1;
   data->mouseover   = -1;
   data->weapons     = -1;

   /* Create the widget. */
   window_addCust( wid, x, y, w, h, "cstEquipment", 0,
         equipment_renderSlots, equipment_mouseSlots, data );
   window_custSetClipping( wid, "cstEquipment", 0 );
   window_custSetOverlay( wid, "cstEquipment", equipment_renderOverlaySlots );
}
/**
 * @brief Renders an outfit column.
 */
static void equipment_renderColumn( double x, double y, double w, double h,
      int n, PilotOutfitSlot *lst, const char *txt,
      int selected, Outfit *o, Pilot *p, CstSlotWidget *wgt )
{
   int i, level;
   const glColour *c, *dc, *rc;
   glColour bc;

   /* Render text. */
   if ((o != NULL) && (lst[0].sslot->slot.type == o->slot.type))
      c = &cDConsole;
   else
      c = &cBlack;
   gl_printMidRaw( &gl_smallFont, 60.,
         x-15., y+h+10., c, txt );

   /* Iterate for all the slots. */
   for (i=0; i<n; i++) {
      /* Choose default colour. */
      if (wgt->weapons >= 0) {
         level = pilot_weapSetCheck( p, wgt->weapons, &lst[i] );
         if (level == 0)
            dc = &cFontRed;
         else if (level == 1)
            dc = &cFontYellow;
         else if (lst[i].active)
            dc = &cFontBlue;
         else
            dc = &cInert;
      }
      else
         dc = outfit_slotSizeColour( &lst[i].sslot->slot );

      /* Choose colours based on size. */
      if ((i==selected) && (dc == NULL))
         dc = &cGrey60;

      /* Draw background. */
      bc = *dc;
      bc.a = 0.4;
      if (i==selected)
         c = &cDConsole;
      else
         c = &bc;
      toolkit_drawRect( x, y, w, h, c, NULL );

      if (lst[i].outfit != NULL) {
         /* Draw bugger. */
         gl_blitScale( lst[i].outfit->gfx_store,
               x, y, w, h, NULL );
      }
      else if ((o != NULL) &&
            (lst[i].sslot->slot.type == o->slot.type)) {
         /* Render the appropriate sprite, centered in each slot. */
         if (pilot_canEquip( p, &lst[i], o ) != NULL)
            gl_blitScale( equip_ico_no,
               x + w * .1, y + h * .1, w * .8, h * .8, NULL);
         else
            gl_blitScale( equip_ico_yes,
               x + w * .1, y + h * .1, w * .8, h * .8, NULL);
      }

      /* Must rechoose colour based on slot properties. */
      if (lst[i].sslot->required)
         rc = &cFontRed;
      else if (lst[i].sslot->slot.spid != 0)
         rc = &cDRestricted;
      else
         rc = dc;

      /* Draw outline. */
      toolkit_drawOutlineThick( x, y, w, h, 1, 3, rc, NULL );
      toolkit_drawOutline( x-1, y-1, w+3, h+3, 0, &cBlack, NULL );
      /* Go to next one. */
      y -= h+20;
   }
}


/**
 * @brief Calculates the size the slots need to be for a given window.
 *
 *    @param p Pilot to calculate the slots of.
 *    @param bw Base widget width.
 *    @param bh Base window height.
 *    @param w Width to use.
 *    @param h Height to use.
 *    @param n Number of columns.
 *    @param m Number of rows.
 */
static void equipment_calculateSlots( Pilot *p, double bw, double bh,
      double *w, double *h, int *n, int *m )
{
   double tw, th, s;
   int tm;

   /* Calculate size. */
   tm = MAX( MAX( p->outfit_nweapon, p->outfit_nutility ), p->outfit_nstructure );
   th = bh / (double)tm;
   tw = bw / 3.;
   s  = MIN( th, tw ) - 20.;
   th = s;
   tw = s;

   /* Return. */
   *w = tw;
   *h = th;
   *n = 3;
   *m = tm;
}
/**
 * @brief Renders the equipment slots.
 *
 *    @param bx Base X position of the widget.
 *    @param by Base Y position of the widget.
 *    @param bw Width of the widget.
 *    @param bh Height of the widget.
 *    @param data Custom widget data.
 */
static void equipment_renderSlots( double bx, double by, double bw, double bh, void *data )
{
   double x, y;
   double w, h;
   double tw;
   int n, m;
   CstSlotWidget *wgt;
   Pilot *p;
   int selected;

   /* Get data. */
   wgt = (CstSlotWidget*) data;
   p   = wgt->selected;
   selected = wgt->slot;

   /* Must have selected ship. */
   if (p == NULL)
      return;

   /* Get dimensions. */
   equipment_calculateSlots( p, bw, bh, &w, &h, &n, &m );
   tw = bw / (double)n;

   /* Draw weapon outfits. */
   x  = bx + (tw-w)/2;
   y  = by + bh - (h+20) + (h+20-h)/2;
   equipment_renderColumn( x, y, w, h,
         p->outfit_nweapon, p->outfit_weapon, "Weapon",
         selected, wgt->outfit, wgt->selected, wgt );

   /* Draw systems outfits. */
   selected -= p->outfit_nweapon;
   x += tw;
   y  = by + bh - (h+20) + (h+20-h)/2;
   equipment_renderColumn( x, y, w, h,
         p->outfit_nutility, p->outfit_utility, "Utility",
         selected, wgt->outfit, wgt->selected, wgt );

   /* Draw structure outfits. */
   selected -= p->outfit_nutility;
   x += tw;
   y  = by + bh - (h+20) + (h+20-h)/2;
   equipment_renderColumn( x, y, w, h,
         p->outfit_nstructure, p->outfit_structure, "Structure",
         selected, wgt->outfit, wgt->selected, wgt );
}
/**
 * @brief Renders the custom equipment widget.
 *
 *    @param bx Base X position of the widget.
 *    @param by Base Y position of the widget.
 *    @param bw Width of the widget.
 *    @param bh Height of the widget.
 *    @param data Custom widget data.
 */
static void equipment_renderMisc( double bx, double by, double bw, double bh, void *data )
{
   (void) data;
   Pilot *p;
   double percent;
   double x, y;
   double w, h;
   const glColour *lc, *c, *dc;

   /* Must have selected ship. */
   if (eq_wgt.selected == NULL)
      return;

   p = eq_wgt.selected;

   /* Render CPU and energy bars. */
   lc = &cWhite;
   c = &cGrey80;
   dc = &cGrey60;
   w = 120;
   h = 20;
   x = bx + 10.;
   y = by + bh - 30 - h;

   gl_printMidRaw( &gl_smallFont, w,
      x, y + h + 10., &cBlack, "CPU Free" );

   percent = (p->cpu_max > 0) ? CLAMP(0., 1., (float)p->cpu / (float)p->cpu_max) : 0.;
   toolkit_drawRect( x, y, w * percent, h, &cFontGreen, NULL );
   toolkit_drawRect( x + w * percent, y, w * (1.-percent), h, &cFontRed, NULL );
   toolkit_drawOutline( x, y, w, h, 1., lc, c  );
   toolkit_drawOutline( x, y, w, h, 2., dc, NULL  );
   gl_printMid( &gl_smallFont, w,
      x, y + h / 2. - gl_smallFont.h / 2.,
      &cBlack, "%d / %d", p->cpu, p->cpu_max );

   y -= h;

   gl_printMidRaw( &gl_smallFont, w,
      x, y, &cBlack, "Mass Limit Left" );

   y -= gl_smallFont.h + h;

   percent = (p->stats.engine_limit > 0) ? CLAMP(0., 1.,
      (p->stats.engine_limit - p->solid->mass) / p->stats.engine_limit) : 0.;
   toolkit_drawRect( x, y, w * percent, h, &cFontGreen, NULL );
   toolkit_drawRect( x + w * percent, y, w * (1.-percent), h, &cRestricted, NULL );
   toolkit_drawOutline( x, y, w, h, 1., lc, c  );
   toolkit_drawOutline( x, y, w, h, 2., dc, NULL  );
   gl_printMid( &gl_smallFont, w,
      x, y + h / 2. - gl_smallFont.h / 2.,
      &cBlack, "%.0f / %.0f", p->stats.engine_limit - p->solid->mass, p->stats.engine_limit );

   if (p->stats.engine_limit > 0. && p->solid->mass > p->stats.engine_limit) {
      y -= h;
      gl_printMid( &gl_smallFont, w,
         x, y, &cFontRed, "%.0f%% Slower",
         (1. - p->speed / p->speed_base) * 100);
   }

   x += w/2.;

   /* Render ship graphic. */
   equipment_renderShip( bx, by, bw, bh, x, y, p );
}


/**
 * @brief Renders an outfit column.
 *
 *    @param x X position to render at.
 *    @param y Y position to render at.
 *    @param w Width.
 *    @param h Height.
 *    @param n Number of elements.
 *    @param lst List of elements.
 *    @param mover Slot for which mouseover is active
 *    @param wgt Widget rendering.
 */
static void equipment_renderOverlayColumn( double x, double y, double w, double h,
      int n, PilotOutfitSlot *lst, int mover, CstSlotWidget *wgt )
{
   int i;
   const glColour *c;
   glColour tc;
   int text_width, xoff, yoff, top;
   const char *display;
   int subtitle;

   /* Iterate for all the slots. */
   for (i=0; i<n; i++) {
      subtitle = 0;
      if (lst[i].outfit != NULL) {
         /* See if needs a subtitle. */
         if ((outfit_isLauncher(lst[i].outfit) ||
                  (outfit_isFighterBay(lst[i].outfit))) &&
               ((lst[i].u.ammo.outfit == NULL) ||
                (lst[i].u.ammo.quantity < outfit_amount(lst[i].outfit))))
            subtitle = 1;
      }
      /* Draw bottom. */
      if ((i==mover) || subtitle) {
         display = NULL;
         if ((i==mover) && (wgt->canmodify)) {
            if (lst[i].outfit != NULL) {
               top = 1;
               display = pilot_canEquip( wgt->selected, &lst[i], NULL );
               if (display != NULL)
                  c = &cRed;
               else {
                  display = "Right click to remove";
                  c = &cDConsole;
               }
            }
            else if ((wgt->outfit != NULL) &&
                  (lst->sslot->slot.type == wgt->outfit->slot.type)) {
               top = 0;
               display = pilot_canEquip( wgt->selected, &lst[i], wgt->outfit );
               if (display != NULL)
                  c = &cRed;
               else {
                  display = "Right click to add";
                  c = &cDConsole;
               }
            }
         }
         else if (lst[i].outfit != NULL) {
            top = 1;
            if (outfit_isLauncher(lst[i].outfit) ||
                  (outfit_isFighterBay(lst[i].outfit))) {
               if ((lst[i].u.ammo.outfit == NULL) ||
                     (lst[i].u.ammo.quantity == 0)) {
                  if (outfit_isFighterBay(lst[i].outfit))
                     display = "Bay empty";
                  else
                     display = "Out of ammo";
                  c = &cRed;
               }
               else if (lst[i].u.ammo.quantity + lst[i].u.ammo.deployed <
                     outfit_amount(lst[i].outfit)) {
                  if (outfit_isFighterBay(lst[i].outfit))
                     display = "Bay low";
                  else
                     display = "Low ammo";
                  c = &cYellow;
               }
            }
         }

         if (display != NULL) {
            text_width = gl_printWidthRaw( &gl_smallFont, display );
            xoff = -(text_width - w)/2;
            if (top)
               yoff = h + 2;
            else
               yoff = -gl_smallFont.h - 3;
            tc.r = 1.;
            tc.g = 1.;
            tc.b = 1.;
            tc.a = 0.5;
            toolkit_drawRect( x+xoff-5, y -3. + yoff,
                  text_width+10, gl_smallFont.h+5,
                  &tc, NULL );
            gl_printMaxRaw( &gl_smallFont, text_width,
                  x+xoff, y + yoff,
                  c, display );
         }
      }
      /* Go to next one. */
      y -= h+20;
   }
}
/**
 * @brief Renders the equipment overlay.
 *
 *    @param bx Base X position of the widget.
 *    @param by Base Y position of the widget.
 *    @param bw Width of the widget.
 *    @param bh Height of the widget.
 *    @param data Custom widget data.
 */
static void equipment_renderOverlaySlots( double bx, double by, double bw, double bh,
      void *data )
{
   (void) bw;
   Pilot *p;
   int mover;
   double x, y;
   double w, h;
   double tw;
   int n, m;
   PilotOutfitSlot *slot;
   char alt[512];
   int pos;
   Outfit *o;
   CstSlotWidget *wgt;

   /* Get data. */
   wgt = (CstSlotWidget*) data;
   p   = wgt->selected;

   /* Must have selected ship. */
   if (wgt->selected == NULL)
      return;

   /* Get dimensions. */
   equipment_calculateSlots( p, bw, bh, &w, &h, &n, &m );
   tw = bw / (double)n;

   /* Get selected. */
   mover    = wgt->mouseover;

   /* Render weapon outfits. */
   x  = bx + (tw-w)/2;
   y  = by + bh - (h+20) + (h+20-h)/2;
   equipment_renderOverlayColumn( x, y, w, h,
         p->outfit_nweapon, p->outfit_weapon, mover, wgt );
   mover    -= p->outfit_nweapon;
   x += tw;
   y  = by + bh - (h+20) + (h+20-h)/2;
   equipment_renderOverlayColumn( x, y, w, h,
         p->outfit_nutility, p->outfit_utility, mover, wgt );
   mover    -= p->outfit_nutility;
   x += tw;
   y  = by + bh - (h+20) + (h+20-h)/2;
   equipment_renderOverlayColumn( x, y, w, h,
         p->outfit_nstructure, p->outfit_structure, mover, wgt );

   /* Mouse must be over something. */
   if (wgt->mouseover < 0)
      return;

   /* Get the slot. */
   if (wgt->mouseover < p->outfit_nweapon)
      slot = &p->outfit_weapon[wgt->mouseover];
   else if (wgt->mouseover < p->outfit_nweapon + p->outfit_nutility)
      slot = &p->outfit_utility[ wgt->mouseover - p->outfit_nweapon ];
   else
      slot = &p->outfit_structure[ wgt->mouseover -
         p->outfit_nweapon - p->outfit_nutility ];

   /* For comfortability. */
   o = slot->outfit;

   /* Slot is empty. */
   if (o == NULL) {
      if (slot->sslot->slot.spid == 0)
         return;

      pos = snprintf( alt, sizeof(alt),
            "\eS%s", sp_display( slot->sslot->slot.spid ) );
      if (slot->sslot->slot.exclusive && (pos < (int)sizeof(alt)))
         pos += snprintf( &alt[pos], sizeof(alt)-pos,
               " [exclusive]" );
      if (pos < (int)sizeof(alt))
         snprintf( &alt[pos], sizeof(alt)-pos,
               "\n\n%s", sp_description( slot->sslot->slot.spid ) );
      toolkit_drawAltText( bx + wgt->altx, by + wgt->alty, alt );
      return;
   }

   /* Get text. */
   if (o->desc_short == NULL)
      return;
   pos = nsnprintf( alt, sizeof(alt),
         "%s",
         o->name );
   if ((o->slot.spid!=0) && (pos < (int)sizeof(alt)))
      pos += snprintf( &alt[pos], sizeof(alt)-pos, "\n\eSSlot %s\e0",
            sp_display( o->slot.spid ) );
   if (pos < (int)sizeof(alt))
      pos += snprintf( &alt[pos], sizeof(alt)-pos, "\n\n%s", o->desc_short );
   if ((o->mass > 0.) && (pos < (int)sizeof(alt)))
      snprintf( &alt[pos], sizeof(alt)-pos,
            "\n%.0f Tons",
            o->mass );

   /* Draw the text. */
   toolkit_drawAltText( bx + wgt->altx, by + wgt->alty, alt );
}


/**
 * @brief Renders the ship in the equipment window.
 *
 *    @param bx Base X position of the widget.
 *    @param by Base Y position of the widget.
 *    @param bw Width of the widget.
 *    @param bh Height of the widget.
 *    @param x X position to render at.
 *    @param y Y position to render at.
 *    @param p Pilot to render.
 */
static void equipment_renderShip( double bx, double by,
      double bw, double bh, double x, double y, Pilot* p )
{
   int sx, sy;
   const glColour *lc, *c, *dc;
   unsigned int tick;
   double dt;
   double px, py;
   double pw, ph;
   double w, h;
   Vector2d v;
   GLshort vertex[2*4];

   tick = SDL_GetTicks();
   dt   = (double)(tick - equipment_lastick)/1000.;
   equipment_lastick = tick;
   equipment_dir += p->turn * dt;
   if (equipment_dir > 2*M_PI)
      equipment_dir = fmod( equipment_dir, 2*M_PI );
   gl_getSpriteFromDir( &sx, &sy, p->ship->gfx_space, equipment_dir );

   /* Render ship graphic. */
   if (p->ship->gfx_space->sw > 128) {
      pw = 128;
      ph = 128;
   }
   else {
      pw = p->ship->gfx_space->sw;
      ph = p->ship->gfx_space->sh;
   }
   w  = 128;
   h  = 128;
   px = (x+30) + (bx+bw - (x+30) - pw)/2;
   py = by + bh - 30 - h + (h-ph)/2 + 30;
   x  = (x+30) + (bx+bw - (x+30) - w)/2;
   y  = by + bh - 30 - h + 30;
   toolkit_drawRect( x-5, y-5, w+10, h+10, &cBlack, NULL );
   gl_blitScaleSprite( p->ship->gfx_space,
         px, py, sx, sy, pw, ph, NULL );
   if ((eq_wgt.slot >= 0) && (eq_wgt.slot < p->outfit_nweapon)) {
      p->tsx = sx;
      p->tsy = sy;
      pilot_getMount( p, &p->outfit_weapon[eq_wgt.slot], &v );
      px += pw/2;
      py += ph/2;
      v.x *= pw / p->ship->gfx_space->sw;
      v.y *= ph / p->ship->gfx_space->sh;
      /* Render it. */
      vertex[0] = px + v.x + 0.;
      vertex[1] = py + v.y - 7;
      vertex[2] = vertex[0];
      vertex[3] = py + v.y + 7;
      vertex[4] = px + v.x - 7.;
      vertex[5] = py + v.y + 0.;
      vertex[6] = px + v.x + 7.;
      vertex[7] = vertex[5];
      glLineWidth( 3. );
      gl_vboSubData( equipment_vbo, 0, sizeof(vertex), vertex );
      gl_vboActivateOffset( equipment_vbo, GL_VERTEX_ARRAY, 0, 2, GL_SHORT, 0 );
      gl_vboActivateOffset( equipment_vbo, GL_COLOR_ARRAY,
            sizeof(vertex), 4, GL_FLOAT, 0 );
      glDrawArrays( GL_LINES, 0, 4 );
      gl_vboDeactivate();
      glLineWidth( 1. );
   }
   lc = toolkit_colLight;
   c  = toolkit_col;
   dc = toolkit_colDark;
   toolkit_drawOutline( x - 4., y-4., w+7., h+2., 1., lc, c  );
   toolkit_drawOutline( x - 4., y-4., w+7., h+2., 2., dc, NULL  );
}
/**
 * @brief Handles a mouse press in column.
 *
 *    @param y Y position of the column.
 *    @param h Height of column.
 *    @param n Number of elements in column.
 *    @param my Mouse press position.
 *    @return Number pressed (or -1 if none).
 */
static int equipment_mouseInColumn( double y, double h, int n, double my )
{
   int i;

   for (i=0; i<n; i++) {
      if ((my > y) && (my < y+h+20))
         return i;
      y -= h+20;
   }

   return -1;
}
/**
 * @brief Handles a mouse press in a column.
 *
 *    @param wid Parent window id.
 *    @param event Mouse input event.
 *    @param mx Mouse X event position.
 *    @param my Mouse Y event position.
 *    @param y Y position of the column.
 *    @param h Height of the column.
 *    @param n Number of elements in the column.
 *    @param os Pointer to elements in the column.
 *    @param p Pilot to which the elements belong.
 *    @param selected Currently selected element.
 *    @param wgt Slot widget.
 */
static int equipment_mouseColumn( unsigned int wid, SDL_Event* event,
      double mx, double my, double y, double h, int n, PilotOutfitSlot* os,
      Pilot *p, int selected, CstSlotWidget *wgt )
{
   int ret, exists, level;

   ret = equipment_mouseInColumn( y, h, n, my );
   if (ret < 0)
      return 0;

   if (event->type == SDL_MOUSEBUTTONDOWN) {
      /* Normal mouse usage. */
      if (wgt->weapons < 0) {
         if (event->button.button == SDL_BUTTON_LEFT)
            wgt->slot = selected + ret;
         else if ((event->button.button == SDL_BUTTON_RIGHT) &&
               wgt->canmodify)
            equipment_swapSlot( wid, p, &os[ret] );
      }
      /* Viewing weapon slots. */
      else {
         /* See if it exists. */
         exists = pilot_weapSetCheck( p, wgt->weapons, &os[ret] );
         /* Get the level of the selection. */
         if (event->button.button== SDL_BUTTON_LEFT)
            level = 0;
         else if (event->button.button== SDL_BUTTON_RIGHT)
            level = 1;
         else
            return 0; /* We ignore this type of click. */
         /* See if we should add it or remove it. */
         if (exists==level)
            pilot_weapSetRm( p, wgt->weapons, &os[ret] );
         else {
            /* This is a bloody awful place to do this. I hate it. HATE!. */
            /* Case active outfit, convert the weapon group to active outfit. */
            if ((os->sslot->slot.type == OUTFIT_SLOT_STRUCTURE) ||
                  (os->sslot->slot.type == OUTFIT_SLOT_UTILITY)) {
               pilot_weapSetRmSlot( p, wgt->weapons, OUTFIT_SLOT_WEAPON );
               pilot_weapSetAdd( p, wgt->weapons, &os[ret], 0 );
               pilot_weapSetType( p, wgt->weapons, WEAPSET_TYPE_ACTIVE );
            }
            /* Case change weapon groups or active weapon. */
            else {
               pilot_weapSetRmSlot( p, wgt->weapons, OUTFIT_SLOT_STRUCTURE );
               pilot_weapSetRmSlot( p, wgt->weapons, OUTFIT_SLOT_UTILITY );
               if (pilot_weapSetTypeCheck( p, wgt->weapons) == WEAPSET_TYPE_CHANGE)
                  pilot_weapSetType( p, wgt->weapons, WEAPSET_TYPE_CHANGE );
               else {
                  pilot_weapSetType( p, wgt->weapons, WEAPSET_TYPE_WEAPON );
                  level = 0;
               }
               pilot_weapSetAdd( p, wgt->weapons, &os[ret], level );
            }
         }
         p->autoweap = 0; /* Disable autoweap. */
         info_update(); /* Need to update weapons. */
      }
   }
   else {
      wgt->mouseover  = selected + ret;
      wgt->altx       = mx;
      wgt->alty       = my;
   }

   return 1;
}
/**
 * @brief Does mouse input for the custom equipment widget.
 *
 *    @param wid Parent window id.
 *    @param event Mouse input event.
 *    @param mx Mouse X event position.
 *    @param my Mouse Y event position.
 *    @param bw Base window width.
 *    @param bh Base window height.
 *    @param data Custom widget data.
 */
static int equipment_mouseSlots( unsigned int wid, SDL_Event* event,
      double mx, double my, double bw, double bh, void *data )
{
   (void) bw;
   Pilot *p;
   int selected, ret;
   double x, y;
   double w, h;
   double tw;
   CstSlotWidget *wgt;
   int n, m;

   /* Get data. */
   wgt = (CstSlotWidget*) data;
   p   = wgt->selected;

   /* Must have selected ship. */
   if (p == NULL)
      return 0;

   /* Must be left click for now. */
   if ((event->type != SDL_MOUSEBUTTONDOWN) &&
         (event->type != SDL_MOUSEMOTION))
      return 0;

   /* Get dimensions. */
   equipment_calculateSlots( p, bw, bh, &w, &h, &n, &m );
   tw = bw / (double)n;

   /* Render weapon outfits. */
   selected = 0;
   x  = (tw-w)/2;
   y  = bh - (h+20) + (h+20-h)/2 - 10;
   if ((mx > x-10) && (mx < x+w+10)) {
      ret = equipment_mouseColumn( wid, event, mx, my, y, h,
            p->outfit_nweapon, p->outfit_weapon, p, selected, wgt );
      if (ret)
         return !!(event->type == SDL_MOUSEBUTTONDOWN);
   }
   selected += p->outfit_nweapon;
   x += tw;
   if ((mx > x-10) && (mx < x+w+10)) {
      ret = equipment_mouseColumn( wid, event, mx, my, y, h,
            p->outfit_nutility, p->outfit_utility, p, selected, wgt );
      if (ret)
         return !!(event->type == SDL_MOUSEBUTTONDOWN);
   }
   selected += p->outfit_nutility;
   x += tw;
   if ((mx > x-10) && (mx < x+w+10)) {
      ret = equipment_mouseColumn( wid, event, mx, my, y, h,
            p->outfit_nstructure, p->outfit_structure, p, selected, wgt );
      if (ret)
         return !!(event->type == SDL_MOUSEBUTTONDOWN);
   }

   /* Not over anything. */
   wgt->mouseover = -1;
   return 0;
}


/**
 * @brief Swaps an equipment slot.
 *
 *    @param wid Parent window id.
 *    @param p Pilot swapping slots.
 *    @param slot Slot to swap.
 */
static int equipment_swapSlot( unsigned int wid, Pilot *p, PilotOutfitSlot *slot )
{
   int ret;
   Outfit *o, *ammo;
   int q;

   /* Remove outfit. */
   if (slot->outfit != NULL) {
      o = slot->outfit;

      /* Must be able to remove. */
      if (pilot_canEquip( eq_wgt.selected, slot, NULL ) != NULL)
         return 0;

      /* Remove ammo first. */
      ammo = outfit_ammo(o);
      if (ammo != NULL) {
         ammo = slot->u.ammo.outfit;
         q    = pilot_rmAmmo( eq_wgt.selected, slot, slot->u.ammo.quantity );
         player_addOutfit( ammo, q );
      }

      /* Remove outfit. */
      ret = pilot_rmOutfit( eq_wgt.selected, slot );
      if (ret == 0)
         player_addOutfit( o, 1 );
   }
   /* Add outfit. */
   else {
      /* Must have outfit. */
      o = eq_wgt.outfit;
      if (o==NULL)
         return 0;

      /* Must fit slot. */
      if (!outfit_fitsSlot( o, &slot->sslot->slot ))
         return 0;

      /* Must be able to add. */
      if (pilot_canEquip( eq_wgt.selected, slot, o ) != NULL)
         return 0;

      /* Add outfit to ship. */
      ret = player_rmOutfit( o, 1 );
      if (ret == 1) {
         pilot_addOutfitRaw( eq_wgt.selected, o, slot );

         /* Recalculate stats. */
         pilot_calcStats( eq_wgt.selected );
      }

      equipment_addAmmo();
   }

   /* Refuel if necessary. */
   land_refuel();

   /* Recalculate stats. */
   pilot_calcStats( p );
   pilot_healLanded( p );

   /* Redo the outfits thingy. */
   equipment_regenLists( wid, 1, 1 );

   /* Update outfits. */
   outfits_updateEquipmentOutfits();

   /* Update weapon sets if needed. */
   if (eq_wgt.selected->autoweap)
      pilot_weaponAuto( eq_wgt.selected );
   pilot_weaponSane( eq_wgt.selected );

   /* Notify GUI of modification. */
   gui_setShip();

   return 0;
}


/**
 * @brief Regenerates the equipment window lists.
 *
 *    @param wid Window to regenerate lists.
 *    @param outfits Whether or not should regenerate outfits list.
 *    @param ships Whether or not to regenerate ships list.
 */
void equipment_regenLists( unsigned int wid, int outfits, int ships )
{
   int i, ret;
   int nship;
   double offship;
   char *s, selship[PATH_MAX];
   char *focused;

   /* Default.s */
   nship    = 0;
   offship  = 0.;

   /* Must exist. */
   if (!window_existsID( equipment_wid ))
      return;

   /* Save focus. */
   focused = strdup(window_getFocus(wid));

   /* Save positions. */
   if (outfits) {
      i = window_tabWinGetActive( wid, EQUIPMENT_OUTFIT_TAB );
      toolkit_saveImageLayeredArrayData( wid, EQUIPMENT_OUTFITS, &iar_data[i] );
      window_destroyWidget( wid, EQUIPMENT_OUTFITS );
   }
   if (ships) {
      nship   = toolkit_getImageArrayPos(    wid, EQUIPMENT_SHIPS );
      offship = toolkit_getImageArrayOffset( wid, EQUIPMENT_SHIPS );
      s       = toolkit_getImageArray(       wid, EQUIPMENT_SHIPS );
      strncpy( selship, s, sizeof(selship) );
      selship[PATH_MAX-1] = '\0'; /* Just in case. */
      window_destroyWidget( wid, EQUIPMENT_SHIPS );
   }

   /* Regenerate lists. */
   equipment_genLists( wid );

   /* Restore positions. */
   if (outfits) {
      toolkit_setImageLayeredArrayPos(    wid, EQUIPMENT_OUTFITS, iar_data[i].pos );
      toolkit_setImageLayeredArrayOffset( wid, EQUIPMENT_OUTFITS, iar_data[i].offset );
      equipment_updateOutfits( wid, NULL );
   }
   if (ships) {
      toolkit_setImageArrayPos(    wid, EQUIPMENT_SHIPS, nship );
      toolkit_setImageArrayOffset( wid, EQUIPMENT_SHIPS, offship );
      /* Try to maintain same ship selected. */
      s = toolkit_getImageArray( wid, EQUIPMENT_SHIPS );
      if ((s != NULL) && (selship != NULL) && (strcmp(s,selship)!=0)) {
         ret = toolkit_setImageArray( wid, EQUIPMENT_SHIPS, selship );
         if (ret != 0) /* Failed to maintain. */
            toolkit_setImageArrayPos( wid, EQUIPMENT_SHIPS, nship );

         /* Update ships. */
         equipment_updateShips( wid, NULL );
      }
   }

   /* Restore focus. */
   window_setFocus( wid, focused );
   free(focused);
}


/**
 * @brief Adds all the ammo it can to the player.
 */
void equipment_addAmmo (void)
{
   int i;
   Outfit *o, *ammo;
   int q;
   Pilot *p;

   /* Get player. */
   if (eq_wgt.selected == NULL)
      p = player.p;
   else
      p = eq_wgt.selected;

   /* Add ammo to all outfits. */
   for (i=0; i<p->noutfits; i++) {
      o = p->outfits[i]->outfit;

      /* Must be valid outfit. */
      if (o == NULL)
         continue;

      /* Add ammo if able to. */
      ammo = outfit_ammo(o);
      if (ammo == NULL)
         continue;
      q    = player_outfitOwned(ammo);

      /* Add ammo. */
      q = pilot_addAmmo( p, p->outfits[i], ammo, q );

      /* Remove from player. */
      player_rmOutfit( ammo, q );
   }

   /* Update weapon sets if needed. */
   if (p->autoweap)
      pilot_weaponAuto( p );
   pilot_weaponSane( p );

   /* Notify GUI of modification. */
   gui_setShip();
}


/**
 * @brief Creates and allocates a string containing the ship stats.
 *
 *    @param buf Buffer to write to.
 *    @param max_len Maximum length of the string to allocate.
 *    @param s Pilot to get stats of.
 *    @param dpseps Whether or not to display dps and eps.
 */
int equipment_shipStats( char *buf, int max_len,  const Pilot *s, int dpseps )
{
   int j, l;
   Outfit *o;
   double mod_energy, mod_damage, mod_shots;
   double eps, dps, shots;
   const Damage *dmg;

   dps = 0.;
   eps = 0.;
   /* Calculate damage and energy per second. */
   if (dpseps) {
      for (j=0; j<s->noutfits; j++) {
         o = s->outfits[j]->outfit;
         if (o==NULL)
            continue;
         switch (o->type) {
            case OUTFIT_TYPE_BOLT:
               mod_energy = s->stats.fwd_energy;
               mod_damage = s->stats.fwd_damage;
               mod_shots  = 2. - s->stats.fwd_firerate;
               break;
            case OUTFIT_TYPE_TURRET_BOLT:
               mod_energy = s->stats.tur_energy;
               mod_damage = s->stats.tur_damage;
               mod_shots  = 2. - s->stats.tur_firerate;
               break;
            case OUTFIT_TYPE_LAUNCHER:
            case OUTFIT_TYPE_TURRET_LAUNCHER:
               mod_energy = 1.;
               mod_damage = 1.;
               mod_shots  = 1.; /* @todo Should be: 2. - s>stats.launch_rate */
               break;
            case OUTFIT_TYPE_BEAM:
            case OUTFIT_TYPE_TURRET_BEAM:
               /* Special case due to continuous fire. */
               dps += outfit_damage(o)->damage;
               eps += outfit_energy(o);

               continue;
            default:
               continue;
         }
         shots = 1. / (mod_shots * outfit_delay(o));

         /* Special case: Ammo-based weapons. */
         if (outfit_isLauncher(o))
            dmg = outfit_damage(o->u.lau.ammo);
         else
            dmg = outfit_damage(o);
         dps  += shots * mod_damage * dmg->damage;
         eps  += shots * mod_energy * MAX( outfit_energy(o), 0. );
      }
   }

   /* Write to buffer. */
   l = 0;
   if (dps > 0.)
      l += nsnprintf( &buf[l], (max_len-l),
            "%s%.2f DPS [%.2f EPS]", (l!=0)?"\n":"", dps, eps );
   l += ss_statsDesc( &s->stats, &buf[l], (max_len-l), 1 );
   return l;
}


/**
 * @brief Generates a new ship/outfit lists if needed.
 *
 *    @param wid Parent window id.
 */
static void equipment_genLists( unsigned int wid )
{
   /* Ship list. */
   equipment_genShipList( wid );

   /* Outfit list. */
   equipment_genOutfitList( wid );

   /* Update window. */
   equipment_updateOutfits(wid, NULL);
   equipment_updateShips(wid, NULL);
}


/**
 * @brief Generates the ship list.
 *    @param wid Window to generate list on.
 */
static void equipment_genShipList( unsigned int wid )
{
   int i, l;
   char **sships;
   glTexture **tships;
   int nships;
   int w, h;
   int sw, sh;
   char **alt;
   Pilot *s;

   /* Get dimensions. */
   equipment_getDim( wid, &w, &h, &sw, &sh, NULL, NULL,
         NULL, NULL, NULL, NULL, NULL, NULL );

   /* Ship list. */
   if (!widget_exists( wid, EQUIPMENT_SHIPS )) {
      eq_wgt.selected = NULL;
      if (planet_hasService(land_planet, PLANET_SERVICE_SHIPYARD))
         nships   = player_nships()+1;
      else
         nships   = 1;
      sships   = malloc(sizeof(char*)*nships);
      tships   = malloc(sizeof(glTexture*)*nships);
      /* Add player's current ship. */
      sships[0] = strdup(player.p->name);
      tships[0] = player.p->ship->gfx_store;
      if (planet_hasService(land_planet, PLANET_SERVICE_SHIPYARD))
         player_ships( &sships[1], &tships[1] );
      window_addImageArray( wid, 20, -40,
            sw, sh, EQUIPMENT_SHIPS, 64./96.*128., 64.,
            tships, sships, nships, equipment_updateShips, NULL );

      /* Ship stats in alt text. */
      alt   = malloc( sizeof(char*) * nships );
      for (i=0; i<nships; i++) {
         s        = player_getShip( sships[i] );
         alt[i]   = malloc( SHIP_ALT_MAX );
         l        = nsnprintf( &alt[i][0], SHIP_ALT_MAX, "Ship Stats\n" );
         l        = equipment_shipStats( &alt[i][l], SHIP_ALT_MAX-l, s, 1 );
         if (l == 0) {
            free( alt[i] );
            alt[i] = NULL;
         }
      }
      toolkit_setImageArrayAlt( wid, EQUIPMENT_SHIPS, alt );
   }
}


static int equipment_outfitFilterWeapon( const Outfit *o )
{ return ((o->slot.type == OUTFIT_SLOT_WEAPON) && !sp_required( o->slot.spid )); }

static int equipment_outfitFilterUtility( const Outfit *o )
{ return ((o->slot.type == OUTFIT_SLOT_UTILITY) && !sp_required( o->slot.spid )); }

static int equipment_outfitFilterStructure( const Outfit *o )
{ return ((o->slot.type == OUTFIT_SLOT_STRUCTURE) && !sp_required( o->slot.spid )); }

static int equipment_outfitFilterCore( const Outfit *o )
{ return sp_required( o->slot.spid ); }


/**
 * @brief Generates the outfit list.
 *    @param wid Window to generate list on.
 */
static void equipment_genOutfitList( unsigned int wid )
{
   int x, y, w, h, ow, oh;
   int ix, iy, iw, ih, barw; /* Input filter. */
   char *filtertext;
   int (*tabfilters[])( const Outfit *o ) = {
      NULL,
      equipment_outfitFilterWeapon,
      equipment_outfitFilterUtility,
      equipment_outfitFilterStructure,
      equipment_outfitFilterCore
   };
   const char *tabnames[] = {
      "All", "\eb W ", "\eg U ", "\ep S ", "\eRCore"
   };

   int active, i, l, p, noutfits;
   char **soutfits, **alt, **quantity, **slottype;
   glTexture ***toutfits;
   int *ntoutfits;
   Outfit *o, **outfits;
   const glColour *c;
   glColour *bg, blend;
   const char *typename;

   /* Get dimensions. */
   equipment_getDim( wid, &w, &h, NULL, NULL, &ow, &oh,
         NULL, NULL, NULL, NULL, NULL, NULL );

   /* Deselect. */
   eq_wgt.outfit = NULL;

   /* Calculate position. */
   x = 20;
   y = 20;

   /* Create tabbed window. */
   if (!widget_exists( wid, EQUIPMENT_OUTFIT_TAB )) {
      window_addTabbedWindow( wid, x, y + oh - 30, ow, 30,
            EQUIPMENT_OUTFIT_TAB, OUTFIT_TABS, tabnames, 1 );

      barw = window_tabWinGetBarWidth( wid, EQUIPMENT_OUTFIT_TAB );

      iw = CLAMP(0, 150, ow - barw - 30);
      ih = 20;

      ix = ow - iw;
      iy = oh - (30 - ih) / 2; /* Centered relative to 30 px tab bar */

      /* Only create the filter widget if it will be a reasonable size. */
      if (iw >= 30) {
         window_addInput( wid, ix, iy, iw, ih, EQUIPMENT_FILTER, 32, 1, &gl_smallFont );
         window_setInputCallback( wid, EQUIPMENT_FILTER, equipment_filterOutfits );
      }
   }

   window_tabWinOnChange( wid, EQUIPMENT_OUTFIT_TAB, equipment_changeTab );
   active = window_tabWinGetActive( equipment_wid, EQUIPMENT_OUTFIT_TAB );

   /* Widget must not already exist. */
   if (widget_exists( wid, EQUIPMENT_OUTFITS ))
      return;

   /* Allocate space. */
   noutfits = MAX( 1, player_numOutfits() ); /* This is the most we'll need, probably less due to filtering. */
   outfits  = calloc( noutfits, sizeof(Outfit*) );
   soutfits = calloc( noutfits, sizeof(char*) );
   toutfits = calloc( noutfits, sizeof(glTexture**) );
   ntoutfits = calloc( noutfits, sizeof(int) );

   filtertext = NULL;
   if (widget_exists(equipment_wid, EQUIPMENT_FILTER)) {
      filtertext = window_getInput( equipment_wid, EQUIPMENT_FILTER );
      if (strlen(filtertext) == 0)
         filtertext = NULL;
   }

   /* Get the outfits. */
   noutfits = player_getOutfitsFiltered( outfits, toutfits, ntoutfits,
         tabfilters[active], filtertext );

   if (noutfits == 0) {
      noutfits = 1;
      soutfits[0] = strdup( "None" );
      toutfits[0] = NULL;
      ntoutfits[0] = 0;

      /* Clean up. */
      free(outfits);
   }
   else
      for (i=0; i<noutfits; i++)
         soutfits[i] = strdup( outfits[i]->name );

   /* Create the actual image array. */
   window_addImageLayeredArray( wid, x, y, ow, oh - 31,
         EQUIPMENT_OUTFITS, 50., 50.,
         toutfits, ntoutfits, soutfits, noutfits,
         equipment_updateOutfits,
         equipment_rightClickOutfits );

   /* Case there are none we don't need to do more. */
   if (strcmp( soutfits[0], "None" )==0)
      return;

   /* Set alt text. */
   alt      = malloc( sizeof(char*) * noutfits );
   quantity = malloc( sizeof(char*) * noutfits );
   slottype = malloc( sizeof(char*) * noutfits );
   bg       = malloc( sizeof(glColour) * noutfits );

   /* Process all the outfits. */
   for (i=0; i<noutfits; i++) {
      o = outfits[i];

      /* Background colour. */
      c = outfit_slotSizeColour( &o->slot );
      if (c == NULL)
         c = &cBlack;
      col_blend( &blend, c, &cGrey70, 0.4 );
      bg[i] = blend;

      /* Short description. */
      if (o->desc_short == NULL)
         alt[i] = NULL;
      else {
         l = strlen(o->desc_short) + 128;
         alt[i] = malloc( l );
         p  = snprintf( &alt[i][0], l, "%s\n", o->name );
         if ((o->slot.spid!=0) && (p < l))
            p += snprintf( &alt[i][p], l-p, "\eSSlot %s\e0\n",
                  sp_display( o->slot.spid ) );
         if (p < l)
            p += snprintf( &alt[i][p], l-p, "\n%s", o->desc_short );
         if ((o->mass > 0.) && (p < l))
            snprintf( &alt[i][p], l-p,
                  "\n%.0f Tons",
                  o->mass );
      }

      /* Quantity. */
      p = player_outfitOwned(o);
      l = p / 10 + 4;
      quantity[i] = malloc( l );
      snprintf( quantity[i], l, "%d", p );

      /* Slot type. */
      if ((strcmp(outfit_slotName(o),"NA") != 0) &&
            (strcmp(outfit_slotName(o),"NULL") != 0)) {
         typename       = outfit_slotName(o);
         slottype[i]    = malloc( 2 );
         slottype[i][0] = typename[0];
         slottype[i][1] = '\0';
      }
      else
         slottype[i] = NULL;
   }

   /* Clean up. */
   free(outfits);

   /* Set misc stuff. */
   toolkit_setImageLayeredArrayAlt( wid,         EQUIPMENT_OUTFITS, alt );
   toolkit_setImageLayeredArrayQuantity( wid,    EQUIPMENT_OUTFITS, quantity );
   toolkit_setImageLayeredArraySlotType( wid,    EQUIPMENT_OUTFITS, slottype );
   toolkit_setImageLayeredArrayBackground( wid,  EQUIPMENT_OUTFITS, bg );
}


/**
 * @brief Gets the colour for comparing a current value vs a ship base value.
 */
static char eq_qCol( double cur, double base, int inv )
{
   if (cur > 1.2*base)
      return (inv) ? 'r' : 'D';
   else if (cur < 0.8*base)
      return (inv) ? 'D' : 'r';
   return '0';
}


#define EQ_COMP( cur, base, inv ) \
eq_qCol( cur, base, inv ), cur
/**
 * @brief Updates the player's ship window.
 *    @param wid Window to update.
 *    @param str Unused.
 */
void equipment_updateShips( unsigned int wid, char* str )
{
   (void)str;
   char buf[1024], sysname[128], buf2[ECON_CRED_STRLEN], buf3[ECON_CRED_STRLEN];
   char errorReport[256];
   char *shipname;
   Pilot *ship;
   char *loc, *nt;
   credits_t price;
   int onboard;
   int cargo;

   /* Clear defaults. */
   eq_wgt.slot          = -1;
   eq_wgt.mouseover     = -1;
   equipment_lastick    = SDL_GetTicks();

   /* Get the ship. */
   shipname = toolkit_getImageArray( wid, EQUIPMENT_SHIPS );
   if (strcmp(shipname,player.p->name)==0) { /* no ships */
      ship    = player.p;
      loc     = "Onboard";
      price   = 0;
      onboard = 1;
      sysname[0] = '\0';
   }
   else {
      ship   = player_getShip( shipname );
      loc    = player_getLoc(ship->name);
      price  = equipment_transportPrice( shipname );
      onboard = 0;
      nsnprintf( sysname, sizeof(sysname), " in the %s system",
            planet_getSystem(loc) );
   }
   eq_wgt.selected = ship;

   /* update text */
   price2str( buf2, price, player.p->credits, 2 ); /* transport */
   credits2str( buf3, player_shipPrice(shipname), 2 ); /* sell price */
   cargo = pilot_cargoFree(ship) + pilot_cargoUsed(ship);
   nt = ntime_pretty( pilot_hyperspaceDelay( ship ), 2 );

   /* Get ship error report. */
   pilot_reportSpaceworthy( ship, errorReport, sizeof(errorReport));

   /* Fill the buffer. */
   nsnprintf( buf, sizeof(buf),
         "%s\n"
         "%s\n"
         "%s\n"
         "%s credits\n"
         "\n"
         "%.0f\e0 tonnes\n"
         "\e%c%s\e0 average\n"
         "\e%c%.0f\e0 kN/tonne\n"
         "\e%c%.0f\e0 m/s (max \e%c%.0f\e0 m/s)\n"
         "\e%c%.0f\e0 deg/s\n"
         "\n"
         "\e%c%.0f%%\n"
         "\e%c%.0f\e0 MJ (\e%c%.1f\e0 MW)\n"
         "\e%c%.0f\e0 MJ (\e%c%.1f\e0 MW)\n"
         "\e%c%.0f\e0 MJ (\e%c%.1f\e0 MW)\n"
         "%d / \e%c%d\e0 tonnes\n"
         "%.0f / \e%c%.0f\e0 units (%d jumps)\n"
         "\n"
         "%s credits\n"
         "%s%s\n"
         "\e%c%s\e0",
         /* Generic. */
      ship->name,
      ship->ship->name,
      ship_class(ship->ship),
      buf3,
      /* Movement. */
      ship->solid->mass,
      '0', nt,
      EQ_COMP( ship->thrust/ship->solid->mass, ship->ship->thrust/ship->ship->mass, 0 ),
      EQ_COMP( ship->speed, ship->ship->speed, 0 ),
      EQ_COMP( solid_maxspeed( ship->solid, ship->speed, ship->thrust ),
            solid_maxspeed( ship->solid, ship->ship->speed, ship->ship->thrust), 0 ),
      EQ_COMP( ship->turn*180./M_PI, ship->ship->turn*180./M_PI, 0 ),
      /* Health. */
      EQ_COMP( ship->dmg_absorb * 100, ship->ship->dmg_absorb * 100, 0 ),
      EQ_COMP( ship->shield_max, ship->ship->shield, 0 ),
      EQ_COMP( ship->shield_regen, ship->ship->shield_regen, 0 ),
      EQ_COMP( ship->armour_max, ship->ship->armour, 0 ),
      EQ_COMP( ship->armour_regen, ship->ship->armour_regen, 0 ),
      EQ_COMP( ship->energy_max, ship->ship->energy, 0 ),
      EQ_COMP( ship->energy_regen, ship->ship->energy_regen, 0 ),
      /* Misc. */
      pilot_cargoUsed(ship), EQ_COMP( cargo, ship->ship->cap_cargo, 0 ),
      ship->fuel, EQ_COMP( ship->fuel_max, ship->ship->fuel, 0 ), pilot_getJumps(ship),
      /* Transportation. */
      buf2,
      loc, sysname,
      pilot_checkSpaceworthy(ship) ? 'r' : '0', errorReport );
   window_modifyText( wid, "txtDDesc", buf );

   /* Clean up. */
   free( nt );

   /* button disabling */
   if (onboard) {
      window_disableButton( wid, "btnSellShip" );
      window_disableButton( wid, "btnChangeShip" );
   }
   else {
      if (strcmp(land_planet->name,loc)) { /* ship not here */
         window_buttonCaption( wid, "btnChangeShip", "Transport" );
         if (!player_hasCredits( price ))
            window_disableButton( wid, "btnChangeShip" );
         else
            window_enableButton( wid, "btnChangeShip" );
      }
      else { /* ship is here */
         window_buttonCaption( wid, "btnChangeShip", "Swap Ship" );
         window_enableButton( wid, "btnChangeShip" );
      }
      /* If ship is there you can always sell. */
      window_enableButton( wid, "btnSellShip" );
   }
}
#undef EQ_COMP
/**
 * @brief Updates the player's outfit list.
 *    @param wid Window to update.
 *    @param str Unused.
 */
void equipment_updateOutfits( unsigned int wid, char* str )
{
   (void) wid;
   (void) str;
   const char *oname;

   /* Must have outfit. */
   oname = toolkit_getImageLayeredArray( wid, EQUIPMENT_OUTFITS );
   if (strcmp(oname,"None")==0) {
      eq_wgt.outfit = NULL;
      return;
   }

   eq_wgt.outfit = outfit_get( oname );
}

/**
 * @brief Handles text input in the filter input widget.
 *    @param wid Window containing the widget.
 *    @param str Unused.
 */
static void equipment_filterOutfits( unsigned int wid, char *str )
{
   (void) str;
   equipment_regenLists(wid, 1, 0);
}

/**
 * @brief Ensures the tab's selected item is reflected in the ship slot list
 *
 *    @param wid Unused.
 *    @param wgt Unused.
 *    @param tab Tab changed to.
 */
static void equipment_changeTab( unsigned int wid, char *wgt, int old, int tab )
{
   (void) wid;
   (void) wgt;
   int pos;
   double offset;

   toolkit_saveImageLayeredArrayData( wid, EQUIPMENT_OUTFITS, &iar_data[old] );

   /* Store the currently-saved positions for the new tab. */
   pos    = iar_data[tab].pos;
   offset = iar_data[tab].offset;

   /* Resetting the input will cause the outfit list to be regenerated. */
   if (widget_exists(wid, EQUIPMENT_FILTER))
      window_setInput(wid, EQUIPMENT_FILTER, NULL);
   else
      equipment_regenLists(wid, 1, 0);

   /* Set positions for the new tab. This is necessary because the stored
    * position for the new tab may have exceeded the size of the old tab,
    * resulting in it being clipped. */
   toolkit_setImageLayeredArrayPos(    wid, EQUIPMENT_OUTFITS, pos );
   toolkit_setImageLayeredArrayOffset( wid, EQUIPMENT_OUTFITS, offset );

   /* Focus the outfit image array. */
   window_setFocus( wid, EQUIPMENT_OUTFITS );
}

/**
 * @brief Changes or transport depending on what is active.
 *    @param wid Window player is attempting to change ships in.
 *    @param str Unused.
 */
static void equipment_transChangeShip( unsigned int wid, char* str )
{
   (void) str;
   char *shipname, *loc;

   shipname = toolkit_getImageArray( wid, EQUIPMENT_SHIPS );
   loc   = player_getLoc( shipname );

   if (strcmp(land_planet->name,loc)) /* ship not here */
      equipment_transportShip( wid );
   else
      equipment_changeShip( wid );

   /* update the window to reflect the change */
   equipment_updateShips( wid, NULL );
}
/**
 * @brief Player attempts to change ship.
 *    @param wid Window player is attempting to change ships in.
 */
static void equipment_changeShip( unsigned int wid )
{
   char *shipname, *filtertext;
   int i;

   shipname = toolkit_getImageArray( wid, EQUIPMENT_SHIPS );

   if (land_errDialogue( shipname, "swapEquipment" ))
      return;

   /* Store active tab, filter text, and positions for the outfits. */
   i = window_tabWinGetActive( wid, EQUIPMENT_OUTFIT_TAB );
   toolkit_saveImageLayeredArrayData( wid, EQUIPMENT_OUTFITS, &iar_data[i] );
   if (widget_exists(wid, EQUIPMENT_FILTER))
      filtertext = window_getInput( equipment_wid, EQUIPMENT_FILTER );

   /* Swap ship. */
   player_swapShip( shipname );
   pilot_healLanded( player.p );

   /* What happens here is the gui gets recreated when the player swaps ship.
    * This causes all the windows to be destroyed and the 'wid' we have here
    * becomes invalid. However, since we store it in a global variable we can
    * recover it and use it instead. */
   wid = equipment_wid;

   /* Restore outfits image array properties. */
   window_tabWinSetActive( wid, EQUIPMENT_OUTFIT_TAB, i );
   toolkit_setImageLayeredArrayPos(    wid, EQUIPMENT_OUTFITS, iar_data[i].pos );
   toolkit_setImageLayeredArrayOffset( wid, EQUIPMENT_OUTFITS, iar_data[i].offset );
   if (widget_exists(wid, EQUIPMENT_FILTER))
      window_setInput(wid, EQUIPMENT_FILTER, filtertext);

   /* Regenerate ship widget. */
   equipment_regenLists( wid, 0, 1 );

   /* Focus new ship. */
   toolkit_setImageArrayPos(    wid, EQUIPMENT_SHIPS, 0 );
   toolkit_setImageArrayOffset( wid, EQUIPMENT_SHIPS, 0. );
}
/**
 * @brief Player attempts to transport their ship to the planet they're at.
 *
 *    @param wid Window player is trying to transport their ship from.
 */
static void equipment_transportShip( unsigned int wid )
{
   credits_t price;
   char *shipname, buf[ECON_CRED_STRLEN];

   shipname = toolkit_getImageArray( wid, EQUIPMENT_SHIPS );

   price = equipment_transportPrice( shipname );
   if (price==0) { /* already here */
      dialogue_alert( "Your ship '%s' is already here.", shipname );
      return;
   }
   else if (!player_hasCredits( price )) { /* not enough money. */
      credits2str( buf, price-player.p->credits, 2 );
      dialogue_alert( "You need %s more credits to transport '%s' here.",
            buf, shipname );
      return;
   }

   /* Obligatory annoying dialogue. */
   credits2str( buf, price, 2 );
   if (dialogue_YesNo("Are you sure?", /* confirm */
            "Do you really want to spend %s transporting your ship %s here?",
            buf, shipname )==0)
      return;

   /* success */
   player_modCredits( -price );
   player_setLoc( shipname, land_planet->name );
}



/**
 * @brief Unequips the player's ship.
 *
 *    @param wid Window id.
 *    @param name of widget.
 */
static void equipment_unequipShip( unsigned int wid, char* str )
{
   (void) str;
   int ret;
   int i;
   Pilot *ship;
   Outfit *o, *ammo;

   ship = eq_wgt.selected;

   /*
    * Unequipping is disallowed under two conditions. Firstly, the ship may not
    * be unequipped when it has fighters deployed in space. Secondly, it cannot
    * unequip if it's carrying more cargo than the ship normally fits, i.e.
    * by equipping cargo pods.
    */
   for (i=0; i<ship->noutfits; i++) {
      /* Must have outfit. */
      if (ship->outfits[i]->outfit == NULL)
         continue;
      /* Must be fighter bay. */
      if (!outfit_isFighterBay( ship->outfits[i]->outfit))
         continue;
      /* Must not have deployed fighters. */
      if (ship->outfits[i]->u.ammo.deployed > 0) {
         dialogue_alert( "You can not unequip your ship while you have deployed fighters!" );
         return;
      }
   }
   if (pilot_cargoUsed( ship ) > ship->ship->cap_cargo) {
      dialogue_alert( "You can not unequip your ship when you have more cargo than it can hold without modifications!" );
      return;
   }

   if (dialogue_YesNo("Unequip Ship", /* confirm */
         "Are you sure you want to remove all equipment from your ship?")==0)
      return;

   /* Remove all outfits. */
   for (i=0; i<ship->noutfits; i++) {
      o = ship->outfits[i]->outfit;

      /* Skip null outfits. */
      if (o==NULL)
         continue;

      /* Remove ammo first. */
      ammo = outfit_ammo(o);
      if (ammo != NULL) {
         ret = pilot_rmAmmo( ship, ship->outfits[i], outfit_amount(o) );
         player_addOutfit( ammo, ret );
      }

      /* Remove rest. */
      ret = pilot_rmOutfitRaw( ship, ship->outfits[i] );
      if (ret==0)
         player_addOutfit( o, 1 );
   }

   /* Recalculate stats. */
   pilot_calcStats( ship );
   pilot_healLanded( ship );

   /* Regenerate list. */
   equipment_regenLists( wid, 1, 1 );

   /* Regenerate outfits. */
   outfits_updateEquipmentOutfits();

   /* Update weapon sets if needed. */
   if (ship->autoweap)
      pilot_weaponAuto( ship );
   pilot_weaponSane( ship );

   /* Notify GUI of modification. */
   gui_setShip();
}
/**
 * @brief Player tries to sell a ship.
 *    @param wid Window player is selling ships in.
 *    @param str Unused.
 */
static void equipment_sellShip( unsigned int wid, char* str )
{
   (void)str;
   char *shipname, buf[ECON_CRED_STRLEN], *name;
   credits_t price;

   shipname = toolkit_getImageArray( wid, EQUIPMENT_SHIPS );

   if (land_errDialogue( shipname, "sell" ))
      return;

   /* Calculate price. */
   price = player_shipPrice(shipname);
   credits2str( buf, price, 2 );

   /* Check if player really wants to sell. */
   if (!dialogue_YesNo( "Sell Ship",
         "Are you sure you want to sell your ship %s for %s credits?", shipname, buf))
      return;

   /* Sold. */
   name = strdup(shipname);
   player_modCredits( price );
   player_rmShip( shipname );

   /* Destroy widget - must be before widget. */
   equipment_regenLists( wid, 0, 1 );

   /* Display widget. */
   dialogue_msg( "Ship Sold", "You have sold your ship %s for %s credits.", name, buf );
   free(name);
}


/**
 * @brief Renames the selected ship.
 *
 *    @param The ship to rename.
 */
static void equipment_renameShip( unsigned int wid, char *str )
{
   (void)str;
   Pilot *ship;
   char *shipname, *newname;

   shipname = toolkit_getImageArray( wid, EQUIPMENT_SHIPS );
   ship = player_getShip(shipname);
   newname = dialogue_input( "Ship Name", 3, 20,
         "Please enter a new name for your %s:", ship->ship->name );

   /* Player cancelled the dialogue. */
   if (newname == NULL)
      return;

   /* Must not have same name. */
   if (player_hasShip(newname)) {
      dialogue_msg( "Name Collision",
            "Please do not give the ship the same name as another of your ships.");
      return;
   }

   if (ship->name != NULL)
      free (ship->name);

   ship->name = strdup( newname );

   /* Destroy widget - must be before widget. */
   equipment_regenLists( wid, 0, 1 );
}


/**
 * @brief Gets the ship's transport price.
 *    @param shipname Name of the ship to get the transport price.
 *    @return The price to transport the ship to the current planet.
 */
static credits_t equipment_transportPrice( char* shipname )
{
   char *loc;
   Pilot* ship;
   credits_t price;
   StarSystem **s;
   int jumps;

   ship = player_getShip(shipname);
   loc = player_getLoc(shipname);
   if ( strcmp( loc,land_planet->name ) == 0 ) /* already here */
      return 0;

   /* Here we also use hidden jump points, which may not be the best idea but ensures
    * that things can be reached. */
   if ( planet_getSystem( loc ) == NULL )
      /* Planet doesn't exist; assume a huge number of jumps. */
      jumps = 200;
   else if ( strcmp( planet_getSystem( loc ), cur_system->name ) != 0 )
   {
      s = map_getJumpPath( &jumps, cur_system->name, planet_getSystem( loc ), 1,
         1, NULL );
      if ( s == NULL )
         jumps = 50; /* Just consider a large number. */
      free(s);
   }
   else /* Ship is in the same system and no jump path can be generated */
      jumps = 0;
   /* Modest base price scales fairly rapidly with distance. */
   price = (credits_t)(ceil(sqrt(ship->ship->mass) * pow(jumps + 1, .6) * 10.) * 100.);

   return price;
}


/**
 * @brief Cleans up after the equipment stuff.
 */
void equipment_cleanup (void)
{
   /* Destroy the VBO. */
   if (equipment_vbo != NULL)
      gl_vboDestroy( equipment_vbo );

   equipment_vbo = NULL;

   /* Free stored positions. */
   if (iar_data != NULL) {
      free(iar_data);
      iar_data = NULL;
   }

   /* Free icons. */
   if (equip_ico_yes != NULL)
      gl_freeTexture(equip_ico_yes);
   equip_ico_yes = NULL;
   if (equip_ico_no != NULL)
      gl_freeTexture(equip_ico_no);
   equip_ico_no = NULL;
}

