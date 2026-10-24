#include "hub_dock.h"
#include "hub_profile.h"
#include <cassert>
#include <iostream>
int main(){
 sparkbot::BaseEcho e;
 assert(!e.Feed("noise x0.0",10));assert(!e.Feed("00 y0.",6));assert(e.Feed("000",3));
 assert(!e.Feed("x9.999 y0.000",13));assert(!e.Feed("x0.0aa y0.000",13));
 assert(e.Feed("x-0.125 y0.750x0.000 y0.000",27));
 sparkbot::DockPresence d;assert(!d.Connected(0));d.Reply(100);assert(!d.Connected(100));d.Reply(150);d.Reply(200);assert(d.Connected(200));
 assert(d.Connected(4900));assert(!d.Connected(5300));d.Reply(5400);assert(!d.Connected(5400));
 for(const auto* c:{"tracks","wheels","quadruped","none"})assert(sparkbot::ValidChassis(c));
 for(const auto* c:{"", "<script>","plane"})assert(!sparkbot::ValidChassis(c));
 std::cout<<"PASS: fragmented echo, malformed echo, dock debounce/disconnect and chassis validation\n";
}
