#include "hub_input.h"
#include "hub_balance.h"
#include <cassert>
#include <iostream>
using namespace sparkbot;
int main() {
 // Grasping both sides must not turn into a click or navigation after release.
 TouchGestures grip;
 grip.Edge(2,true,100);grip.Edge(3,true,130);
 assert(grip.Edge(2,false,250)==Input::None);
 assert(grip.Edge(3,false,270)==Input::None);
 assert(grip.Poll(800)==Input::None);
 grip.Edge(2,true,1000);grip.Edge(2,false,1050);
 assert(grip.Poll(1351)==Input::LeftClick);
 // A 15 ms release while holding the pad is chatter, not a double click.
 TouchGestures chatter;
 chatter.Edge(2,true,100);chatter.Edge(2,false,200);
 chatter.Edge(2,true,215);chatter.Edge(2,false,500);
 assert(chatter.Poll(900)==Input::None);
 chatter.Edge(2,true,1200);chatter.Edge(2,false,1250);
 assert(chatter.Poll(1551)==Input::LeftClick);
 assert(ResolveNavigation(Input::LeftClick,true)==Navigation::Previous);
 assert(ResolveNavigation(Input::RightClick,true)==Navigation::Next);
 assert(ResolveNavigation(Input::Previous,true)==Navigation::Enter);
 assert(ResolveNavigation(Input::Next,true)==Navigation::Enter);
 assert(ResolveNavigation(Input::Launcher,false)==Navigation::Menu);
 assert(ResolveNavigation(Input::Launcher,true)==Navigation::Menu);
 assert(ResolveNavigation(Input::Home,false)==Navigation::Home);
 assert(ResolveNavigation(Input::Home,true)==Navigation::Home);
 assert(ResolveNavigation(Input::Previous,false)==Navigation::None);
 assert(ResolveNavigation(Input::Next,false)==Navigation::None);
 assert(ResolveNavigation(Input::Click,true)==Navigation::Next);
 // Exercise actual pad edges through the production navigation resolver.
 for(int channel=2;channel<=3;++channel) {
  for(int taps=1;taps<=3;++taps) {
   TouchGestures sequence;
   Input event=Input::None;
   for(int tap=0;tap<taps;++tap) {
    int64_t start=100+tap*150;
    assert(sequence.Edge(channel,true,start)==Input::None);
    assert(sequence.Poll(start+25)==Input::None);
    event=sequence.Edge(channel,false,start+50);
    if(tap<2)assert(event==Input::None);
   }
   if(taps<3)event=sequence.Poll(100+(taps-1)*150+351);
   auto expected=taps==1?(channel==2?Navigation::Previous:Navigation::Next):
     taps==2?Navigation::Enter:channel==2?Navigation::Menu:Navigation::Home;
   assert(ResolveNavigation(event,true)==expected);
   assert(ResolveNavigation(event,false)==(taps==3?expected:Navigation::None));
   assert(sequence.Poll(2000)==Input::None);
  }
 }
 TouchGestures quick;
 quick.Edge(2,true,100);quick.Edge(2,false,150);
 assert(quick.Poll(449)==Input::None);
 assert(quick.Poll(451)==Input::LeftClick);
 TouchGestures center;
 center.Edge(1,true,100);assert(center.Edge(1,false,150)==Input::Click);
 assert(center.Poll(1000)==Input::None);
 TouchGestures t;
 t.Edge(2,true,100);t.Edge(2,false,200);
 t.Edge(2,true,290);t.Edge(2,false,390);
 assert(t.Poll(680)==Input::None);
 t.Edge(2,true,680);assert(t.Edge(2,false,780)==Input::Launcher);
 assert(t.Poll(1500)==Input::None);
 TouchGestures slow;
 slow.Edge(3,true,100);slow.Edge(3,false,400);
 slow.Edge(3,true,550);assert(slow.Poll(845)==Input::None);slow.Edge(3,false,850);
 slow.Edge(3,true,1000);assert(slow.Edge(3,false,1300)==Input::Home);
 assert(slow.Poll(1800)==Input::None);
 t.Edge(3,true,2000);t.Edge(3,false,2100);t.Edge(3,true,2200);t.Edge(3,false,2300);
 t.Edge(3,true,2400);assert(t.Edge(3,false,2500)==Input::Home);
 TouchGestures d;
 d.Edge(2,true,100);d.Edge(2,false,200);d.Edge(2,true,300);d.Edge(2,false,400);
 assert(d.Poll(841)==Input::Previous);assert(d.Poll(1600)==Input::None);
 d.Edge(3,true,2000);d.Edge(3,false,2120);assert(d.Poll(2600)==Input::RightClick);
 for(int i=0;i<10;++i){d.Edge(2,true,3000+i*60);d.Edge(2,false,3020+i*60);}
 assert(d.Poll(4200)==Input::None);
 d.Edge(2,true,5000);assert(d.Poll(6300)==Input::None);
 assert(d.Edge(2,false,6500)==Input::None);assert(d.Poll(7000)==Input::None);
 std::cout<<"PASS: triple navigation, delayed double, noise and grip rejection\n";
 BalanceGame game;
 for(int i=0;i<200;++i)game.Step(0,0,.04f);
 assert(game.X()==0 && game.Y()==0 && game.Score()==0);
 for(int i=0;i<400;++i)game.Step(1,1,.04f);
 assert(game.X()<=.9f && game.Y()<=.9f);
 game.Toggle();float x=game.X();game.Step(-1,0,.04f);assert(game.X()==x);
 game.Reset();assert(game.Score()==0 && !game.Paused() && game.X()==0);
 for(int i=0;i<800 && game.Score()==0;++i)game.Step((game.TargetX()-game.X())*2,(game.TargetY()-game.Y())*2,.04f);
 assert(game.Score()>0);
 std::cout<<"PASS: balance toy movement, boundaries, pause, reset and target collection\n";
}
