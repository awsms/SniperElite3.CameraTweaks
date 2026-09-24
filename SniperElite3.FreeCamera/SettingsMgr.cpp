#include "IniReader.h"
#include "SettingsMgr.h"
#include <windows.h>

eSettingsManager* SettingsMgr = new eSettingsManager;

eSettingsManager::eSettingsManager()
{
	CIniReader ini("");
	iFovMenuKey = ini.ReadInteger("FOV", "MenuKey", VK_F6);
	if (iFovMenuKey < 1 || iFovMenuKey > 255) iFovMenuKey = VK_F6;
	fFovScale = ini.ReadFloat("FOV", "Scale", 1.0f);

	iFreeCameraEnableKey = ini.ReadInteger("Settings", "iFreeCameraEnableKey", VK_F5);
	iFreeCameraKeyForward = ini.ReadInteger("Settings", "iFreeCameraKeyForward", VK_NUMPAD8);
	iFreeCameraKeyBack = ini.ReadInteger("Settings", "iFreeCameraKeyBack", VK_NUMPAD2);
	iFreeCameraKeyLeft = ini.ReadInteger("Settings", "iFreeCameraKeyLeft", VK_NUMPAD6);
	iFreeCameraKeyRight = ini.ReadInteger("Settings", "iFreeCameraKeyRight", VK_NUMPAD4);
	iFreeCameraKeyUp = ini.ReadInteger("Settings", "iFreeCameraKeyUp", VK_NUMPAD7);
	iFreeCameraKeyDown = ini.ReadInteger("Settings", "iFreeCameraKeyDown", VK_NUMPAD1);
	iFreeCameraKeySlowDown = ini.ReadInteger("Settings", "iFreeCameraKeySlowDown", VK_NUMPAD5);
	iFreeCameraKeySpeedUp = ini.ReadInteger("Settings", "iFreeCameraKeySpeedUp", VK_NUMPAD9);
	fFreeCameraSpeed = ini.ReadFloat("Settings", "fFreeCameraSpeed", 0.05f);
	fFreeCameraModifierScale = ini.ReadFloat("Settings", "fFreeCameraModifierScale", 4.0f);
}

void eSettingsManager::SaveSettings()
{
	CIniReader ini("");
	ini.WriteFloat("FOV", "Scale", fFovScale);
}

void eSettingsManager::ResetKeys()
{
	iFreeCameraEnableKey = VK_F5;
	iFreeCameraKeyForward = 104;
	iFreeCameraKeyBack = 98;
	iFreeCameraKeyLeft = 102;
	iFreeCameraKeyRight = 100;
	iFreeCameraKeyUp = 103;
	iFreeCameraKeyDown = 97;
	iFreeCameraKeySlowDown = 101;
	iFreeCameraKeySpeedUp = VK_NUMPAD9;

	fFreeCameraSpeed = 0.05f;
	fFreeCameraModifierScale = 4.0f;
}
