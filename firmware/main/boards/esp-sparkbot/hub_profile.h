#pragma once
#include <string>
#include <cstdint>
namespace sparkbot {
inline bool ValidChassis(const std::string& value) {
 return value=="tracks"||value=="wheels"||value=="quadruped"||value=="none";
}
inline bool ValidColor(const std::string& s) {
 if(s.size()!=7||s[0]!='#')return false;
 for(size_t i=1;i<s.size();++i)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')||(s[i]>='A'&&s[i]<='F')))return false;
 return true;
}
inline bool ValidDeviceName(const std::string& s) {
 if(s.empty()||s.size()>48||s.front()==' '||s.back()==' ')return false;
 for(size_t i=0;i<s.size();) {
  unsigned char c=s[i++];uint32_t cp=c;int count=0;
  if(c<0x20||c==0x7f)return false;
  if(c>=0xc2&&c<=0xdf){cp=c&31;count=1;}
  else if(c>=0xe0&&c<=0xef){cp=c&15;count=2;}
  else if(c>=0xf0&&c<=0xf4){cp=c&7;count=3;}
  else if(c>=0x80)return false;
  if(i+count>s.size())return false;
  for(int j=0;j<count;++j){unsigned char n=s[i++];if((n&0xc0)!=0x80)return false;cp=(cp<<6)|(n&63);}
  if((count==2&&cp<0x800)||(count==3&&cp<0x10000)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)||(cp>=0x80&&cp<=0x9f))return false;
 }
 return true;
}
}
