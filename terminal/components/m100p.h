

static const int BUF_SIZE = 1024;

void uart_init();
int sendStringData(const char* logName, const char* str);
bool send_command_and_wait(const char *cmd);
void generate_mqtt_params(char *client_id, char *username, char *password);
void MQTT_cloud_data_forwarding(int mode,int temperature,int humidity);
int m100p_init();