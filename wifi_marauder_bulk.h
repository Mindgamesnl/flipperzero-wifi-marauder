#include "wifi_marauder_app_i.h"
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

typedef struct FoundAccessPoint {
    // string of the ID
    char* id;
    // string of the SSID
    char* ssid;
} WifiMarauderAccessPoint;

void parseAccessPoint(const char* input, WifiMarauderAccessPoint* ap);
char* custom_strtok(char* str, const char* delim, char** save_ptr);


void unsignedStringToChar(uint8_t* unsignedStr, char** charStr, size_t len);
void handle_scan_backfeed(uint8_t* buf, size_t len, void* context);

char* generateMessage(int numAccessPoints, WifiMarauderAccessPoint* accessPoints);

char* getLastData(const char* input);

void scan_and_select_all(WifiMarauderApp* app);