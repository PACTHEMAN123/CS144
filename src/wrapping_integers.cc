#include "wrapping_integers.hh"
#include <cmath>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + static_cast<uint32_t>(n);
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t offset = static_cast<uint64_t>(raw_value_ - zero_point.raw_value_);
  uint64_t t = offset + (checkpoint & 0xffffffff00000000);
  uint64_t dis1 = (t + (1UL << 32) > checkpoint) ? (t + (1UL << 32) - checkpoint) : (checkpoint - t - (1UL << 32));
  uint64_t dis2 = (t > checkpoint) ? (t - checkpoint) : (checkpoint - t);
  uint64_t dis3 = (t - (1UL << 32) > checkpoint) ? (t - (1UL << 32) - checkpoint) : (checkpoint - t + (1UL << 32));
  uint64_t ans;
  if(dis1 < dis2){
    if(dis1 < dis3)
      ans = t + (1UL << 32);
    else 
      ans = t - (1UL << 32);
  } else {
    if(dis2 < dis3)
      ans = t;
    else
      ans = t - (1UL << 32);
  }
  return ans;
}
