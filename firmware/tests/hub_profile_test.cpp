#include "hub_profile.h"
#include <cassert>
#include <iostream>
int main(){
 using namespace sparkbot;
 assert(ValidDeviceName("小白 · 一号"));
 assert(!ValidDeviceName("")); assert(!ValidDeviceName("   "));
 assert(!ValidDeviceName(std::string(49,'x')));
 assert(!ValidDeviceName("a\nb")); assert(!ValidDeviceName(std::string("\xc0\xaf",2)));
 assert(!ValidDeviceName(std::string("\xed\xa0\x80",3)));
 assert(ValidColor("#00aAFF"));assert(!ValidColor("red"));assert(!ValidColor("#ffffff;"));
 assert(!ValidColor("#fffffg"));
 std::cout << "Profile name / UTF-8 / color validation PASS\n";
}
