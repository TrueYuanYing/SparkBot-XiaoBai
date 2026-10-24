#pragma once
#include <lvgl.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
#include "hub_core.h"
#include "hub_input.h"
#include "hub_voyage.h"
#include "hub_dock.h"
#include "hub_profile.h"

#define CONFIG_USE_WECHAT_MESSAGE_STYLE 0
extern "C" {
#include <font_awesome.h>
#include <font_emoji.h>
LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);
LV_FONT_DECLARE(font_awesome_30_4);
}
struct cJSON{int valueint=0,type=0;double valuedouble=0;std::map<std::string,cJSON*>items;};
inline cJSON* cJSON_CreateObject(){return new cJSON;}
inline void cJSON_AddNumberToObject(cJSON*j,const char*k,double v){auto n=new cJSON;n->type=1;n->valueint=int(v);n->valuedouble=v;j->items[k]=n;}
inline void cJSON_AddBoolToObject(cJSON*j,const char*k,bool v){cJSON_AddNumberToObject(j,k,v);j->items[k]->type=2;}
inline void cJSON_AddStringToObject(cJSON*,const char*,const char*){}
inline cJSON* cJSON_GetObjectItem(const cJSON*j,const char*k){auto i=j->items.find(k);return i==j->items.end()?nullptr:i->second;}
inline cJSON* cJSON_GetObjectItemCaseSensitive(const cJSON*j,const char*k){return cJSON_GetObjectItem(j,k);}
inline bool cJSON_IsNumber(const cJSON*j){return j&&j->type==1;}
inline bool cJSON_IsBool(const cJSON*j){return j&&j->type==2;}
inline bool cJSON_IsTrue(const cJSON*j){return j&&j->valueint!=0;}
inline void cJSON_Delete(cJSON*j){for(auto&item:j->items)cJSON_Delete(item.second);delete j;}
class Settings{public:Settings(const char*,bool=false){} int GetInt(const char*,int d=0){return d;}bool GetBool(const char*,bool d=false){return d;}std::string GetString(const std::string&,const std::string&d=""){return d;}void SetInt(const char*,int){}void SetBool(const char*,bool){}void SetString(const std::string&,const std::string&){};};
enum DeviceState{kDeviceStateIdle,kDeviceStateConnecting,kDeviceStateListening,kDeviceStateSpeaking,kDeviceStateActivating,kDeviceStateFatalError};
class Application{public:DeviceState state=kDeviceStateIdle;static Application&GetInstance(){static Application a;return a;}DeviceState GetDeviceState(){return state;}};
class Display{public:virtual ~Display()=default;virtual void SetChatMessage(const char*,const char*){}virtual void SetStatus(const char*){}virtual void SetEmotion(const char*){}virtual void ShowNotification(const char*,int=3000){};};
struct DisplayLockGuard{explicit DisplayLockGuard(Display*){}};
namespace Lang{namespace Strings{inline const char*INITIALIZING="待命";inline const char*BATTERY_NEED_CHARGE="请充电";}}
struct DisplayFonts{const lv_font_t*text_font=&font_puhui_20_4,*icon_font=&font_awesome_20_4,*emoji_font=nullptr;};
struct ThemeColors{lv_color_t background=lv_color_white(),text=lv_color_black(),chat_background=lv_color_hex(0xe0e0e0),border=lv_color_hex(0xe0e0e0),low_battery=lv_color_black();};
class SpiLcdDisplay:public Display{public:
 int width_=240,height_=240;DisplayFonts fonts_;ThemeColors current_theme_;
 lv_obj_t*container_=nullptr,*status_bar_=nullptr,*content_=nullptr,*emotion_label_=nullptr,*preview_image_=nullptr,*chat_message_label_=nullptr,*network_label_=nullptr,*notification_label_=nullptr,*status_label_=nullptr,*mute_label_=nullptr,*battery_label_=nullptr,*low_battery_popup_=nullptr,*low_battery_label_=nullptr;
 SpiLcdDisplay(){fonts_.emoji_font=font_emoji_64_init();}
 void SetupUI();void SetEmotion(const char*)override;
 void SetChatMessage(const char*,const char*text)override{if(chat_message_label_)lv_label_set_text(chat_message_label_,text);}
 void SetStatus(const char*s)override{if(status_label_)lv_label_set_text(status_label_,s);}
 void ShowNotification(const char*,int=3000)override{}
 void SetIcon(const char*s){if(emotion_label_)lv_label_set_text(emotion_label_,s);}
};
class HubCamera{public:std::vector<uint8_t>pixels;lv_image_dsc_t preview{};bool Ready()const{return true;}const lv_image_dsc_t*Preview(){return &preview;}};
struct HubImu{void Init(void*){}};struct HubTouch{};struct HubImuReading{float x=0,y=0,z=1;};
using i2c_master_bus_handle_t=void*;
constexpr int ESP_MAC_WIFI_STA=0;
inline void esp_read_mac(uint8_t*p,int){memset(p,0,6);}
inline uint32_t esp_random(){static uint32_t s=1234567;s^=s<<13;s^=s>>17;s^=s<<5;return s;}
inline int64_t Now(){return lv_tick_get();}
inline time_t demo_clock(){return time_t(1792850400)+lv_tick_get()/1000;}
#ifdef _WIN32
inline tm* localtime_r(const time_t*t,tm*out){localtime_s(out,t);return out;}
#endif
inline std::string HotspotName(){return "SparkBot-DEMO";}
struct WifiStation{static WifiStation&GetInstance(){static WifiStation w;return w;}bool IsConnected(){return false;}std::string GetIpAddress(){return "192.168.1.100";}};
struct Codec{int output_volume(){return 50;}};
struct Board{static Board&GetInstance(){static Board b;return b;}Codec*GetAudioCodec(){static Codec c;return &c;}};
inline std::vector<uint8_t>muyu_bytes,icon_bytes;
inline const uint8_t*muyu_img_start=nullptr,*app_icons_start=nullptr;
