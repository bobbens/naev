/*
 * See Licensing and Copyright notice in naev.h
 */



#ifndef WGT_IMAGE_LAYERED_H
#  define WGT_IMAGE_LAYERED_H


#include "font.h"
#include "colour.h"


/**
 * @brief The image widget data
 */
typedef struct WidgetImageLayeredData_{
   glTexture** layers; /**< layers to display, in order. */
   int nlayers;
   glColour colour; /**< Colour to warp to. */
   int border; /**< 1 if widget should have border. */
} WidgetImageLayeredData;


/* Required functions. */
void window_addImageLayered( const unsigned int wid,
      const int x, const int y, /* position */
      const int w, const int h, /* dimensions */
      char* name, glTexture** layers,int nlayers, int border ); /* label and image itself */

/* Misc functions. */
void window_modifyImageLayered( const unsigned int wid,
      char* name, glTexture** layers,int nlayers, int w, int h );
void window_imgColourLayered( const unsigned int wid,
      char* name, const glColour* colour );
glTexture* window_getImageLayer( const unsigned int wid, char* name, int pos );


#endif /* WGT_IMAGE_LAYERED_H */

