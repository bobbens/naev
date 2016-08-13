/*
 * See Licensing and Copyright notice in naev.h
 */

/**
 * @file imagelayered.c
 *
 * @brief ImageLayered widget. Identical to Image widget except that each image is made up of multiple layers.
 */


#include "tk/toolkit_priv.h"


static void img_render( Widget* img, double bx, double by );
/* Clean up. */
static void iar_cleanup( Widget* iar );


/**
 * @brief Adds an imagelayered widget to the window.
 *
 * Position origin is 0,0 at bottom left.  If you use negative X or Y
 *  positions.  They actually count from the opposite side in.
 *
 *    @param wid ID of the window to add the widget to.
 *    @param x X position within the window to use.
 *    @param y Y position within the window to use.
 *    @param name Name of the widget to use internally.
 *    @param layers to use.
 *    @param nlayers number of layers
 *    @param border Whether to use a border.
 */
void window_addImageLayered( const unsigned int wid,
                      const int x, const int y,
                      const int w, const int h,
                      char* name, glTexture** layers,int nlayers, int border )
{
   Window *wdw = window_wget(wid);
   Widget *wgt = window_newWidget(wdw, name);
   if (wgt == NULL)
      return;

   glTexture **p = malloc(nlayers * sizeof(glTexture *));
   memcpy(p, layers, nlayers * sizeof(glTexture *));


   /* generic */
   wgt->type   = WIDGET_IMAGE_LAYERED;

   /* specific */
   wgt->render          = img_render;
   wgt->cleanup         = iar_cleanup;
   wgt->dat.imgl.layers   = p;
   wgt->dat.imgl.nlayers = nlayers;
   wgt->dat.imgl.border  = border;
   wgt->dat.imgl.colour  = cWhite; /* normal colour */

   /* position/size */
   wgt->w = (w > 0) ? w : ((nlayers==0 || p[0]==NULL) ? 0 : p[0]->sw);
   wgt->h = (h > 0) ? h : ((nlayers==0 || p[0]==NULL) ? 0 : p[0]->sh);
   toolkit_setPos( wdw, wgt, x, y );
}


/**
 * @brief Renders a imagelayered widget.
 *
 *    @param img ImageLayered widget to render.
 *    @param bx Base X position.
 *    @param by Base Y position.
 */
static void img_render( Widget* img, double bx, double by )
{
   double x,y;
   double w,h;
   int i;

   /* Values. */
   x = bx + img->x;
   y = by + img->y;
   w = img->w;
   h = img->h;

   /*
    * layers
    */
   for (i=0;i<img->dat.imgl.nlayers;i++) {
	   if (img->dat.imgl.layers[i] != NULL) {
		   gl_blitScale( img->dat.imgl.layers[i], x, y,
			 w, h, &img->dat.imgl.colour );
	   }
   }

   if (img->dat.imgl.border) {
      /* inner outline (outwards) */
      toolkit_drawOutline( x, y+1, w-1,
         h-1, 1., toolkit_colLight, toolkit_col );
      /* outer outline */
      toolkit_drawOutline( x, y+1, w-1,
            h-1, 2., toolkit_colDark, NULL );
   }
}


/**
 * @brief Gets a layer from an image widget
 *
 *    @param wid ID of the window to get widget from.
 *    @param name Name of the widget.
 *    @param pos Pos of the layer
 */
glTexture* window_getImageLayer( const unsigned int wid, char* name, int pos )
{
   Widget *wgt;

   /* Get the widget. */
   wgt = window_getwgt(wid,name);
   if (wgt == NULL)
      return NULL;

   /* Check the type. */
   if (wgt->type != WIDGET_IMAGE_LAYERED) {
      WARN("Trying to get imagelayered from non-imagelayered widget '%s'.", name);
      return NULL;
   }

   if (pos>=wgt->dat.imgl.nlayers) {
	   WARN("Trying to get layer %d in a widget with only %d.", pos, wgt->dat.imgl.nlayers);
	   return NULL;
   }


   /* Get the value. */
   return (wgt) ? wgt->dat.imgl.layers[pos] : NULL;
}


/**
 * Modifies an existing imagelayered's layers.
 *
 *    @param wid ID of the window to get widget from.
 *    @param name Name of the widget to modify image of.
 *    @param layers new layers to use.
 *    @param nlayers number of layers
 *    @param w New width to set, 0 uses image, -1 doesn't change and >0 sets directly.
 *    @param h New height to set, 0 uses image, -1 doesn't change and >0 sets directly.
 */
void window_modifyImageLayered( const unsigned int wid,
      char* name, glTexture** layers,int nlayers, int w, int h )
{
   Widget *wgt;

   /* Get the widget. */
   wgt = window_getwgt(wid,name);
   if (wgt == NULL)
      return;

   /* Check the type. */
   if (wgt->type != WIDGET_IMAGE_LAYERED) {
      WARN("Not modifying image on non-image widget '%s'.", name);
      return;
   }

   /* free previous layer array, if any */
   if (wgt->dat.imgl.layers!=NULL) {
	   free(wgt->dat.imgl.layers);
   }

   glTexture **p = malloc(nlayers * sizeof(glTexture *));
   memcpy(p, layers, nlayers * sizeof(glTexture *));

   /* Set the layers. */
   wgt->dat.imgl.layers = p;
   wgt->dat.imgl.nlayers = nlayers;

   /* Adjust size. */
   if (w >= 0)
      wgt->w = (w > 0) ? w : ((nlayers==0 || layers[0]==NULL) ? 0 : layers[0]->sw);
   if (h >= 0)
      wgt->h = (h > 0) ? h : ((nlayers==0 || layers[0]==NULL) ? 0 : layers[0]->sh);
}


/**
 * Modifies an existing imagelayered's colour.
 *
 *    @param wid ID of the window to get widget from.
 *    @param name Name of the widget to modify image colour of.
 *    @param colour New colour to use.
 */
void window_imgColourLayered( const unsigned int wid,
      char* name, const glColour* colour )
{
   Widget *wgt;

   /* Get the widget. */
   wgt = window_getwgt(wid,name);
   if (wgt == NULL)
      return;

   /* Check the type. */
   if (wgt->type != WIDGET_IMAGE_LAYERED) {
      WARN("Not modifying imagelayered on non-imagelayered widget '%s'.", name);
      return;
   }

   /* Set the colour. */
   wgt->dat.imgl.colour = *colour;
}


static void iar_cleanup( Widget* iar )
{
	if (iar->dat.imgl.layers!=NULL)
		free(iar->dat.imgl.layers);
}

