#include "hub_camera.h"
#include "board.h"
#include "system_info.h"
#include <esp_log.h>
#include <img_converters.h>
#include <cstring>
#include <esp_timer.h>

HubCamera::HubCamera(const camera_config_t& config) {
    ready_ = esp_camera_init(&config) == ESP_OK;
    if (!ready_) { ESP_LOGE("HubCamera", "Camera unavailable; other apps remain usable"); return; }
    preview_pixels_.resize(240 * 240 * 2);
    preview_.header.magic = LV_IMAGE_HEADER_MAGIC;
    preview_.header.cf = LV_COLOR_FORMAT_RGB565;
    preview_.header.w = 240; preview_.header.h = 240; preview_.header.stride = 480;
    preview_.data_size = preview_pixels_.size(); preview_.data = preview_pixels_.data();
}

bool HubCamera::CaptureLocked(std::vector<uint8_t>& jpeg, bool preview) {
    if (!ready_) return false;
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return false;
    uint8_t* encoded = nullptr; size_t length = 0;
    bool ok = preview || frame2jpg(fb, 60, &encoded, &length);
    if (ok && encoded && length) jpeg.assign(encoded, encoded + length);
    else if(!preview)ok = false;
    if (preview && fb->format == PIXFORMAT_RGB565 && fb->len == preview_pixels_.size()) {
        // The display task and pixel update share the LVGL lock.
        DisplayLockGuard lock(Board::GetInstance().GetDisplay());
        for (size_t i=0; i<fb->len; i+=2) {
            preview_pixels_[i] = fb->buf[i+1]; preview_pixels_[i+1] = fb->buf[i];
        }
    }
    free(encoded);
    esp_camera_fb_return(fb);
    return ok;
}

bool HubCamera::Snapshot(std::vector<uint8_t>& jpeg, bool preview) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(preview)return CaptureLocked(jpeg,true);
    int64_t now=esp_timer_get_time()/1000;
    if(!cached_jpeg_.empty() && now-cached_at_<100){jpeg=cached_jpeg_;return true;}
    if(!CaptureLocked(jpeg,false))return false;
    cached_jpeg_=jpeg;cached_at_=esp_timer_get_time()/1000;return true;
}
bool HubCamera::Capture() {
    std::lock_guard<std::mutex> lock(mutex_);
    return CaptureLocked(vision_jpeg_, false);
}
void HubCamera::SetExplainUrl(const std::string& url, const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_); url_ = url; token_ = token;
}
bool HubCamera::SetHMirror(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto sensor = ready_ ? esp_camera_sensor_get() : nullptr;
    return sensor && sensor->set_hmirror(sensor, enabled) == 0;
}
bool HubCamera::SetVFlip(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto sensor = ready_ ? esp_camera_sensor_get() : nullptr;
    return sensor && sensor->set_vflip(sensor, enabled) == 0;
}
std::string HubCamera::Explain(const std::string& question) {
    std::string url, token, body;
    const std::string boundary = "SparkBotVisionBoundary7a93";
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (url_.empty() || vision_jpeg_.empty()) return R"({"success":false,"message":"Vision service or image unavailable"})";
        url = url_; token = token_;
        body = "--"+boundary+"\r\nContent-Disposition: form-data; name=\"question\"\r\n\r\n"+question+
               "\r\n--"+boundary+"\r\nContent-Disposition: form-data; name=\"file\"; filename=\"camera.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
        body.append(reinterpret_cast<const char*>(vision_jpeg_.data()), vision_jpeg_.size());
        body += "\r\n--"+boundary+"--\r\n";
    }
    auto& board = Board::GetInstance();
    auto http = board.GetNetwork()->CreateHttp(3);
    http->SetHeader("Device-Id", SystemInfo::GetMacAddress());
    http->SetHeader("Client-Id", board.GetUuid());
    if (!token.empty()) http->SetHeader("Authorization", "Bearer " + token);
    http->SetHeader("Content-Type", "multipart/form-data; boundary="+boundary);
    http->SetContent(std::move(body));
    if (!http->Open("POST", url)) return R"({"success":false,"message":"Vision server connection failed"})";
    int status = http->GetStatusCode();
    std::string result = http->ReadAll(); http->Close();
    if (status != 200) return R"({"success":false,"message":"Vision server returned an error"})";
    return result;
}
