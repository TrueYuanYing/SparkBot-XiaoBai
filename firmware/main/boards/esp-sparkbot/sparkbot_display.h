#pragma once
#include "display/lcd_display.h"
#include <atomic>
#include <cJSON.h>
#include "hub_input.h"

class SparkbotDisplay : public SpiLcdDisplay {
    lv_obj_t *face_ = nullptr, *eyes_[2]{}, *subtitle_ = nullptr, *notice_ = nullptr;
    lv_obj_t *mouth_=nullptr,*cheeks_[2]{},*brows_[2]{};
    lv_obj_t* eye_cuts_[2]{};
    struct EyePose {float x=120,y=122,w=72,h=82;};
    EyePose poses_[2]{{72,122,72,82},{168,122,72,82}};
    std::atomic<uint32_t> touch_feedback_{0},render_fps_milli_{0},render_sample_at_{0};
    uint32_t last_frame_at_=0,render_frame_count_=0;
    float touch_strength_=0;
    lv_obj_t* mecha_=nullptr;
    void DrawMecha(lv_event_t* event);
    uint32_t expression_until_=0,next_expression_=0;
    int expression_=0;
    lv_timer_t* animation_ = nullptr;
    std::atomic<int> style_{0};
    std::atomic<bool> home_hold_{false};
    std::atomic<bool> subtitles_{false};
    uint32_t notice_until_ = 0;
    std::string message_, status_;
    void Animate();
public:
    using SpiLcdDisplay::SpiLcdDisplay;
    void InitFace();
    void SetChatMessage(const char* role, const char* content) override;
    void SetStatus(const char* status) override;
    void SetEmotion(const char* emotion) override;
    using Display::ShowNotification;
    void ShowNotification(const char* text, int duration_ms = 3000) override;
    bool Configure(const cJSON* json);
    void AddSettings(cJSON* json) const;
    void HoldEyes(bool hold) { home_hold_=hold; }
    void SetTouchFeedback(sparkbot::TouchFeedback feedback);
};
