#include "hub_voyage.h"
#include <cassert>
#include <iostream>
int main(){
 sparkbot::VoyageGame g; assert(g.Paused());g.Toggle();
 for(int i=0;i<600;++i){g.Steer(g.CoinLane()-g.Lane());g.Step(.04f);}
 assert(g.Score()>=50 && g.Lives()==3); // Real input/step loop can collect without collisions.
 g.Toggle();float y=g.Y();g.Step(.04f);assert(g.Y()==y);
 g.Reset();g.Toggle();
 for(int i=0;i<800 && !g.Over();++i){g.Steer(g.RockLane()-g.Lane());g.Step(.04f);}
 assert(g.Over() && g.Lives()==0);g.Toggle();assert(!g.Over() && !g.Paused() && g.Score()==0);
 g.Steer(-99);assert(g.Lane()==0);g.Steer(99);assert(g.Lane()==2);
 std::cout<<"PASS: voyage collection, collision, pause, restart and lane boundaries\n";
}
