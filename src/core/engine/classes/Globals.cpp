#include "Globals.hpp"

#include "core/engine/Engine.hpp"
#include "core/offsets/Dumper.hpp"

bool Globals::Update() {
	auto p = Engine::GetProcess();
	auto client = Engine::GetClient();

	if (!p)
		return false;

	this->address = p->read<uintptr_t>(client.base + offsets::globalVars);

	if (!this->address)
		return false;

	this->current_time = p->read<float>(this->address + offsets::global::currentTime);

	this->max_clients = p->read<int>(this->address + offsets::global::maxClients);
	// clamp 到合法范围：读取失败返回 0，垃圾值会导致 Cache::RefreshImpl
	// 的 reserve/循环失控（CS2 服务器上限 64 人）。
	this->max_clients = std::clamp(this->max_clients, 0, 64);
	this->in_match = this->max_clients > 1; // In the lobby, it seems that it still counts one

	auto map_name_addr = p->read<DWORD64>(this->address + offsets::global::currentMapName);

	if (!p->read_raw(map_name_addr, this->map_name, sizeof(this->map_name)))
		return false;

	// Ensure termination in case the name fills the whole buffer.
	this->map_name[sizeof(this->map_name) - 1] = '\0';

	// Read frame timing for adaptive cache refresh
	// interval_per_tick = 1.0 / tick_rate (in seconds)
	// frame_time is typically around 16.67ms for 60 FPS
	this->frame_time = p->read<float>(this->address + offsets::global::intervalPerTick);
	if (this->frame_time <= 0.0f || this->frame_time > 100.0f) {
		this->frame_time = 16.67f; // fallback
	}
	this->frame_time *= 1000.0f; // convert to ms

	this->tick_rate = p->read<float>(this->address + offsets::global::tickRate);
	if (this->tick_rate <= 0.0f || this->tick_rate > 128.0f) {
		this->tick_rate = 64.0f; // fallback
	}

	return true;
}

