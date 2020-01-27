// Copyright (c) 2012-2020 Matt Campbell
// MIT license (see License.txt)

#include "device_codes.h"
#include "appdata.h"
#include "bb_criticalsection.h"
#include "parson/parson.h"

static deviceCodes_t g_deviceCodes;
static bb_critical_section g_deviceCodes_cs;
static UINT g_deviceCodes_reloadMessage;

const sbs_t *deviceCodes_lock(void)
{
	bb_critical_section_lock(&g_deviceCodes_cs);
	return &g_deviceCodes.deviceCodes;
}

void deviceCodes_unlock(void)
{
	bb_critical_section_unlock(&g_deviceCodes_cs);
}

static u32 deviceCodes_RegisterMessage(const char *message)
{
	u32 val = 0;
	if(message && *message) {
		val = RegisterWindowMessageA(message);
		BB_LOG("deviceCodes", "%s registered as %u", message, val);
	}
	return val;
}

static void deviceCodes_reload(void)
{
	if(!g_deviceCodes_cs.initialized)
		return;

	deviceCodes_lock();

	sb_t path = appdata_get("bb");
	sb_append(&path, "\\bb_device_codes.json");
	JSON_Value *val = json_parse_file(sb_get(&path));
	if(val) {
		sbs_reset(&g_deviceCodes.deviceCodes);
		g_deviceCodes.deviceCodes = json_deserialize_sbs_t(val);
		json_value_free(val);
	}
	sb_reset(&path);

	deviceCodes_unlock();
}

void deviceCodes_init(void)
{
	bb_critical_section_init(&g_deviceCodes_cs);
	deviceCodes_reload();
	g_deviceCodes_reloadMessage = deviceCodes_RegisterMessage("bb_reloadDeviceCodesMessage");
}

void deviceCodes_shutdown(void)
{
	bb_critical_section_shutdown(&g_deviceCodes_cs);
	sbs_reset(&g_deviceCodes.deviceCodes);
}

LRESULT WINAPI deviceCodes_HandleWindowMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	BB_UNUSED(hWnd);
	BB_UNUSED(wParam);
	BB_UNUSED(lParam);
	if(g_deviceCodes_reloadMessage && msg == g_deviceCodes_reloadMessage) {
		deviceCodes_reload();
		return 1;
	}
	return 0;
}
