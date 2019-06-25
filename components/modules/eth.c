
#include <string.h>

#include "module.h"
#include "lauxlib.h"
#include "lextra.h"
#include "lmem.h"
#include "nodemcu_esp_event.h"
#include "ip_fmt.h"
#include "common.h"

// example specific includes
#include "driver/gpio.h"

// phy includes
#include "eth_phy/phy_lan8720.h"
#include "eth_phy/phy_tlk110.h"
#include "eth_phy/phy_ip101.h"


typedef enum {
  ETH_PHY_LAN8720 = 0,
  ETH_PHY_TLK110,
  ETH_PHY_IP101,
  ETH_PHY_MAX
} eth_phy_t;

static struct {
  const eth_config_t *eth_config;
  gpio_num_t pin_power, pin_mdc, pin_mdio;
} module_config;



static void phy_device_power_enable_via_gpio( bool enable )
{
  if (!enable)
    module_config.eth_config->phy_power_enable( false );

  gpio_pad_select_gpio( module_config.pin_power );
  gpio_set_level( module_config.pin_power, enable ? 1 : 0 );
  gpio_set_direction( module_config.pin_power, GPIO_MODE_OUTPUT );

  // Allow the power up/down to take effect, min 300us
  //vTaskDelay(1);

  if (enable)
    module_config.eth_config->phy_power_enable(true);
}

/**
 * @brief gpio specific init
 *
 * @note RMII data pins are fixed in esp32:
 * TXD0 <=> GPIO19
 * TXD1 <=> GPIO22
 * TX_EN <=> GPIO21
 * RXD0 <=> GPIO25
 * RXD1 <=> GPIO26
 * CLK <=> GPIO0
 *
 */
static void eth_gpio_config_rmii( void )
{
  phy_rmii_configure_data_interface_pins();
  phy_rmii_smi_configure_pins( module_config.pin_mdc, module_config.pin_mdio );
}



// --- Event handling -----------------------------------------------------

typedef void (*fill_cb_arg_fn) (lua_State *L, const system_event_t *evt);
typedef struct
{
  const char *name;
  system_event_id_t event_id;
  fill_cb_arg_fn fill_cb_arg;
} event_desc_t;

#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))

static void eth_got_ip (lua_State *L, const system_event_t *evt);
static void empty_arg (lua_State *L, const system_event_t *evt) {}

static const event_desc_t events[] =
{
  { "start",            SYSTEM_EVENT_ETH_START,           empty_arg     },
  { "stop",             SYSTEM_EVENT_ETH_STOP,            empty_arg     },
  { "connected",        SYSTEM_EVENT_ETH_CONNECTED,       empty_arg     },
  { "disconnected",     SYSTEM_EVENT_ETH_DISCONNECTED,    empty_arg     },
  { "got_ip",           SYSTEM_EVENT_ETH_GOT_IP,          eth_got_ip    },
};

#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))
static int event_cb[ARRAY_LEN(events)];

static int eth_event_idx_by_id( const event_desc_t *table, unsigned n, system_event_id_t id )
{
  for (unsigned i = 0; i < n; ++i)
    if (table[i].event_id == id)
      return i;
  return -1;
}

static int eth_event_idx_by_name( const event_desc_t *table, unsigned n, const char *name )
{
  for (unsigned i = 0; i < n; ++i)
    if (strcmp( table[i].name, name ) == 0)
      return i;
  return -1;
}


static void eth_got_ip( lua_State *L, const system_event_t *evt )
{
  (void)evt;
  tcpip_adapter_ip_info_t ip_info;

  memset(&ip_info, 0, sizeof(tcpip_adapter_ip_info_t));
  if (tcpip_adapter_get_ip_info( ESP_IF_ETH, &ip_info ) != ESP_OK) {
    luaL_error( L, "error from tcpip_adapter_get_ip_info!" );
  }

  char ipstr[IP_STR_SZ] = { 0 };
  ip4str( ipstr, &ip_info.ip );
  lua_pushstring( L, ipstr );
  lua_setfield( L, -2, "ip" );

  ip4str( ipstr, &ip_info.netmask );
  lua_pushstring( L, ipstr );
  lua_setfield( L, -2, "netmask" );

  ip4str(ipstr, &ip_info.gw );
  lua_pushstring( L, ipstr );
  lua_setfield( L, -2, "gw" );
}

static void on_event( const system_event_t *evt )
{
  int idx = eth_event_idx_by_id( events, ARRAY_LEN(events), evt->event_id );
  if (idx < 0 || event_cb[idx] == LUA_NOREF)
    return;

  lua_State *L = lua_getstate();
  lua_rawgeti( L, LUA_REGISTRYINDEX, event_cb[idx] );
  lua_pushstring( L, events[idx].name );
  lua_createtable( L, 0, 5 );
  events[idx].fill_cb_arg( L, evt );
  lua_call( L, 2, 0 );
}

NODEMCU_ESP_EVENT(SYSTEM_EVENT_ETH_START,           on_event);
NODEMCU_ESP_EVENT(SYSTEM_EVENT_ETH_STOP,            on_event);
NODEMCU_ESP_EVENT(SYSTEM_EVENT_ETH_CONNECTED,       on_event);
NODEMCU_ESP_EVENT(SYSTEM_EVENT_ETH_DISCONNECTED,    on_event);
NODEMCU_ESP_EVENT(SYSTEM_EVENT_ETH_GOT_IP,          on_event);



// Lua API

static int leth_set_mac( lua_State *L )
{
  uint8_t mac[6];
  const char *macstr = luaL_checkstring( L, 1 );

  if (6 != sscanf( macstr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                   &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] )) {
    return luaL_error( L, "invalid mac string" );
  }

  if (ESP_OK != esp_eth_set_mac( mac )) {
    return luaL_error( L, "error setting mac" );
  }

  return 0;
}

static int leth_get_mac( lua_State *L )
{
  char temp[64];
  uint8_t mac[6];

  esp_eth_get_mac( mac );
  snprintf( temp, 63, MACSTR, MAC2STR(mac) );
  lua_pushstring( L, temp );
  return 1;
}

static int leth_get_speed( lua_State *L )
{
  eth_speed_mode_t speed = esp_eth_get_speed();
  switch (speed) {
  case ETH_SPEED_MODE_10M:
    lua_pushnumber( L, 10 );
    break;
  case ETH_SPEED_MODE_100M:
    lua_pushnumber( L, 100 );
    break;
  default:
    return luaL_error( L, "invalid speed" );
    break;
  }
  return 1;
}

static int leth_on( lua_State *L )
{
  const char *event_name = luaL_checkstring( L, 1 );
  if (!lua_isnoneornil( L, 2 )) {
    luaL_checkanyfunction( L, 2 );
  }
  lua_settop( L, 2 );

  int idx = eth_event_idx_by_name( events, ARRAY_LEN(events), event_name );
  if (idx < 0)
    return luaL_error( L, "unknown event: %s", event_name );

  luaL_unref( L, LUA_REGISTRYINDEX, event_cb[idx] );
  event_cb[idx] = lua_isnoneornil( L, 2 ) ?
    LUA_NOREF : luaL_ref( L, LUA_REGISTRYINDEX );

  return 0;
}

static int leth_init( lua_State *L )
{
  int stack = 0;
  int top = lua_gettop( L );

  luaL_checktype( L, ++stack, LUA_TTABLE );
  // temporarily copy option table to top of stack for opt_ functions
  lua_pushvalue( L, stack );

  eth_phy_t phy = opt_checkint_range( L, "phy", -1, 0, ETH_PHY_MAX );
  eth_phy_base_t phy_addr = opt_checkint_range( L, "addr", -1, 0, PHY31 );
  eth_clock_mode_t clock_mode = opt_checkint_range( L, "clock_mode", -1, 0, ETH_CLOCK_GPIO17_OUT );
  module_config.pin_power  = opt_checkint( L, "power", -1 );
  module_config.pin_mdc    = opt_checkint( L, "mdc",   -1 );
  module_config.pin_mdio   = opt_checkint( L, "mdio",  -1 );

  lua_settop( L, top );

  eth_config_t config;

  switch (phy) {
  case ETH_PHY_LAN8720:
    config = phy_lan8720_default_ethernet_config;
    module_config.eth_config = &phy_lan8720_default_ethernet_config;
    break;
  case ETH_PHY_TLK110:
    config = phy_tlk110_default_ethernet_config;
    module_config.eth_config = &phy_tlk110_default_ethernet_config;
    break;
  case ETH_PHY_IP101:
    config = phy_ip101_default_ethernet_config;
    module_config.eth_config = &phy_ip101_default_ethernet_config;
    break;
  default:
    // prevented by opt_checkint_range
    break;
      
  };

  config.phy_addr = phy_addr;
  config.gpio_config = eth_gpio_config_rmii;
  config.tcpip_input = tcpip_adapter_eth_input;
  config.clock_mode = clock_mode;

  config.phy_power_enable = phy_device_power_enable_via_gpio;

  if (esp_eth_init( &config ) != ESP_OK) {
    luaL_error( L, "esp_eth_init failed" );
  }
  if (esp_eth_enable() != ESP_OK) {
    luaL_error( L, "esp_eth_enable failed" );
  }

  return 0;
}


static const LUA_REG_TYPE eth_map[] =
{
  { LSTRKEY( "init"      ), LFUNCVAL( leth_init      ) },
  { LSTRKEY( "on"        ), LFUNCVAL( leth_on        ) },
  { LSTRKEY( "get_speed" ), LFUNCVAL( leth_get_speed ) },
  { LSTRKEY( "get_mac"   ), LFUNCVAL( leth_get_mac   ) },
  { LSTRKEY( "set_mac"   ), LFUNCVAL( leth_set_mac   ) },

  { LSTRKEY( "PHY_LAN8720" ), LNUMVAL( ETH_PHY_LAN8720 ) },
  { LSTRKEY( "PHY_TLK110"  ), LNUMVAL( ETH_PHY_TLK110  ) },
  { LSTRKEY( "PHY_IP101"   ), LNUMVAL( ETH_PHY_IP101   ) },

  { LSTRKEY( "CLOCK_GPIO0_IN"   ), LNUMVAL( ETH_CLOCK_GPIO0_IN   ) },
  { LSTRKEY( "CLOCK_GPIO0_OUT"  ), LNUMVAL( ETH_CLOCK_GPIO0_OUT  ) },
  { LSTRKEY( "CLOCK_GPIO16_OUT" ), LNUMVAL( ETH_CLOCK_GPIO16_OUT ) },
  { LSTRKEY( "CLOCK_GPIO17_OUT" ), LNUMVAL( ETH_CLOCK_GPIO17_OUT ) },

  { LNILKEY, LNILVAL }
};

static int eth_init( lua_State *L )
{
  return 1;
}

NODEMCU_MODULE(ETH, "eth", eth_map, eth_init);
