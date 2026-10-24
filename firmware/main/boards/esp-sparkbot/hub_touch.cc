#include "hub_touch.h"
#include <touch_element/touch_button.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <driver/touch_sensor_legacy.h>
#include "settings.h"
#include <cJSON.h>

namespace {
// Top uses Factory's original value after the FPC repair. Smaller means a
// weaker finger signal can trigger; retain the established left-side setting.
constexpr float sensitivity[]{0.035f,0.095f,0.085f};
constexpr float threshold_divider=0.8f;
}

bool HubTouch::Init() {
    Settings saved("hub_profile");enabled_=saved.GetBool("touch",true);
    // Factory channels and hardware filters, with tuned relative thresholds.
    touch_elem_global_config_t global=TOUCH_ELEM_GLOBAL_DEFAULT_CONFIG();
    touch_button_global_config_t buttons=TOUCH_BUTTON_GLOBAL_DEFAULT_CONFIG();
    buttons.threshold_divider=threshold_divider;
    if(touch_element_install(&global)!=ESP_OK || touch_button_install(&buttons)!=ESP_OK) return false;
    queue_=xQueueCreate(24,sizeof(Edge));
    if(!queue_)return false;
    for(int i=1;i<=3;++i) {
        touch_button_config_t config{};
        config.channel_num=static_cast<touch_pad_t>(i);config.channel_sens=sensitivity[i-1];
        touch_button_handle_t handle=nullptr;
        contexts_[i-1]={this,i};
        if(touch_button_create(&config,&handle)!=ESP_OK ||
           touch_button_subscribe_event(handle,TOUCH_ELEM_EVENT_ON_PRESS|TOUCH_ELEM_EVENT_ON_RELEASE,&contexts_[i-1])!=ESP_OK ||
           touch_button_set_dispatch_method(handle,TOUCH_ELEM_DISP_CALLBACK)!=ESP_OK) return false;
        if(touch_button_set_callback(handle,[](touch_button_handle_t,touch_button_message_t* message,void* arg) {
            auto context=static_cast<Context*>(arg);
            Edge edge{context->channel,message->event==TOUCH_BUTTON_EVT_ON_PRESS,esp_timer_get_time()/1000};
            if(xQueueSend(context->self->queue_,&edge,0)!=pdTRUE)context->self->overflow_=true;
        })!=ESP_OK)return false;
    }
    ready_=touch_element_start()==ESP_OK;
    settle_until_=esp_timer_get_time()/1000+5000;protected_=true;
    ESP_LOGI("HubTouch","Factory touch channels 1/2/3 ready=%d",ready_);
    return ready_;
}
sparkbot::Input HubTouch::Poll(int64_t now) {
    if(!ready_) return sparkbot::Input::None;
    if(recalibrate_.exchange(false)){
        if(scanning_){touch_element_stop();scanning_=false;}
        calibration_={};calibrated_=false;protected_=true;
    }
    const bool wanted=enabled_.load() && !suspended_.load();
    if(wanted!=scanning_) {
        scanning_=wanted;
        if(scanning_){touch_element_start();settle_until_=now+5000;calibration_={};calibrated_=false;protected_=true;}else touch_element_stop();
        xQueueReset(queue_);gestures_=sparkbot::TouchGestures{};
        ESP_LOGI("HubTouch","Scanning %s",scanning_?"enabled":"disabled");
    }
    if(!scanning_)return sparkbot::Input::None;
    if(!calibrated_) {
        if(now>settle_until_-4000)for(int i=0;i<3;++i){uint32_t raw=0;touch_pad_read_raw_data(static_cast<touch_pad_t>(i+1),&raw);calibration_[i].Sample(raw);}
        if(now>=settle_until_) {
            bool enough=true;for(const auto& calibration:calibration_)enough&=calibration.Ready();
            if(!enough){settle_until_=now+1000;xQueueReset(queue_);return sparkbot::Input::None;}
            bool noisy=false;
            for(int i=0;i<3;++i){uint32_t benchmark=0;auto pad=static_cast<touch_pad_t>(i+1);touch_pad_read_benchmark(pad,&benchmark);
                auto threshold=calibration_[i].Threshold(benchmark,sensitivity[i]*threshold_divider);touch_pad_set_thresh(pad,threshold);noisy|=calibration_[i].Noisy();
                ESP_LOGI("HubTouch","calibration channel=%d baseline=%lu threshold=%lu noisy=%d",i+1,benchmark,threshold,calibration_[i].Noisy());}
            calibrated_=true;protected_=noisy;gestures_=sparkbot::TouchGestures{};xQueueReset(queue_);
        }
    }
    if(overflow_.exchange(false)) {xQueueReset(queue_);gestures_=sparkbot::TouchGestures{};}
    Edge edge{};
    while(xQueuePeek(queue_,&edge,0)==pdTRUE) {
        if(edge.time<settle_until_) {xQueueReceive(queue_,&edge,0);continue;}
        // Expire a preceding single tap before consuming a later unrelated tap.
        auto expired=gestures_.Poll(edge.time);
        if(expired!=sparkbot::Input::None)return expired;
        xQueueReceive(queue_,&edge,0);
        ESP_LOGI("HubTouch","channel=%d %s",edge.channel,edge.down?"press":"release");
        auto input=gestures_.Edge(edge.channel,edge.down,edge.time);
        if(input!=sparkbot::Input::None) return input;
    }
    return gestures_.Poll(now);
}

void HubTouch::AddDiagnostics(cJSON* root) const {
    auto channels=cJSON_AddArrayToObject(root,"touch_channels");
    if(!ready_)return;
    for(int i=0;i<3;++i){
        const auto pad=static_cast<touch_pad_t>(i+1);
        uint32_t raw=0,benchmark=0,threshold=0;
        bool valid=touch_pad_read_raw_data(pad,&raw)==ESP_OK;
        valid&=touch_pad_read_benchmark(pad,&benchmark)==ESP_OK;
        valid&=touch_pad_get_thresh(pad,&threshold)==ESP_OK;
        auto item=cJSON_CreateObject();cJSON_AddItemToArray(channels,item);
        cJSON_AddNumberToObject(item,"channel",i+1);cJSON_AddBoolToObject(item,"valid",valid);
        if(valid){cJSON_AddNumberToObject(item,"raw",raw);cJSON_AddNumberToObject(item,"benchmark",benchmark);cJSON_AddNumberToObject(item,"threshold",threshold);}
    }
}
