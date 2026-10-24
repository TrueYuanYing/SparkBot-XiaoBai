#pragma once
#include "hub_input.h"
#include "hub_touch_noise.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>
#include <cJSON.h>
class HubTouch {
    struct Edge {int channel;bool down;int64_t time;};
    struct Context {HubTouch* self;int channel;};
    Context contexts_[3]{};
    QueueHandle_t queue_=nullptr;
    std::atomic<bool> overflow_{false};
    sparkbot::TouchGestures gestures_;
    bool ready_=false;
    int64_t settle_until_=0;
    std::atomic<bool> enabled_{true};
    std::atomic<bool> suspended_{false};
    bool scanning_=true;
    std::array<sparkbot::TouchNoiseCalibration,3> calibration_;
    std::atomic<bool> recalibrate_{false},protected_{false};
    bool calibrated_=false;
    int burst_count_=0;
    int64_t burst_start_=0;
public:
    bool Init();
    sparkbot::Input Poll(int64_t now);
    sparkbot::TouchFeedback Feedback(int64_t now) const { return scanning_ && calibrated_ ? gestures_.Feedback(now) : sparkbot::TouchFeedback{}; }
    bool Ready() const { return ready_; }
    bool Enabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_=enabled; }
    void Suspend(bool suspended) { suspended_=suspended; }
    void Recalibrate() { recalibrate_=true; }
    bool Protected() const { return protected_; }
    void AddDiagnostics(cJSON* root) const;
};
