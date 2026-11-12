#include "network.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <esp_system.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <math.h>

#include "temperature.h"

namespace {
constexpr char PREFS_NAMESPACE[] = "network";
constexpr char AP_PASSWORD_KEY[] = "ap_pass";
constexpr char AP_PASSWORD_ENABLED_KEY[] = "ap_protect";
constexpr char WIFI_ENABLED_KEY[] = "wifi_enabled";
constexpr char ALLOW_UNSECURE_KEY[] = "allow_unsecure";
constexpr uint32_t SERVER_POLL_MS = 10000;
constexpr uint32_t WIFI_RETRY_MS = 15000;
constexpr uint32_t INTERNET_CHECK_MS = 30000;
constexpr float REMOTE_SETPOINT_MIN_C = 5.0f;
constexpr float REMOTE_SETPOINT_MAX_C = 35.0f;

struct NetworkConfig {
    String ssid;
    String wifi_password;
    String server_url;
    String auth_token;
    String root_ca;
    String ap_password;
};

struct Telemetry {
    float room_c;
    float floor_c;
    float setpoint_c;
};

NetworkConfig config;
bool config_loaded = false;
std::atomic<bool> portal_enabled(false);
std::atomic<bool> ap_password_enabled(false);
std::atomic<bool> wifi_feature_enabled(true);
std::atomic<bool> allow_unsecure_connection(false);
std::atomic<bool> setup_ap_available(false);
std::atomic<bool> wifi_connecting(false);
std::atomic<bool> wifi_connected(false);
std::atomic<bool> internet_accessible(false);
std::atomic<int> server_http_status(0);
portMUX_TYPE telemetry_mux = portMUX_INITIALIZER_UNLOCKED;
Telemetry latest_telemetry = {NAN, NAN, NAN};
QueueHandle_t command_queue = nullptr;
WebServer setup_server(IPAddress(192, 168, 4, 1), 80);
bool setup_routes_registered = false;
String setup_ssid;

String hardware_id()
{
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    return mac.substring(mac.length() > 6 ? mac.length() - 6 : 0);
}

void load_config()
{
    if (config_loaded) return;

    Preferences prefs;
    // Open read/write so first boot creates the namespace instead of logging
    // NVS NOT_FOUND when Preferences tries to open a missing read-only namespace.
    prefs.begin(PREFS_NAMESPACE, false);
    config.ssid = prefs.isKey("ssid") ? prefs.getString("ssid", "") : "";
    config.wifi_password = prefs.isKey("wifi_pass") ? prefs.getString("wifi_pass", "") : "";
    config.server_url = prefs.isKey("url") ? prefs.getString("url", "") : "";
    config.auth_token = prefs.isKey("token") ? prefs.getString("token", "") : "";
    config.root_ca = prefs.isKey("root_ca") ? prefs.getString("root_ca", "") : "";
    config.ap_password = prefs.isKey(AP_PASSWORD_KEY) ? prefs.getString(AP_PASSWORD_KEY, "") : "";
    portal_enabled.store(prefs.isKey("portal") ? prefs.getBool("portal", false) : false);
    ap_password_enabled.store(prefs.isKey(AP_PASSWORD_ENABLED_KEY) ?
                              prefs.getBool(AP_PASSWORD_ENABLED_KEY, false) : false);
    wifi_feature_enabled.store(prefs.isKey(WIFI_ENABLED_KEY) ?
                               prefs.getBool(WIFI_ENABLED_KEY, true) : true);
    allow_unsecure_connection.store(prefs.isKey(ALLOW_UNSECURE_KEY) ?
                                    prefs.getBool(ALLOW_UNSECURE_KEY, false) : false);
    prefs.end();

    config_loaded = true;
}

String html_escape(const String &value)
{
    String escaped;
    escaped.reserve(value.length() + 16);
    for (size_t i = 0; i < value.length(); ++i) {
        switch (value[i]) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '\"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped += value[i]; break;
        }
    }
    return escaped;
}

bool valid_server_url(const String &url)
{
    return url.length() > 7 && url.length() <= 255 &&
           (url.startsWith("http://") || url.startsWith("https://"));
}

void send_scan_results()
{
    static bool retried_empty_scan = false;
    if (setup_server.arg("refresh") == "1") {
        WiFi.scanDelete();
        const int scan_result = WiFi.scanNetworks(true);
        retried_empty_scan = false;
        Serial.printf("Network: requested Wi-Fi rescan; start result=%d.\n", scan_result);
    }

    int found = WiFi.scanComplete();
    if (found == WIFI_SCAN_FAILED) {
        WiFi.scanDelete();
        found = WiFi.scanNetworks(true);
        Serial.printf("Network: previous Wi-Fi scan failed; restarted scan (result=%d).\n", found);
    }

    // Leave room for larger nearby network lists; ArduinoJson otherwise omits
    // entries once the document capacity is exhausted.
    StaticJsonDocument<4096> result;
    result["scanning"] = found == WIFI_SCAN_RUNNING;
    JsonArray networks = result.createNestedArray("networks");
    for (int i = 0; found >= 0 && i < found; ++i) {
        const String ssid = WiFi.SSID(i);
        if (ssid.isEmpty()) continue;
        JsonObject network = networks.createNestedObject();
        network["ssid"] = ssid;
        network["rssi"] = WiFi.RSSI(i);
    }
    if (found == 0 && !retried_empty_scan) {
        retried_empty_scan = true;
        const int scan_result = WiFi.scanNetworks(true);
        result["scanning"] = scan_result == WIFI_SCAN_RUNNING;
        Serial.printf("Network: Wi-Fi scan found no networks; retrying (result=%d).\n", scan_result);
    } else if (found > 0) {
        retried_empty_scan = false;
    }
    if (found >= 0)
        Serial.printf("Network: Wi-Fi scan complete; %d networks found.\n", found);

    String json;
    serializeJson(result, json);
    setup_server.sendHeader("Cache-Control", "no-store");
    setup_server.send(200, "application/json", json);
}

void send_setup_page()
{
    String page;
    page.reserve(6500);
    page += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
              "<title>Thermostat setup</title><style>body{font:16px sans-serif;max-width:560px;margin:2em auto;padding:0 1em}"
              "input,select,textarea,button{box-sizing:border-box;width:100%;padding:.65em;margin:.35em 0 1em}"
              "textarea{height:9em}button{background:#1677d2;color:white;border:0}</style>");
    page += F("</head><body>"
              "<h1>Thermostat setup</h1><form method='post' action='/save'>"
              "<label>Wi-Fi network</label><select id='ssid' name='ssid' data-saved-ssid='");
    page += html_escape(config.ssid);
    page += F("' required><option value=''>Scanning for networks...</option></select>"
              "<button type='button' id='rescan'>Rescan Wi-Fi networks</button>"
              "<div id='scan_status' aria-live='polite'>Scanning for Wi-Fi networks...</div>");

    page += F("<label>Wi-Fi password</label><input name='wifi_password' type='password' autocomplete='new-password' "
              "placeholder='Leave blank to keep saved password'>"
              "<label>Server URL (HTTP POST endpoint)</label><input name='url' type='url' value='");
    page += html_escape(config.server_url);
    page += F("' required><label>Optional bearer token</label><input name='token' type='password' autocomplete='new-password' "
              "placeholder='Leave blank to keep saved token'><details><summary>HTTPS certificate (PEM, optional)</summary>"
              "<p>Required for HTTPS. Leave blank to keep the saved certificate.</p><textarea name='root_ca'></textarea></details>"
              "<button type='submit'>Save and connect</button></form>"
              "<p>After saving, the device will connect to Wi-Fi and POST temperature data every 10 seconds.</p>"
              "<script>const s=document.getElementById('ssid'),st=document.getElementById('scan_status');"
              "const saved=s.dataset.savedSsid;let timer=0;"
              "async function scan(refresh=false){try{const r=await fetch('/scan'+(refresh?'?refresh=1':''),{cache:'no-store'});"
              "const d=await r.json();if(d.scanning){st.textContent='Scanning for Wi-Fi networks...';timer=setTimeout(()=>scan(),1500);return;}"
              "const old=s.value||saved;s.innerHTML='';const p=document.createElement('option');p.value='';p.textContent='Select a Wi-Fi network';s.appendChild(p);"
              "for(const n of d.networks){const o=document.createElement('option');o.value=n.ssid;"
              "o.textContent=n.ssid+' (RSSI '+n.rssi+' dBm)';s.appendChild(o);}"
              "if([...s.options].some(o=>o.value===old))s.value=old;"
              "st.textContent=d.networks.length?d.networks.length+' network(s) found':'No networks found. Try rescanning.';"
              "}catch(e){st.textContent='Could not scan for networks. Try rescanning.';}}"
              "document.getElementById('rescan').addEventListener('click',()=>{clearTimeout(timer);st.textContent='Scanning for Wi-Fi networks...';scan(true);});"
              "scan();</script></body></html>");
    setup_server.sendHeader("Cache-Control", "no-store");
    setup_server.send(200, "text/html; charset=utf-8", page);
}

void save_setup()
{
    const String new_ssid = setup_server.arg("ssid");
    const String new_url = setup_server.arg("url");
    if (new_ssid.isEmpty() || new_ssid.length() > 32 || !valid_server_url(new_url)) {
        setup_server.send(400, "text/plain", "SSID or server URL is invalid.");
        return;
    }

    const String supplied_password = setup_server.arg("wifi_password");
    const String supplied_token = setup_server.arg("token");
    const String supplied_ca = setup_server.arg("root_ca");
    if (supplied_password.length() > 63 || supplied_token.length() > 128 || supplied_ca.length() > 3000) {
        setup_server.send(400, "text/plain", "A configuration value is too long.");
        return;
    }

    if (!supplied_password.isEmpty() || new_ssid != config.ssid)
        config.wifi_password = supplied_password;
    if (!supplied_token.isEmpty()) config.auth_token = supplied_token;
    if (!supplied_ca.isEmpty()) config.root_ca = supplied_ca;
    config.ssid = new_ssid;
    config.server_url = new_url;

    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putString("ssid", config.ssid);
    prefs.putString("wifi_pass", config.wifi_password);
    prefs.putString("url", config.server_url);
    prefs.putString("token", config.auth_token);
    prefs.putString("root_ca", config.root_ca);
    prefs.putBool("portal", false);
    prefs.end();

    setup_server.send(200, "text/html; charset=utf-8",
        "<!doctype html><html><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<h1>Saved</h1><p>Wi-Fi settings saved. The setup access point is shutting down while the device connects to your Wi-Fi network.</p>"
        "<p>You can close this page.</p></html>");
    delay(500); // Allow the confirmation page to reach the phone before stopping the AP.
    portal_enabled.store(false);
    Serial.println("Network: setup AP disabled after saving; switching to configured Wi-Fi.");
}

void start_setup_server()
{
    setup_ssid = "CYD-Thermostat-" + hardware_id();
    // Setup runs in AP-only mode. This leaves the radio free to scan for routers.
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_AP_STA);
    const IPAddress ap_ip(192, 168, 4, 1);
    WiFi.softAPConfig(ap_ip, ap_ip, IPAddress(255, 255, 255, 0));
    if (ap_password_enabled.load()) {
        if (config.ap_password.length() < 8) {
            char generated_password[17];
            snprintf(generated_password, sizeof(generated_password), "%08lx%08lx",
                     static_cast<unsigned long>(esp_random()),
                     static_cast<unsigned long>(esp_random()));
            config.ap_password = generated_password;
            Preferences prefs;
            prefs.begin(PREFS_NAMESPACE, false);
            prefs.putString(AP_PASSWORD_KEY, config.ap_password);
            prefs.end();
        }
    }
    const bool ap_started = ap_password_enabled.load() ?
        WiFi.softAP(setup_ssid.c_str(), config.ap_password.c_str()) :
        WiFi.softAP(setup_ssid.c_str());
    setup_ap_available.store(ap_started);
    const int scan_result = WiFi.scanNetworks(true);
    Serial.printf("Network: started Wi-Fi scan for setup (result=%d).\n", scan_result);

    if (!setup_routes_registered) {
        setup_server.on("/", HTTP_GET, send_setup_page);
        setup_server.on("/scan", HTTP_GET, send_scan_results);
        setup_server.on("/save", HTTP_POST, save_setup);
        setup_server.onNotFound([]() {
            setup_server.sendHeader("Location", "http://192.168.4.1/");
            setup_server.send(302, "text/plain", "Opening setup page");
        });
        setup_routes_registered = true;
    }
    setup_server.begin();
    Serial.printf("Setup AP start=%s IP=%s SSID=%s\n", ap_started ? "ok" : "failed",
                  WiFi.softAPIP().toString().c_str(), setup_ssid.c_str());
    if (ap_password_enabled.load())
        Serial.printf("Setup AP password: %s\n", config.ap_password.c_str());
    else
        Serial.println("Setup AP is open (no password).");
    Serial.println("Setup page: http://192.168.4.1/");
}

void stop_setup_server()
{
    setup_server.stop();
    WiFi.softAPdisconnect(true);
    setup_ap_available.store(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
}

void check_internet_connectivity()
{
    if (!wifi_feature_enabled.load() || WiFi.status() != WL_CONNECTED) {
        internet_accessible.store(false);
        return;
    }

    HTTPClient http;
    http.setConnectTimeout(2500);
    http.setTimeout(2500);
    if (!http.begin("http://connectivitycheck.gstatic.com/generate_204")) {
        internet_accessible.store(false);
        return;
    }
    const int status = http.GET();
    internet_accessible.store(status == 204);
    http.end();
}

void post_telemetry()
{
    static bool logged_missing_url = false;
    static bool logged_setup_ap_suppression = false;
    if (portal_enabled.load() || setup_ap_available.load()) {
        server_http_status.store(0);
        if (!logged_setup_ap_suppression)
            Serial.println("Network: server POST skipped while Wi-Fi setup AP is enabled.");
        logged_setup_ap_suppression = true;
        return;
    }
    logged_setup_ap_suppression = false;

    if (!wifi_feature_enabled.load()) {
        server_http_status.store(0);
        Serial.println("Network: server POST skipped; Wi-Fi is disabled.");
        return;
    }
    if (config.server_url.isEmpty()) {
        server_http_status.store(0);
        if (!logged_missing_url)
            Serial.println("Network: server POST skipped; no server URL is configured.");
        logged_missing_url = true;
        return;
    }
    logged_missing_url = false;
    if (WiFi.status() != WL_CONNECTED) {
        server_http_status.store(0);
        Serial.printf("Network: server POST skipped; Wi-Fi is not connected (status %d).\n",
                      static_cast<int>(WiFi.status()));
        return;
    }

    Telemetry telemetry;
    portENTER_CRITICAL(&telemetry_mux);
    telemetry = latest_telemetry;
    portEXIT_CRITICAL(&telemetry_mux);

    StaticJsonDocument<256> request;
    request["device_id"] = hardware_id();
    JsonObject values = request.createNestedObject("temperatures_c");
    if (isfinite(telemetry.room_c) && telemetry.room_c != TEMP_SENSOR_NOT_CONNECTED)
        values["room"] = telemetry.room_c;
    else
        values["room"] = nullptr;
    if (isfinite(telemetry.floor_c) && telemetry.floor_c != TEMP_SENSOR_NOT_CONNECTED)
        values["floor"] = telemetry.floor_c;
    else
        values["floor"] = nullptr;
    if (isfinite(telemetry.setpoint_c)) values["setpoint"] = telemetry.setpoint_c;
    else values["setpoint"] = nullptr;

    String body;
    serializeJson(request, body);
    Serial.printf("Network: telemetry payload (%u bytes): %s\n",
                  static_cast<unsigned>(body.length()), body.c_str());

    HTTPClient http;
    WiFiClientSecure unsecure_client;
    http.setConnectTimeout(3000);
    http.setTimeout(3000);
    bool started = false;
    if (config.server_url.startsWith("https://")) {
        if (config.root_ca.isEmpty()) {
            if (!allow_unsecure_connection.load()) {
                Serial.println("Network: HTTPS request skipped; no root CA PEM is saved and unsecure connections are disabled.");
                Serial.println("Network: nothing was sent. Add the server's root CA in the setup page, or enable 'Allow unsecure connection'.");
                server_http_status.store(0);
                return;
            }
            Serial.println("Network: WARNING: HTTPS certificate verification is disabled by setting.");
            unsecure_client.setInsecure();
            started = http.begin(unsecure_client, config.server_url);
        } else {
            Serial.printf("Network: starting HTTPS request with saved root CA (%u bytes).\n",
                          static_cast<unsigned>(config.root_ca.length()));
            started = http.begin(config.server_url, config.root_ca.c_str());
        }
    } else {
        Serial.println("Network: starting HTTP request (unencrypted).");
        started = http.begin(config.server_url);
    }
    if (!started) {
        Serial.println("Network: HTTPClient could not start the request; no server response was received.");
        server_http_status.store(0);
        return;
    }

    http.addHeader("Content-Type", "application/json");
    if (!config.auth_token.isEmpty())
        http.addHeader("Authorization", "Bearer " + config.auth_token);
    const bool is_https = config.server_url.startsWith("https://");
    if (is_https) {
        Serial.printf("Network: heap before TLS request: free=%u largest-block=%u min-free=%u bytes.\n",
                      static_cast<unsigned>(ESP.getFreeHeap()),
                      static_cast<unsigned>(ESP.getMaxAllocHeap()),
                      static_cast<unsigned>(ESP.getMinFreeHeap()));
    }
    Serial.println("Network: sending telemetry POST; waiting up to 3 seconds for server response.");
    const uint32_t request_started_ms = millis();
    const int status = http.POST(body);
    server_http_status.store(status > 0 ? status : 0);
    if (is_https) {
        Serial.printf("Network: heap after TLS request: free=%u largest-block=%u bytes.\n",
                      static_cast<unsigned>(ESP.getFreeHeap()),
                      static_cast<unsigned>(ESP.getMaxAllocHeap()));
    }
    if (status > 0) {
        const String response = http.getString();
        Serial.printf("Network: server responded HTTP %d in %lu ms; body length %u bytes.\n",
                      status, static_cast<unsigned long>(millis() - request_started_ms),
                      static_cast<unsigned>(response.length()));
        Serial.printf("Network: response body: %s\n",
                      response.isEmpty() ? "<empty>" : response.c_str());
        if (status >= 300 && status < 400) {
            const String location = http.getLocation();
            Serial.printf("Network: redirect requested (HTTP %d) to: %s\n",
                          status, location.isEmpty() ? "<no Location header>" : location.c_str());
            Serial.println("Network: redirects are not followed for telemetry POSTs; configure the final endpoint URL.");
        }
        StaticJsonDocument<256> reply;
        const DeserializationError error = deserializeJson(reply, response);
        if (status >= 200 && status < 300 && !error) {
            ServerCommand command = {};
            const JsonVariantConst setpoint = reply["setpoint_c"];
            if (setpoint.is<float>()) {
                const float value = setpoint.as<float>();
                if (isfinite(value) && value >= REMOTE_SETPOINT_MIN_C && value <= REMOTE_SETPOINT_MAX_C) {
                    command.has_setpoint = true;
                    command.setpoint_c = value;
                }
            }
            const JsonVariantConst enabled = reply["enabled"];
            if (enabled.is<bool>()) {
                command.has_enabled = true;
                command.enabled = enabled.as<bool>();
            }
            if (command.has_setpoint || command.has_enabled) {
                if (command_queue) xQueueOverwrite(command_queue, &command);
            }
        } else if (status < 200 || status >= 300) {
            Serial.printf("Network: server returned HTTP %d.\n", status);
        } else {
            Serial.println("Network: response JSON is invalid.");
        }
    } else {
        Serial.printf("Network: no server response after %lu ms; HTTPClient error %d: %s\n",
                      static_cast<unsigned long>(millis() - request_started_ms), status,
                      http.errorToString(status).c_str());
    }
    http.end();
}

void network_task(void *)
{
    load_config();
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    bool wifi_running = wifi_feature_enabled.load();
    bool portal_running = false;
    bool active_ap_password_enabled = false;
    bool station_started = false;
    uint32_t last_connect_attempt = 0;
    uint32_t last_post = millis();
    uint32_t last_internet_check = millis() - INTERNET_CHECK_MS;

    if (wifi_running) {
        if (portal_enabled.load()) {
            start_setup_server();
            portal_running = true;
            active_ap_password_enabled = ap_password_enabled.load();
            station_started = false;
        } else {
            WiFi.mode(WIFI_STA);
            if (!config.ssid.isEmpty()) {
                WiFi.begin(config.ssid.c_str(), config.wifi_password.c_str());
                station_started = true;
            }
        }
    } else {
        WiFi.mode(WIFI_OFF);
    }

    for (;;) {
        const bool should_run_wifi = wifi_feature_enabled.load();
        if (should_run_wifi != wifi_running) {
            if (!should_run_wifi) {
                if (portal_running) stop_setup_server();
                WiFi.disconnect(false, false);
                WiFi.mode(WIFI_OFF);
                portal_running = false;
                wifi_running = false;
                station_started = false;
                setup_ap_available.store(false);
                wifi_connecting.store(false);
                wifi_connected.store(false);
                internet_accessible.store(false);
                server_http_status.store(0);
            } else {
                wifi_running = true;
                station_started = false;
                last_connect_attempt = 0;
                if (portal_enabled.load()) {
                    start_setup_server();
                    portal_running = true;
                    active_ap_password_enabled = ap_password_enabled.load();
                    station_started = false;
                } else {
                    WiFi.mode(WIFI_STA);
                }
            }
        }

        if (wifi_running) {
            const bool should_run_portal = portal_enabled.load();
            if (should_run_portal != portal_running) {
                if (should_run_portal) start_setup_server();
                else stop_setup_server();
                portal_running = should_run_portal;
                active_ap_password_enabled = ap_password_enabled.load();
            } else if (portal_running && ap_password_enabled.load() != active_ap_password_enabled) {
                stop_setup_server();
                start_setup_server();
                active_ap_password_enabled = ap_password_enabled.load();
            }

            if (portal_running) setup_server.handleClient();

            const bool connected = WiFi.status() == WL_CONNECTED;
            wifi_connected.store(connected);
            wifi_connecting.store(!connected && !config.ssid.isEmpty());
            if (!connected) {
                internet_accessible.store(false);
                server_http_status.store(0);
            }

            if (!portal_running && !config.ssid.isEmpty() && !connected &&
                (!station_started || millis() - last_connect_attempt >= WIFI_RETRY_MS)) {
                WiFi.begin(config.ssid.c_str(), config.wifi_password.c_str());
                station_started = true;
                last_connect_attempt = millis();
            }

            if (millis() - last_post >= SERVER_POLL_MS) {
                last_post = millis();
                post_telemetry();
            }

            if (!portal_running && connected && millis() - last_internet_check >= INTERNET_CHECK_MS) {
                last_internet_check = millis();
                check_internet_connectivity();
            }
        } else {
            setup_ap_available.store(false);
            wifi_connecting.store(false);
            wifi_connected.store(false);
            internet_accessible.store(false);
            server_http_status.store(0);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
} // namespace

bool network_setup_portal_enabled(void)
{
    load_config();
    return portal_enabled.load();
}

bool network_wifi_enabled(void)
{
    load_config();
    return wifi_feature_enabled.load();
}

void network_set_wifi_enabled(bool enabled)
{
    load_config();
    wifi_feature_enabled.store(enabled);
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putBool(WIFI_ENABLED_KEY, enabled);
    prefs.end();
}

void network_set_setup_portal_enabled(bool enabled)
{
    load_config();
    portal_enabled.store(enabled);
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putBool("portal", enabled);
    prefs.end();
}

bool network_setup_ap_password_enabled(void)
{
    load_config();
    return ap_password_enabled.load();
}

bool network_allow_unsecure_connection(void)
{
    load_config();
    return allow_unsecure_connection.load();
}

void network_set_allow_unsecure_connection(bool enabled)
{
    load_config();
    allow_unsecure_connection.store(enabled);
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putBool(ALLOW_UNSECURE_KEY, enabled);
    prefs.end();
}

void network_set_setup_ap_password_enabled(bool enabled)
{
    load_config();
    ap_password_enabled.store(enabled);
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putBool(AP_PASSWORD_ENABLED_KEY, enabled);
    prefs.end();
}

void network_update_telemetry(float room_c, float floor_c, float setpoint_c)
{
    portENTER_CRITICAL(&telemetry_mux);
    latest_telemetry.room_c = room_c;
    latest_telemetry.floor_c = floor_c;
    latest_telemetry.setpoint_c = setpoint_c;
    portEXIT_CRITICAL(&telemetry_mux);
}

bool network_take_server_command(ServerCommand *command)
{
    return command_queue && xQueueReceive(command_queue, command, 0) == pdTRUE;
}

void network_get_status(NetworkStatus *status)
{
    if (!status) return;
    status->wifi_enabled = wifi_feature_enabled.load();
    status->setup_ap_available = setup_ap_available.load();
    status->wifi_connecting = wifi_connecting.load();
    status->wifi_connected = wifi_connected.load();
    status->internet_accessible = internet_accessible.load();
    status->server_http_status = server_http_status.load();
}

void network_init(void)
{
    load_config();
    command_queue = xQueueCreate(1, sizeof(ServerCommand));
    if (!command_queue) {
        Serial.println("Network: could not create command queue.");
        return;
    }
    xTaskCreate(network_task, "network", 8192, nullptr, 1, nullptr);
}
