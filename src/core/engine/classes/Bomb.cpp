#include "Bomb.hpp"

#include <algorithm>
#include <ctime>

#include "core/engine/Engine.hpp"
#include "core/offsets/Dumper.hpp"

bool Bomb::Update() {

	auto p = Engine::GetProcess();

	if (!p)
		return false;

	auto client = Engine::GetClient();

	// Resolve the C4 carrier via the global weaponC4 pointer
	// client.base + weaponC4 -> ptr to C4 entity -> m_hOwnerEntity = pawn handle
	this->carrier = 0;
	auto c4_ptr = p->read<uintptr_t>(client.base + offsets::weaponC4);
	if (c4_ptr) {
		if (auto e = p->read<uintptr_t>(c4_ptr))
			this->carrier = (uintptr_t)p->read<int>(e + offsets::m_hOwnerEntity);
	}

	this->address = p->read<uintptr_t>(client.base + offsets::plantedC4);
	this->is_planted = (this->address != 0);

	if (!this->is_planted) {
		Bomb::prev_is_planted = false;
		return true;
	}

	auto site = p->read<uint32_t>(this->address + offsets::bomb::m_nBombSite);
	this->site = (site == 1) ? BombSite::B : BombSite::A;

	auto node = p->read<uintptr_t>(this->address + offsets::pawn::m_pGameSceneNode);

	if (node)
		this->pos = p->read<Vec3_t>(node + offsets::bomb::m_vecAbsOrigin);

	if (!Bomb::prev_is_planted) 
		plant_time = std::time(nullptr);

	// 防御系统时间被修改导致负值。
	this->time_left = std::max(0.f, 41.f - (float)(std::time(nullptr) - plant_time));

	Bomb::prev_is_planted = true;
	return true;
}

std::time_t Bomb::plant_time{};
bool Bomb::prev_is_planted = false;