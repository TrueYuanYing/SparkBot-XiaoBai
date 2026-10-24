#include "hub_input.h"
#include <cassert>
#include <iostream>
using namespace sparkbot;
int main() {
 for(int channel=1;channel<=3;++channel) {
  TouchGestures touch;
  touch.Edge(channel,true,100);
  assert(touch.Feedback(120).zone==0); // Short noise has no expression.
  auto hit=touch.Feedback(150);
  assert(hit.zone==channel && hit.mood==TouchMood::Bump && hit.strength>0);
  touch.Edge(channel,false,220);
  assert(touch.Feedback(300).zone==channel);
  assert(touch.Feedback(900).zone==0);
 }
 std::cout<<"PASS: top, left and right short touches have directional reactions\n";
 for(int channel=1;channel<=3;++channel) {
  TouchGestures touch;
  touch.Edge(channel,true,100);
  assert(touch.Poll(800)==Input::None);
  assert(touch.Feedback(800).mood==TouchMood::Pet);
  assert(touch.Feedback(10000).zone==channel);
  assert(touch.Edge(channel,false,10001)==Input::None);
  assert(touch.Poll(10400)==Input::None);
  assert(touch.Feedback(10400).mood==TouchMood::Pet);
  assert(touch.Feedback(11000).zone==0);
  touch.Edge(channel,true,12000);touch.Edge(channel,false,12020);
  assert(touch.Feedback(12021).zone==0);
 }
 TouchGestures grip;
 grip.Edge(2,true,100);grip.Edge(3,true,200);
 assert(grip.Feedback(1000).zone==0);
 grip.Edge(2,false,1100);grip.Edge(3,false,1200);
 assert(grip.Feedback(1400).zone==0);
 assert(grip.Poll(1800)==Input::None);
 grip.Edge(1,true,2000);assert(grip.Feedback(2050).zone==1);
 TouchGestures settling;
 settling.Edge(2,true,100);settling.Edge(3,true,120);
 settling.Edge(2,false,150);settling.Edge(3,false,180);
 settling.Edge(1,true,220); // A contact during grip recovery must wait for release.
 assert(settling.Feedback(1000).zone==0);
 assert(settling.Edge(1,false,1200)==Input::None);
 assert(settling.Feedback(1300).zone==0);
 settling.Edge(1,true,1500);assert(settling.Feedback(1550).zone==1);
 std::cout<<"PASS: sustained petting, release fade, no long-press clicks, grip cancellation\n";
}
