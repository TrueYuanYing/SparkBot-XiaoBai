#include "sparkbot_hub.h"
#include "application.h"
#include "board.h"
#include "config.h"
#include "sparkbot_display.h"
#include "audio_codec.h"
#include "hub_profile.h"
#include "settings.h"
#include <esp_mac.h>
#include <algorithm>
#include <esp_random.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <wifi_station.h>
#include <driver/uart.h>
#include <esp_wifi.h>
#include <ctime>
#include <cstring>

LV_FONT_DECLARE(font_puhui_20_4);
extern const uint8_t muyu_pcm_start[] asm("_binary_muyu_pcm_start");
extern const uint8_t muyu_pcm_end[] asm("_binary_muyu_pcm_end");
extern const uint8_t muyu_img_start[] asm("_binary_muyu_rgb565a8_start");
extern const uint8_t app_icons_start[] asm("_binary_app_icons_rgb565a8_start");
static int64_t Now() { return esp_timer_get_time()/1000; }
static void EnterMotion(lv_obj_t* obj,int distance=14) {
    lv_anim_t move;lv_anim_init(&move);lv_anim_set_var(&move,obj);lv_anim_set_values(&move,distance,0);
    lv_anim_set_duration(&move,220);lv_anim_set_path_cb(&move,lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&move,[](void* p,int32_t v){lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(p),v,0);});lv_anim_start(&move);
    lv_anim_t fade;lv_anim_init(&fade);lv_anim_set_var(&fade,obj);lv_anim_set_values(&fade,70,255);lv_anim_set_duration(&fade,180);
    lv_anim_set_exec_cb(&fade,[](void* p,int32_t v){lv_obj_set_style_opa(static_cast<lv_obj_t*>(p),v,0);});lv_anim_start(&fade);
}
static const char* app_names[]{"chat","muyu","dice","game","camera","clock","voyage","settings","menu"};
static const char* app_titles[]{"小白","敲木鱼","摇骰子","2048","摄像头","时钟","海上航行","设置","应用选择"};
static std::string HotspotName() {uint8_t mac[6]{};esp_wifi_get_mac(WIFI_IF_AP,mac);char name[24];snprintf(name,sizeof(name),"SparkBot-%02X%02X",mac[4],mac[5]);return name;}

SparkbotHub::SparkbotHub(Display* display, HubCamera* camera, i2c_master_bus_handle_t bus)
    :display_(display),camera_(camera) {
    uint8_t mac[6]{};esp_read_mac(mac,ESP_MAC_WIFI_STA);char id[13];
    snprintf(id,sizeof(id),"%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);device_id_=id;
    Settings saved("hub_profile",true);
    // The repaired FPC should not inherit the old diagnostic touch/page locks.
    // Once migrated, future user choices are retained across every reboot.
    if(saved.GetInt("touch_v9",0)==0){saved.SetBool("touch",true);saved.SetBool("auto_hold",false);saved.SetBool("home_hold",false);saved.SetInt("touch_v9",1);}
    auto name=saved.GetString("name","小白");if(sparkbot::ValidDeviceName(name))device_name_=name;
    for(size_t i=0;i<colors_.size();++i){auto value=saved.GetString("color"+std::to_string(i),colors_[i]);if(sparkbot::ValidColor(value))colors_[i]=value;}
    chassis_=saved.GetString("chassis","tracks");if(!sparkbot::ValidChassis(chassis_))chassis_="tracks";
    auto_hold_=saved.GetBool("auto_hold",true);manual_hold_=saved.GetBool("home_hold",false);
    imu_.Init(bus);
    game_.Spawn(esp_random()); game_.Spawn(esp_random());
    muyu_image_.header.magic=LV_IMAGE_HEADER_MAGIC;
    muyu_image_.header.cf=LV_COLOR_FORMAT_RGB565A8;
    muyu_image_.header.w=256; muyu_image_.header.h=256; muyu_image_.header.stride=512;
    muyu_image_.data_size=256*256*3; muyu_image_.data=muyu_img_start;
    for(size_t i=0;i<app_images_.size();++i) {
        auto& image=app_images_[i];
        image.header.magic=LV_IMAGE_HEADER_MAGIC;image.header.cf=LV_COLOR_FORMAT_RGB565A8;
        image.header.w=80;image.header.h=80;image.header.stride=160;
        image.data_size=80*80*3;image.data=app_icons_start+i*image.data_size;
    }
}
void SparkbotHub::Start() {
    if (started_.exchange(true)) return;
    { DisplayLockGuard lock(display_); chat_screen_=lv_screen_active(); }
    touch_.Init();
    BaseType_t motor=xTaskCreate([](void* p){static_cast<SparkbotHub*>(p)->MotorLoop();},"hub_motor",3072,this,6,nullptr);
    BaseType_t apps=xTaskCreate([](void* p){static_cast<SparkbotHub*>(p)->AppLoop();},"hub_apps",8192,this,2,nullptr);
    BaseType_t camera=xTaskCreate([](void* p){static_cast<SparkbotHub*>(p)->CameraLoop();},"hub_preview",6144,this,1,nullptr);
    ESP_ERROR_CHECK(motor==pdPASS && apps==pdPASS && camera==pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
bool SparkbotHub::Move(uint32_t owner,uint32_t seq,float x,float y,int duration) {
    std::lock_guard<std::mutex> lock(motion_mutex_);
    if (dance_until_ > Now()) return false;
    auto drive=sparkbot::RemoteDrive(x,y);
    return motion_.Move(owner,seq,drive.x,drive.y,Now(),duration);
}
void SparkbotHub::Stop(bool emergency) {
    std::lock_guard<std::mutex> lock(motion_mutex_);
    dance_until_=0;
    if (emergency) motion_.EmergencyStop(); else motion_.Stop();
}
void SparkbotHub::Arm() { std::lock_guard<std::mutex> lock(motion_mutex_); dance_until_=0; motion_.Arm(); }
void SparkbotHub::Disconnect(uint32_t owner) {
    std::lock_guard<std::mutex> lock(motion_mutex_);
    dance_until_=0;
    motion_.Disconnect(owner);
}
bool SparkbotHub::VoiceMove(float x,float y,int duration) {
    std::lock_guard<std::mutex> lock(motion_mutex_);
    dance_until_=0;
    return motion_.Move(0xffffffffu,++voice_seq_,x,y,Now(),duration);
}
bool SparkbotHub::Dance() {
    std::lock_guard<std::mutex> lock(motion_mutex_);
    auto v=motion_.At(Now());
    if (motion_.IsEmergency() || v.x || v.y) return false;
    motion_.Stop(); dance_until_=Now()+3000; pending_light_=7; return true;
}
bool SparkbotHub::SetLight(int mode) {
    if (mode<2 || mode>8) return false;
    std::lock_guard<std::mutex> lock(motion_mutex_); light_=mode; pending_light_=mode; return true;
}
void SparkbotHub::MotorLoop() {
    // Never use the base's blocking d1 dance: timed x/y commands remain interruptible.
    while(true) {
        {
            std::lock_guard<std::mutex> lock(motion_mutex_);
            int64_t now=Now();
            auto v=motion_.At(now);
            if (dance_until_>now && !motion_.IsEmergency())
                v={((dance_until_-now)/300)%2 ? 0.55f : -0.55f,0.f};
            char command[48];
            if (pending_light_>=0) {
                snprintf(command,sizeof(command),"w%d",pending_light_); pending_light_=-1;
            } else snprintf(command,sizeof(command),"x%.3f y%.3f",v.x,v.y);
            uart_write_bytes(ECHO_UART_PORT_NUM,command,strlen(command));
            uint8_t echo[96]; int count=uart_read_bytes(ECHO_UART_PORT_NUM,echo,sizeof(echo),0);
            if(count>0) {
                if(base_echo_.Feed(reinterpret_cast<char*>(echo),count)){last_base_reply_=now;dock_.Reply(now);}
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
void SparkbotHub::SetApp(HubApp app) {
    if (static_cast<int>(app)<0 || static_cast<int>(app)>8) return;
    std::lock_guard<std::mutex> lock(mutex_);
    if(home_hold_ && app!=HubApp::Chat)return;
    Stop();
    if(app!=HubApp::Menu && app!=HubApp::Chat) selected_=static_cast<int>(app);
    if(app==HubApp::Game || app==HubApp::Balance){neutral_x_=last_reading_.x;neutral_y_=last_reading_.y;}
    app_=app; dirty_=true;
}
void SparkbotHub::UpdateDock() {
    bool connected;{std::lock_guard<std::mutex> lock(motion_mutex_);connected=dock_.Connected(Now());}
    std::lock_guard<std::mutex> lock(mutex_);
    const bool hold=manual_hold_||(auto_hold_&&connected);
    if((connected&&!dock_connected_)||(hold&&!home_hold_)){popup_pending_=true;dirty_=true;}
    dock_connected_=connected;
    if(hold!=home_hold_){
        home_hold_=hold;
        static_cast<SparkbotDisplay*>(display_)->HoldEyes(hold);
        if(hold){Stop();app_=HubApp::Chat;}dirty_=true;
        ESP_LOGI("HubDock","connected=%d home_hold=%d",connected,hold);
    }
}
void SparkbotHub::NextApp() {
    Navigate(1);
}
void SparkbotHub::Navigate(int delta) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(app_!=HubApp::Menu)return;
    Stop(); selected_=(selected_-1+delta+7)%7+1;
    dirty_=true;
}
void SparkbotHub::Click() {
    HubApp app; int selected; { std::lock_guard<std::mutex> lock(mutex_); app=app_;selected=selected_; }
    if(app==HubApp::Menu) {SetApp(static_cast<HubApp>(selected));return;}
    if (app==HubApp::Muyu) Knock();
    else if (app==HubApp::Dice) Roll();
    else if (app==HubApp::Game) {
        bool over;{std::lock_guard<std::mutex> lock(mutex_);over=game_.GameOver();}
        if(over)NewGame();
    }
    else if(app==HubApp::Balance){std::lock_guard<std::mutex> lock(mutex_);voyage_.Toggle();dirty_=true;}
}
void SparkbotHub::HandleInput(sparkbot::Input input) {
    using sparkbot::Input;
    if(input==Input::None)return;
    HubApp app;
    {std::lock_guard<std::mutex> lock(mutex_);if(home_hold_)return;app=app_;++input_count_;last_input_=input;}
    ESP_LOGI("HubInput","gesture=%d app=%s",static_cast<int>(input),app_names[static_cast<int>(app)]);
    using sparkbot::Navigation;
    switch(sparkbot::ResolveNavigation(input,app==HubApp::Menu)) {
        case Navigation::Home: SetApp(HubApp::Chat);return;
        case Navigation::Menu: SetApp(HubApp::Menu);return;
        case Navigation::Previous: Navigate(-1);return;
        case Navigation::Next: Navigate(1);return;
        case Navigation::Enter: Click();return;
        case Navigation::None: break;
    }
    if(app==HubApp::Settings && (input==Input::Previous || input==Input::Next)) {
        auto codec=Board::GetInstance().GetAudioCodec();
        codec->SetOutputVolume(std::clamp(codec->output_volume()+(input==Input::Previous?-10:10),0,100));
        std::lock_guard<std::mutex> lock(mutex_);dirty_=true;return;
    }
    if(app==HubApp::Balance && (input==Input::LeftClick || input==Input::RightClick || input==Input::Previous || input==Input::Next)) {
        std::lock_guard<std::mutex> lock(mutex_);voyage_.Steer(input==Input::LeftClick||input==Input::Previous?-1:1);dirty_=true;return;
    }
    if(input==Input::Previous || input==Input::Next)return;
    if(app==HubApp::Settings) {
        auto display=static_cast<SparkbotDisplay*>(display_);
        auto config=cJSON_CreateObject();display->AddSettings(config);
        int style=cJSON_GetObjectItem(config,"screen_style")->valueint;
        bool subtitles=cJSON_IsTrue(cJSON_GetObjectItem(config,"subtitles"));
        cJSON_AddNumberToObject(config,"style",input==Input::RightClick?style:(style+1)%3);
        if(input==Input::RightClick)cJSON_ReplaceItemInObject(config,"subtitles",cJSON_CreateBool(!subtitles));
        display->Configure(config);cJSON_Delete(config);
        std::lock_guard<std::mutex> lock(mutex_);dirty_=true;
    } else Click();
}
void SparkbotHub::Knock() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (Now()-last_hit_<180) return;
        ++count_; last_hit_=Now(); dirty_=true;
    }
    auto& app=Application::GetInstance();
    if (app.GetDeviceState()!=kDeviceStateSpeaking) {
        size_t count=(muyu_pcm_end-muyu_pcm_start)/2;
        std::vector<int16_t> pcm(count); memcpy(pcm.data(),muyu_pcm_start,count*2);
        app.GetAudioService().PlayLocalPcm(std::move(pcm));
    }
}
void SparkbotHub::Roll() { std::lock_guard<std::mutex> lock(mutex_); dice_until_=Now()+700; dirty_=true; }
void SparkbotHub::GameMove(sparkbot::Direction direction) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (game_.Slide(direction)) {game_.Spawn(esp_random()); dirty_=true;}
}
void SparkbotHub::NewGame() {
    std::lock_guard<std::mutex> lock(mutex_);
    game_.SetBoard({}); game_.Spawn(esp_random()); game_.Spawn(esp_random()); dirty_=true;
}
static lv_obj_t* Label(lv_obj_t* parent, const char* text, int x,int y,const lv_font_t* font=&font_puhui_20_4) {
    auto obj=lv_label_create(parent); lv_label_set_text(obj,text); lv_obj_set_style_text_font(obj,font,0);
    lv_obj_set_style_text_color(obj,lv_color_hex(0xf0eee7),0); lv_obj_align(obj,LV_ALIGN_TOP_MID,x,y); return obj;
}
void SparkbotHub::CreateScreen(HubApp app) {
    auto previous=rendered_app_;screen_=nullptr;
    rendered_app_=app;shown_selection_=-1;tiles_.fill(nullptr);pips_.fill(nullptr);menu_dots_.fill(nullptr);picture_=nullptr;
    if(app==HubApp::Chat) {lv_screen_load_anim(chat_screen_,LV_SCR_LOAD_ANIM_MOVE_BOTTOM,180,0,previous!=HubApp::Chat);return;}
    screen_=lv_obj_create(nullptr); lv_obj_remove_flag(screen_,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen_,lv_color_black(),0);
    title_=Label(screen_,app_titles[static_cast<int>(app)],0,7);
    value_=Label(screen_,"",0,43,&lv_font_montserrat_32);
    hint_=Label(screen_,"右侧三击回主页",0,213);
    lv_obj_set_style_text_color(title_,lv_color_hex(0xaeb4bf),0);
    lv_obj_set_style_text_color(hint_,lv_color_hex(0xaeb4bf),0);
    if(app==HubApp::Menu) {
        lv_label_set_text(title_,"应用");
        auto card=lv_obj_create(screen_);lv_obj_set_size(card,204,148);lv_obj_set_pos(card,18,38);
        lv_obj_set_style_bg_color(card,lv_color_hex(0x111b25),0);lv_obj_set_style_border_width(card,1,0);lv_obj_set_style_border_color(card,lv_color_hex(0x344a5c),0);
        lv_obj_set_style_radius(card,28,0);lv_obj_set_style_pad_all(card,0,0);lv_obj_remove_flag(card,LV_OBJ_FLAG_SCROLLABLE);
        picture_=lv_image_create(card);lv_obj_align(picture_,LV_ALIGN_TOP_MID,0,10);
        pips_[0]=lv_obj_create(card);lv_obj_set_size(pips_[0],80,80);lv_obj_align(pips_[0],LV_ALIGN_TOP_MID,0,10);
        lv_obj_set_style_bg_color(pips_[0],lv_color_hex(0x087da7),0);lv_obj_set_style_pad_all(pips_[0],0,0);lv_obj_set_style_radius(pips_[0],22,0);lv_obj_set_style_border_width(pips_[0],0,0);
        auto hull=lv_obj_create(pips_[0]);lv_obj_set_size(hull,50,15);lv_obj_set_pos(hull,15,48);lv_obj_set_style_radius(hull,9,0);lv_obj_set_style_bg_color(hull,lv_color_hex(0xffdd83),0);lv_obj_set_style_border_width(hull,0,0);
        auto sail=lv_obj_create(pips_[0]);lv_obj_set_size(sail,25,33);lv_obj_set_pos(sail,26,13);lv_obj_set_style_radius(sail,5,0);lv_obj_set_style_bg_color(sail,lv_color_hex(0xfffaf1),0);lv_obj_set_style_border_width(sail,0,0);
        auto wave=lv_obj_create(pips_[0]);lv_obj_set_size(wave,55,4);lv_obj_set_pos(wave,12,68);lv_obj_set_style_bg_color(wave,lv_color_hex(0x71cadd),0);lv_obj_set_style_border_width(wave,0,0);
        lv_obj_delete(value_);value_=Label(card,"",0,105);
        for(int i=0;i<7;++i){auto dot=menu_dots_[i]=lv_obj_create(screen_);lv_obj_remove_style_all(dot);lv_obj_set_size(dot,5,5);lv_obj_set_pos(dot,79+i*13,198);lv_obj_set_style_radius(dot,3,0);lv_obj_set_style_bg_opa(dot,255,0);}
        lv_label_set_text(hint_,"单击切换 · 双击进入");
    } else if(app==HubApp::Settings) {
        lv_obj_delete(value_);value_=Label(screen_,"",0,34,&lv_font_montserrat_14);
        lv_obj_set_width(value_,228);lv_obj_set_style_text_align(value_,LV_TEXT_ALIGN_CENTER,0);
        picture_=Label(screen_,"",0,112);lv_obj_set_style_text_color(picture_,lv_color_hex(0x81ead3),0);
        lv_obj_set_style_text_align(picture_,LV_TEXT_ALIGN_CENTER,0);
        lv_obj_align(hint_,LV_ALIGN_TOP_MID,0,191);lv_obj_set_style_text_align(hint_,LV_TEXT_ALIGN_CENTER,0);
        lv_label_set_text(hint_,"双击左减 / 右加音量\n单击左样式 / 右字幕");
    }
    if(app==HubApp::Muyu) {
        lv_obj_set_style_text_font(value_,&font_puhui_20_4,0);lv_obj_align(value_,LV_ALIGN_TOP_MID,0,37);
        picture_=lv_image_create(screen_);lv_image_set_src(picture_,&muyu_image_);
        lv_image_set_scale(picture_,128);lv_obj_align(picture_,LV_ALIGN_CENTER,0,17);
    } else if(app==HubApp::Dice) {
        auto face=lv_obj_create(screen_);lv_obj_set_size(face,132,132);lv_obj_align(face,LV_ALIGN_CENTER,0,5);
        lv_obj_set_style_bg_color(face,lv_color_hex(0xf2ebdc),0);lv_obj_set_style_radius(face,20,0);
        lv_obj_set_style_pad_all(face,0,0);lv_obj_set_style_border_width(face,0,0);
        const int coords[7][2]={{24,24},{90,24},{24,57},{90,57},{24,90},{90,90},{57,57}};
        for(int i=0;i<7;++i){pips_[i]=lv_obj_create(face);lv_obj_set_size(pips_[i],18,18);
            lv_obj_set_pos(pips_[i],coords[i][0],coords[i][1]);lv_obj_set_style_radius(pips_[i],9,0);
            lv_obj_set_style_bg_color(pips_[i],lv_color_hex(0x193d3e),0);lv_obj_set_style_border_width(pips_[i],0,0);}
        lv_obj_add_flag(value_,LV_OBJ_FLAG_HIDDEN);
    } else if(app==HubApp::Game) {
        lv_obj_align(value_,LV_ALIGN_TOP_RIGHT,-10,10);lv_obj_set_style_text_font(value_,&lv_font_montserrat_14,0);
        lv_obj_align(title_,LV_ALIGN_TOP_LEFT,10,7);
        for(int i=0;i<16;++i){auto tile=lv_obj_create(screen_);lv_obj_set_size(tile,49,39);
            lv_obj_set_pos(tile,15+(i%4)*54,43+(i/4)*41);lv_obj_set_style_pad_all(tile,0,0);
            lv_obj_set_style_border_width(tile,0,0);lv_obj_set_style_radius(tile,6,0);
            tiles_[i]=lv_label_create(tile);lv_obj_set_style_text_font(tiles_[i],&lv_font_montserrat_14,0);lv_obj_center(tiles_[i]);}
    } else if(app==HubApp::Camera) {
        picture_=lv_image_create(screen_);lv_image_set_src(picture_,camera_->Preview());
        lv_image_set_scale(picture_,176);lv_obj_align(picture_,LV_ALIGN_CENTER,0,0);
        lv_obj_add_flag(value_,LV_OBJ_FLAG_HIDDEN);
    } else if(app==HubApp::Clock) {lv_obj_align(value_,LV_ALIGN_CENTER,0,-8);}
    else if(app==HubApp::Balance) {
        lv_obj_align(value_,LV_ALIGN_TOP_RIGHT,-10,8);lv_obj_set_style_text_font(value_,&lv_font_montserrat_14,0);
        lv_obj_align(title_,LV_ALIGN_TOP_LEFT,10,5);
        auto sea=lv_obj_create(screen_);lv_obj_set_size(sea,220,171);lv_obj_set_pos(sea,10,36);
        lv_obj_set_style_pad_all(sea,0,0);lv_obj_set_style_radius(sea,20,0);lv_obj_set_style_border_width(sea,0,0);
        lv_obj_set_style_bg_color(sea,lv_color_hex(0x064b72),0);lv_obj_remove_flag(sea,LV_OBJ_FLAG_SCROLLABLE);
        for(int i=1;i<3;++i){auto line=lv_obj_create(sea);lv_obj_set_size(line,1,171);lv_obj_set_pos(line,73*i,0);lv_obj_set_style_bg_color(line,lv_color_hex(0x227c94),0);lv_obj_set_style_border_width(line,0,0);}
        // Ship, reef, coin: all colours remain legible on the blue sea.
        for(int i=0;i<3;++i){pips_[i]=lv_obj_create(sea);lv_obj_set_size(pips_[i],i==0?28:24,i==0?34:24);lv_obj_set_style_pad_all(pips_[i],0,0);
            lv_obj_set_style_radius(pips_[i],i==0?12:i==1?6:12,0);lv_obj_set_style_border_width(pips_[i],i==2?3:0,0);
            lv_obj_set_style_border_color(pips_[i],lv_color_hex(0xffdc6e),0);lv_obj_set_style_bg_color(pips_[i],lv_color_hex(i==0?0xf5faf7:i==1?0xe97565:0xffb72c),0);}
        auto cabin=lv_obj_create(pips_[0]);lv_obj_set_size(cabin,14,13);lv_obj_set_pos(cabin,7,9);lv_obj_set_style_bg_color(cabin,lv_color_hex(0x32acc1),0);lv_obj_set_style_border_width(cabin,0,0);
    }
    auto motion=previous==HubApp::Chat?LV_SCR_LOAD_ANIM_MOVE_TOP:app==HubApp::Menu?LV_SCR_LOAD_ANIM_MOVE_RIGHT:LV_SCR_LOAD_ANIM_MOVE_LEFT;
    lv_screen_load_anim(screen_,motion,180,0,previous!=HubApp::Chat);
}
void SparkbotHub::Render() {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!dirty_ && !dock_popup_ && !popup_pending_ && app_!=HubApp::Clock && app_!=HubApp::Camera && app_!=HubApp::Dice && app_!=HubApp::Muyu && app_!=HubApp::Settings && app_!=HubApp::Balance)return;
    const int period=app_==HubApp::Balance?40:app_==HubApp::Camera?200:app_==HubApp::Clock||app_==HubApp::Settings?500:40;
    if(!dirty_ && !popup_pending_ && Now()-last_render_at_<period)return;
    last_render_at_=Now();
    DisplayLockGuard display_lock(display_);
    if(app_!=rendered_app_)CreateScreen(app_);
    dirty_=false;
    if(dock_popup_&&Now()>popup_until_){lv_obj_delete(dock_popup_);dock_popup_=nullptr;}
    if(dock_popup_&&Now()>popup_until_-300)lv_obj_set_style_opa(dock_popup_,std::max<int64_t>(0,(popup_until_-Now())*255/300),0);
    if(popup_pending_){
        popup_pending_=false;if(dock_popup_)lv_obj_delete(dock_popup_);
        dock_popup_=lv_obj_create(lv_layer_top());lv_obj_set_size(dock_popup_,208,74);lv_obj_align(dock_popup_,LV_ALIGN_BOTTOM_MID,0,-18);
        lv_obj_set_style_radius(dock_popup_,20,0);lv_obj_set_style_bg_color(dock_popup_,lv_color_hex(0x174b47),0);
        lv_obj_set_style_border_width(dock_popup_,1,0);lv_obj_set_style_border_color(dock_popup_,lv_color_hex(0x6ae6c2),0);lv_obj_set_style_pad_all(dock_popup_,0,0);
        Label(dock_popup_,dock_connected_?"底座已连接":"双眼保持已开启",0,10);
        Label(dock_popup_,home_hold_?"安心相伴 · 语音可用":"遥控已就绪",0,39);EnterMotion(dock_popup_,24);popup_until_=Now()+2200;
    }
    if(app_==HubApp::Chat)return;
    if(app_==HubApp::Menu) {
        lv_image_set_src(picture_,&app_images_[selected_]);lv_obj_align(picture_,LV_ALIGN_TOP_MID,0,10);
        if(selected_==6){lv_obj_add_flag(picture_,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(pips_[0],LV_OBJ_FLAG_HIDDEN);}
        else {lv_obj_remove_flag(picture_,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(pips_[0],LV_OBJ_FLAG_HIDDEN);}
        lv_label_set_text(value_,app_titles[selected_]);
        if(shown_selection_!=selected_){
            auto card=lv_obj_get_parent(picture_);int direction=shown_selection_<0?1:selected_>shown_selection_?1:-1;
            lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,card);lv_anim_set_values(&a,direction*18,0);lv_anim_set_duration(&a,180);lv_anim_set_path_cb(&a,lv_anim_path_ease_out);
            lv_anim_set_exec_cb(&a,[](void* p,int32_t v){lv_obj_set_style_translate_x(static_cast<lv_obj_t*>(p),v,0);});lv_anim_start(&a);
            for(int i=0;i<7;++i){bool selected=i+1==selected_;lv_obj_set_size(menu_dots_[i],selected?11:5,5);lv_obj_set_style_bg_color(menu_dots_[i],lv_color_hex(selected?0x93e6d0:0x344453),0);}
            shown_selection_=selected_;
        }
    }else if(app_==HubApp::Settings) {
        auto config=cJSON_CreateObject();static_cast<SparkbotDisplay*>(display_)->AddSettings(config);
        const char* styles[]{"灵动双眼","机甲双眼","经典小智"};
        auto& station=WifiStation::GetInstance();
        std::string local=station.IsConnected()?"http://"+station.GetIpAddress():"offline";
        lv_label_set_text_fmt(value_,"Wi-Fi: %s\nPassword: sparkbot2026\nAP: http://192.168.4.1\nLAN: %s",HotspotName().c_str(),local.c_str());
        lv_label_set_text_fmt(picture_,"音量 %d%%\n%s · 字幕%s",Board::GetInstance().GetAudioCodec()->output_volume(),
            styles[cJSON_GetObjectItem(config,"screen_style")->valueint],cJSON_IsTrue(cJSON_GetObjectItem(config,"subtitles"))?"开":"关");
        cJSON_Delete(config);
    }else if(app_==HubApp::Balance) {
        lv_obj_set_pos(pips_[0],voyage_.Lane()*73+22,122);
        lv_obj_set_pos(pips_[1],voyage_.RockLane()*73+24,int(voyage_.Y()));
        lv_obj_set_pos(pips_[2],voyage_.CoinLane()*73+24,int(voyage_.Y()));
        lv_label_set_text_fmt(value_,"%d / %d HP",voyage_.Score(),voyage_.Lives());
        lv_label_set_text(hint_,voyage_.Over()?"结束 · 中间触摸重来":voyage_.Paused()?"中间触摸开始 / 继续":"左右躲礁 · 收集金币");
    }else if(app_==HubApp::Muyu){lv_label_set_text_fmt(value_,"%lu",static_cast<unsigned long>(count_));
        lv_image_set_scale(picture_,Now()-last_hit_<150?119:128);}
    else if(app_==HubApp::Dice){
        if(Now()<dice_until_) dice_=1+esp_random()%6;
        const uint8_t masks[]{0,0x40,0x21,0x61,0x33,0x73,0x3f};
        for(int i=0;i<7;++i){if(masks[dice_]&(1<<i))lv_obj_remove_flag(pips_[i],LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(pips_[i],LV_OBJ_FLAG_HIDDEN);}
    }else if(app_==HubApp::Game){
        lv_label_set_text_fmt(value_,"%lu",static_cast<unsigned long>(game_.Score()));
        for(int i=0;i<16;++i){auto v=game_.Cells()[i];
            if(v)lv_label_set_text_fmt(tiles_[i],"%lu",static_cast<unsigned long>(v));else lv_label_set_text(tiles_[i],"");
            lv_obj_set_style_bg_color(lv_obj_get_parent(tiles_[i]),lv_color_hex(v?0xedd7a4:0x2a4246),0);
            lv_obj_set_style_text_color(tiles_[i],lv_color_hex(0x193d3e),0);lv_obj_center(tiles_[i]);}
        lv_label_set_text(hint_,game_.GameOver()?"结束·单击重来":"倾斜或网页操作");
    }else if(app_==HubApp::Camera){lv_obj_invalidate(picture_);lv_label_set_text(hint_,camera_->Ready()?"右侧三击回主页":"摄像头未就绪");}
    else if(app_==HubApp::Clock){
        time_t t=time(nullptr);struct tm tm{};localtime_r(&t,&tm);char text[32];
        if(t>1700000000){strftime(text,sizeof(text),"%H:%M:%S",&tm);lv_label_set_text(value_,text);
            strftime(text,sizeof(text),"%Y / %m / %d",&tm);lv_label_set_text(hint_,text);}
        else{lv_label_set_text(value_,"--:--");lv_label_set_text(hint_,"联网后自动校时");}
    }
}
void SparkbotHub::AppLoop() {
    int64_t gesture_at=0; bool tilt_ready=true; float baseline=1;
    int64_t step_at=Now();
    auto previous_state=Application::GetInstance().GetDeviceState();
    while(true){
        auto state=Application::GetInstance().GetDeviceState();
        auto voice=[](DeviceState s){return s==kDeviceStateConnecting||s==kDeviceStateListening||s==kDeviceStateSpeaking;};
        if(voice(state) && !voice(previous_state))SetApp(HubApp::Chat);
        previous_state=state;
        UpdateDock();
        HandleInput(touch_.Poll(Now()));
        HubApp app;{std::lock_guard<std::mutex> lock(mutex_);app=app_;}
        static_cast<SparkbotDisplay*>(display_)->SetTouchFeedback(app==HubApp::Chat?touch_.Feedback(Now()):sparkbot::TouchFeedback{});
        HubImuReading reading;
        if(imu_.Read(reading)){
            {std::lock_guard<std::mutex> lock(mutex_);last_reading_=reading;++imu_samples_;}
            float norm=sqrtf(reading.x*reading.x+reading.y*reading.y+reading.z*reading.z);
            float impulse=fabsf(norm-baseline);baseline=baseline*0.9f+norm*0.1f;
            if(app==HubApp::Dice && impulse>0.65f && Now()-gesture_at>850){Roll();gesture_at=Now();}
            if(app==HubApp::Muyu && impulse>0.45f && Now()-gesture_at>250){Knock();gesture_at=Now();}
            if(app==HubApp::Game){
                float x,y;{std::lock_guard<std::mutex> lock(mutex_);x=reading.x-neutral_x_;y=reading.y-neutral_y_;}
                if(fabsf(x)<0.12f && fabsf(y)<0.12f)tilt_ready=true;
                if(tilt_ready && Now()-gesture_at>350 && (fabsf(x)>0.25f || fabsf(y)>0.25f)){
                    GameMove(fabsf(x)>fabsf(y)?(x>0?sparkbot::Direction::Right:sparkbot::Direction::Left):
                        (y>0?sparkbot::Direction::Down:sparkbot::Direction::Up));tilt_ready=false;gesture_at=Now();}
            }

        }
        int64_t step_now=Now();float elapsed=std::min<int64_t>(step_now-step_at,100)/1000.f;step_at=step_now;
        if(app==HubApp::Balance){std::lock_guard<std::mutex> lock(mutex_);voyage_.Step(elapsed);}
        Render();vTaskDelay(pdMS_TO_TICKS(20));
    }
}
void SparkbotHub::CameraLoop() {
    while(true) {
        bool preview;{std::lock_guard<std::mutex> lock(mutex_);preview=app_==HubApp::Camera;}
        if(preview) {std::vector<uint8_t> jpeg;camera_->Snapshot(jpeg,true);}
        vTaskDelay(pdMS_TO_TICKS(220));
    }
}
cJSON* SparkbotHub::Status() {
    std::lock_guard<std::mutex> lock(mutex_);
    if(app_==HubApp::Balance&&!voyage_.Paused()&&!voyage_.Over())WifiStation::GetInstance().DeferBackgroundScan();
    std::lock_guard<std::mutex> motion_lock(motion_mutex_);auto root=cJSON_CreateObject();
    cJSON_AddStringToObject(root,"device_id",device_id_.c_str());
    cJSON_AddStringToObject(root,"device_name",device_name_.c_str());
    cJSON_AddStringToObject(root,"chassis",chassis_.c_str());
    cJSON_AddBoolToObject(root,"docked",dock_connected_);cJSON_AddBoolToObject(root,"auto_hold",auto_hold_);
    cJSON_AddBoolToObject(root,"home_hold",home_hold_);cJSON_AddBoolToObject(root,"manual_hold",manual_hold_);
    auto palette=cJSON_AddArrayToObject(root,"colors");for(const auto& color:colors_)cJSON_AddItemToArray(palette,cJSON_CreateString(color.c_str()));
    wifi_ap_record_t ap{};if(esp_wifi_sta_get_ap_info(&ap)==ESP_OK)cJSON_AddNumberToObject(root,"rssi",ap.rssi);
    cJSON_AddStringToObject(root,"app",app_names[static_cast<int>(app_)]);
    cJSON_AddBoolToObject(root,"emergency",motion_.IsEmergency());
    auto v=motion_.At(Now());cJSON_AddNumberToObject(root,"x",v.x);cJSON_AddNumberToObject(root,"y",v.y);
    cJSON_AddBoolToObject(root,"dancing",dance_until_>Now());cJSON_AddNumberToObject(root,"light",light_);
    cJSON_AddBoolToObject(root,"base_link",last_base_reply_>0 && Now()-last_base_reply_<2000);
    cJSON_AddNumberToObject(root,"muyu",count_);cJSON_AddNumberToObject(root,"dice",dice_);
    cJSON_AddNumberToObject(root,"score",game_.Score());cJSON_AddBoolToObject(root,"game_over",game_.GameOver());
    auto cells=cJSON_AddArrayToObject(root,"cells");for(auto v:game_.Cells())cJSON_AddItemToArray(cells,cJSON_CreateNumber(v));
    cJSON_AddBoolToObject(root,"camera",camera_->Ready());cJSON_AddBoolToObject(root,"imu",imu_.Ready());
    cJSON_AddBoolToObject(root,"touch",touch_.Ready());
    cJSON_AddBoolToObject(root,"touch_enabled",touch_.Enabled());
    cJSON_AddBoolToObject(root,"touch_protected",touch_.Protected());
    touch_.AddDiagnostics(root);
    cJSON_AddStringToObject(root,"imu_model",imu_.Name());
    cJSON_AddNumberToObject(root,"imu_samples",imu_samples_);
    auto accel=cJSON_AddArrayToObject(root,"acceleration");
    for(auto v:{last_reading_.x,last_reading_.y,last_reading_.z})cJSON_AddItemToArray(accel,cJSON_CreateNumber(v));
    cJSON_AddNumberToObject(root,"input_count",input_count_);
    cJSON_AddNumberToObject(root,"last_input",static_cast<int>(last_input_));
    cJSON_AddStringToObject(root,"selected_app",app_names[selected_]);
    cJSON_AddStringToObject(root,"control_url","http://192.168.4.1");
    cJSON_AddStringToObject(root,"hotspot",HotspotName().c_str());
    cJSON_AddNumberToObject(root,"volume",Board::GetInstance().GetAudioCodec()->output_volume());
    cJSON_AddBoolToObject(root,"audio_output",Board::GetInstance().GetAudioCodec()->output_enabled());
    cJSON_AddNumberToObject(root,"balance_score",voyage_.Score());
    cJSON_AddNumberToObject(root,"voyage_lane",voyage_.Lane());cJSON_AddNumberToObject(root,"voyage_rock",voyage_.RockLane());
    cJSON_AddNumberToObject(root,"voyage_coin",voyage_.CoinLane());cJSON_AddNumberToObject(root,"voyage_y",voyage_.Y());
    cJSON_AddNumberToObject(root,"voyage_speed",voyage_.Speed());cJSON_AddNumberToObject(root,"voyage_lives",voyage_.Lives());
    cJSON_AddBoolToObject(root,"voyage_over",voyage_.Over());
    cJSON_AddBoolToObject(root,"balance_paused",voyage_.Paused());
    cJSON_AddBoolToObject(root,"wifi",WifiStation::GetInstance().IsConnected());
    cJSON_AddStringToObject(root,"ip",WifiStation::GetInstance().GetIpAddress().c_str());
    cJSON_AddNumberToObject(root,"voice_state",static_cast<int>(Application::GetInstance().GetDeviceState()));
    static_cast<SparkbotDisplay*>(display_)->AddSettings(root);
    return root;
}
bool SparkbotHub::Action(const cJSON* json,std::string& error) {
    auto action=cJSON_GetObjectItemCaseSensitive(json,"action");
    if(!cJSON_IsString(action)){error="缺少操作名称";return false;}
    std::string a=action->valuestring;
    if(a=="stop")Stop();else if(a=="emergency")Stop(true);else if(a=="arm")Arm();
    else if(a=="knock")Knock();else if(a=="roll")Roll();else if(a=="new_game")NewGame();
    else if(a=="dance"){if(!Dance()){error="请先停车并解除急停";return false;}}
    else if(a=="chat"){SetApp(HubApp::Chat);Application::GetInstance().ToggleChatState();}
    else if(a=="app"){
        auto name=cJSON_GetObjectItemCaseSensitive(json,"name");bool found=false;
        {std::lock_guard<std::mutex> lock(mutex_);if(home_hold_&&cJSON_IsString(name)&&strcmp(name->valuestring,"chat")){error="双眼保持中，请在设置关闭保持后进入应用";return false;}}
        if(cJSON_IsString(name)&&!strcmp(name->valuestring,"balance")){SetApp(HubApp::Balance);found=true;}
        if(cJSON_IsString(name))for(int i=0;i<9;++i)if(!strcmp(name->valuestring,app_names[i])){SetApp(static_cast<HubApp>(i));found=true;break;}
        if(!found){error="应用不存在";return false;}
    }else if(a=="light"){
        auto mode=cJSON_GetObjectItemCaseSensitive(json,"mode");
        if(!cJSON_IsNumber(mode)||mode->valuedouble!=mode->valueint||!SetLight(mode->valueint)){error="灯效编号应为 2–8";return false;}
    }else if(a=="game"){
        auto direction=cJSON_GetObjectItemCaseSensitive(json,"direction");bool found=false;
        const char* names[]{"left","right","up","down"};
        if(cJSON_IsString(direction))for(int i=0;i<4;++i)if(!strcmp(direction->valuestring,names[i])){GameMove(static_cast<sparkbot::Direction>(i));found=true;break;}
        if(!found){error="无效方向";return false;}
    }else if(a=="voyage"){
        auto direction=cJSON_GetObjectItemCaseSensitive(json,"direction");
        if(!cJSON_IsString(direction)||(strcmp(direction->valuestring,"left")&&strcmp(direction->valuestring,"right"))){error="无效方向";return false;}
        std::lock_guard<std::mutex> lock(mutex_);voyage_.Steer(!strcmp(direction->valuestring,"left")?-1:1);dirty_=true;
    }else if(a=="home_hold"||a=="auto_hold"){
        auto enabled=cJSON_GetObjectItemCaseSensitive(json,"enabled");if(!cJSON_IsBool(enabled)){error="需要开关状态";return false;}
        {std::lock_guard<std::mutex> lock(mutex_);if(a=="home_hold")manual_hold_=cJSON_IsTrue(enabled);else auto_hold_=cJSON_IsTrue(enabled);
        Settings saved("hub_profile",true);saved.SetBool(a,cJSON_IsTrue(enabled));}
        UpdateDock();
    }else if(a=="balance_pause"){
        std::lock_guard<std::mutex> lock(mutex_);voyage_.Toggle();dirty_=true;
    }else if(a=="balance_reset"){
        std::lock_guard<std::mutex> lock(mutex_);voyage_.Reset();dirty_=true;
    }else if(a=="touch"){
        auto enabled=cJSON_GetObjectItemCaseSensitive(json,"enabled");
        if(!cJSON_IsBool(enabled)){error="需要触摸开关状态";return false;}
        touch_.SetEnabled(cJSON_IsTrue(enabled));
        Settings saved("hub_profile",true);saved.SetBool("touch",cJSON_IsTrue(enabled));
    }else if(a=="touch_calibrate"){
        if(!touch_.Enabled()){error="请先启用机身触摸";return false;}
        touch_.Recalibrate();
    }else if(a=="profile"){
        auto name=cJSON_GetObjectItemCaseSensitive(json,"name");auto colors=cJSON_GetObjectItemCaseSensitive(json,"colors");
        if(!cJSON_IsString(name)||!sparkbot::ValidDeviceName(name->valuestring)||!cJSON_IsArray(colors)||cJSON_GetArraySize(colors)!=4){error="名称须为 1–48 字节有效文字，配色须有 4 项";return false;}
        auto chassis=cJSON_GetObjectItemCaseSensitive(json,"chassis");
        if(chassis&&(!cJSON_IsString(chassis)||!sparkbot::ValidChassis(chassis->valuestring))){error="底盘样式无效";return false;}
        std::array<std::string,4> next;
        for(int i=0;i<4;++i){auto color=cJSON_GetArrayItem(colors,i);if(!cJSON_IsString(color)||!sparkbot::ValidColor(color->valuestring)){error="颜色须为 #RRGGBB";return false;}next[i]=color->valuestring;}
        std::lock_guard<std::mutex> lock(mutex_);
        Settings saved("hub_profile",true);
        if(chassis&&chassis_!=chassis->valuestring){chassis_=chassis->valuestring;saved.SetString("chassis",chassis_);}
        if(device_name_!=name->valuestring){device_name_=name->valuestring;saved.SetString("name",device_name_);}
        for(size_t i=0;i<next.size();++i)if(colors_[i]!=next[i]){colors_[i]=next[i];saved.SetString("color"+std::to_string(i),colors_[i]);}
    }else if(a=="volume"){
        auto value=cJSON_GetObjectItemCaseSensitive(json,"value");
        if(!cJSON_IsNumber(value)||value->valuedouble!=value->valueint||value->valueint<0||value->valueint>100){error="音量应为 0–100 的整数";return false;}
        Board::GetInstance().GetAudioCodec()->SetOutputVolume(value->valueint);
        std::lock_guard<std::mutex> lock(mutex_);dirty_=true;
    }else if(a=="display"){
        if(!static_cast<SparkbotDisplay*>(display_)->Configure(json)){error="屏幕设置无效";return false;}
        std::lock_guard<std::mutex> lock(mutex_);dirty_=true;
    }else {error="不支持的操作";return false;}
    return true;
}

cJSON* SparkbotHub::Identity() {
    std::lock_guard<std::mutex> lock(mutex_);auto root=cJSON_CreateObject();
    cJSON_AddStringToObject(root,"service","xiaobai");cJSON_AddNumberToObject(root,"protocol",1);
    cJSON_AddStringToObject(root,"id",device_id_.c_str());cJSON_AddStringToObject(root,"name",device_name_.c_str());
    cJSON_AddNumberToObject(root,"port",80);return root;
}
