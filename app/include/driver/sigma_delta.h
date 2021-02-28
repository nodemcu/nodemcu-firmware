#ifndef SIGMA_DELTA_APP_H
#define SIGMA_DELTA_APP_H

#include "eagle_soc.h"
#include "os_type.h"

void sigma_delta_setup( void );
void sigma_delta_stop( void );
void sigma_delta_set_prescale_target( sint16 prescale, sint16 target );

#endif
