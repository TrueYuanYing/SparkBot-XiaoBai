#pragma once
#include "sparkbot_hub.h"
#include <esp_http_server.h>

class HubServer {
    SparkbotHub& hub_;
    httpd_handle_t server_=nullptr, camera_server_=nullptr;
    std::string token_;
    static esp_err_t Page(httpd_req_t* req);
    static esp_err_t AndroidPackage(httpd_req_t* req);
    static esp_err_t Status(httpd_req_t* req);
    static esp_err_t Action(httpd_req_t* req);
    static esp_err_t Socket(httpd_req_t* req);
    static esp_err_t Snapshot(httpd_req_t* req);
    bool QueryToken(httpd_req_t* req);
    bool HeaderToken(httpd_req_t* req);
    void DiscoveryLoop();
public:
    explicit HubServer(SparkbotHub& hub):hub_(hub){}
    void Start();
};
