#include <Windows.h>
#include "SAMPFUNCS_API.h"
#include "game_api.h"
#include "XORSTR.h"
#include <algorithm>
#include <array>
#include <list>

SAMPFUNCS *SF = new SAMPFUNCS();

bool state = false;
std::list <int> shootObjects;
std::list <int> buffershootObjects;
DWORD lastCreateTirObject = 0;
int shootDelay = 1;
DWORD lastShoot = 0;

bool canShoot = true;
bool LBM = false;

int tirModels[] = { 13679, 13680, 13681, 13682, 13683, 13684, 13685 };

bool __stdcall onSendPacket(stRakNetHookParams* param);
bool __stdcall onRecvRpc(stRakNetHookParams* param);

bool isGoodWeapon(int id);
bool isTir(int model);
int GetWeaponID();
void leftButtonSync();
bool sendBulletData(int i);


void __stdcall mainloop()
{
	static bool initialized = false;
	if (!initialized)
	{
		if (GAME && GAME->GetSystemState() == eSystemState::GS_PLAYING_GAME && SF->getSAMP()->IsInitialized())
		{
			initialized = true;

			SF->getRakNet()->registerRakNetCallback(RAKHOOK_TYPE_OUTCOMING_PACKET, onSendPacket);
			SF->getRakNet()->registerRakNetCallback(RAKHOOK_TYPE_INCOMING_RPC, onRecvRpc);

			SF->getSAMP()->registerChatCommand("tir", [](std::string arg) {
				int num = std::stoi(arg);
				if (num >= 0)
				{
					shootDelay = num;
					state = !state;
					SF->getSAMP()->getChat()->AddChatMessage(-1, xorstr("AUTHOR TIR-ÁÎÒÀ: https://www.youtube.com/@edgekich"));
					shootObjects.clear();
					canShoot = true;
					LBM = false;
				}
				else {
					shootDelay = 1;
					state = !state;
					shootObjects.clear();
					canShoot = true;
					LBM = false;
				}
			});
		}
	}

	if (initialized) {
		if (state) {
			if (buffershootObjects.size() != 0) {
				if (GetTickCount() - shootDelay > lastCreateTirObject) {
					shootObjects.merge(buffershootObjects);
					buffershootObjects.clear();
				}
			}

			if (shootObjects.size() > 0 && canShoot) {
				if (isGoodWeapon(GetWeaponID())) {
					leftButtonSync();
					if (sendBulletData(shootObjects.front())) {
						canShoot = false;
						lastShoot = GetTickCount();
						CWeapon* myWeapon = PEDSELF->GetWeapon(PEDSELF->GetCurrentWeaponSlot());
						myWeapon->SetAmmoTotal(myWeapon->GetAmmoTotal() - 1);
						shootObjects.pop_front();
					}
				}
			}
			if (GetTickCount() - shootDelay > lastShoot) {
				canShoot = true;
			}
		}
	}
}

void leftButtonSync()
{
	LBM = true;
	SF->getSAMP()->getPlayers()->localPlayerInfo.data->ForceSendOnfootSync();
	LBM = false;
}

bool isGoodWeapon(int id) 
{
	return (id >= 22 && id <= 34) || id == 38;
}

bool isTir(int model)
{
	return std::find(std::begin(tirModels), std::end(tirModels), model) != std::end(tirModels);
}

int GetWeaponID()
{
	return PEDSELF->GetWeapon(PEDSELF->GetCurrentWeaponSlot())->GetType();
}

bool __stdcall onSendPacket(stRakNetHookParams* param)
{
	if (param->packetId == PacketEnumeration::ID_PLAYER_SYNC) {
		if (state && isGoodWeapon(GetWeaponID())) {
			OnFootData data;
			memset(&data, 0, sizeof(OnFootData));
			byte packet;

			param->bitStream->ResetReadPointer();
			param->bitStream->Read(packet); 
			param->bitStream->Read((PCHAR)&data, sizeof(OnFootData));
			param->bitStream->ResetReadPointer();

			data.controllerState.rightShoulder1 = 1;

			if (LBM) {
				data.controllerState.buttonCircle = 1;
			}
			BitStream Onfoot;
			Onfoot.ResetWritePointer();
			Onfoot.Write(packet);
			Onfoot.Write((PCHAR)&data, sizeof(data));
		}
	}
	return true;
}

bool __stdcall onRecvRpc(stRakNetHookParams* param) {
	if (param->packetId == ScriptRPCEnumeration::RPC_ScrCreateObject) {
		uint16_t objectId;
		uint16_t modelId;

		param->bitStream->ResetReadPointer();
		param->bitStream->Read(objectId);
		param->bitStream->Read(modelId);

		if (isTir(modelId)) {
			buffershootObjects.push_back(objectId);
			lastCreateTirObject = GetTickCount();
		}
	}

	if (param->packetId == ScriptRPCEnumeration::RPC_ScrDestroyObject) {
		uint16_t objectId;

		param->bitStream->ResetReadPointer();
		param->bitStream->Read(objectId);

		shootObjects.remove(objectId);
	}

	if (param->packetId == ScriptRPCEnumeration::RPC_ScrTogglePlayerControllable || param->packetId == RPC_ScrSetPlayerArmedWeapon) {
		return !state;
	}

	return true;
}


bool sendBulletData(int i)
{
	Object* pObject = SF->getSAMP()->getNetGame()->pools->objectPool->object[i];
	if (!pObject) return false;
	BulletData sync;
	ZeroMemory(&sync, sizeof(BulletData));

	sync.targetId = i;

	sync.origin[0] = PEDSELF->GetPosition()->fX;
	sync.origin[1] = PEDSELF->GetPosition()->fY;
	sync.origin[2] = PEDSELF->GetPosition()->fZ;

	sync.target[0] = pObject->position[0];
	sync.target[1] = pObject->position[1];
	sync.target[2] = pObject->position[2];

	sync.center[0] = 0.0;
	sync.center[1] = 0.0;
	sync.center[2] = 0.5;

	sync.weaponId = GetWeaponID();

	sync.targetType = 3;

	BitStream BulletSync;
	BulletSync.Write((BYTE)PacketEnumeration::ID_BULLET_SYNC);
	BulletSync.Write((PCHAR)&sync, sizeof(BulletData));
	SF->getRakNet()->SendPacket(&BulletSync);
	return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReasonForCall, LPVOID lpReserved)
{
	if (dwReasonForCall == DLL_PROCESS_ATTACH)
		SF->initPlugin(mainloop, hModule);
	return TRUE;
}
