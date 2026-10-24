#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace sparkbot {
class VoyageGame {
 int lane_=1,rock_=0,coin_=2,lives_=3,score_=0,row_=0;
 float y_=-18;
 bool paused_=true,resolved_=false;
 uint32_t seed_=0x835ca91;
 void NextRow(){seed_^=seed_<<13;seed_^=seed_>>17;seed_^=seed_<<5;rock_=seed_%3;coin_=(rock_+1+(seed_/3)%2)%3;y_=-18;resolved_=false;++row_;}
public:
 int Lane()const{return lane_;} int RockLane()const{return rock_;} int CoinLane()const{return coin_;}
 int Lives()const{return lives_;} int Score()const{return score_;} int Row()const{return row_;}
 float Y()const{return y_;} float Speed()const{return 42.f+std::min(row_,30)*1.4f;}
 bool Paused()const{return paused_;} bool Over()const{return lives_==0;}
 void Reset(){*this=VoyageGame{};}
 void Toggle(){if(Over()){Reset();paused_=false;}else paused_=!paused_;}
 void Steer(int delta){if(!paused_&&!Over())lane_=std::clamp(lane_+delta,0,2);}
 void Step(float dt){
  if(paused_||Over()||!std::isfinite(dt)||dt<=0)return;
  y_+=Speed()*std::min(dt,.08f);
  if(y_>=122&&!resolved_){resolved_=true;if(lane_==rock_)--lives_;else if(lane_==coin_)score_+=10;}
  if(y_>170&&!Over()){++score_;NextRow();}
 }
};
}
