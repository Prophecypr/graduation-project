// #include "serial.h"

static const int RX_BUF_SIZE = 1024;

void serial_init(void);
int sendData(const char* logName, const uint8_t* data, size_t len);
