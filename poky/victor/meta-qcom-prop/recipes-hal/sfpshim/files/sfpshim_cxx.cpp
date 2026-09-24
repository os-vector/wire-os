#include <ostream>
extern "C" __attribute__((pcs("aapcs"), visibility("default")))
std::ostream *__sfp_ostream_insert_double(std::ostream *os, double v) { return &(*os << v); }
