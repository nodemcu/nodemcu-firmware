#ifndef __USER_LIGHT_ADJ_H__
#define __USER_LIGHT_ADJ_H__
/*pwm.h: function and macro definition of PWM API , driver level */
/*user_light.h: user interface for light setting, user level*/
/*user_light_adj: API for color changing and lighting effects, user level*/




/*save RGB params to flash when calling light_set_aim*/
#define SAVE_LIGHT_PARAM  0  //set to 0: do not save color params

/*check current consumption and limit the total current for LED driver IC*/
/*NOTE: YOU SHOULD REPLACE WIHT THE LIMIT CURRENT OF YOUR OWN APPLICATION*/
#define LIGHT_CURRENT_LIMIT  0 //set to 0: do not limit total current
#if LIGHT_CURRENT_LIMIT
#define LIGHT_TOTAL_CURRENT_MAX  (450*1000) //450000/1000 MA AT MOST
#define LIGHT_CURRENT_MARGIN  (80*1000) //80000/1000 MA CURRENT RAISES WHILE TEMPERATURE INCREASING
#define LIGHT_CURRENT_MARGIN_L2  (110*1000) //110000/1000 MA 
#define LIGHT_CURRENT_MARGIN_L3  (140*1000) //140000/1000 MA 
#endif


/*set target duty for PWM channels, change each channel duty gradually */
void  light_set_aim(uint32 r,uint32 g,uint32 b,uint32 cw,uint32 ww,uint32 period);//'white' channel is not used in default demo
void light_set_aim_r(uint32 r);
void light_set_aim_g(uint32 g);
void light_set_aim_b(uint32 b);
void light_set_aim_cw(uint32 cw);
void light_set_aim_ww(uint32 ww);

#endif

