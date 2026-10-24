#include "hub_touch_noise.h"
#include <cassert>
#include <iostream>
int main(){
 sparkbot::TouchNoiseCalibration noisy;
 for(int i=0;i<50;++i)noisy.Sample(28900+(i%2?5500:0));
 assert(noisy.Noisy());assert(noisy.Threshold(29000,.08f)<=2500);
 sparkbot::TouchNoiseCalibration clean;
 for(int i=0;i<50;++i)clean.Sample(34600+i%20);
 assert(!clean.Noisy());assert(clean.Threshold(34600,.08f)<2900);
 sparkbot::TouchNoiseCalibration invalid;invalid.Sample(4194303);invalid.Sample(0);
 assert(!invalid.Ready());
 std::cout<<"PASS: noise reported without desensitizing taps; invalid samples ignored\n";
}
