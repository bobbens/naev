/*
 * See Licensing and Copyright notice in naev.h
 */



#ifndef LAND_OUTFITS_H
#  define LAND_OUTFITS_H


#include "land.h"
#include "outfit.h"


void outfits_open( unsigned int wid );
void outfits_regenList( unsigned int wid, char *str );
void outfits_update( unsigned int wid, char* str );
void outfits_updateEquipmentOutfits( void );
int outfits_filter( Outfit **outfits, glTexture ***toutfits, int *ntoutfits, int n,
      int(*filter)( const Outfit *o ), char *name );
int outfit_canBuy( char *outfit, Planet *planet );
int outfit_canSell( char *outfit );
void outfits_cleanup( void );

#endif /* LAND_OUTFITS_H */
