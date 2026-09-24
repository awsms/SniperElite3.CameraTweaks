#include "core.h"
#include "FovOverlay.h"
#include "FreeCamera.h"
#include "se3/Camera.h"
#include "SettingsMgr.h"

bool FreeCamera::ms_bEnabled = false;

void FreeCamera::Init()
{
	HANDLE thread = CreateThread(nullptr, 0, Thread, nullptr, 0, nullptr);
	if (thread) CloseHandle(thread);
}

DWORD WINAPI FreeCamera::Thread(LPVOID)
{
	bool wasDown = false;
	bool patched = false;
	while (true)
	{
		DWORD foregroundPid = 0;
		GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);
		const bool down = (GetAsyncKeyState(SettingsMgr->iFreeCameraEnableKey) & 0x8000) != 0;
		const bool pressed = down && !wasDown;
		wasDown = down;
		if (FovOverlay::IsOpen() || foregroundPid != GetCurrentProcessId()) { Sleep(10); continue; }
		Camera* cam = GetCamera();
		if (pressed)
			ms_bEnabled ^= 1;

		if (ms_bEnabled)
		{
			if (!patched) {
				Nop(_addr(0x90127F), 2);
				Nop(_addr(0x901284), 3);
				Nop(_addr(0x90128A), 3);
				patched = true;
			}

			if (cam)
			{
				float speed = SettingsMgr->fFreeCameraSpeed;

				if (GetAsyncKeyState(SettingsMgr->iFreeCameraKeySlowDown))
					speed /= SettingsMgr->fFreeCameraModifierScale;

				if (GetAsyncKeyState(SettingsMgr->iFreeCameraKeySpeedUp))
					speed *= SettingsMgr->fFreeCameraModifierScale;

				Vector fwd = cam->Rotation.GetForward();
				Vector up = cam->Rotation.GetUp();
				Vector right = cam->Rotation.GetRight();
				if (GetAsyncKeyState(SettingsMgr->iFreeCameraKeyForward))
					cam->Position += fwd * speed * -1;
				if (GetAsyncKeyState(SettingsMgr->iFreeCameraKeyBack))
					cam->Position += fwd * speed * 1;

				if (GetAsyncKeyState(SettingsMgr->iFreeCameraKeyUp))
					cam->Position += up * speed * -1;
				if (GetAsyncKeyState(SettingsMgr->iFreeCameraKeyDown))
					cam->Position += up * speed * 1;

				if (GetAsyncKeyState(SettingsMgr->iFreeCameraKeyRight))
					cam->Position += right * speed * -1;
				if (GetAsyncKeyState(SettingsMgr->iFreeCameraKeyLeft))
					cam->Position += right * speed * 1;
			}
		}
		else if (patched)
		{
			patched = false;
			Patch<short>(_addr(0x90127F), 0x189);
			Patch<short>(_addr(0x901284), 0x4189);
			Patch<char>(_addr(0x901284 + 2), 0x4);
			Patch<short>(_addr(0x90127F), 0x189);
			Patch<short>(_addr(0x90128A), 0x4189);
			Patch<char>(_addr(0x90128A + 2), 0x8);
		}

		Sleep(1);
	}
}
