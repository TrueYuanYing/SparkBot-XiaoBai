#pragma once
#include <string>
#include <cstdint>
#include <cmath>
#include <cstdlib>
namespace sparkbot {
// Echo has no delimiter and UART reads may split anywhere. Accept only complete
// x[+-]d.ddd y[+-]d.ddd frames within the permitted control range.
class BaseEcho {
 std::string bytes_;
 static bool Number(const std::string& s,size_t& p){
  size_t start=p;if(p<s.size()&&(s[p]=='-'||s[p]=='+'))++p;
  if(p+5>s.size()||s[p]<'0'||s[p]>'1'||s[p+1]!='.')return false;
  for(int i=2;i<5;++i)if(s[p+i]<'0'||s[p+i]>'9')return false;
  p+=5;return std::abs(std::strtof(s.c_str()+start,nullptr))<=1.f;
 }
public:
 bool Feed(const char* data,size_t count){
  bytes_.append(data,count);if(bytes_.size()>192)bytes_.erase(0,bytes_.size()-192);
  bool found=false;size_t consumed=0;
  for(size_t start=0;start<bytes_.size();++start){
   if(bytes_[start]!='x')continue;
   size_t p=start+1;
   if(!Number(bytes_,p)||p+2>bytes_.size()||bytes_.compare(p,2," y")!=0)continue;
   p+=2;if(!Number(bytes_,p))continue;found=true;consumed=p;start=p-1;
  }
  if(consumed)bytes_.erase(0,consumed);
  return found;
 }
};
class DockPresence {
 int hits_=0;int64_t last_=-1;bool connected_=false;
public:
 void Reply(int64_t now){if(last_<0||now-last_>750)hits_=0;last_=now;if(++hits_>=3)connected_=true;}
 bool Connected(int64_t now){if(last_<0||now-last_>5000){connected_=false;hits_=0;}return connected_;}
};
}
