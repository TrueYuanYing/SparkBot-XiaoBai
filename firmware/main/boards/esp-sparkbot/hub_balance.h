#pragma once
#include <algorithm>
#include <cmath>
namespace sparkbot {
class BalanceGame {
    float x_=0,y_=0,vx_=0,vy_=0;
    int score_=0;
    bool paused_=false;
public:
    float X() const{return x_;} float Y() const{return y_;}
    int Score() const{return score_;} bool Paused() const{return paused_;}
    float TargetX() const {const float x[]{.55f,-.55f,.45f,-.4f,0};return x[score_%5];}
    float TargetY() const {const float y[]{-.45f,.5f,.55f,-.5f,0};return y[score_%5];}
    void Reset(){x_=y_=vx_=vy_=0;score_=0;paused_=false;}
    void Toggle(){paused_=!paused_;vx_=vy_=0;}
    void Step(float x,float y,float dt) {
        if(paused_||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(dt)||dt<=0)return;
        dt=std::min(dt,.08f);
        vx_=(vx_+std::clamp(x,-1.f,1.f)*dt*2.5f)*.96f;
        vy_=(vy_+std::clamp(y,-1.f,1.f)*dt*2.5f)*.96f;
        x_+=vx_*dt;y_+=vy_*dt;
        if(std::abs(x_)>.9f){x_=std::clamp(x_,-.9f,.9f);vx_*=-.35f;}
        if(std::abs(y_)>.9f){y_=std::clamp(y_,-.9f,.9f);vy_*=-.35f;}
        float dx=x_-TargetX(),dy=y_-TargetY();
        if(dx*dx+dy*dy<.016f){++score_;vx_*=.35f;vy_*=.35f;}
    }
};
}
