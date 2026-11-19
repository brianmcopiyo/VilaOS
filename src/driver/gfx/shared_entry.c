#include "gfx_driver.h"

const vilaos_gfx_driver_t *vilaos_gfx_driver_entry(void)
{
    return vilaos_gfx_builtin_driver();
}
