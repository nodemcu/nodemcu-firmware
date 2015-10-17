#include "sections.h"
#include "rom.h"
#include <xtensa/corebits.h>

void load_non_32_wide_handler (struct exception_frame *ef, uint32_t cause) TEXT_SECTION_ATTR;
