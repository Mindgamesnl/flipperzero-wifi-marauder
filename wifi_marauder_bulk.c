#include "wifi_marauder_bulk.h"
#include "wifi_marauder_app_i.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

void parseAccessPoint(const char* input, WifiMarauderAccessPoint* ap) {
    if (input == NULL || ap == NULL) {
        // Handle invalid input (e.g., return an error code or exit)
        return;
    }

    // Initialize id and ssid pointers to NULL
    ap->id = NULL;
    ap->ssid = NULL;

    // Find the "Ch: " substring
    const char* chStart = strstr(input, "Ch:");
    if (chStart != NULL) {
        // Find the space character after "Ch:"
        const char* chEnd = chStart + 3;
        while (*chEnd == ' ') {
            chEnd++;
        }

        if (*chEnd != '\0') {
            // Calculate the length of the "ch" value
            size_t chLength = strcspn(chEnd, " ");

            // Adjust chEnd to point to the end of the ID value
            chEnd += chLength;

            // Allocate memory for the ID and copy the "ch" value
            ap->id = malloc((chLength + 1) * sizeof(char));
            if (ap->id == NULL) {
                // Handle memory allocation failure
                // Add appropriate error handling (e.g., return an error code or exit)
                return;
            }
            strncpy(ap->id, chEnd - chLength, chLength);
            ap->id[chLength] = '\0';
        }
        else {
            ap->id = NULL; // Set id to NULL if "Ch:" substring is found but no value
        }
    }

    // Find the "ESSID: " substring
    const char* essidStart = strstr(input, "ESSID:");
    if (essidStart != NULL) {
        // Find the space character after "ESSID:"
        const char* essidEnd = essidStart + 6;
        while (*essidEnd == ' ') {
            essidEnd++;
        }

        if (*essidEnd != '\0') {
            // Calculate the length of the ESSID
            size_t essidLength = strlen(essidEnd);

            // Allocate memory for the SSID and copy the ESSID value
            ap->ssid = malloc((essidLength + 1) * sizeof(char));
            if (ap->ssid == NULL) {
                // Handle memory allocation failure
                // Add appropriate error handling (e.g., return an error code or exit)
                free(ap->id);  // Free previously allocated memory for ap->id
                ap->id = NULL; // Reset ap->id to NULL
                return;
            }
            strncpy(ap->ssid, essidEnd, essidLength);
            ap->ssid[essidLength] = '\0';
        }
        else {
            ap->ssid = NULL; // Set ssid to NULL if "ESSID:" substring is found but no value
        }
    }
}


// array of access points
// set numAccessPoints to 0
int numAccessPoints = 0;
WifiMarauderAccessPoint* accessPoints = NULL;

// large string of total output
char* totalOutput = "";


// Custom tokenization function
char* custom_strtok(char* str, const char* delim, char** save_ptr) {
    if (!str && !(*save_ptr)) {
        return NULL;
    }

    char* token_start;
    char* token_end;
    if (str) {
        token_start = str;
    } else {
        token_start = *save_ptr;
    }

    token_end = token_start;
    while (*token_end != '\0' && strchr(delim, *token_end) == NULL) {
        token_end++;
    }

    if (*token_end == '\0') {
        *save_ptr = NULL;
    } else {
        *token_end = '\0';
        *save_ptr = token_end + 1;
    }

    return token_start;
}

void unsignedStringToChar(uint8_t* unsignedStr, char** charStr, size_t len) {
    // Determine the character set (e.g., ASCII, UTF-8, etc.)
    // Here, we assume ASCII encoding

    // Get the length of the unsigned string
    size_t unsignedStrLen = len;

    // Allocate memory for the char* buffer
    *charStr = (char*)malloc((unsignedStrLen + 1) * sizeof(char));
    if (*charStr == NULL) {
        // Handle memory allocation failure
        printf("Memory allocation failed.\n");
        return;
    }

    // Convert unsigned string to char* (ASCII encoding)
    size_t i, j = 0;
    for (i = 0; i < unsignedStrLen; i++) {
        // Check if the character is within the ASCII range (0-127)
        if (unsignedStr[i] <= 127) {
            (*charStr)[j++] = (char)unsignedStr[i];
        }
        else {
            // Character is outside the ASCII range, substitute or skip
            // Here, we choose to substitute it with a question mark '?'
            (*charStr)[j++] = '?';
        }
    }

    // Null-terminate the char* string
    (*charStr)[j] = '\0';
}

void handle_scan_backfeed(uint8_t* buf, size_t len, void* context) {
    len = len;
    context = context;

    // use unsignedStringToChar to convert buf to a char*
    char* charBuf = '\0';
    unsignedStringToChar(buf, &charBuf, len);

    // append charBuf to totalOutput
    if (*totalOutput == '\0') {
        totalOutput = charBuf;
    } else {
        totalOutput = realloc(totalOutput, strlen(totalOutput) + strlen(charBuf) + 1);
        strcat(totalOutput, charBuf);
    }
}

char* generateMessage(int numAccessPoints, WifiMarauderAccessPoint* accessPoints) {
    const int MAX_MESSAGE_LENGTH = 5000; // Maximum length of the generated message
    const int MAX_LINE_LENGTH = 50;      // Maximum length of each line

    // Allocate memory for the message
    char* message = (char*)malloc(MAX_MESSAGE_LENGTH * sizeof(char));
    if (message == NULL) {
        // Handle memory allocation failure
        // Add appropriate error handling (e.g., return an error code or exit)
        return NULL;
    }

    // Initialize the message
    message[0] = '\0';

    // Concatenate the initial line
    strcat(message, "Found ");
    char numAccessPointsStr[MAX_LINE_LENGTH];
    snprintf(numAccessPointsStr, MAX_LINE_LENGTH, "%d", numAccessPoints);
    strcat(message, numAccessPointsStr);
    strcat(message, " access points:\n");

    // Concatenate access point details
    for (int i = 0; i < numAccessPoints; i++) {
        char indexStr[MAX_LINE_LENGTH];
        snprintf(indexStr, MAX_LINE_LENGTH, "%d", i + 1);

        // Calculate the remaining space in the message buffer
        int remainingSpace = MAX_MESSAGE_LENGTH - strlen(message);

        // Ensure enough space is available for the current line
        if (remainingSpace < MAX_LINE_LENGTH) {
            // Handle insufficient space in the message buffer
            // Add appropriate error handling (e.g., return an error code or exit)
            free(message);
            return "Insufficient space in the message buffer. Continuing without summary.\n";
        }

        // Concatenate the access point line
        strcat(message, accessPoints[i].id);
        strcat(message, ". \"");
        strcat(message, accessPoints[i].ssid);
        strcat(message, "\"\n");
    }

    return message;
}

char* getLastData(const char* input) {
    if (input == NULL) {
        // Handle invalid input (e.g., return an error code or exit)
        return NULL;
    }

    // Find the last space character in the input string
    const char* lastSpace = strrchr(input, ' ');
    if (lastSpace != NULL) {
        // Return a copy of the substring after the last space character
        return strdup(lastSpace + 1);
    }

    return NULL; // No space character found, return NULL
}

void scan_and_select_all(WifiMarauderApp* app) {

    // Reset text box and set font
    TextBox* text_box = app->text_box;
    text_box_reset(text_box);
    text_box_set_font(text_box, TextBoxFontText);
    text_box_set_focus(text_box, TextBoxFocusEnd);

    scene_manager_set_scene_state(app->scene_manager, WifiMarauderSceneConsoleOutput, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, WifiMarauderAppViewConsoleOutput);

    // send command to ESP32
    text_box_set_text(app->text_box, "Clearing list...\n");
    wifi_marauder_uart_tx((uint8_t*)("clearlist -a\n"), strlen("clearlist -a\n"));
    furi_delay_ms(500);

    wifi_marauder_uart_set_handle_rx_data_cb(app->uart, handle_scan_backfeed);
    wifi_marauder_uart_tx((uint8_t*)("scanap\n"), strlen("scanap\n"));

    int secondsRemaining = 60;
    while (secondsRemaining > 0) {
        char secondsRemainingStr[30];
        snprintf(secondsRemainingStr, 30, "Scanning for %d seconds...", secondsRemaining);
        text_box_set_text(app->text_box, secondsRemainingStr);
        furi_delay_ms(1000);
        secondsRemaining--;
    }

    wifi_marauder_uart_tx((uint8_t*)("stopscan\n"), strlen("stopscan\n"));

    furi_delay_ms(500);

    // loop over all lines in totalOutput and parse them
    char* savePtr = NULL;
    char* line = custom_strtok(totalOutput, "\n", &savePtr);
    while (line != NULL) {
        // parse line
        WifiMarauderAccessPoint ap;

        // initialize ap
        ap.ssid = NULL;
        ap.id = NULL;

        parseAccessPoint(line, &ap);

        // continue if ap.ssid is NULL or ap.id is NULL
        if (ap.ssid == NULL || ap.id == NULL) {
            // does this string start with "Beacon"?

            // fallback, is the line empty or null?

            if (strncmp(line, "Beacon", strlen("Beacon")) == 0) {
                char* last = getLastData(line);
                if (last != NULL) {
                    // make sure that we have accessPoints
                    if (accessPoints != NULL && numAccessPoints != 0) {
                        // grab the previous AP from accessPoints
                        WifiMarauderAccessPoint *prev = &accessPoints[numAccessPoints - 1];
                        // set the channel
                        prev->id = last;
                    }
                }
            }

            line = custom_strtok(NULL, "\n", &savePtr);
            continue;
        }

        // add ap to accessPoints
        if (accessPoints == NULL) {
            accessPoints = (WifiMarauderAccessPoint*)malloc(sizeof(WifiMarauderAccessPoint));
            accessPoints[0] = ap;
            numAccessPoints = 1;
        } else {
            accessPoints = realloc(accessPoints, sizeof(WifiMarauderAccessPoint) * (numAccessPoints + 1));
            accessPoints[numAccessPoints] = ap;
            numAccessPoints++;
        }

        line = custom_strtok(NULL, "\n", &savePtr);
    }

    // free totalOutput
    free(totalOutput);
    totalOutput = "";

    // make a message like
    /*
     * Found 3 access points:
     * 1. "SSID1"
     * 2. "SSID2"
     */
    char* message = generateMessage(numAccessPoints, accessPoints);
    // set text box to message
    text_box_set_text(app->text_box, message);

    // loop over all access points and subscribe, then free them
    for (int i = 0; i < numAccessPoints; i++) {
        // subscribe to access point
        char* command = (char*)malloc(sizeof(char) * (strlen("select -a ") + strlen(accessPoints[i].id) + 1));
        strcpy(command, "select -a ");
        strcat(command, accessPoints[i].id);
        strcat(command, "\n");
        wifi_marauder_uart_tx((uint8_t*)command, strlen(command));
        furi_delay_ms(50);
        free(command);

        // free access point
        free(accessPoints[i].ssid);
        free(accessPoints[i].id);
    }

    free(message);
    free(accessPoints);
    accessPoints = NULL;
    numAccessPoints = 0;

    furi_delay_ms(3000);

    // back to home
    scene_manager_set_scene_state(app->scene_manager, WifiMarauderSceneStart, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, WifiMarauderSceneStart);
}