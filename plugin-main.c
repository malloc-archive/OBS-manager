/*
Plugin Name
Copyright (C) <Year> <Developer> <Email Address>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <plugin-support.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <windows.h>
#include <psapi.h>
#include <string.h>
#include <direct.h>
#include <errno.h>
#include <stdlib.h>


OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

wchar_t playing[520] = L"No fullscreen app detected";
char *game_list[] = {"EscapeFromTarkov.exe", "s"};
int size = 2;
char real_name[256] = "";

char *concat(const char *a, const char *b)
{
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);

    char *result = (char *)malloc(len_a + len_b + 1);

    if (!result)
        return NULL;

    strcpy(result, a);
    strcat(result, b);

    return result;
}

bool contains(char *current, char *list[], int size)
{
	for (int i = 0; i < size; i++) {
		if (strcmp(current, list[i]) == 0) {
			return true;
		}
	}
	return false;
}

void get_real_name(LPWSTR path, LPWSTR name_buffer, DWORD buffer_size)
{
	LPWSTR last_backslash = wcsrchr(path, L'\\');
	if (last_backslash) {
		wcsncpy(name_buffer, last_backslash + 1, buffer_size - 1);
		name_buffer[buffer_size - 1] = L'\0';
	} else {
		wcsncpy(name_buffer, path, buffer_size - 1);
		name_buffer[buffer_size - 1] = L'\0';
	}
}

char *normalize_path(const char *path)
{
	
    if (!path || !*path)
        return NULL;
	char *temp = _strdup(path);
    size_t len = strlen(temp);

    size_t end = len;

    if (end > 0 && (temp[end - 1] == '\\' || temp[end - 1] == '/'))
        end--;

    for (size_t i = end; i > 0; i--) {
        if (temp[i - 1] == '\\' || temp[i - 1] == '/') {
            return &temp[i];
        }
    }

    return temp;
}

static void on_recording_stop(void *data, calldata_t *cd)
{
	obs_log(LOG_INFO, "recording stopped");

	const char *path = calldata_string(cd, "recording_path");
	if (!path)
		return;
	obs_log(LOG_INFO, "recording saved to: %s", path);
}

void get_fullscreen_app_name(LPWSTR name_buffer, DWORD buffer_size, char *path_buffer, DWORD path_buffer_size)
{
	HWND hwnd = GetForegroundWindow();
	if (!hwnd) {
		wcscpy(name_buffer, L"No fullscreen app detected");
		return;
	}

	RECT window_rect;
	GetWindowRect(hwnd, &window_rect);

	int screen_width = GetSystemMetrics(SM_CXSCREEN);
	int screen_height = GetSystemMetrics(SM_CYSCREEN);

	if ((window_rect.right - window_rect.left) >= screen_width &&
	    (window_rect.bottom - window_rect.top) >= screen_height) {

		DWORD process_id;
		GetWindowThreadProcessId(hwnd, &process_id);
		HANDLE process_handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);

		if (process_handle) {
			DWORD size_needed = buffer_size;

			if (!QueryFullProcessImageNameW(process_handle, 0, name_buffer, &size_needed)) {
				obs_log(LOG_ERROR, "QueryFullProcessImageNameW failed: %lu", GetLastError());

				wcscpy(name_buffer, L"Unknown");
			} else {
				get_real_name(name_buffer, name_buffer, size_needed);
				char utf8[1024];
				wcstombs(utf8, name_buffer, sizeof(utf8));
				strncpy(path_buffer, utf8, path_buffer_size - 1);
				path_buffer[path_buffer_size - 1] = '\0';
				obs_log(LOG_INFO, "Fullscreen app detected: %s", utf8);
			}
			CloseHandle(process_handle);
		} else {
			//strncpy(name_buffer, "OpenProcess failed", buffer_size - 1);
			//name_buffer[buffer_size - 1] = '\0';
			obs_log(LOG_ERROR, "Failed to open process: %lu", GetLastError());
			wcscpy(name_buffer, L"Unknown");
			name_buffer[buffer_size - 1] = L'\0';
		}
	} else {
		wcscpy(name_buffer, L"No fullscreen app detected");
		name_buffer[buffer_size - 1] = L'\0';
	}
}
static void on_recording_start(void *data, calldata_t *cd)
{
	config_t *config = obs_frontend_get_profile_config();
	
	const char *mode = config_get_string(config, "Output", "Mode"); //not sure with the section and name, need to check obs config file

	const char *path = NULL;
	

	if (strcmp(mode, "Advanced") == 0) {
    	path = config_get_string(config, "AdvOut", "RecFilePath");
	} else {
    	path = config_get_string(config, "SimpleOutput", "FilePath");
	}
	char *normalized_path = normalize_path(path);
	obs_log(LOG_INFO, "Recording dir: %s", path);

	obs_log(LOG_INFO, "Normalized recording dir: %s", normalized_path);
	char *temp_path = "";

	obs_log(LOG_INFO, "recording started");
	get_fullscreen_app_name(playing, 520, real_name, 256);
	if (strcmp(real_name, "No fullscreen app detected") == 0 || strcmp(real_name, "Unknown") == 0) {
		obs_log(LOG_INFO, "no supported fullscreen app detected");
	} else if (contains(real_name, game_list, size)) {
		obs_log(LOG_INFO, "detected supported fullscreen app: %s", real_name);
		if(strcmp(normalized_path, "temp") == 0) {
			strncpy(temp_path, path, 256);
			temp_path[255] = '\0';
			obs_log(LOG_INFO, "Recording path is already set to temp directory: %s", temp_path);
		} else {
			//strncpy(temp_path, concat(path, "\\temp"), 256);
			temp_path[255] = '\0';
			/*if(_mkdir(temp_path) == 0) {
				obs_log(LOG_INFO, "Created temp directory at: %s", temp_path);
			} else if (errno == EEXIST) {
				obs_log(LOG_INFO, "Temp directory already exists at: %s", temp_path);
			} else {
				obs_log(LOG_ERROR, "Failed to create temp directory at: %s, error: %d", path, errno);
				return;
			}*/
			obs_log(LOG_INFO, "Setting recording path to temp directory: %s", temp_path);
		}
		
		obs_log(LOG_INFO, "Moving recording to temp directory: %s", temp_path);
	}
}

void on_event(enum obs_frontend_event event, void *private_data)
{
	if (event == OBS_FRONTEND_EVENT_RECORDING_STARTING) {
		obs_log(LOG_INFO, "recording starting event detected");

		obs_output_t *output = obs_frontend_get_recording_output();
		if (!output) {
			obs_log(LOG_WARNING, "No recording output");
			return;
		}
		signal_handler_t *sh = obs_output_get_signal_handler(output);
		if (!sh) {
			obs_log(LOG_WARNING, "Failed to get signal handler");
			return;
		}

		signal_handler_connect(sh, "start", on_recording_start, NULL);
	}
}

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	obs_frontend_add_event_callback(on_event, NULL);
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
