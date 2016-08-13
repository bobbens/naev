/*
 * See Licensing and Copyright notice in naev.h
 */

/**
 * @file imagelayeredarray.c
 *
 * @brief Layered Image array widget; each image is assembled from several layers. Otherwise identical to ImageArray.
 */


#include "tk/toolkit_priv.h"

#include <stdlib.h>
#include "nstring.h"

#include "opengl.h"


/* Render. */
static void iar_render( Widget* iar, double bx, double by );
static void iar_renderOverlay( Widget* iar, double bx, double by );
/* Key. */
static int iar_key( Widget* iar, SDLKey key, SDLMod mod );
/* Mouse. */
static int iar_mclick( Widget* iar, int button, int x, int y );
#if SDL_VERSION_ATLEAST(2,0,0)
static int iar_mwheel( Widget* lst, SDL_MouseWheelEvent event );
#endif /* SDL_VERSION_ATLEAST(2,0,0) */
static int iar_mmove( Widget* iar, int x, int y, int rx, int ry );
/* Focus. */
static int iar_focusImage( Widget* iar, double bx, double by );
static void iar_focus( Widget* iar, double bx, double by );
static void iar_scroll( Widget* iar, int direction );
static void iar_centerSelected( Widget *iar );
/* Misc. */
static void iar_setAltTextPos( Widget *iar, double bx, double by );
static Widget *iar_getWidget( const unsigned int wid, const char *name );
static char* toolkit_getNameById( Widget *wgt, int elem );
/* Clean up. */
static void iar_cleanup( Widget* iar );


/**
 * @brief Adds an Image Layered Array widget.
 *
 * Position origin is 0,0 at bottom left. If you use negative X or Y
 *  positions. They actually count from the opposite side in.
 *
 *    @param wid Window to add to.
 *    @param x X position.
 *    @param y Y position.
 *    @param w Width.
 *    @param h Height.
 *    @param name Internal widget name.
 *    @param iw Image width to use.
 *    @param ih Image height to use.
 *    @param layers layers array to use (not freed).
 *    @param nlayers number of layers for each element
 *    @param caption Caption array to use (freed).
 *    @param nelem Elements in layers, nlayers and caption.
 *    @param call Callback when modified.
 */
void window_addImageLayeredArray( const unsigned int wid,
                           const int x, const int y, /* position */
                           const int w, const int h, /* size */
                           char* name, const int iw, const int ih,
                           glTexture*** layers,int* nlayers, char** caption, int nelem,
                           void (*call) (unsigned int wdw, char* wgtname),
                           void (*rmcall) (unsigned int wdw, char* wgtname) )
{

   int i;

   Window *wdw = window_wget(wid);
   Widget *wgt = window_newWidget(wdw, name);
   if (wgt == NULL)
      return;

   /* generic */
   wgt->type   = WIDGET_IMAGELAYEREDARRAY;

   /* position/size */
   wgt->w = (double) w;
   wgt->h = (double) h;
   toolkit_setPos( wdw, wgt, x, y );

   glTexture ***p = malloc(nelem * sizeof(glTexture **));

   for (i=0;i<nelem;i++) {
	   p[i]=malloc(nlayers[i] * sizeof(glTexture *));
	   memcpy(p[i], layers[i], nlayers[i] * sizeof(glTexture **));
   }

   /* specific */
   wgt->render             = iar_render;
   wgt->renderOverlay      = iar_renderOverlay;
   wgt->cleanup            = iar_cleanup;
   wgt_setFlag(wgt, WGT_FLAG_CANFOCUS);
   wgt->keyevent           = iar_key;
   wgt->mclickevent        = iar_mclick;
#if SDL_VERSION_ATLEAST(2,0,0)
   wgt->mwheelevent        = iar_mwheel;
#endif /* SDL_VERSION_ATLEAST(2,0,0) */
   wgt->mmoveevent         = iar_mmove;
   wgt_setFlag(wgt, WGT_FLAG_ALWAYSMMOVE);
   wgt->dat.iarl.layers     = p;
   wgt->dat.iarl.nlayers    = nlayers;
   wgt->dat.iarl.captions   = caption;
   wgt->dat.iarl.nelements  = nelem;
   wgt->dat.iarl.selected   = 0;
   wgt->dat.iarl.pos        = 0;
   wgt->dat.iarl.alt        = -1;
   wgt->dat.iarl.altx       = -1;
   wgt->dat.iarl.alty       = -1;
   wgt->dat.iarl.iw         = iw;
   wgt->dat.iarl.ih         = ih;
   wgt->dat.iarl.fptr       = call;
   wgt->dat.iarl.rmptr      = rmcall;
   wgt->dat.iarl.xelem      = floor((w - 10.) / (double)(wgt->dat.iarl.iw+10));
   wgt->dat.iarl.yelem      = (wgt->dat.iarl.xelem == 0) ? 0 :
         (int)wgt->dat.iarl.nelements / wgt->dat.iarl.xelem + 1;

   if (wdw->focus == -1) /* initialize the focus */
      toolkit_nextFocus( wdw );
}


/**
 * @brief Gets image array effective dimensions.
 */
static void iar_getDim( Widget* iar, double *w, double *h )
{
   if (w != NULL)
      *w = iar->dat.iarl.iw + 5.*2.;
   if (h != NULL)
      *h = iar->dat.iarl.ih + 5.*2. + 2. + gl_smallFont.h;
}


/**
 * @brief Renders an image array.
 *
 *    @param iar Image array widget to render.
 *    @param bx Base X position.
 *    @param by Base Y position.
 */
static void iar_render( Widget* iar, double bx, double by )
{
   int i,j, k, pos;
   double x,y, w,h, xcurs,ycurs;
   double scroll_pos;
   int xelem, yelem;
   double xspace;
   const glColour *c, *dc, *lc;
   glColour tc, fontcolour;
   int is_selected;
   int tw;
   double d;

   /*
    * Calculations.
    */
   /* position */
   x = bx + iar->x;
   y = by + iar->y;

   /* element dimensions */
   iar_getDim( iar, &w, &h );

   /* number of elements */
   xelem = iar->dat.iarl.xelem;
   yelem = iar->dat.iarl.yelem;
   xspace = (double)(((int)iar->w - 10) % (int)w) / (double)(xelem + 1);

   /* background */
   toolkit_drawRect( x, y, iar->w, iar->h, &cBlack, NULL );

   /*
    * Scrollbar.
    */
   d          = h * (yelem - (int)(iar->h / h));
   if (fabs(d) < 1e-05)
      scroll_pos = 0.;
   else
      scroll_pos = iar->dat.iarl.pos / d;
   toolkit_drawScrollbar( x + iar->w - 10., y, 10., iar->h, scroll_pos );

   /*
    * Main drawing loop.
    */
   gl_clipRect( x, y, iar->w, iar->h );
   ycurs = y + iar->h - h + iar->dat.iarl.pos;
   for (j=0; j<yelem; j++) {
      xcurs = x + xspace;

      /*  Skip rows that are wholly outside of the viewport. */
      if ((ycurs > y + iar->h) || (ycurs + h < y)) {
         ycurs -= h;
         continue;
      }

      for (i=0; i<xelem; i++) {

         /* Get position. */
         pos = j*xelem + i;

         /* Out of elements. */
         if ((pos) >= iar->dat.iarl.nelements)
            break;

         is_selected = (iar->dat.iarl.selected == pos) ? 1 : 0;

         fontcolour = cWhite;
         /* Draw background. */
         if (is_selected)
            toolkit_drawRect( xcurs + 2.,
                  ycurs + 2.,
                  w - 5., h - 5., &cDConsole, NULL );
         else if (iar->dat.iarl.background != NULL) {
            toolkit_drawRect( xcurs + 2.,
                  ycurs + 2.,
                  w - 5., h - 5., &iar->dat.iarl.background[pos], NULL );

            tc = iar->dat.iarl.background[pos];

            if (((tc.r + tc.g + tc.b) / 3) > 0.5)
               fontcolour = cBlack;
         }

         /* layers */
         for (k=0;k<iar->dat.iarl.nlayers[pos];k++) {
			 if (iar->dat.iarl.layers[pos][k] != NULL)
				gl_blitScale( iar->dat.iarl.layers[pos][k],
					  xcurs + 5., ycurs + gl_smallFont.h + 7.,
					  iar->dat.iarl.iw, iar->dat.iarl.ih, NULL );
         }

         /* caption */
         if (iar->dat.iarl.captions[pos] != NULL)
            gl_printMidRaw( &gl_smallFont, iar->dat.iarl.iw, xcurs + 5., ycurs + 5.,
                     (is_selected) ? &cBlack : &fontcolour,
                     iar->dat.iarl.captions[pos] );

         /* quantity. */
         if (iar->dat.iarl.quantity != NULL) {
            if (iar->dat.iarl.quantity[pos] != NULL) {
               /* Rectangle to highlight better. */
               tw = gl_printWidthRaw( &gl_smallFont,
                     iar->dat.iarl.quantity[pos] );

               if (is_selected)
                  tc = cDConsole;
               else if (iar->dat.iarl.background != NULL)
                  tc = iar->dat.iarl.background[pos];
               else
                  tc = cBlack;

               tc.a = 0.75;
               toolkit_drawRect( xcurs + 2.,
                     ycurs + 5. + iar->dat.iarl.ih,
                     tw + 4., gl_smallFont.h + 4., &tc, NULL );
               /* Quantity number. */
               gl_printMaxRaw( &gl_smallFont, iar->dat.iarl.iw,
                     xcurs + 5., ycurs + iar->dat.iarl.ih + 7.,
                     &fontcolour, iar->dat.iarl.quantity[pos] );
            }
         }

         /* Slot type. */
         if (iar->dat.iarl.slottype != NULL) {
            if (iar->dat.iarl.slottype[pos] != NULL) {
               /* Rectangle to highlight better. Width is a hack due to lack of monospace font. */
               tw = gl_printWidthRaw( &gl_smallFont, "M" );

               if (is_selected)
                  tc = cDConsole;
               else if (iar->dat.iarl.background != NULL)
                  tc = iar->dat.iarl.background[pos];
               else
                  tc = cBlack;

               tc.a = 0.75;
               toolkit_drawRect( xcurs + iar->dat.iarl.iw - 6.,
                     ycurs + 5. + iar->dat.iarl.ih,
                     tw + 2., gl_smallFont.h + 4., &tc, NULL );
               /* Slot size letter. */
               gl_printMaxRaw( &gl_smallFont, iar->dat.iarl.iw,
                     xcurs + iar->dat.iarl.iw - 4., ycurs + iar->dat.iarl.ih + 7.,
                     &fontcolour, iar->dat.iarl.slottype[pos] );
            }
         }

         /* outline */
         if (is_selected) {
            lc = &cWhite;
            c = &cGrey80;
            dc = &cGrey60;
         }
         else {
            lc = toolkit_colLight;
            c = toolkit_col;
            dc = toolkit_colDark;
         }
         toolkit_drawOutline( xcurs + 2.,
               ycurs + 2.,
               w - 4., h - 4., 1., lc, c );
         toolkit_drawOutline( xcurs + 2.,
               ycurs + 2.,
               w - 4., h - 4., 2., dc, NULL );
         xcurs += w + xspace;
      }
      ycurs -= h;
   }
   gl_unclipRect();

   /*
    * Final outline.
    */
   toolkit_drawOutline( x+1, y+1, iar->w-2, iar->h-2, 1., toolkit_colLight, toolkit_col );
   toolkit_drawOutline( x+1, y+1, iar->w-2, iar->h-2, 2., toolkit_colDark, NULL );
}


/**
 * @brief Renders the overlay.
 */
static void iar_renderOverlay( Widget* iar, double bx, double by )
{
   double x, y;

   /*
    * Draw Alt text if applicable.
    */
   if ((iar->dat.iarl.alts != NULL) && (iar->dat.iarl.alt >= 0) &&
         (iar->dat.iarl.altx != -1) && (iar->dat.iarl.alty != -1) &&
         (iar->dat.iarl.alts[iar->dat.iarl.alt] != NULL)) {

      /* Calculate position. */
      x = bx + iar->x + iar->dat.iarl.altx;
      y = by + iar->y + iar->dat.iarl.alty;

      /* Draw alt text. */
      toolkit_drawAltText( x, y, iar->dat.iarl.alts[iar->dat.iarl.alt] );
   }
}


/**
 * @brief Handles input for an image array widget.
 *
 *    @param iar Image array widget to handle event.
 *    @param key Key being handled.
 *    @param mod Mods when key is being pressed.
 *    @return 1 if the event was used, 0 if it wasn't.
 */
static int iar_key( Widget* iar, SDLKey key, SDLMod mod )
{
   (void) mod;

   switch (key) {
      case SDLK_UP:
         iar->dat.iarl.selected -= iar->dat.iarl.xelem;
         break;
      case SDLK_DOWN:
         iar->dat.iarl.selected += iar->dat.iarl.xelem;
         break;
      case SDLK_RIGHT:
         iar->dat.iarl.selected += 1;
         break;
      case SDLK_LEFT:
         iar->dat.iarl.selected -= 1;
         break;

      default:
         return 0;
   }

   /* Check boundaries. */
   iar->dat.iarl.selected = CLAMP( 0, iar->dat.iarl.nelements-1, iar->dat.iarl.selected);

   /* Run function pointer if needed. */
   if (iar->dat.iarl.fptr)
      iar->dat.iarl.fptr( iar->wdw, iar->name);

   iar_centerSelected( iar );
   return 1;
}


/**
 * @brief Centers on the selection if needed.
 *
 *    @param iar Widget to center on selection.
 */
static void iar_centerSelected( Widget *iar )
{
   int y;
   double h;
   double hmax;
   double ypos;

   /* Get dimensions. */
   iar_getDim( iar, NULL, &h );

   /* Ignore fancy stuff if smaller than height. */
   if (h * iar->dat.iarl.yelem < iar->h)
      return;

   /* Move if needed. */
   hmax = h * (iar->dat.iarl.yelem - (int)(iar->h / h));
   y = iar->dat.iarl.selected / iar->dat.iarl.xelem;
   ypos = y * h;
   /* Below. */
   if (ypos < iar->dat.iarl.pos)
      iar->dat.iarl.pos = ypos;
   /* Above. */
   if (ypos > iar->dat.iarl.pos + iar->h - h)
      iar->dat.iarl.pos = ypos - h*floor(iar->h/h) + 10.;
   iar->dat.iarl.pos = CLAMP( 0., hmax, iar->dat.iarl.pos );

   iar_setAltTextPos( iar, iar->dat.iarl.altx, iar->dat.iarl.alty );
}


/**
 * @brief Image array widget mouse click handler.
 *
 *    @param iar Widget receiving the event.
 *    @param mclick The mouse click event.
 *    @return 1 if event is used.
 */
static int iar_mclick( Widget* iar, int button, int x, int y )
{
   /* Handle different mouse clicks. */
   switch (button) {
      case SDL_BUTTON_LEFT:
         iar_focus( iar, x, y );
         return 1;
#if !SDL_VERSION_ATLEAST(2,0,0)
      case SDL_BUTTON_WHEELUP:
         iar_scroll( iar, +1 );
         return 1;
      case SDL_BUTTON_WHEELDOWN:
         iar_scroll( iar, -1 );
         return 1;
#endif /* !SDL_VERSION_ATLEAST(2,0,0) */
      case SDL_BUTTON_RIGHT:
         iar_focus( iar, x, y );
         if (iar->dat.iarl.rmptr != NULL)
            iar->dat.iarl.rmptr( iar->wdw, iar->name );

         iar_setAltTextPos( iar, x, y );

         return 1;

      default:
         break;
   }
   return 0;
}


#if SDL_VERSION_ATLEAST(2,0,0)
/**
 * @brief Handler for mouse wheel events for an image array.
 *
 *    @param iar The widget handling the mouse wheel event.
 *    @param event The event the widget should handle.
 *    @return 1 if the widget uses the event.
 */
static int iar_mwheel( Widget* iar, SDL_MouseWheelEvent event )
{
   if (event.y > 0)
      iar_scroll( iar, +1 );
   else
      iar_scroll( iar, -1 );

   return 1;
}
#endif /* SDL_VERSION_ATLEAST(2,0,0) */


/**
 * @brief Handles mouse movement for an image array.
 *
 *    @param iar Widget handling the mouse motion.
 *    @param mmove Mouse motion event to handle.
 *    @return 1 if the event is used.
 */
static int iar_mmove( Widget* iar, int x, int y, int rx, int ry )
{
   (void) rx;
   (void) ry;
   double w,h;
   int yelem;
   double hmax;

   if (iar->status == WIDGET_STATUS_SCROLLING) {

      y = CLAMP( 15, iar->h - 15., iar->h - y );

      /* element dimensions */
      iar_getDim( iar, &w, &h );

      /* number of elements */
      yelem = iar->dat.iarl.yelem;

      hmax = h * (yelem - (int)(iar->h / h));
      iar->dat.iarl.pos = (y - 15.) * hmax / (iar->h - 30.);

      /* Does boundary checks. */
      iar_scroll( iar, 0 );

      return 1;
   }
   else {
      if ((x < 0) || (x >= iar->w) || (y < 0) || (y >= iar->h))
         iar->dat.iarl.alt  = -1;
      else
         iar_setAltTextPos( iar, x, y );
   }

   return 0;
}


/**
 * @brief Clean up function for the image array widget.
 *
 *    @param iar Image array widget to clean up.
 */
static void iar_cleanup( Widget* iar )
{
   int i;

   if (iar->dat.iarl.nelements > 0) { /* Free each text individually */
      for (i=0; i<iar->dat.iarl.nelements; i++) {
         if (iar->dat.iarl.captions[i])
            free(iar->dat.iarl.captions[i]);
         if (iar->dat.iarl.alts && iar->dat.iarl.alts[i])
            free(iar->dat.iarl.alts[i]);
         if (iar->dat.iarl.quantity && iar->dat.iarl.quantity[i])
            free(iar->dat.iarl.quantity[i]);
	    if (iar->dat.iarl.layers && iar->dat.iarl.layers[i])
			free( iar->dat.iarl.layers[i] );
      }
   }

   /* Clean up slottypes. */
   if (iar->dat.iarl.slottype != NULL) {
      for (i=0; i<iar->dat.iarl.nelements; i++) {
         if (iar->dat.iarl.slottype[i] != NULL)
            free( iar->dat.iarl.slottype[i] );
      }
      free(iar->dat.iarl.slottype);
   }


   /* Free the arrays */
   if (iar->dat.iarl.captions != NULL)
      free( iar->dat.iarl.captions );

   if (iar->dat.iarl.layers != NULL)
      free( iar->dat.iarl.layers );

   if (iar->dat.iarl.alts != NULL)
      free(iar->dat.iarl.alts);
   if (iar->dat.iarl.quantity != NULL)
      free(iar->dat.iarl.quantity);
   if (iar->dat.iarl.background != NULL)
      free(iar->dat.iarl.background);
}


/**
 * @brief Tries to scroll a widget up/down by direction.
 *
 *    @param wgt Widget to scroll.
 *    @param direction Direction to scroll. Positive is up, negative
 *           is down and absolute value is number of elements to scroll.
 */
static void iar_scroll( Widget* iar, int direction )
{
   double w,h;
   int yelem;
   double hmax;

   if (iar == NULL)
      return;

   /* element dimensions */
   iar_getDim( iar, &w, &h );

   /* number of elements */
   yelem = iar->dat.iarl.yelem;

   /* maximum */
   hmax = h * (yelem - (int)(iar->h / h));
   if (hmax < 0.)
      hmax = 0.;

   /* move */
   iar->dat.iarl.pos -= direction * h;

   /* Boundary check. */
   iar->dat.iarl.pos = CLAMP( 0., hmax, iar->dat.iarl.pos );
   if (iar->dat.iarl.fptr)
      iar->dat.iarl.fptr( iar->wdw, iar->name );

   iar_setAltTextPos( iar, iar->dat.iarl.altx, iar->dat.iarl.alty );
}


/**
 * @brief See what widget is being focused.
 */
static int iar_focusImage( Widget* iar, double bx, double by )
{
   int x, y;
   double w, h;
   int xelem;
   double xspace;

   /* element dimensions */
   iar_getDim( iar, &w, &h );

   /* number of elements */
   xelem = iar->dat.iarl.xelem;
   xspace = (double)(((int)iar->w - 10) % (int)w) / (double)(xelem + 1);

   x = bx / (xspace + w);
   y = (iar->h - by + iar->dat.iarl.pos) / h;

   /* Reject anything too close to the scroll bar or exceeding nelements. */
   if (y * xelem + x >= iar->dat.iarl.nelements || bx >= iar->w - 10.)
      return -1;

   /* Verify that the mouse is on an icon. */
   if ((bx < (x+1) * xspace + x * w) || (bx > (x+1) * (xspace + w) - 4.) ||
         (by > iar->h + iar->dat.iarl.pos - y * h - 4.))
      return -1;

   return y * xelem + x;
}



/**
 * @brief Mouse event focus on image array.
 *
 *    @param iar Image Array widget.
 *    @param bx X position click.
 *    @param by Y position click.
 */
static void iar_focus( Widget* iar, double bx, double by )
{
   double y, h;
   double scroll_pos, hmax;
   int yelem;
   int selected;

   /* element dimensions */
   iar_getDim( iar, NULL, &h );

   /* number of elements */
   yelem = iar->dat.iarl.yelem;

   /* Test for item click. */
   selected = iar_focusImage( iar, bx, by );
   if (selected >= 0) {
      iar->dat.iarl.selected = selected;
      if (iar->dat.iarl.fptr != NULL)
         iar->dat.iarl.fptr( iar->wdw, iar->name );
   }
   /* Scrollbar click. */
   else if (bx > iar->w - 10.) {
      /* Get bar position (center). */
      hmax = h * (yelem - (int)(iar->h / h));
      if (fabs(hmax) < 1e-05)
         scroll_pos = 0.;
      else
         scroll_pos = iar->dat.iarl.pos / hmax;
      y = iar->h - (iar->h - 30.) * scroll_pos - 15.;

      /* Click below the bar. */
      if (by < y-15.)
         iar_scroll( iar, -2 );
      /* Click above the bar. */
      else if (by > y+15.)
         iar_scroll( iar, +2 );
      /* Click on the bar. */
      else
         iar->status = WIDGET_STATUS_SCROLLING;
   }
}


/**
 * @brief Chooses correct alt text for the given coordinates
 *
 */
static void iar_setAltTextPos( Widget *iar, double bx, double by )
{
   iar->dat.iarl.alt  = iar_focusImage( iar, bx, by );
   iar->dat.iarl.altx = bx;
   iar->dat.iarl.alty = by;
}


/**
 * @brief Gets an image array.
 */
static Widget *iar_getWidget( const unsigned int wid, const char *name )
{
   Widget *wgt = window_getwgt(wid,name);

   /* Must be found in stack. */
   if (wgt == NULL) {
      WARN("Widget '%s' not found", name);
      return NULL;
   }

   /* Must be an image array. */
   if (wgt->type != WIDGET_IMAGELAYEREDARRAY) {
      WARN("Widget '%s' is not an image array.", name);
      return NULL;
   }

   return wgt;
}


/**
 * @brief Gets the name of the element.
 *
 *    @param wid Window where image array is.
 *    @param name Name of the image array.
 *    @param elem The element in the image array.
 *    @return The name of the selected object.
 */
static char* toolkit_getNameById( Widget *wgt, int elem )
{
   if (wgt == NULL)
      return NULL;

   /* Nothing selected. */
   if (elem == -1)
      return NULL;

   return wgt->dat.iarl.captions[ elem ];
}


/**
 * @brief Gets what is selected currently in an Image Array.
 *
 *    @param wid Window where image array is.
 *    @param name Name of the image array.
 *    @return The name of the selected object.
 */
char* toolkit_getImageLayeredArray( const unsigned int wid, const char* name )
{
   Widget *wgt = iar_getWidget( wid, name );
   if (wgt == NULL)
      return NULL;

   return toolkit_getNameById( wgt, wgt->dat.iarl.selected );
}


/**
 * @brief Sets an image array based on value.
 */
int toolkit_setImageLayeredArray( const unsigned int wid, const char* name, char* elem )
{
   int i;
   Widget *wgt = iar_getWidget( wid, name );
   if (wgt == NULL)
      return -1;

   /* Case NULL. */
   if (elem == NULL) {
      wgt->dat.iarl.selected = -1;
      return 0;
   }

   /* Try to find the element. */
   for (i=0; i<wgt->dat.iarl.nelements; i++) {
      if (strcmp(elem,wgt->dat.iarl.captions[i])==0) {
         wgt->dat.iarl.selected = i;
         return 0;
      }
   }

   /* Element not found. */
   return -1;
}


/**
 * @brief Gets what is selected currently in an Image Array.
 *
 *    @param wid Window where image array is.
 *    @param name Name of the image array.
 *    @return The position of selected object.
 */
int toolkit_getImageLayeredArrayPos( const unsigned int wid, const char* name )
{
   Widget *wgt = iar_getWidget( wid, name );
   if (wgt == NULL)
      return -1;

   return wgt->dat.iarl.selected;
}


/**
 * @brief Gets the Image Array offset.
 */
double toolkit_getImageLayeredArrayOffset( const unsigned int wid, const char* name )
{
   Widget *wgt = iar_getWidget( wid, name );
   if (wgt == NULL)
      return -1.;

   return wgt->dat.iarl.pos;
}


/**
 * @brief Sets the Image Array offset.
 */
int toolkit_setImageLayeredArrayOffset( const unsigned int wid, const char* name, double off )
{
   double h;
   double hmax;

   Widget *wgt = iar_getWidget( wid, name );
   if (wgt == NULL)
      return -1;

   /* Get dimensions. */
   iar_getDim( wgt, NULL, &h );

   /* Ignore fancy stuff if smaller than height. */
   if (h * wgt->dat.iarl.yelem < wgt->h) {
      wgt->dat.iarl.pos = 0.;
      return 0;
   }

   /* Move if needed. */
   hmax = h * (wgt->dat.iarl.yelem - (int)(wgt->h / h));
   wgt->dat.iarl.pos = CLAMP( 0., hmax, off );

   iar_setAltTextPos( wgt, wgt->dat.iarl.altx, wgt->dat.iarl.alty );

   return 0;
}


/**
 * @brief Sets the active element in the Image Array.
 *
 *    @param wid Window where image array is.
 *    @param name Name of the image array.
 *    @param pos Position to set to.
 *    @return 0 on success.
 */
int toolkit_setImageLayeredArrayPos( const unsigned int wid, const char* name, int pos )
{
   Widget *wgt = iar_getWidget( wid, name );
   if (wgt == NULL)
      return -1;

   /* Set position. */
   wgt->dat.iarl.selected = CLAMP( 0, wgt->dat.iarl.nelements-1, pos );

   /* Call callback - dangerous if called from within callback. */
   if (wgt->dat.iarl.fptr != NULL)
      wgt->dat.iarl.fptr( wgt->wdw, wgt->name );

   iar_centerSelected( wgt );

   return 0;
}


/**
 * @brief Sets the alt text for the images in the image array.
 *
 *    @param wid Window where image array is.
 *    @param name Name of the image array.
 *    @param alt Array of alt text the size of the images in the array.
 *    @return 0 on success.
 */
int toolkit_setImageLayeredArrayAlt( const unsigned int wid, const char* name, char **alt )
{
   int i;
   Widget *wgt = iar_getWidget( wid, name );
   if (wgt == NULL)
      return -1;

   /* Clean up. */
   if (wgt->dat.iarl.alts != NULL) {
      for (i=0; i<wgt->dat.iarl.nelements; i++)
         if (wgt->dat.iarl.alts[i] != NULL)
            free(wgt->dat.iarl.alts[i]);
      free(wgt->dat.iarl.alts);
   }

   /* Set. */
   wgt->dat.iarl.alts = alt;
   return 0;
}


/**
 * @brief Sets the quantity text for the images in the image array.
 *
 *    @param wid Window where image array is.
 *    @param name Name of the image array.
 *    @param quantity Array of quantities for the images in the array.
 *    @return 0 on success.
 */
int toolkit_setImageLayeredArrayQuantity( const unsigned int wid, const char* name,
      char **quantity )
{
   int i;
   Widget *wgt = iar_getWidget( wid, name );
   if (wgt == NULL)
      return -1;

   /* Clean up. */
   if (wgt->dat.iarl.quantity != NULL) {
      for (i=0; i<wgt->dat.iarl.nelements; i++)
         if (wgt->dat.iarl.quantity[i] != NULL)
            free(wgt->dat.iarl.quantity[i]);
      free(wgt->dat.iarl.quantity);
   }

   /* Set. */
   wgt->dat.iarl.quantity = quantity;
   return 0;
}


/**
 * @brief Sets the slot type text for the images in the image array.
 *
 *    @param wid Window where image array is.
 *    @param name Name of the image array.
 *    @param slottype Array of slot sizes for the images in the array.
 *    @return 0 on success.
 */
int toolkit_setImageLayeredArraySlotType( const unsigned int wid, const char* name,
      char **slottype )
{
   int i;
   Widget *wgt = iar_getWidget( wid, name );
   if (wgt == NULL)
      return -1;

   /* Clean up. */
   if (wgt->dat.iarl.slottype != NULL) {
      for (i=0; i<wgt->dat.iarl.nelements; i++)
         if (wgt->dat.iarl.slottype[i] != NULL)
            free(wgt->dat.iarl.slottype[i]);
      free(wgt->dat.iarl.slottype);
   }

   /* Set. */
   wgt->dat.iarl.slottype = slottype;
   return 0;
}


/**
 * @brief Sets the background colour for the images in the image array.
 *
 *    @param wid Window where image array is.
 *    @param name Name of the image array.
 *    @param bg Background colour for the image array.
 *    @return 0 on success.
 */
int toolkit_setImageLayeredArrayBackground( const unsigned int wid, const char* name,
      glColour *bg )
{
   Widget *wgt = iar_getWidget( wid, name );
   if (wgt == NULL)
      return -1;

   /* Free if already exists. */
   if (wgt->dat.iarl.background != NULL)
      free( wgt->dat.iarl.background );

   /* Set. */
   wgt->dat.iarl.background = bg;
   return 0;
}


/**
 * @brief Stores several image array attributes.
 *
 *    @param wid Window containing the image array.
 *    @param name Name of the image array widget.
 *    @param iar_data Pointer to an iar_data_t struct.
 *    @return 0 on success.
 */
int toolkit_saveImageLayeredArrayData( const unsigned int wid, const char *name,
      iar_data_t *iar_data )
{
   Widget *wgt = iar_getWidget( wid, name );
   if (wgt == NULL)
      return -1;

   iar_data->pos    = wgt->dat.iarl.selected;
   iar_data->offset = wgt->dat.iarl.pos;

   return 0;
}
