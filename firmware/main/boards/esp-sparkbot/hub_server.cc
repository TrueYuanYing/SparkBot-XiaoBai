#include "hub_server.h"
#include "ssid_manager.h"
#include "settings.h"
#include <esp_random.h>
#include <esp_system.h>
#include <esp_log.h>
#include <cstring>
#include <cmath>
#include <lwip/sockets.h>
#include <esp_timer.h>
#include <wifi_station.h>

extern const uint8_t hub_page_start[] asm("_binary_hub_html_start");
extern const uint8_t hub_page_end[] asm("_binary_hub_html_end");
extern const uint8_t android_apk_start[] asm("_binary_xiaobai_apk_start");
extern const uint8_t android_apk_end[] asm("_binary_xiaobai_apk_end");
struct ControlSession {SparkbotHub* hub;uint32_t owner;};
static esp_err_t Json(httpd_req_t* req,cJSON* json,const char* status="200 OK") {
    char* body=cJSON_PrintUnformatted(json);cJSON_Delete(json);
    if(!body)return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Out of memory");
    httpd_resp_set_type(req,"application/json; charset=utf-8");httpd_resp_set_status(req,status);
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    esp_err_t result=httpd_resp_send(req,body,HTTPD_RESP_USE_STRLEN);cJSON_free(body);return result;
}
bool HubServer::QueryToken(httpd_req_t* req) {
    char query[100]{},token[40]{};
    return httpd_req_get_url_query_str(req,query,sizeof(query))==ESP_OK &&
        httpd_query_key_value(query,"token",token,sizeof(token))==ESP_OK && token_==token;
}
bool HubServer::HeaderToken(httpd_req_t* req) {
    char token[40]{};
    return httpd_req_get_hdr_value_str(req,"X-Sparkbot-Token",token,sizeof(token))==ESP_OK && token_==token;
}
esp_err_t HubServer::Page(httpd_req_t* req) {
    WifiStation::GetInstance().DeferBackgroundScan();
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"X-Frame-Options","DENY");
    httpd_resp_set_hdr(req,"Referrer-Policy","no-referrer");
    return httpd_resp_send(req,reinterpret_cast<const char*>(hub_page_start),hub_page_end-hub_page_start);
}
esp_err_t HubServer::Status(httpd_req_t* req) {
    auto* self=static_cast<HubServer*>(req->user_ctx);auto json=self->hub_.Status();
    cJSON_AddStringToObject(json,"token",self->token_.c_str());
    cJSON_AddStringToObject(json,"version","XiaoBai 0.9 / XiaoZhi 1.9.4");
    return Json(req,json);
}
esp_err_t HubServer::AndroidPackage(httpd_req_t* req) {
    httpd_resp_set_type(req,"application/vnd.android.package-archive");
    httpd_resp_set_hdr(req,"Content-Disposition","attachment; filename=XiaoBai-0.9.apk");
    return httpd_resp_send(req,reinterpret_cast<const char*>(android_apk_start),android_apk_end-android_apk_start);
}
esp_err_t HubServer::Action(httpd_req_t* req) {
    auto* self=static_cast<HubServer*>(req->user_ctx);
    if(!self->HeaderToken(req))return httpd_resp_send_err(req,HTTPD_403_FORBIDDEN,"Invalid token");
    WifiStation::GetInstance().DeferBackgroundScan();
    if(req->content_len==0 || req->content_len>512)return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid length");
    char body[513]{};int offset=0;
    while(offset<req->content_len){int count=httpd_req_recv(req,body+offset,req->content_len-offset);
        if(count<=0)return ESP_FAIL;
        offset+=count;}
    cJSON* json=cJSON_ParseWithLength(body,offset);
    if(!cJSON_IsObject(json)){cJSON_Delete(json);return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid JSON");}
    auto a=cJSON_GetObjectItemCaseSensitive(json,"action");
    bool network=cJSON_IsString(a)&&!strcmp(a->valuestring,"wifi");
    std::string error;bool ok=false;
    if(network){
        auto ssid=cJSON_GetObjectItemCaseSensitive(json,"ssid");auto password=cJSON_GetObjectItemCaseSensitive(json,"password");
        if(cJSON_IsString(ssid)&&cJSON_IsString(password)&&strlen(ssid->valuestring)>0&&strlen(ssid->valuestring)<=32&&
            (strlen(password->valuestring)==0||(strlen(password->valuestring)>=8&&strlen(password->valuestring)<=63))){
            self->hub_.Stop(true);
            // Prevent the scanning task from reading a vector while it is edited: save on a restart path.
            Settings settings("hub_wifi",true);settings.SetString("ssid",ssid->valuestring);settings.SetString("password",password->valuestring);
            ok=true;
        }else error="Wi-Fi 名称 1–32 字节，密码为空或 8–63 字节";
    }else ok=self->hub_.Action(json,error);
    cJSON_Delete(json);auto response=cJSON_CreateObject();cJSON_AddBoolToObject(response,"ok",ok);
    if(!ok)cJSON_AddStringToObject(response,"error",error.c_str());
    esp_err_t result=Json(req,response,ok?"200 OK":"400 Bad Request");
    if(network&&ok)xTaskCreate([](void*){vTaskDelay(pdMS_TO_TICKS(1200));esp_restart();},"hub_reboot",2048,nullptr,1,nullptr);
    return result;
}
esp_err_t HubServer::Socket(httpd_req_t* req) {
    auto* self=static_cast<HubServer*>(req->user_ctx);
    if(req->method==HTTP_GET){
        // IDF has already upgraded this socket; close it before allocating control state.
        if(!self->QueryToken(req))return ESP_FAIL;
        auto* session=new(std::nothrow) ControlSession{&self->hub_,esp_random()};
        if(!session)return ESP_ERR_NO_MEM;
        if(!session->owner||session->owner==0xffffffffu)session->owner=1;
        req->sess_ctx=session;req->free_ctx=[](void* ptr){auto* session=static_cast<ControlSession*>(ptr);
            session->hub->Disconnect(session->owner);delete session;};
        return ESP_OK;
    }
    auto* session=static_cast<ControlSession*>(req->sess_ctx);if(!session)return ESP_FAIL;
    httpd_ws_frame_t frame{};
    esp_err_t result=httpd_ws_recv_frame(req,&frame,0);if(result!=ESP_OK)return result;
    if(frame.len>192 || (frame.type!=HTTPD_WS_TYPE_TEXT&&frame.type!=HTTPD_WS_TYPE_CLOSE)){self->hub_.Disconnect(session->owner);return ESP_FAIL;}
    uint8_t data[193]{};frame.payload=data;
    if(frame.len && httpd_ws_recv_frame(req,&frame,frame.len)!=ESP_OK)return ESP_FAIL;
    if(frame.type==HTTPD_WS_TYPE_CLOSE){self->hub_.Disconnect(session->owner);return ESP_FAIL;}
    auto json=cJSON_ParseWithLength(reinterpret_cast<char*>(data),frame.len);
    auto seq=cJSON_GetObjectItemCaseSensitive(json,"seq");auto x=cJSON_GetObjectItemCaseSensitive(json,"x");auto y=cJSON_GetObjectItemCaseSensitive(json,"y");
    bool valid=cJSON_IsNumber(seq)&&seq->valuedouble>=1&&seq->valuedouble<=2147483647&&seq->valuedouble==seq->valueint&&
        cJSON_IsNumber(x)&&cJSON_IsNumber(y)&&std::isfinite(x->valuedouble)&&std::isfinite(y->valuedouble);
    bool ok=valid&&self->hub_.Move(session->owner,seq->valueint,x->valuedouble,y->valuedouble);
    if(valid)WifiStation::GetInstance().DeferBackgroundScan();
    cJSON_Delete(json);
    const char* reply=ok?"{\"ok\":true}":"{\"ok\":false}";
    httpd_ws_frame_t ack{};ack.type=HTTPD_WS_TYPE_TEXT;ack.payload=reinterpret_cast<uint8_t*>(const_cast<char*>(reply));ack.len=strlen(reply);
    return httpd_ws_send_frame(req,&ack);
}
esp_err_t HubServer::Snapshot(httpd_req_t* req) {
    auto* self=static_cast<HubServer*>(req->user_ctx);
    if(!self->QueryToken(req))return httpd_resp_send_err(req,HTTPD_403_FORBIDDEN,"Invalid token");
    WifiStation::GetInstance().DeferBackgroundScan();
    std::vector<uint8_t> jpeg;
    if(!self->hub_.Camera()->Snapshot(jpeg)) {
        httpd_resp_set_status(req,"503 Service Unavailable");
        return httpd_resp_send(req,"Camera unavailable",HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_set_type(req,"image/jpeg");httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_send(req,reinterpret_cast<const char*>(jpeg.data()),jpeg.size());
}
void HubServer::Start() {
    char token[33];snprintf(token,sizeof(token),"%08lx%08lx%08lx%08lx",static_cast<unsigned long>(esp_random()),
        static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()));token_=token;
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();cfg.max_uri_handlers=6;cfg.max_open_sockets=5;cfg.stack_size=6144;
    cfg.recv_wait_timeout=2;cfg.send_wait_timeout=2;cfg.lru_purge_enable=true;
    ESP_ERROR_CHECK(httpd_start(&server_,&cfg));
    auto route=[this](const char* uri,httpd_method_t method,esp_err_t(*fn)(httpd_req_t*),bool ws=false){
        httpd_uri_t def{};def.uri=uri;def.method=method;def.handler=fn;def.user_ctx=this;def.is_websocket=ws;
        ESP_ERROR_CHECK(httpd_register_uri_handler(server_,&def));};
    route("/",HTTP_GET,Page);route("/api/status",HTTP_GET,Status);route("/api/action",HTTP_POST,Action);route("/ws",HTTP_GET,Socket,true);
    route("/xiaobai.apk",HTTP_GET,AndroidPackage);
    cfg.server_port=81;cfg.ctrl_port=32769;cfg.max_uri_handlers=2;cfg.max_open_sockets=2;cfg.stack_size=8192;
    ESP_ERROR_CHECK(httpd_start(&camera_server_,&cfg));
    httpd_uri_t snapshot{};snapshot.uri="/snapshot";snapshot.method=HTTP_GET;snapshot.handler=Snapshot;snapshot.user_ctx=this;
    ESP_ERROR_CHECK(httpd_register_uri_handler(camera_server_,&snapshot));
    ESP_LOGI("HubServer","Control http://192.168.4.1 | camera port 81");
    xTaskCreate([](void* arg){static_cast<HubServer*>(arg)->DiscoveryLoop();},"hub_discovery",3072,this,2,nullptr);
}

void HubServer::DiscoveryLoop() {
    // Unicast reply to the requesting client. No control token in discovery.
    while(true) {
        int fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if(fd<0){vTaskDelay(pdMS_TO_TICKS(5000));continue;}
        sockaddr_in local{};local.sin_family=AF_INET;local.sin_port=htons(42107);local.sin_addr.s_addr=htonl(INADDR_ANY);
        if(bind(fd,reinterpret_cast<sockaddr*>(&local),sizeof(local))<0){close(fd);vTaskDelay(pdMS_TO_TICKS(5000));continue;}
        int64_t last_reply=0;
        while(true){
            char buffer[64]{};sockaddr_in peer{};socklen_t length=sizeof(peer);
            int count=recvfrom(fd,buffer,sizeof(buffer),0,reinterpret_cast<sockaddr*>(&peer),&length);
            if(count<0)break;
            static constexpr char probe[]="XIAOBAI_DISCOVER_V1";
            if(count!=sizeof(probe)-1||memcmp(buffer,probe,count))continue;
            int64_t now=esp_timer_get_time();if(now-last_reply<100000)continue;last_reply=now;
            auto identity=hub_.Identity();char* json=cJSON_PrintUnformatted(identity);cJSON_Delete(identity);
            if(json){sendto(fd,json,strlen(json),0,reinterpret_cast<sockaddr*>(&peer),length);cJSON_free(json);}
        }
        close(fd);vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
