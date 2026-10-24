#pragma once
#include "camera.h"
#include "display.h"
#include <esp_camera.h>
#include <mutex>
#include <vector>

// One sensor owner serves both local preview and XiaoZhi vision calls.
class HubCamera : public Camera {
    std::mutex mutex_;
    bool ready_ = false;
    std::string url_, token_;
    std::vector<uint8_t> vision_jpeg_;
    std::vector<uint8_t> cached_jpeg_;
    int64_t cached_at_=0;
    std::vector<uint8_t> preview_pixels_;
    lv_image_dsc_t preview_{};
    bool CaptureLocked(std::vector<uint8_t>& jpeg, bool preview);
public:
    explicit HubCamera(const camera_config_t& config);
    bool Ready() const { return ready_; }
    bool Snapshot(std::vector<uint8_t>& jpeg, bool preview = false);
    const lv_image_dsc_t* Preview() const { return &preview_; }
    void SetExplainUrl(const std::string& url, const std::string& token) override;
    bool Capture() override;
    bool SetHMirror(bool enabled) override;
    bool SetVFlip(bool enabled) override;
    std::string Explain(const std::string& question) override;
};
