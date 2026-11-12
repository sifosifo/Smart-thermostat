#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

#include <Arduino.h>

struct ServerCommand {
    bool has_setpoint;
    float setpoint_c;
    bool has_enabled;
    bool enabled;
};

struct NetworkStatus {
    bool wifi_enabled;
    bool setup_ap_available;
    bool wifi_connecting;
    bool wifi_connected;
    bool internet_accessible;
    int server_http_status;
};

void network_init(void);
bool network_wifi_enabled(void);
void network_set_wifi_enabled(bool enabled);
bool network_setup_portal_enabled(void);
void network_set_setup_portal_enabled(bool enabled);
bool network_setup_ap_password_enabled(void);
void network_set_setup_ap_password_enabled(bool enabled);
bool network_allow_unsecure_connection(void);
void network_set_allow_unsecure_connection(bool enabled);
void network_update_telemetry(float room_c, float floor_c, float setpoint_c);
bool network_take_server_command(ServerCommand *command);
void network_get_status(NetworkStatus *status);

#endif
