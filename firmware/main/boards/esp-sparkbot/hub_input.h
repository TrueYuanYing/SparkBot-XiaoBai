#pragma once
#include <cstdint>
#include <array>
namespace sparkbot {
enum class Input { None, Click, LeftClick, RightClick, Previous, Next, Home, Launcher };
enum class TouchMood { None, Bump, Pet };
struct TouchFeedback {
 int zone=0; // 1: shared top electrode, 2: left, 3: right.
 TouchMood mood=TouchMood::None;
 float strength=0;
};
enum class Navigation { None, Home, Menu, Previous, Next, Enter };
inline Navigation ResolveNavigation(Input input,bool in_menu) {
 if(input==Input::Home)return Navigation::Home;
 if(input==Input::Launcher)return Navigation::Menu;
 if(!in_menu)return Navigation::None;
 if(input==Input::LeftClick)return Navigation::Previous;
 if(input==Input::RightClick || input==Input::Click)return Navigation::Next;
 if(input==Input::Previous || input==Input::Next)return Navigation::Enter;
 return Navigation::None;
}
class TouchGestures {
 struct Pad {bool down=false,held=false;int count=0;int64_t pressed=0,released=-1;};
 std::array<Pad,3> pads_{};
 bool gripping_=false;
 int64_t quiet_until_=0;
 int feedback_zone_=0;
 int64_t feedback_started_=0,feedback_until_=0;
 TouchMood feedback_mood_=TouchMood::None;
 void CancelTaps() { feedback_zone_=0;for(auto& pad:pads_){pad.count=0;pad.released=-1;pad.held=true;} }
public:
 TouchFeedback Feedback(int64_t now) const {
  if(gripping_ || now<quiet_until_)return {};
  for(int i=0;i<3;++i)if(pads_[i].down) {
   auto age=now-pads_[i].pressed;
   if(age<35)return {};
   if(age>=650)return {i+1,TouchMood::Pet,float(age<810?.35f+.65f*(age-650)/160.f:1.f)};
   float strength=1.f-float(age-35)/615.f;
   return {i+1,TouchMood::Bump,strength};
  }
  if(feedback_zone_ && now<feedback_until_) {
   float strength=float(feedback_until_-now)/float(feedback_until_-feedback_started_);
   return {feedback_zone_,feedback_mood_,strength};
  }
  return {};
 }
 Input Edge(int channel,bool down,int64_t now) {
  if(channel<1 || channel>3)return Input::None;
  auto& p=pads_[channel-1];if(p.down==down)return Input::None;p.down=down;
  int contacts=0;for(const auto& pad:pads_)if(pad.down)++contacts;
  if(contacts>1 || (down && p.released>=0 && now-p.released<45)) {
   gripping_=true;CancelTaps();
  }
  if(gripping_ || now<quiet_until_) {
   CancelTaps();
   // A contact beginning inside recovery stays suppressed until all pads lift.
   if(contacts>0)gripping_=true;
   if(contacts==0){gripping_=false;quiet_until_=now+120;}
   return Input::None;
  }
  if(down){p.pressed=now;p.held=false;feedback_zone_=0;return Input::None;}
  if(now-p.pressed>=650) {
   feedback_zone_=channel;feedback_mood_=TouchMood::Pet;feedback_started_=now;feedback_until_=now+950;
   p.count=0;p.released=-1;return Input::None;
  }
  if(p.held){p.count=0;p.released=-1;return Input::None;}
  if(now-p.pressed<35)return Input::None;
  feedback_zone_=channel;feedback_mood_=TouchMood::Bump;feedback_started_=p.pressed+35;feedback_until_=p.pressed+650;
  // The confirmation pad has no double/triple action: dispatch on release.
  if(channel==1){p.count=0;p.released=-1;return Input::Click;}
  // Measure the gap before this press, not its duration: deliberate 300 ms
  // taps must still form a triple when the finger lifts briefly between taps.
  if(p.released<0 || p.pressed-p.released>300)p.count=0;
  ++p.count;p.released=now;
  if(p.count==3 && channel!=1) {
   for(auto& pad:pads_){pad.count=0;pad.released=-1;}
   return channel==2?Input::Launcher:Input::Home;
  }
  return Input::None;
 }
 Input Poll(int64_t now) {
  for(int i=0;i<3;++i) {
   auto& p=pads_[i];
   if(p.down && now-p.pressed>=650){p.held=true;p.count=0;p.released=-1;}
   if(!p.down && p.released>=0 && now-p.released>300) {
    int count=p.count;p.count=0;p.released=-1;
    if(count>=2 && i!=0)return i==1?Input::Previous:Input::Next;
    return i==0?Input::Click:i==1?Input::LeftClick:Input::RightClick;
   }
  }
  return Input::None;
 }
};
}
