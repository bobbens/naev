/*
 * See Licensing and Copyright notice in naev.h
 */



#ifndef WGT_IMAGELAYEREDARRAY_H
#  define WGT_IMAGELAYEREDARRAY_H


#include "opengl.h"
#include "font.h"
#include "colour.h"


/**
 * @brief The image layered array widget data.
 */
typedef struct WidgetImageLayeredArrayData_ {
   glTexture ***layers; /**< Layers array. */
   int *nlayers; /** numbers of layers for each element **/
   char **captions; /**< Corresponding caption array. */
   char **alts; /**< Alt text when mouse over. */
   char **quantity; /**< Number in top-left corner. */
   char **slottype; /**< Letter in top-right corner. */
   glColour *background; /**< Background of each of the elements. */
   int nelements; /**< Number of elements. */
   int xelem; /**< Number of horizontal elements. */
   int yelem; /**< Number of vertical elements. */
   int selected; /**< Currently selected element. */
   int alt; /**< Currently displaying alt text. */
   int altx; /**< Alt x position. */
   int alty; /**< Alt y position. */
   double pos; /**< Current y position. */
   int iw; /**< Image width to use. */
   int ih; /**< Image height to use. */
   void (*fptr) (unsigned int,char*); /**< Modify callback - triggered on selection. */
   void (*rmptr) (unsigned int,char*); /**< Right click callback. */
} WidgetImageLayeredArrayData;



/* Required functions. */
void window_addImageLayeredArray( const unsigned int wid,
      const int x, const int y, /* position */
      const int w, const int h, /* size */
      char* name, const int iw, const int ih, /* name and image sizes */
      glTexture*** tex, int *nlayers, char** caption, int nelem, /* elements */
      void (*call) (unsigned int,char*), /* update callback */
      void (*rmcall) (unsigned int,char*) ); /* right click callback */

/* Misc functions. */
char* toolkit_getImageLayeredArray( const unsigned int wid, const char* name );
int toolkit_setImageLayeredArray( const unsigned int wid, const char* name, char* elem );
int toolkit_getImageLayeredArrayPos( const unsigned int wid, const char* name );
int toolkit_setImageLayeredArrayPos( const unsigned int wid, const char* name, int pos );
double toolkit_getImageLayeredArrayOffset( const unsigned int wid, const char* name );
int toolkit_setImageLayeredArrayOffset( const unsigned int wid, const char* name, double off );
int toolkit_setImageLayeredArrayAlt( const unsigned int wid, const char* name, char **alt );
int toolkit_setImageLayeredArrayQuantity( const unsigned int wid, const char* name,
      char **quantity );
int toolkit_setImageLayeredArraySlotType( const unsigned int wid, const char* name,
      char **slottype );
int toolkit_setImageLayeredArrayBackground( const unsigned int wid, const char* name,
      glColour *bg );
int toolkit_saveImageLayeredArrayData( const unsigned int wid, const char *name,
      iar_data_t *iar_data );


#endif /* WGT_IMAGELAYEREDARRAY_H */

