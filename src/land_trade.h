/*
 * See Licensing and Copyright notice in naev.h
 */



#ifndef LAND_TRADE_H
#  define LAND_TRADE_H


#include "land.h"
#include "land_takeoff.h"

/*
 * Helper functions.
 */
void commodity_exchange_open( unsigned int wid );
void commodity_update( unsigned int wid, char* str );
void commodity_buy( unsigned int wid, char* str );
void commodity_sell( unsigned int wid, char* str );
int commodity_canBuy( char *name );
int commodity_canSell( char *name );
int commodity_getMod (void);
void commodity_renderMod( double bx, double by, double w, double h, void *data );

#endif /* LAND_TRADE_H */
