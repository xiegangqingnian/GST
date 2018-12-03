/**
 *  @file
 *  @copyright defined in gst/LICENSE.txt
 */
#include <gstiolib/gstio.hpp>

#include "test_api.hpp"

void test_types::types_size() {

   gstio_assert( sizeof(int64_t) == 8, "int64_t size != 8");
   gstio_assert( sizeof(uint64_t) ==  8, "uint64_t size != 8");
   gstio_assert( sizeof(uint32_t) ==  4, "uint32_t size != 4");
   gstio_assert( sizeof(int32_t) ==  4, "int32_t size != 4");
   gstio_assert( sizeof(uint128_t) == 16, "uint128_t size != 16");
   gstio_assert( sizeof(int128_t) == 16, "int128_t size != 16");
   gstio_assert( sizeof(uint8_t) ==  1, "uint8_t size != 1");

   gstio_assert( sizeof(account_name) ==  8, "account_name size !=  8");
   gstio_assert( sizeof(table_name) ==  8, "table_name size !=  8");
   gstio_assert( sizeof(time) ==  4, "time size !=  4");
   gstio_assert( sizeof(gstio::key256) == 32, "key256 size != 32" );
}

void test_types::char_to_symbol() {

   gstio_assert( gstio::char_to_symbol('1') ==  1, "gstio::char_to_symbol('1') !=  1");
   gstio_assert( gstio::char_to_symbol('2') ==  2, "gstio::char_to_symbol('2') !=  2");
   gstio_assert( gstio::char_to_symbol('3') ==  3, "gstio::char_to_symbol('3') !=  3");
   gstio_assert( gstio::char_to_symbol('4') ==  4, "gstio::char_to_symbol('4') !=  4");
   gstio_assert( gstio::char_to_symbol('5') ==  5, "gstio::char_to_symbol('5') !=  5");
   gstio_assert( gstio::char_to_symbol('a') ==  6, "gstio::char_to_symbol('a') !=  6");
   gstio_assert( gstio::char_to_symbol('b') ==  7, "gstio::char_to_symbol('b') !=  7");
   gstio_assert( gstio::char_to_symbol('c') ==  8, "gstio::char_to_symbol('c') !=  8");
   gstio_assert( gstio::char_to_symbol('d') ==  9, "gstio::char_to_symbol('d') !=  9");
   gstio_assert( gstio::char_to_symbol('e') == 10, "gstio::char_to_symbol('e') != 10");
   gstio_assert( gstio::char_to_symbol('f') == 11, "gstio::char_to_symbol('f') != 11");
   gstio_assert( gstio::char_to_symbol('g') == 12, "gstio::char_to_symbol('g') != 12");
   gstio_assert( gstio::char_to_symbol('h') == 13, "gstio::char_to_symbol('h') != 13");
   gstio_assert( gstio::char_to_symbol('i') == 14, "gstio::char_to_symbol('i') != 14");
   gstio_assert( gstio::char_to_symbol('j') == 15, "gstio::char_to_symbol('j') != 15");
   gstio_assert( gstio::char_to_symbol('k') == 16, "gstio::char_to_symbol('k') != 16");
   gstio_assert( gstio::char_to_symbol('l') == 17, "gstio::char_to_symbol('l') != 17");
   gstio_assert( gstio::char_to_symbol('m') == 18, "gstio::char_to_symbol('m') != 18");
   gstio_assert( gstio::char_to_symbol('n') == 19, "gstio::char_to_symbol('n') != 19");
   gstio_assert( gstio::char_to_symbol('o') == 20, "gstio::char_to_symbol('o') != 20");
   gstio_assert( gstio::char_to_symbol('p') == 21, "gstio::char_to_symbol('p') != 21");
   gstio_assert( gstio::char_to_symbol('q') == 22, "gstio::char_to_symbol('q') != 22");
   gstio_assert( gstio::char_to_symbol('r') == 23, "gstio::char_to_symbol('r') != 23");
   gstio_assert( gstio::char_to_symbol('s') == 24, "gstio::char_to_symbol('s') != 24");
   gstio_assert( gstio::char_to_symbol('t') == 25, "gstio::char_to_symbol('t') != 25");
   gstio_assert( gstio::char_to_symbol('u') == 26, "gstio::char_to_symbol('u') != 26");
   gstio_assert( gstio::char_to_symbol('v') == 27, "gstio::char_to_symbol('v') != 27");
   gstio_assert( gstio::char_to_symbol('w') == 28, "gstio::char_to_symbol('w') != 28");
   gstio_assert( gstio::char_to_symbol('x') == 29, "gstio::char_to_symbol('x') != 29");
   gstio_assert( gstio::char_to_symbol('y') == 30, "gstio::char_to_symbol('y') != 30");
   gstio_assert( gstio::char_to_symbol('z') == 31, "gstio::char_to_symbol('z') != 31");

   for(unsigned char i = 0; i<255; i++) {
      if((i >= 'a' && i <= 'z') || (i >= '1' || i <= '5')) continue;
      gstio_assert( gstio::char_to_symbol((char)i) == 0, "gstio::char_to_symbol() != 0");
   }
}

void test_types::string_to_name() {

   gstio_assert( gstio::string_to_name("a") == N(a) , "gstio::string_to_name(a)" );
   gstio_assert( gstio::string_to_name("ba") == N(ba) , "gstio::string_to_name(ba)" );
   gstio_assert( gstio::string_to_name("cba") == N(cba) , "gstio::string_to_name(cba)" );
   gstio_assert( gstio::string_to_name("dcba") == N(dcba) , "gstio::string_to_name(dcba)" );
   gstio_assert( gstio::string_to_name("edcba") == N(edcba) , "gstio::string_to_name(edcba)" );
   gstio_assert( gstio::string_to_name("fedcba") == N(fedcba) , "gstio::string_to_name(fedcba)" );
   gstio_assert( gstio::string_to_name("gfedcba") == N(gfedcba) , "gstio::string_to_name(gfedcba)" );
   gstio_assert( gstio::string_to_name("hgfedcba") == N(hgfedcba) , "gstio::string_to_name(hgfedcba)" );
   gstio_assert( gstio::string_to_name("ihgfedcba") == N(ihgfedcba) , "gstio::string_to_name(ihgfedcba)" );
   gstio_assert( gstio::string_to_name("jihgfedcba") == N(jihgfedcba) , "gstio::string_to_name(jihgfedcba)" );
   gstio_assert( gstio::string_to_name("kjihgfedcba") == N(kjihgfedcba) , "gstio::string_to_name(kjihgfedcba)" );
   gstio_assert( gstio::string_to_name("lkjihgfedcba") == N(lkjihgfedcba) , "gstio::string_to_name(lkjihgfedcba)" );
   gstio_assert( gstio::string_to_name("mlkjihgfedcba") == N(mlkjihgfedcba) , "gstio::string_to_name(mlkjihgfedcba)" );
   gstio_assert( gstio::string_to_name("mlkjihgfedcba1") == N(mlkjihgfedcba2) , "gstio::string_to_name(mlkjihgfedcba2)" );
   gstio_assert( gstio::string_to_name("mlkjihgfedcba55") == N(mlkjihgfedcba14) , "gstio::string_to_name(mlkjihgfedcba14)" );

   gstio_assert( gstio::string_to_name("azAA34") == N(azBB34) , "gstio::string_to_name N(azBB34)" );
   gstio_assert( gstio::string_to_name("AZaz12Bc34") == N(AZaz12Bc34) , "gstio::string_to_name AZaz12Bc34" );
   gstio_assert( gstio::string_to_name("AAAAAAAAAAAAAAA") == gstio::string_to_name("BBBBBBBBBBBBBDDDDDFFFGG") , "gstio::string_to_name BBBBBBBBBBBBBDDDDDFFFGG" );
}

void test_types::name_class() {

   gstio_assert( gstio::name{gstio::string_to_name("azAA34")}.value == N(azAA34), "gstio::name != N(azAA34)" );
   gstio_assert( gstio::name{gstio::string_to_name("AABBCC")}.value == 0, "gstio::name != N(0)" );
   gstio_assert( gstio::name{gstio::string_to_name("AA11")}.value == N(AA11), "gstio::name != N(AA11)" );
   gstio_assert( gstio::name{gstio::string_to_name("11AA")}.value == N(11), "gstio::name != N(11)" );
   gstio_assert( gstio::name{gstio::string_to_name("22BBCCXXAA")}.value == N(22), "gstio::name != N(22)" );
   gstio_assert( gstio::name{gstio::string_to_name("AAAbbcccdd")} == gstio::name{gstio::string_to_name("AAAbbcccdd")}, "gstio::name == gstio::name" );

   uint64_t tmp = gstio::name{gstio::string_to_name("11bbcccdd")};
   gstio_assert(N(11bbcccdd) == tmp, "N(11bbcccdd) == tmp");
}
