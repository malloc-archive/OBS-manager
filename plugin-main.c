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
#include <windows.h>
#include <psapi.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

char playing[256] = "No fullscreen app detected";
char *game_list[] = {
	"a",
	"s"
};
int size = 2;
char *path = "";

bool contains(char *current, char *list[], int size) {
	for (int i = 0; i < size; i++) {
		if (strcmp(current, list[i]) == 0) {
			return true;
		}
	}
	return false;
}

static void on_recording_stop(void *data, calldata_t *cd)
{
	obs_log(LOG_INFO, "recording stopped");

	const char *path = calldata_string(cd, "recording_path");
	if (!path)
		return;
	obs_log(LOG_INFO, "recording saved to: %s", path);



}

void get_fullscreen_app_name(char *name_buffer, DWORD buffer_size)
{
	HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
    	strncpy(name_buffer, "No fullscreen app detected", buffer_size - 1);
		name_buffer[buffer_size - 1] = '\0';
    	return;
	}	

	RECT window_rect;
	GetWindowRect(hwnd, &window_rect);

	int screen_width = GetSystemMetrics(SM_CXSCREEN);
	int screen_height = GetSystemMetrics(SM_CYSCREEN);

	if((window_rect.right - window_rect.left) >= screen_width && (window_rect.bottom - window_rect.top) >= screen_height){

		DWORD process_id;
		GetWindowThreadProcessId(hwnd, &process_id);
		HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process_id);

		if (process_handle) {
    		if (GetModuleBaseNameA(process_handle, NULL, name_buffer, buffer_size) == 0) {
				obs_log(LOG_ERROR, "Failed to get module base name: %lu", GetLastError());
        		strncpy(name_buffer, "Unknown", buffer_size - 1);
        		name_buffer[buffer_size - 1] = '\0';
			}
    	CloseHandle(process_handle);
		} else {
    		strncpy(name_buffer, "OpenProcess failed", buffer_size - 1);
    		name_buffer[buffer_size - 1] = '\0';
		}
	}
	else {
		strncpy(name_buffer, "No fullscreen app detected", buffer_size - 1);
		name_buffer[buffer_size - 1] = '\0';
	}
}
static void on_recording_start(void *data, calldata_t *cd)
{

	obs_log(LOG_INFO, "recording started");
	get_fullscreen_app_name(playing, 256);
	if (strcmp(playing, "No fullscreen app detected") == 0) {

	} else if(contains(playing, game_list, size)) {
		
	}
	obs_output_t *output = obs_get_output_by_name("adv_file_output");
    if (!output) output = obs_get_output_by_name("simple_file_output");
	obs_log(LOG_INFO, "method called, PNAMELEN: %s", playing);
	if (output) {
		obs_data_t *settings = obs_output_get_settings(output);
		obs_log(LOG_INFO, "output found: %s", obs_data_get_json(obs_output_get_settings(output)));
		const char *path = obs_data_get_string(settings, "path");
		obs_log(LOG_INFO, "current recording path: %s", path);
	}
}

void on_event(enum obs_frontend_event event, void *private_data)
{
	if(event == OBS_FRONTEND_EVENT_RECORDING_STARTING) {
		obs_log(LOG_INFO, "recording starting event detected");

		obs_output_t *output = obs_frontend_get_recording_output();
		if (!output) {
    		obs_log(LOG_WARNING, "No recording output");
    		return;
		}
		signal_handler_t *sh = obs_output_get_signal_handler(output);

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
