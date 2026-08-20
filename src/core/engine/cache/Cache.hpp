#pragma once
#include <memory>
#include <mutex>
#include <vector>
#include "core/engine/classes/Game.hpp"
#include "core/engine/classes/Bomb.hpp"
#include "core/engine/classes/Player.hpp"
#include "core/engine/classes/Globals.hpp"
#include "core/visibility/Visibility.hpp"

struct Snapshot {
	Game game;
	Bomb bomb;
	Player local;
	Globals globals;
	std::vector<Player> players;
	std::vector<SmokeBox> smokes; // active smoke AABBs (from voxel data)
};

// Double-buffered snapshot: the engine thread builds a fresh Snapshot off-lock
// and atomically publishes a shared_ptr to it. The render thread only copies
// the shared_ptr (refcount bump), so the previous per-frame deep copy + heap
// allocations of up to 64 players are eliminated entirely.
class Cache {
public:
	static Cache& Get()
	{
		static Cache instance{};
		return instance;
	}

	static std::shared_ptr<const Snapshot> CopySnapshot();

	static bool Refresh();
private:
	// Per-smoke voxel cache: keyed by entity address + voxel update counter, so
	// the (up to 8KB) voxel buffer is only re-read when the smoke actually
	// changes shape — not on every 2ms refresh.
	struct SmokeCacheEntry {
		uintptr_t entity;
		int32_t voxel_update;
		SmokeBox box;
	};
	std::shared_ptr<Snapshot> current;
	std::mutex mtx;
	std::chrono::steady_clock::time_point last{};
	std::vector<SmokeCacheEntry> smoke_cache;
private:
	bool RefreshImpl();
};
