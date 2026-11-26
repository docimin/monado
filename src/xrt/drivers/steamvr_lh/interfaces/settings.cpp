// Copyright 2023, Shawn Wallace
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief OpenVR IVRSettings interface implementation.
 * @author Shawn Wallace <yungwallace@live.com>
 * @ingroup drv_steamvr_lh
 */

#include <openvr_driver.h>
#include <optional>
#include <cstring>
#include <map>
#include <string_view>

#include "settings.hpp"
#include "context.hpp"
#include "util/u_json.hpp"
#include "util/u_logging.h"

using namespace std::string_view_literals;

// Default to 100% brightness.
DEBUG_GET_ONCE_FLOAT_OPTION(lh_default_brightness, "LH_DEFAULT_BRIGHTNESS", 1.0)
DEBUG_GET_ONCE_BOOL_OPTION(lh_standby_on_exit, "LH_STANDBY_ON_EXIT", false)

using xrt::auxiliary::util::json::JSONNode;

#define SETTINGS_TRACE(...) U_LOG_IFL_T(context->log_level, __VA_ARGS__)

namespace {

/*
 * Settings the Lighthouse driver uses to set up the Valve Index display (it programs them into
 * the headset when it starts). They are answered from the settings files like SteamVR does,
 * every other numeric setting still reads as 0.
 */
bool
is_index_display_setting(const char *section, const char *key)
{
	if (section == nullptr || key == nullptr) {
		return false;
	}

	const std::string_view s{section};
	const std::string_view k{key};

	if (s == vr::k_pch_SteamVR_Section) {
		// Display compatibility mode from SteamVR's video settings.
		return k == "displayPortTrainingMode"sv;
	}
	if (s == "driver_lighthouse"sv) {
		// Without the column correction the panels show a wobbling band while the head moves.
		return k == "indexHmdColumnCorrection"sv || k == "indexHmdBrightness"sv ||
		       k.rfind("indexHmdColumnOffset_"sv, 0) == 0;
	}
	return false;
}

//! Looks a setting up like SteamVR does: the user's steamvr.vrsettings first, then the driver's defaults.
JSONNode
find_setting(const JSONNode &user, const JSONNode &defaults, const char *section, const char *key)
{
	for (const JSONNode *root : {&user, &defaults}) {
		if (!root->isObject() || !root->hasKey(section)) {
			continue;
		}
		JSONNode sec = (*root)[section];
		if (sec.isObject() && sec.hasKey(key)) {
			return sec[key];
		}
	}
	return JSONNode{};
}

} // namespace

Settings::Settings(const std::string &steam_install, const std::string &steamvr_install, Context *context)
    : steamvr_settings(JSONNode::loadFromFile(steam_install + "/config/steamvr.vrsettings")),
      driver_defaults(
          JSONNode::loadFromFile(steamvr_install + "/drivers/lighthouse/resources/settings/default.vrsettings")),
      context{context}
{
	analog_gain = debug_get_float_option_lh_default_brightness();
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
const char *
Settings::GetSettingsErrorNameFromEnum(vr::EVRSettingsError eError)
{
	return nullptr;
}

void
Settings::SetBool(const char *pchSection, const char *pchSettingsKey, bool bValue, vr::EVRSettingsError *peError)
{}

void
Settings::SetInt32(const char *pchSection, const char *pchSettingsKey, int32_t nValue, vr::EVRSettingsError *peError)
{}

void
Settings::SetFloat(const char *pchSection, const char *pchSettingsKey, float flValue, vr::EVRSettingsError *peError)
{
	if (pchSection == std::string_view{vr::k_pch_SteamVR_Section} && pchSettingsKey == "analogGain"sv) {
		analog_gain = flValue;
		context->add_vendor_event(vr::VREvent_SteamVRSectionSettingChanged);
	}
}

void
Settings::SetString(const char *pchSection,
                    const char *pchSettingsKey,
                    const char *pchValue,
                    vr::EVRSettingsError *peError)
{}

bool
Settings::GetBool(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError)
{
	if (pchSection == std::string_view{vr::k_pch_Power_Section} && pchSettingsKey == "powerOffOnExit"sv)
		return debug_get_bool_option_lh_standby_on_exit();

	bool ret = false;
	if (is_index_display_setting(pchSection, pchSettingsKey)) {
		JSONNode value = find_setting(steamvr_settings, driver_defaults, pchSection, pchSettingsKey);
		if (value.isBool()) {
			ret = value.asBool();
		} else if (value.isNumber()) {
			ret = value.asDouble() != 0.0;
		}
	}
	SETTINGS_TRACE("GetBool(%s, %s) -> %d", pchSection, pchSettingsKey, ret);
	return ret;
}

int32_t
Settings::GetInt32(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError)
{
	int32_t ret = 0;
	if (is_index_display_setting(pchSection, pchSettingsKey)) {
		JSONNode value = find_setting(steamvr_settings, driver_defaults, pchSection, pchSettingsKey);
		if (value.isBool()) {
			ret = value.asBool() ? 1 : 0;
		} else if (value.isNumber()) {
			ret = (int32_t)value.asDouble();
		}
	}
	SETTINGS_TRACE("GetInt32(%s, %s) -> %d", pchSection, pchSettingsKey, ret);
	return ret;
}

float
Settings::GetFloat(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError)
{
	if (!strcmp(pchSection, vr::k_pch_SteamVR_Section)) {
		if (!strcmp(pchSettingsKey, "analogGain")) {
			return analog_gain;
		}
		if (!strcmp(pchSettingsKey, "ipd")) {
			// Inform the SteamVR driver we have 0 ipd (in case) it factors this into the eye matrix.
			return 0.f;
		}
	}

	float ret = 0.f;
	if (is_index_display_setting(pchSection, pchSettingsKey)) {
		JSONNode value = find_setting(steamvr_settings, driver_defaults, pchSection, pchSettingsKey);
		if (value.isNumber()) {
			ret = (float)value.asDouble();
		}
	}
	SETTINGS_TRACE("GetFloat(%s, %s) -> %f", pchSection, pchSettingsKey, ret);
	return ret;
}

// Driver requires a few string settings to initialize properly
void
Settings::GetString(const char *pchSection,
                    const char *pchSettingsKey,
                    char *pchValue,
                    uint32_t unValueLen,
                    vr::EVRSettingsError *peError)
{
	if (peError)
		*peError = vr::VRSettingsError_None;

	SETTINGS_TRACE("GetString(%s, %s)", pchSection, pchSettingsKey);

	auto get_string = [pchSection, pchSettingsKey](const JSONNode &root) -> std::optional<std::string> {
		JSONNode section = root[pchSection];
		if (!section.isValid())
			return std::nullopt;

		JSONNode value = section[pchSettingsKey];
		if (!value.isValid() || !value.isString())
			return std::nullopt;

		return std::optional(value.asString());
	};

	std::optional value = get_string(driver_defaults);
	if (!value.has_value())
		value = get_string(steamvr_settings);

	if (value.has_value()) {
		if (unValueLen > value->size())
			std::strncpy(pchValue, value->c_str(), value->size() + 1);
	} else if (peError)
		*peError = vr::VRSettingsError_ReadFailed;
}

void
Settings::RemoveSection(const char *pchSection, vr::EVRSettingsError *peError)
{}

void
Settings::RemoveKeyInSection(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError)
{}
// NOLINTEND(bugprone-easily-swappable-parameters)
