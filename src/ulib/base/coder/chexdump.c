// ============================================================================
//
// = LIBRARY
//    ULib - c++ library
//
// = FILENAME
//    chexdump.c
//
// = AUTHOR
//    Stefano Casazza
//
// ============================================================================

/*
#define DEBUG_DEBUG
*/

#include <ulib/base/utility.h>
#include <ulib/base/coder/hexdump.h>

uint32_t u_hexdump_encode(const unsigned char* restrict input, uint32_t len, unsigned char* restrict result)
{
   uint32_t i;
   unsigned char* restrict r = result;

   U_INTERNAL_TRACE("u_hexdump_encode(%.*s,%u,%p)", U_min(len,128), input, len, result)

   U_INTERNAL_ASSERT_POINTER(input)

   for (i = 0; i < len; ++i)
      {
      unsigned char ch = input[i];

      *r++ = u_hex_lower[(ch >> 4) & 0x0F];
      *r++ = u_hex_lower[(ch     ) & 0x0F];
      }

   *r = 0;

   return (r - result);
}

uint32_t u_hexdump_decode(const char* restrict input, uint32_t len, unsigned char* restrict result)
{
   int32_t i;
   unsigned char* restrict r = result;

   U_INTERNAL_TRACE("u_hexdump_decode(%.*s,%u,%p,%lu)", U_min(len,128), input, len, result)

   U_INTERNAL_ASSERT_POINTER(input)

   for (i = 0; i < (int32_t)len; i += 2)
      {
      U_INTERNAL_ASSERT(u__isxdigit(input[i]))
      U_INTERNAL_ASSERT(u__isxdigit(input[i+1]))

      *r++ = ((u__hexc2int(((unsigned char* restrict)input)[i])   & 0x0F) << 4) |
              (u__hexc2int(((unsigned char* restrict)input)[i+1]) & 0x0F);
      }

   *r = 0;

   return (r - result);
}
