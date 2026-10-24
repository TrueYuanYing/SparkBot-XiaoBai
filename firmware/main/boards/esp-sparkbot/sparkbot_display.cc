#include "sparkbot_display.h"
#include "application.h"
#include "settings.h"
#include <cmath>
#include <algorithm>
#include <esp_random.h>
#include <cstring>

LV_FONT_DECLARE(font_puhui_20_4);

void SparkbotDisplay::InitFace() {
    Settings settings("hub_display");
    style_ = std::clamp<int32_t>(settings.GetInt("style", 0), 0, 2);
    subtitles_ = settings.GetInt("subtitles", 0) != 0;
    DisplayLockGuard lock(this);
    face_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(face_, 240, 240);
    lv_obj_set_pos(face_, 0, 0);
    lv_obj_set_style_bg_color(face_, lv_color_black(), 0);
    lv_obj_set_style_border_width(face_, 0, 0);
    lv_obj_set_style_radius(face_, 0, 0);
    lv_obj_set_style_pad_all(face_, 0, 0);
    lv_obj_remove_flag(face_, LV_OBJ_FLAG_SCROLLABLE);
    for (auto& eye : eyes_) {
        eye = lv_obj_create(face_);
        lv_obj_remove_flag(eye, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(eye, lv_color_white(), 0);
        lv_obj_set_style_border_width(eye, 0, 0);
        lv_obj_set_style_pad_all(eye,0,0);
    }
    for(int i=0;i<2;++i){eye_cuts_[i]=lv_obj_create(eyes_[i]);lv_obj_remove_style_all(eye_cuts_[i]);
        lv_obj_set_style_bg_color(eye_cuts_[i],lv_color_black(),0);lv_obj_set_style_bg_opa(eye_cuts_[i],255,0);
        lv_obj_set_style_radius(eye_cuts_[i],30,0);lv_obj_add_flag(eye_cuts_[i],LV_OBJ_FLAG_HIDDEN);}
    mouth_=lv_obj_create(face_);
    lv_obj_set_style_border_width(mouth_,0,0);
    lv_obj_set_style_bg_color(mouth_,lv_color_white(),0);
    lv_obj_remove_flag(mouth_,LV_OBJ_FLAG_SCROLLABLE);
    for(int i=0;i<2;++i) {
        cheeks_[i]=lv_obj_create(face_);brows_[i]=lv_obj_create(face_);
        for(auto part:{cheeks_[i],brows_[i]}) {
            lv_obj_set_style_border_width(part,0,0);lv_obj_remove_flag(part,LV_OBJ_FLAG_SCROLLABLE);
        }
        lv_obj_set_size(cheeks_[i],22,7);lv_obj_set_style_radius(cheeks_[i],4,0);
        lv_obj_set_style_bg_color(cheeks_[i],lv_color_hex(0xff88b4),0);
        lv_obj_set_style_bg_color(brows_[i],lv_color_hex(0xffb46b),0);
        lv_obj_set_size(brows_[i],50,5);lv_obj_set_style_radius(brows_[i],3,0);
    }
    next_expression_=lv_tick_get()+6500;
    mecha_=lv_obj_create(face_);lv_obj_remove_style_all(mecha_);
    lv_obj_set_size(mecha_,220,128);lv_obj_set_pos(mecha_,10,58);
    lv_obj_remove_flag(mecha_,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(mecha_,[](lv_event_t* event){static_cast<SparkbotDisplay*>(lv_event_get_user_data(event))->DrawMecha(event);},LV_EVENT_DRAW_MAIN,this);
    subtitle_ = lv_label_create(face_);
    lv_obj_set_width(subtitle_, 224);
    lv_obj_set_style_max_height(subtitle_, 70, 0);
    lv_label_set_long_mode(subtitle_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(subtitle_, &font_puhui_20_4, 0);
    lv_obj_set_style_text_color(subtitle_, lv_color_white(), 0);
    lv_obj_set_style_text_align(subtitle_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(subtitle_, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_label_set_text(subtitle_, "");
    notice_ = lv_label_create(face_);
    lv_obj_set_width(notice_, 224);
    lv_label_set_long_mode(notice_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(notice_, &font_puhui_20_4, 0);
    lv_obj_set_style_text_color(notice_, lv_color_hex(0xb5b5b5), 0);
    lv_obj_set_style_text_align(notice_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(notice_, LV_ALIGN_TOP_MID, 0, 6);
    lv_label_set_text(notice_, "");
    animation_ = lv_timer_create([](lv_timer_t* timer) {
        static_cast<SparkbotDisplay*>(lv_timer_get_user_data(timer))->Animate();
    }, 20, this);
    lv_display_add_event_cb(lv_obj_get_display(face_),[](lv_event_t* event){
        auto self=static_cast<SparkbotDisplay*>(lv_event_get_user_data(event));
        ++self->render_frame_count_;uint32_t now=lv_tick_get(),elapsed=now-self->render_sample_at_.load();
        if(elapsed>=1000){self->render_fps_milli_=self->render_frame_count_*1000000u/elapsed;self->render_frame_count_=0;self->render_sample_at_=now;}
    },LV_EVENT_RENDER_READY,this);
    Animate();
}

void SparkbotDisplay::Animate() {
    // LVGL invokes this timer under its own lock, without taking any hub/motor lock.
    if(lv_obj_get_screen(face_)!=lv_screen_active())return;
    int style = style_.load();
    if(home_hold_ && style==2)style=0;
    auto state = Application::GetInstance().GetDeviceState();
    bool system_message = state == kDeviceStateActivating || state == kDeviceStateFatalError;
    if (style == 2) {
        lv_obj_add_flag(face_, LV_OBJ_FLAG_HIDDEN);
        if (chat_message_label_) {
            if (subtitles_ || system_message) lv_obj_remove_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    lv_obj_remove_flag(face_, LV_OBJ_FLAG_HIDDEN);
    uint32_t tick = lv_tick_get();
    float dt=last_frame_at_?std::min<uint32_t>(tick-last_frame_at_,100):20;last_frame_at_=tick;
    uint32_t feedback=touch_feedback_.load();int zone=feedback&3,mood=(feedback>>2)&3;
    float target=(feedback>>8)/255.f;
    touch_strength_+=(target-touch_strength_)*dt/(45.f+dt);
    float contact=touch_strength_;
    bool pet=mood==static_cast<int>(sparkbot::TouchMood::Pet);
    bool bump=mood==static_cast<int>(sparkbot::TouchMood::Bump);
    bool talking = state == kDeviceStateSpeaking;
    bool listening = state == kDeviceStateListening;
    bool blink = (tick % 4600) < 135;
    if(static_cast<int32_t>(tick-next_expression_)>=0) {
        expression_=1+esp_random()%7;
        expression_until_=tick+1800+esp_random()%1400;
        next_expression_=expression_until_+5500+esp_random()%6500;
    }
    int emotion=static_cast<int32_t>(expression_until_-tick)>0?expression_:0;
    if(zone)emotion=pet?1:0;
    float wave = std::sin(tick / (talking ? 135.f : 1100.f));
    int eye_width = style == 1 ? 82 : 72;
    int eye_height = blink ? 5 : (style == 1 ? 48 : 82) + int(wave * (talking ? 12 : 3)) + (listening ? 10 : 0);
    int gaze = int(std::sin(tick / 2400.f) * 7);
    if(zone)gaze=zone==2?(pet?-5:8):zone==3?(pet?5:-8):0;
    int center_y = subtitles_ && !message_.empty() ? 98 : 122;
    // Hidden classic-face geometry does not need layout/style updates in mecha mode.
    if(style!=1) {
    for (int i = 0; i < 2; ++i) {
        int w=eye_width,h=eye_height,offset=0;
        uint32_t color=0xffffff;
        if(!blink) {
            if(emotion==1) {h=25;offset=-5;} // Smile.
            if(emotion==2) {h=i?eye_height+10:eye_height-18;offset=i?-7:4;} // Curious.
            if(emotion==3) {h=26;offset=12;color=0xdde8ff;} // Sleepy.
            if(emotion==4) {w=61;h=95;color=0xe1fbff;} // Surprise.
            if(emotion==5 && i==0) h=7; // Wink.
            if(emotion==6) h=60; // Blush.
            if(emotion==7) {h=43;color=0xffe1b9;} // Determined.
        }
        bool affected=zone==1 || zone==i+2;
        if(bump&&affected){h=std::max(6,int(h*(1-contact*.9f)));offset+=int(contact*(zone==1?10:3));color=0xffd9d4;}
        if(pet){h=30;w=66;offset=-int(contact*(zone==1?7:3));color=0xe4fff5;}
        auto& pose=poses_[i];float ease=dt/(bump?25.f+dt:65.f+dt);
        auto toward=[ease](float& current,float desired){current+=(desired-current)*ease;};
        toward(pose.w,w);toward(pose.h,h);toward(pose.x,(i?168:72)+gaze);toward(pose.y,center_y+offset);
        lv_obj_set_size(eyes_[i],std::lround(pose.w),std::lround(pose.h));
        lv_obj_set_pos(eyes_[i],std::lround(pose.x-pose.w/2),std::lround(pose.y-pose.h/2));
        if(pet||emotion==1){lv_obj_set_size(eye_cuts_[i],std::max(1,int(pose.w)-12),int(pose.h)+4);lv_obj_set_pos(eye_cuts_[i],6,10);lv_obj_remove_flag(eye_cuts_[i],LV_OBJ_FLAG_HIDDEN);}
        else lv_obj_add_flag(eye_cuts_[i],LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_radius(eyes_[i], style == 1 ? 12 : 30, 0);
        lv_obj_set_style_bg_color(eyes_[i],lv_color_hex(color),0);
        lv_obj_set_pos(cheeks_[i],(i?189:28)+gaze,center_y+40);
        lv_obj_set_pos(brows_[i],(i?141:47)+gaze,center_y-43);
        lv_obj_set_style_transform_rotation(brows_[i],i?-130:130,0);
        lv_obj_set_style_opa(cheeks_[i],pet?int(contact*210):255,0);
        if(emotion==6||pet)lv_obj_remove_flag(cheeks_[i],LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(cheeks_[i],LV_OBJ_FLAG_HIDDEN);
        if(emotion==7||(bump&&affected))lv_obj_remove_flag(brows_[i],LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(brows_[i],LV_OBJ_FLAG_HIDDEN);
    }
    bool show_mouth=talking || emotion==1 || emotion==2 || emotion==4 || emotion==5 || emotion==6;
    if(show_mouth) {
        int w=emotion==4?13:26,h=emotion==4?18:6;
        if(talking) {w=19+int((wave+1)*7);h=5+int((wave+1)*6);}
        lv_obj_set_size(mouth_,w,h);lv_obj_set_style_radius(mouth_,8,0);
        lv_obj_set_pos(mouth_,120-w/2+gaze/2,center_y+47);
        lv_obj_remove_flag(mouth_,LV_OBJ_FLAG_HIDDEN);
    }else lv_obj_add_flag(mouth_,LV_OBJ_FLAG_HIDDEN);
    }
    if(style==1) {
        for(int i=0;i<2;++i){lv_obj_add_flag(eyes_[i],LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(cheeks_[i],LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(brows_[i],LV_OBJ_FLAG_HIDDEN);}
        lv_obj_add_flag(mouth_,LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(mecha_,10,center_y-64);
        lv_obj_remove_flag(mecha_,LV_OBJ_FLAG_HIDDEN);lv_obj_invalidate(mecha_);
    }else {
        lv_obj_add_flag(mecha_,LV_OBJ_FLAG_HIDDEN);
        for(auto eye:eyes_)lv_obj_remove_flag(eye,LV_OBJ_FLAG_HIDDEN);
    }
    if ((subtitles_ || system_message) && !message_.empty()) lv_obj_remove_flag(subtitle_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(subtitle_, LV_OBJ_FLAG_HIDDEN);
    bool normal = state == kDeviceStateIdle || state == kDeviceStateListening || state == kDeviceStateSpeaking;
    if (static_cast<int32_t>(notice_until_ - tick) > 0) lv_obj_remove_flag(notice_, LV_OBJ_FLAG_HIDDEN);
    else if (!normal && !status_.empty()) {
        lv_label_set_text(notice_, status_.c_str());
        lv_obj_remove_flag(notice_, LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(notice_, LV_OBJ_FLAG_HIDDEN);
}

void SparkbotDisplay::DrawMecha(lv_event_t* event) {
    auto layer=lv_event_get_layer(event);lv_area_t area{};lv_obj_get_coords(mecha_,&area);
    uint32_t tick=lv_tick_get();
    bool talking=Application::GetInstance().GetDeviceState()==kDeviceStateSpeaking;
    float pulse=(std::sin(tick/(talking?180.f:1000.f))+1)*.5f;
    bool blink=tick%6100<100;
    uint32_t feedback=touch_feedback_.load();int zone=feedback&3,mood=(feedback>>2)&3;
    bool pet=mood==static_cast<int>(sparkbot::TouchMood::Pet),bump=mood==static_cast<int>(sparkbot::TouchMood::Bump);
    float strength=touch_strength_;
    auto point=[&](int x,int y,int side)->lv_point_precise_t {
        bool affected=zone==1||zone==side+2;
        float scale=blink&&!zone?.14f:bump&&affected?1.f-strength*.86f:1.f;
        int shift=zone==2?(pet?-4:6):zone==3?(pet?4:-6):0;
        return {static_cast<lv_value_precise_t>(area.x1+(side?220-x:x)+shift*strength),
                static_cast<lv_value_precise_t>(area.y1+64+(y-64)*scale+(zone==1?(pet?-5:7)*strength:0))};
    };
    int outer[4][2]{{18,29},{99,56},{87,87},{41,73}};
    int core[4][2]{{31,41},{88,60},{81,74},{44,65}};
    if(pet){const int happy[4][2]{{18,64},{58,38},{99,64},{58,52}};
        for(int i=0;i<4;++i)for(int axis=0;axis<2;++axis){outer[i][axis]+=int((happy[i][axis]-outer[i][axis])*strength);core[i][axis]+=int((happy[i][axis]-core[i][axis])*strength);}}
    for(int side=0;side<2;++side) {
        bool affected=zone==1||zone==side+2;
        uint32_t hue=pet?0x75f5d2:bump&&affected?0xff795e:talking?0xffbc69:0x55cfff;
        // One halo plus the crisp rim avoids redundant alpha blending on S3.
        for(int width:{9,2})for(int i=0;i<4;++i) {
            lv_draw_line_dsc_t d{};lv_draw_line_dsc_init(&d);
            d.p1=point(outer[i][0],outer[i][1],side);d.p2=point(outer[(i+1)%4][0],outer[(i+1)%4][1],side);
            d.color=lv_color_hex(hue);d.width=width;d.opa=width==9?55+int(pulse*35):230;
            lv_draw_line(layer,&d);
        }
        auto fill=[&](const int shape[4][2],uint32_t color) {
            for(int triangle=0;triangle<2;++triangle){lv_draw_triangle_dsc_t d{};lv_draw_triangle_dsc_init(&d);
                int indices[3]{0,triangle+1,triangle+2};
                if(pet){indices[0]=triangle==0?0:1;indices[1]=triangle==0?1:2;indices[2]=3;}
                for(int i=0;i<3;++i)d.p[i]=point(shape[indices[i]][0],shape[indices[i]][1],side);
                d.color=lv_color_hex(color);d.opa=255;lv_draw_triangle(layer,&d);}
        };
        fill(outer,pet?0x26a999:bump&&affected?0xd8566c:talking?0xe78254:0x4876de);fill(core,pet?0xdcfff0:bump&&affected?0xffe2cf:talking?0xffead2:0xe5fbff);
        lv_draw_line_dsc_t brow{};lv_draw_line_dsc_init(&brow);
        brow.p1=point(24,17,side);brow.p2=point(102,44,side);brow.color=lv_color_hex(0x7766cc);brow.width=3;brow.opa=160;
        lv_draw_line(layer,&brow);
        if(pet){for(int stripe=0;stripe<2;++stripe){lv_draw_line_dsc_t cheek{};lv_draw_line_dsc_init(&cheek);
            cheek.p1=point(36+stripe*10,98,side);cheek.p2=point(40+stripe*10,90,side);cheek.color=lv_color_hex(0xffbb9c);cheek.width=3;cheek.opa=int(strength*230);lv_draw_line(layer,&cheek);}}
    }
}

void SparkbotDisplay::SetTouchFeedback(sparkbot::TouchFeedback feedback) {
    uint32_t strength=std::clamp(int(feedback.strength*255),0,255);
    touch_feedback_=uint32_t(feedback.zone)|(uint32_t(feedback.mood)<<2)|(strength<<8);
}

void SparkbotDisplay::SetChatMessage(const char* role, const char* content) {
    SpiLcdDisplay::SetChatMessage(role, content ? content : "");
    DisplayLockGuard lock(this);
    message_ = content ? content : "";
    if (subtitle_) lv_label_set_text(subtitle_, message_.c_str());
}
void SparkbotDisplay::SetStatus(const char* status) {
    SpiLcdDisplay::SetStatus(status);
    DisplayLockGuard lock(this);
    status_ = status ? status : "";
}
void SparkbotDisplay::SetEmotion(const char* emotion) {
    SpiLcdDisplay::SetEmotion(emotion);
    if(!emotion)return;
    DisplayLockGuard lock(this);
    int selected=0;
    if(!strcmp(emotion,"happy")||!strcmp(emotion,"laughing")||!strcmp(emotion,"funny"))selected=1;
    else if(!strcmp(emotion,"thinking")||!strcmp(emotion,"confused")||!strcmp(emotion,"curious"))selected=2;
    else if(!strcmp(emotion,"sleepy")||!strcmp(emotion,"sad"))selected=3;
    else if(!strcmp(emotion,"surprised")||!strcmp(emotion,"shocked"))selected=4;
    else if(!strcmp(emotion,"winking")||!strcmp(emotion,"cool"))selected=5;
    else if(!strcmp(emotion,"loving")||!strcmp(emotion,"shy")||!strcmp(emotion,"embarrassed"))selected=6;
    else if(!strcmp(emotion,"angry")||!strcmp(emotion,"confident"))selected=7;
    expression_=selected;expression_until_=lv_tick_get()+4500;
    next_expression_=expression_until_+6000;
}
void SparkbotDisplay::ShowNotification(const char* text, int duration_ms) {
    SpiLcdDisplay::ShowNotification(text, duration_ms);
    DisplayLockGuard lock(this);
    if (notice_) {
        lv_label_set_text(notice_, text ? text : "");
        notice_until_ = lv_tick_get() + duration_ms;
    }
}
bool SparkbotDisplay::Configure(const cJSON* json) {
    auto style = cJSON_GetObjectItemCaseSensitive(json, "style");
    auto subtitles = cJSON_GetObjectItemCaseSensitive(json, "subtitles");
    if (!cJSON_IsNumber(style) || style->valuedouble != style->valueint ||
        style->valueint < 0 || style->valueint > 2 || !cJSON_IsBool(subtitles)) return false;
    Settings settings("hub_display", true);
    settings.SetInt("style", style->valueint);
    settings.SetInt("subtitles", cJSON_IsTrue(subtitles));
    style_ = style->valueint;
    subtitles_ = cJSON_IsTrue(subtitles);
    return true;
}
void SparkbotDisplay::AddSettings(cJSON* json) const {
    cJSON_AddNumberToObject(json, "screen_style", style_.load());
    cJSON_AddBoolToObject(json, "subtitles", subtitles_.load());
    cJSON_AddStringToObject(json, "wake_word", "嘿，小白");
    cJSON_AddNumberToObject(json,"ui_render_fps",lv_tick_get()-render_sample_at_.load()<2000?render_fps_milli_.load()/1000.f:0.f);
    cJSON_AddNumberToObject(json,"ui_target_fps",50);
}
