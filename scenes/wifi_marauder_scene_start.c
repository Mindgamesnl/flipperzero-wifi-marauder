//** Includes sniffbt and sniffskim for compatible ESP32-WROOM hardware.
//wifi_marauder_app_i.h also changed **//
#include "../wifi_marauder_app_i.h"
#include <stdbool.h>
#include <string.h>

// For each command, define whether additional arguments are needed
// (enabling text input to fill them out), and whether the console
// text box should focus at the start of the output or the end
typedef enum { NO_ARGS = 0, INPUT_ARGS, TOGGLE_ARGS } InputArgs;

typedef enum { FOCUS_CONSOLE_END = 0, FOCUS_CONSOLE_START, FOCUS_CONSOLE_TOGGLE } FocusConsole;

#define SHOW_STOPSCAN_TIP (true)
#define NO_TIP (false)

#define MAX_OPTIONS (9)

typedef struct {
    const char* item_string;
    const char* options_menu[MAX_OPTIONS + 1];  // Add 1 for the custom option
    int num_options_menu;
    const char* actual_commands[MAX_OPTIONS];
    InputArgs needs_keyboard;
    FocusConsole focus_console;
    bool show_stopscan_tip;
    void (*custom_code)(WifiMarauderApp*);  // Function pointer for the custom code to be executed
} WifiMarauderItem;


typedef struct foundAccessPoint {
    // string of the ID
    char* id;
    // string of the SSID
    char* ssid;
} WifiMarauderAccessPoint;

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
    const int MAX_MESSAGE_LENGTH = 1000; // Maximum length of the generated message
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
            return NULL;
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

    // stopscan
    text_box_set_text(app->text_box, "Stopping scan\n");
    wifi_marauder_uart_tx((uint8_t*)("stopscan\n"), strlen("stopscan\n"));

    furi_delay_ms(500);

    text_box_set_text(app->text_box, "Parsing output\n");


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


// NUM_MENU_ITEMS defined in wifi_marauder_app_i.h - if you add an entry here, increment it!
const WifiMarauderItem items[NUM_MENU_ITEMS] = {
    {"View Log from", {"start", "end"}, 2, {"", ""}, NO_ARGS, FOCUS_CONSOLE_TOGGLE, NO_TIP, NULL},
    {"Scan",
     {"ap", "station"},
     2,
     {"scanap", "scansta"},
     NO_ARGS,
     FOCUS_CONSOLE_END,
     SHOW_STOPSCAN_TIP,
     NULL},
    {"SSID",
     {"add rand", "add name", "remove"},
     3,
     {"ssid -a -g", "ssid -a -n", "ssid -r"},
     INPUT_ARGS,
     FOCUS_CONSOLE_START,
     NO_TIP,
     NULL},
    {"List",
     {"ap", "ssid", "station"},
     3,
     {"list -a", "list -s", "list -c"},
     NO_ARGS,
     FOCUS_CONSOLE_START,
     NO_TIP,
     NULL},
    {"Select",
     {"ap", "ssid", "station"},
     3,
     {"select -a", "select -s", "select -c"},
     INPUT_ARGS,
     FOCUS_CONSOLE_END,
     NO_TIP,
     NULL},
    {"Clear List",
     {"ap", "ssid", "station"},
     3,
     {"clearlist -a", "clearlist -s", "clearlist -c"},
     NO_ARGS,
     FOCUS_CONSOLE_END,
     NO_TIP,
     NULL},
    {"Attack",
     {"deauth", "probe", "rickroll"},
     3,
     {"attack -t deauth", "attack -t probe", "attack -t rickroll"},
     NO_ARGS,
     FOCUS_CONSOLE_END,
     SHOW_STOPSCAN_TIP,
     NULL},
    {"Targeted Deauth",
     {"station", "manual"},
     2,
     {"attack -t deauth -c", "attack -t deauth -s"},
     TOGGLE_ARGS,
     FOCUS_CONSOLE_END,
     SHOW_STOPSCAN_TIP,
     NULL},
    {"Beacon Spam",
     {"ap list", "ssid list", "random"},
     3,
     {"attack -t beacon -a", "attack -t beacon -l", "attack -t beacon -r"},
     NO_ARGS,
     FOCUS_CONSOLE_END,
     SHOW_STOPSCAN_TIP,
     NULL},
    {"Sniff",
     {"beacon", "deauth", "esp", "pmkid", "probe", "pwn", "raw", "bt", "skim"},
     9,
     {"sniffbeacon",
      "sniffdeauth",
      "sniffesp",
      "sniffpmkid",
      "sniffprobe",
      "sniffpwn",
      "sniffraw",
      "sniffbt",
      "sniffskim"},
     NO_ARGS,
     FOCUS_CONSOLE_END,
     SHOW_STOPSCAN_TIP,
     NULL},
    {"Signal Monitor", {""}, 1, {"sigmon"}, NO_ARGS, FOCUS_CONSOLE_END, SHOW_STOPSCAN_TIP, NULL},
    {"Channel",
     {"get", "set"},
     2,
     {"channel", "channel -s"},
     TOGGLE_ARGS,
     FOCUS_CONSOLE_END,
     NO_TIP, NULL},
    {"Settings",
     {"display", "restore", "ForcePMKID", "ForceProbe", "SavePCAP", "EnableLED", "other"},
     7,
     {"settings",
      "settings -r",
      "settings -s ForcePMKID enable",
      "settings -s ForceProbe enable",
      "settings -s SavePCAP enable",
      "settings -s EnableLED enable",
      "settings -s"},
     TOGGLE_ARGS,
     FOCUS_CONSOLE_START,
     NO_TIP, NULL},
    {"Update", {"ota", "sd"}, 2, {"update -w", "update -s"}, NO_ARGS, FOCUS_CONSOLE_END, NO_TIP, NULL},
    {"Reboot", {""}, 1, {"reboot"}, NO_ARGS, FOCUS_CONSOLE_END, NO_TIP, NULL},
    {"Help", {""}, 1, {"help"}, NO_ARGS, FOCUS_CONSOLE_START, SHOW_STOPSCAN_TIP, NULL},
    {"Scripts", {""}, 1, {""}, NO_ARGS, FOCUS_CONSOLE_END, NO_TIP, NULL},
    {"Save to flipper sdcard", // keep as last entry or change logic in callback below
     {""},
     1,
     {""},
     NO_ARGS,
     FOCUS_CONSOLE_START,
     NO_TIP,NULL},
    {"Select All", {""}, 1, {""}, NO_ARGS, FOCUS_CONSOLE_START, NO_TIP, &scan_and_select_all},
};

static void wifi_marauder_scene_start_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    WifiMarauderApp* app = context;

    furi_assert(index < NUM_MENU_ITEMS);
    const WifiMarauderItem* item = &items[index];

    const int selected_option_index = app->selected_option_index[index];
    furi_assert(selected_option_index < item->num_options_menu);
    app->selected_tx_string = item->actual_commands[selected_option_index];
    app->is_command = (1 <= index);
    app->is_custom_tx_string = false;
    app->selected_menu_index = index;
    app->focus_console_start = (item->focus_console == FOCUS_CONSOLE_TOGGLE) ?
                                   (selected_option_index == 0) :
                                   item->focus_console;
    app->show_stopscan_tip = item->show_stopscan_tip;

    if(!app->is_command && selected_option_index == 0) {
        // View Log from start
        view_dispatcher_send_custom_event(app->view_dispatcher, WifiMarauderEventStartLogViewer);
        return;
    }

    if(app->selected_tx_string &&
       strncmp("sniffpmkid", app->selected_tx_string, strlen("sniffpmkid")) == 0) {
        // sniffpmkid submenu
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WifiMarauderEventStartSniffPmkidOptions);
        return;
    }

    // Select automation script
    if(index == NUM_MENU_ITEMS - 2) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WifiMarauderEventStartScriptSelect);
        return;
    }

    if(index == NUM_MENU_ITEMS - 2) {
        // "Save to flipper sdcard" special case - start SettingsInit widget
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WifiMarauderEventStartSettingsInit);
        return;
    }

    // does it have custom code?
    if(item->custom_code) {
        item->custom_code(app);
        return;
    }

    bool needs_keyboard = (item->needs_keyboard == TOGGLE_ARGS) ? (selected_option_index != 0) :
                                                                  item->needs_keyboard;
    if(needs_keyboard) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WifiMarauderEventStartKeyboard);
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, WifiMarauderEventStartConsole);
    }
}

static void wifi_marauder_scene_start_var_list_change_callback(VariableItem* item) {
    furi_assert(item);

    WifiMarauderApp* app = variable_item_get_context(item);
    furi_assert(app);

    const WifiMarauderItem* menu_item = &items[app->selected_menu_index];
    uint8_t item_index = variable_item_get_current_value_index(item);
    furi_assert(item_index < menu_item->num_options_menu);
    variable_item_set_current_value_text(item, menu_item->options_menu[item_index]);
    app->selected_option_index[app->selected_menu_index] = item_index;
}

void wifi_marauder_scene_start_on_enter(void* context) {
    WifiMarauderApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;

    variable_item_list_set_enter_callback(
        var_item_list, wifi_marauder_scene_start_var_list_enter_callback, app);

    VariableItem* item;
    for(int i = 0; i < NUM_MENU_ITEMS; ++i) {
        item = variable_item_list_add(
            var_item_list,
            items[i].item_string,
            items[i].num_options_menu,
            wifi_marauder_scene_start_var_list_change_callback,
            app);
        variable_item_set_current_value_index(item, app->selected_option_index[i]);
        variable_item_set_current_value_text(
            item, items[i].options_menu[app->selected_option_index[i]]);
    }

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, WifiMarauderSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, WifiMarauderAppViewVarItemList);

    // Wait, if the user hasn't initialized sdcard settings, let's prompt them once (then come back here)
    if(app->need_to_prompt_settings_init) {
        scene_manager_next_scene(app->scene_manager, WifiMarauderSceneSettingsInit);
    }
}

bool wifi_marauder_scene_start_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    WifiMarauderApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WifiMarauderEventStartKeyboard) {
            scene_manager_set_scene_state(
                app->scene_manager, WifiMarauderSceneStart, app->selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WifiMarauderSceneTextInput);
        } else if(event.event == WifiMarauderEventStartConsole) {
            scene_manager_set_scene_state(
                app->scene_manager, WifiMarauderSceneStart, app->selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WifiMarauderSceneConsoleOutput);
        } else if(event.event == WifiMarauderEventStartSettingsInit) {
            scene_manager_set_scene_state(
                app->scene_manager, WifiMarauderSceneStart, app->selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WifiMarauderSceneSettingsInit);
        } else if(event.event == WifiMarauderEventStartLogViewer) {
            scene_manager_set_scene_state(
                app->scene_manager, WifiMarauderSceneStart, app->selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WifiMarauderSceneLogViewer);
        } else if(event.event == WifiMarauderEventStartScriptSelect) {
            scene_manager_set_scene_state(
                app->scene_manager, WifiMarauderSceneStart, app->selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WifiMarauderSceneScriptSelect);
        } else if(event.event == WifiMarauderEventStartSniffPmkidOptions) {
            scene_manager_set_scene_state(
                app->scene_manager, WifiMarauderSceneStart, app->selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WifiMarauderSceneSniffPmkidOptions);
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {
        app->selected_menu_index = variable_item_list_get_selected_item_index(app->var_item_list);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    }

    return consumed;
}

void wifi_marauder_scene_start_on_exit(void* context) {
    WifiMarauderApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
