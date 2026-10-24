#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
namespace sparkbot {
// Measure the actual idle envelope; hardware's minimum-tracking benchmark can
// otherwise interpret every crest of an oscillating electrode as a finger.
class TouchNoiseCalibration {
 uint32_t low_=std::numeric_limits<uint32_t>::max(),high_=0,count_=0;
public:
 void Sample(uint32_t raw){if(raw<1000||raw>1000000)return;low_=std::min(low_,raw);high_=std::max(high_,raw);++count_;}
 bool Ready()const{return count_>=20;}
 uint32_t Threshold(uint32_t benchmark,float sensitivity)const{
  // Noise overlaps real fingers on this board. Raising the threshold to the
  // entire noise envelope made ordinary taps unusable; report noise separately.
  return static_cast<uint32_t>(benchmark*sensitivity);
 }
 bool Noisy()const{return Ready()&&high_-low_>low_*.04f;}
};
}
