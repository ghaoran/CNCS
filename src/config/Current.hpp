#pragma once

namespace cfg {
	inline bool enabled = true;

	// 敌我不分（死亡竞赛）：开启后所有玩家都视为敌人，不再按队伍区分。
	inline bool deathmatch = false;

	namespace esp {
		inline bool team = true;

		inline bool box = true;
		inline bool armor = true;
		inline bool health = true;
		inline bool skeleton = true;
		inline bool head_tracker = true;
		inline bool health_number = false;

		inline bool spotted = false;
		inline bool visible_only = false; // cover check: only render players visible to us

		inline bool tracers = false;
		
		inline bool bomb = true;

		namespace flags {
			inline bool name = true;
			inline bool ping = true;
			inline bool weapon = false;
			inline bool ammo = false;
			inline bool reloading = false;
			inline bool defusing = false;
			inline bool money = false;
			inline bool flashed = false;
			inline bool scoped = false;
			inline bool has_c4 = false;
		}

		namespace colors {
			inline color_t box_team{ 0.f, 1.f, 0.29f, 0.5f };
			inline color_t box_enemy{ 1.f, 0.f, 0.f, 0.5f };

			inline color_t skeleton_team{ 0.f, 1.f, 0.f, 0.5f };
			inline color_t skeleton_enemy{ 1.f, 0.f, 0.f, 0.5f };

			inline color_t tracker_team{ 1.f, 1.f, 1.f, 0.3f };
			inline color_t tracker_enemy{ 1.f, 1.f, 1.f, 0.3f };

			inline color_t tracer_team{ 0.f, 1.f, 0.f, 0.5f };
			inline color_t tracer_enemy{ 1.f, 0.f, 0.f, 0.5f };

			inline color_t bomb{ 1.f, 0.84f, 0.f, 1.f };
			
			namespace flags {
				inline color_t flashed_team{ 1.f, 1.f, 1.f, 0.5f };
				inline color_t flashed_enemy{ 1.f, 1.f, 1.f, 0.8f };

				inline color_t reloading_team{ 1.f, 1.f, 1.f, 0.5f };
				inline color_t reloading_enemy{ 1.f, 1.f, 1.f, 0.8f };

				inline color_t defusing_team{ 1.f, 1.f, 1.f, 0.5f };
				inline color_t defusing_enemy{ 1.f, 1.f, 1.f, 0.8f };

				inline color_t scoped_team{ 1.f, 1.f, 1.f, 0.5f };
				inline color_t scoped_enemy{ 1.f, 1.f, 1.f, 0.8f };

				inline color_t c4_team{ 1.f, 0.84f, 0.f, 1.f };
				inline color_t c4_enemy{ 1.f, 0.84f, 0.f, 1.f };
			}
			
		}

	}

	namespace world {
		namespace spectators {
			inline bool enabled = false;

			inline bool detailed = false;
			inline bool self_only = true;

			inline Vec2_t pos{ 10.f, 100.f };
		}

		namespace bomb {
			inline bool location = true;
			inline bool timer = true;
			inline Vec2_t pos{ 10.f, 300.f };
		}

		namespace crosshair {
			inline bool enabled = false;
		}

		namespace radar {
			inline bool enabled = true;
			inline bool no_rotate = false;
			inline float range = 2000.f;
			inline Vec2_t pos{ 10.f, 10.f };
			inline Vec2_t size{ 200.f, 200.f };
		}

		namespace velocity {
			inline bool enabled = false;
			inline int sample_rate = 35;
			inline float sample_length = 5.f;

			inline Vec2_t size{ 400.f, 100.f };
			inline Vec2_t pos{ 10.f, 400.f };
		}
	}

	namespace settings {
		inline bool watermark = true;
		inline bool streamproof = false;
		inline bool vsync = false;
		inline bool free_cpu = true;
	}

	namespace aimbot {
		inline bool enabled = false;
		// Trigger key as a Win32 virtual-key code (0 = always on).
		inline int key = 0x02; // VK_RBUTTON (mouse right button)
		inline float fov = 200.f;      // max distance from crosshair, in pixels
		// Smoothing: higher = smoother (smaller per-frame step toward the target).
		inline float smoothing = 0.3f; // 0 = fast tracking, 1 = slowest/smoothest
		inline int bone = 7;           // aim bone, bone_index::head by default
		inline bool show_fov = true;   // draw the aim FOV circle
		inline bool visible_only = false; // cover check: only aim at visible targets
		inline bool smart_bone = true; // 露瞄：自动选择可见的最高伤害部位（头>颈>胸>骨盆）
	}

	namespace triggerbot {
		inline bool enabled = false;
		// Trigger key as a Win32 virtual-key code (0 = always on).
		inline int key = 0x10; // VK_SHIFT
		inline bool visible_only = false; // cover check: only fire at visible targets
	}

	// Not stored, just for testing
	namespace dev {
		inline bool console = true;
		inline int open_menu_key = false;
		inline int cache_refresh_rate = 5;
		inline bool force_show_flags = false;
	}
}