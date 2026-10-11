// included ahead of every darling-metal file on PD: names it expects from Apple's headers
#include <stdint.h>
#include <CoreFoundation/CoreFoundation.h>

// a darling-metal header spells uint8_t this way (NS_ENUM(uint8, MTLTextureSwizzle))
typedef uint8_t uint8;
