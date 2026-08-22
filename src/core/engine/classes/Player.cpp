#include "Player.hpp"

#include <cstring>

#include "Weapon.hpp"
#include "core/engine/Engine.hpp"
#include "core/offsets/Dumper.hpp"
#include "core/engine/classes/ObserverServices.hpp"

// 使 1s / 5ms 等 std::chrono 字面量可用
using namespace std::chrono_literals;

bool Player::Update() {
	if (!Engine::GetProcess())
		return false;

	if (!GetController()) {
		//LOGF(WARNING, "获取实体控制器失败，索引({})", index);
		return false;
	}

	if (!GetPawn()) {
		//LOGF(WARNING, "获取实体 pawn 失败，索引({})", index);
		return false;
	}

	if (!UpdateController()) {
		//LOGF(WARNING, "更新实体控制器失败，索引({})", index);
		return false;
	}

	if (!UpdatePawn()) {
		//LOGF(WARNING, "更新实体 pawn 失败，索引({})", index);
		return false;
	}

	return true;
}

bool Player::GetController() {
	auto p = Engine::GetProcess();
	auto client = Engine::GetClient();

	this->controller = p->read<DWORD64>(list_entry + (index + 1) * 0x70); // before was 0x78

	return this->controller != 0;
}

bool Player::GetPawn() {
	auto p = Engine::GetProcess();
	auto client = Engine::GetClient();

	auto entity_pawn_address = p->read<uintptr_t>(controller + offsets::controller::m_hPawn);

	if (!entity_pawn_address)
		return false;

	this->pawn_controller_addr = entity_pawn_address;

	auto entity_pawn_list_entry = p->read<uintptr_t>(
		this->entity_list + offsets::EntityListOffset + offsets::ChunkStride * ((entity_pawn_address & offsets::IndexMask) >> offsets::HandleBits));

	if (!entity_pawn_list_entry)
		return false;

	this->pawn = p->read<uintptr_t>(entity_pawn_list_entry + offsets::EntryStride * (entity_pawn_address & offsets::HandleMask));

	return this->pawn != 0;
}

bool Player::UpdateController() {
	auto p = Engine::GetProcess();

	// Batch-read the whole controller metadata block (name .. ping) in a single
	// RPM instead of 5 separate syscalls. Field offsets are relative to the
	// block base (m_iszPlayerName) and memcpy'd out to avoid alignment issues.
	constexpr uintptr_t base = offsets::controller::m_iszPlayerName;                 // 0x6F4
	constexpr size_t span = offsets::controller::m_iPing + sizeof(int) - base;       // 0x140
	uint8_t block[span];

	if (!p->read_raw(controller + base, block, span))
		return false;

	std::memcpy(this->name, block, 32);
	this->name[31] = '\0';

	std::memcpy(&this->steam_id, block + (offsets::controller::m_steamID - base), 8);
	this->bot = this->steam_id == 0;

	this->localplayer = block[offsets::controller::m_bIsLocalPlayerController - base] != 0;

	std::memcpy(&this->ping, block + (offsets::controller::m_iPing - base), 4);

	uintptr_t money_services;
	std::memcpy(&money_services, block + (offsets::controller::m_pInGameMoneyServices - base), 8);
	if (money_services)
		this->money = p->read<int>(money_services + offsets::controller::m_iAccount);

	return true;
}

bool Player::UpdatePawn() {
	auto p = Engine::GetProcess();

	// Batch 1: health, team, flags  (0x34C..0x3FB ≈ 175 bytes).
	constexpr uintptr_t b1 = offsets::pawn::m_iHealth;                        // 0x34C
	constexpr size_t b1_sz = (offsets::pawn::m_fFlags + 4) - b1;             // 0x3F8 - 0x34C = 0xAC
	uint8_t blk1[b1_sz];
	if (!p->read_raw(pawn + b1, blk1, b1_sz))
		return false;

	std::memcpy(&this->health, blk1 + (offsets::pawn::m_iHealth - b1), 4);
	this->alive = health > 0;

	if (this->health > 255 || this->health < 0) {
		static auto last_warn = std::chrono::steady_clock::time_point{};
		const auto now = std::chrono::steady_clock::now();
		if (now - last_warn > 1s) {
			LOGF(WARNING,
				"血量值异常（超过 255 或小于 0），值为 ({}). 游戏可能已更新 pawn 结构",
				this->health
			);
			last_warn = now;
		}
	}

	if (!alive) {
		UpdateObserverServices();
		return true;
	}

	this->team = blk1[offsets::pawn::m_iTeamNum - b1];
	this->ducking = (blk1[offsets::pawn::m_fFlags - b1] & 0x2) != 0;

	// Batch 2: pos + vel。二者偏移相差约 0xC40 字节，必须分开读取，
	// 不能共用一个 28 字节栈缓冲并按相对偏移取 vel（会越界）。
	uint8_t blk_pv[12];
	if (!p->read_raw(pawn + offsets::pawn::m_vOldOrigin, blk_pv, 12))
		return false;
	std::memcpy((void*)&this->pos, blk_pv, 12);
	if (this->pos.zero())
		return false;

	uint8_t blk_vel[12];
	if (!p->read_raw(pawn + offsets::pawn::m_vecAbsVelocity, blk_vel, 12))
		return false;
	std::memcpy((void*)&this->vel, blk_vel, 12);

	// Batch 3: weapon ptr, scoped, defusing, spotted, flash, armor  (0x1208..0x1CA8 ≈ 672 bytes).
	constexpr uintptr_t b3 = offsets::pawn::m_pWeaponServices;               // 0x1208
	constexpr size_t b3_sz = (offsets::pawn::m_ArmorValue + 4) - b3;        // 0x1CA8 - 0x1208 = 0x2A0
	uint8_t blk3[b3_sz];
	if (!p->read_raw(pawn + b3, blk3, b3_sz))
		return false;

	this->flashed = *(float*)(blk3 + (offsets::pawn::m_flFlashOverlayAlpha - b3)) > 0.f;
	this->scoped  = blk3[offsets::pawn::m_bIsScoped - b3] != 0;
	this->defusing = blk3[offsets::pawn::m_bIsDefusing - b3] != 0;
	std::memcpy(&this->spotted_mask, blk3 + (offsets::pawn::m_entitySpottedState + offsets::pawn::m_bSpottedByMask - b3), 8);
	this->spotted = this->spotted_mask != 0;
	this->armor = *(int*)(blk3 + (offsets::pawn::m_ArmorValue - b3));

	if (!UpdateSkeleton()) {
		return false;
	}

	// Shows errors when player just respawned
	if (!UpdateWeapon()) {
		//LOGF(FATAL, "更新武器失败"); // too verbose
		return false;
	}


	return true;
}

bool Player::UpdateSkeleton() {
	auto p = Engine::GetProcess();

	auto game_scene = p->read<DWORD64>(this->pawn + offsets::pawn::m_pGameSceneNode);

	if (!game_scene)
		return false;

	auto bone_array = p->read<DWORD64>(game_scene + (offsets::bone::m_modelState + 0x80));

	if (!bone_array)
		return false;

	// Stack-local read buffer — kept out of the Player struct so the per-frame
	// snapshot copy doesn't drag 30*32 bytes of scratch space per player.
	bone_data bones[30];
	if (!p->read_raw(bone_array, bones, sizeof(bones)))
		return false;

	for (int i = 0; i < 30; i++)
		this->bone_list[i] = { bones[i].pos };
	this->skeleton_valid = true;

	return true;
}

bool Player::UpdateWeapon() {
	auto p = Engine::GetProcess();

	auto weapon_services = p->read<uintptr_t>(this->pawn + offsets::pawn::m_pWeaponServices);

	if (!weapon_services)
		return false;

	auto active_weapon_index = p->read<int>(weapon_services + offsets::pawn::m_hActiveWeapon);

	if (!active_weapon_index)
		return false;

	auto weapon = Weapon(this->entity_list, active_weapon_index);

	if (!weapon.Update())
		return false;

	this->weapon = weapon;
	this->ammo = weapon.ammo;
	this->is_reloading = weapon.is_reloading;

	return true;
}

bool Player::GetBounds(view_matrix_t matrix, Vec2_t size, std::pair<Vec2_t, Vec2_t>& bounds, Vec2_t* head_screen) const {
	Vec2_t origin;
	bool pt1 = matrix.wts(this->pos, size, origin);


	Vec3_t pos_top;
	if (!this->skeleton_valid)
		pos_top = this->pos + Vec3_t(0, 0, 65.f); // 75.f
	else
		pos_top = this->bone_list[bone_index::head].pos;

	Vec2_t top;
	bool pt2 = matrix.wts(pos_top, size, top);

	// Expose the un-offset head projection so callers (head tracker) can reuse
	// it instead of re-projecting the same point.
	if (head_screen)
		*head_screen = top;

	float height = origin.y - top.y;
	float width = height / 2.4f;

	top.x -= width / 2;
	origin.x += width / 2;

	top.y -= width / 4;

	// Top to bottom
	bounds = { top, origin };

	return pt1 || pt2;
}

// Does not update if match is started
bool Player::UpdateObserverServices() {
	auto p = Engine::GetProcess();
	if (!p) 
		return false;

	DWORD64 address = p->read<DWORD64>(this->pawn + offsets::pawn::m_pObserverServices);
	if (!address) 
		return false;

	this->observer_services.SetAddress(address);
	return this->observer_services.Update();
}