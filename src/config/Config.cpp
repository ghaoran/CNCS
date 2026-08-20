#include "Config.hpp"

#include <algorithm>
#include <mutex>
#include <filesystem>

namespace {
	// 获取 exe 所在目录（与 config.json 同级）
	std::filesystem::path ExeDir() {
		char buf[MAX_PATH];
		DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
		if (n == 0) return std::filesystem::current_path();
		return std::filesystem::path(buf).parent_path();
	}

	std::string ConfigPath() {
		return (ExeDir() / "config.json").string();
	}
}

bool Config::Read() {
	return GetInstance().ReadImpl();
}

bool Config::Write() {
	return GetInstance().WriteImpl();
}

bool Config::ReadImpl() {
	std::ifstream f(ConfigPath());

	if (!f.good()) {
		LOGF(FATAL, "配置文件不存在，正在创建新的");
		WriteImpl();
		return false;
	}

	json data;
	try {
		data = json::parse(f);
	}
	catch (const std::exception&) {
		LOGF(FATAL, "解析配置文件失败");
		WriteImpl();
		return false;
	}

	if (data.empty())
		return false;

	try {
		// general
		cfg::enabled = data.value("enabled", true);
		cfg::deathmatch = data.value("deathmatch", false);

		// esp
		cfg::esp::box = data["visual"].value("box", true);
		cfg::esp::team = data["visual"].value("team", true);
		cfg::esp::armor = data["visual"].value("armor", true);
		cfg::esp::health = data["visual"].value("health", true);
		cfg::esp::spotted = data["visual"].value("spotted", false);
		cfg::esp::visible_only = data["visual"].value("visible_only", false);
		cfg::esp::skeleton = data["visual"].value("skeleton", true);
		cfg::esp::head_tracker = data["visual"].value("head_tracker", true);
		cfg::esp::health_number = data["visual"].value("health_number", false);
		cfg::esp::tracers = data["visual"].value("tracers", false);
		cfg::esp::bomb = data["visual"].value("bomb", true);

		// aimbot
		cfg::aimbot::enabled = data["aim"].value("enabled", false);
		cfg::aimbot::key = data["aim"].value("key", 0x02);
		cfg::aimbot::fov = std::clamp(data["aim"].value("fov", 200.f), 1.f, 1800.f);
		cfg::aimbot::smoothing = std::clamp(data["aim"].value("smoothing", 0.3f), 0.f, 1.f);
		cfg::aimbot::bone = data["aim"].value("bone", 7);
		cfg::aimbot::show_fov = data["aim"].value("show_fov", true);
		cfg::aimbot::visible_only = data["aim"].value("visible_only", false);
		cfg::aimbot::smart_bone = data["aim"].value("smart_bone", true);

		// triggerbot
		cfg::triggerbot::enabled = data["fire"].value("enabled", false);
		cfg::triggerbot::key = data["fire"].value("key", 0x10);
		cfg::triggerbot::visible_only = data["fire"].value("visible_only", false);
		
		// flags
		cfg::esp::flags::name = data["visual"]["flags"].value("name", true);
		cfg::esp::flags::ping = data["visual"]["flags"].value("ping", false);
		cfg::esp::flags::money = data["visual"]["flags"].value("money", false);
		cfg::esp::flags::weapon = data["visual"]["flags"].value("weapon", false);
		cfg::esp::flags::ammo = data["visual"]["flags"].value("ammo", false);
		cfg::esp::flags::reloading = data["visual"]["flags"].value("reloading", false);
		cfg::esp::flags::scoped = data["visual"]["flags"].value("scoped", false);
		cfg::esp::flags::defusing = data["visual"]["flags"].value("defusing", false);
		cfg::esp::flags::flashed = data["visual"]["flags"].value("flashed", false);
		cfg::esp::flags::has_c4 = data["visual"]["flags"].value("has_c4", false);

		// colors
		const auto& col = data["visual"]["colors"];
		cfg::esp::colors::box_team = JsonToColor(col, "box_team", { 0.f, 1.f, 0.29f, 0.5f });
		cfg::esp::colors::box_enemy = JsonToColor(col, "box_enemy", { 1.f, 0.f, 0.f, 0.5f });

		cfg::esp::colors::skeleton_team = JsonToColor(col, "skeleton_team", { 0.f, 1.f, 0.f, 0.5f });
		cfg::esp::colors::skeleton_enemy = JsonToColor(col, "skeleton_enemy", { 1.f, 0.f, 0.f, 0.5f });

		cfg::esp::colors::tracker_team = JsonToColor(col, "tracker_team", { 1.f, 1.f, 1.f, 0.3f });
		cfg::esp::colors::tracker_enemy = JsonToColor(col, "tracker_enemy", { 1.f, 1.f, 1.f, 0.3f });

		cfg::esp::colors::tracer_team = JsonToColor(col, "tracer_team", { 0.f, 1.f, 0.f, 0.5f });
		cfg::esp::colors::tracer_enemy = JsonToColor(col, "tracer_enemy", { 1.f, 0.f, 0.f, 0.5f });

		cfg::esp::colors::bomb = JsonToColor(col, "bomb", { 1.f, 0.84f, 0.f, 1.f });	

		// flag colors
		const auto& fcol = data["visual"]["colors"]["flags"];

		cfg::esp::colors::flags::flashed_team = JsonToColor(fcol, "flashed_team", { 1.f, 1.f, 1.f, 0.5f });
		cfg::esp::colors::flags::flashed_enemy = JsonToColor(fcol, "flashed_enemy", { 1.f, 1.f, 1.f, 0.8f });

		cfg::esp::colors::flags::reloading_team = JsonToColor(fcol, "reloading_team", { 1.f, 1.f, 1.f, 0.5f });
		cfg::esp::colors::flags::reloading_enemy = JsonToColor(fcol, "reloading_enemy", { 1.f, 1.f, 1.f, 0.8f });

		cfg::esp::colors::flags::defusing_team = JsonToColor(fcol, "defusing_team", { 1.f, 1.f, 1.f, 0.5f });
		cfg::esp::colors::flags::defusing_enemy = JsonToColor(fcol, "defusing_enemy", { 1.f, 1.f, 1.f, 0.8f });

		cfg::esp::colors::flags::scoped_team = JsonToColor(fcol, "scoped_team", { 1.f, 1.f, 1.f, 0.5f });
		cfg::esp::colors::flags::scoped_enemy = JsonToColor(fcol, "scoped_enemy", { 1.f, 1.f, 1.f, 0.8f });

		cfg::esp::colors::flags::c4_team = JsonToColor(fcol, "c4_team", { 1.f, 0.84f, 0.f, 1.f });
		cfg::esp::colors::flags::c4_enemy = JsonToColor(fcol, "c4_enemy", { 1.f, 0.84f, 0.f, 1.f });

		// world
		// spectator list
		cfg::world::spectators::enabled = data["world"]["spectators"].value("enabled", true);
		cfg::world::spectators::detailed = data["world"]["spectators"].value("detailed", false);
		cfg::world::spectators::self_only = data["world"]["spectators"].value("self_only", true);
		cfg::world::spectators::pos = JsonToVec2(data["world"]["spectators"], "pos", {10.f, 100.f});

		// bomb
		cfg::world::bomb::location = data["world"]["bomb"].value("location", true);
		cfg::world::bomb::timer = data["world"]["bomb"].value("timer", true);
		cfg::world::bomb::pos = JsonToVec2(data["world"]["bomb"], "pos", { 10.f, 300.f });

		// crosshair
		cfg::world::crosshair::enabled = data["world"]["crosshair"].value("enabled", false); 

		// radar
		cfg::world::radar::enabled = data["world"]["radar"].value("enabled", true);
		cfg::world::radar::no_rotate = data["world"]["radar"].value("no_rotate", false);
		cfg::world::radar::range = data["world"]["radar"].value("range", 2000.f);
		cfg::world::radar::pos = JsonToVec2(data["world"]["radar"], "pos", { 10.f, 10.f });
		cfg::world::radar::size = JsonToVec2(data["world"]["radar"], "size", { 200.f, 200.f });

		// velocity
		cfg::world::velocity::enabled = data["world"]["velocity"].value("enabled", false);
		cfg::world::velocity::sample_rate = std::clamp(data["world"]["velocity"].value("sample_rate", 10), 1, 100);
		cfg::world::velocity::sample_length = std::clamp(data["world"]["velocity"].value("sample_length", 5.f), 0.1f, 20.f);
		cfg::world::velocity::pos = JsonToVec2(data["world"]["velocity"], "pos", { 10.f, 400.f });
		cfg::world::velocity::size = JsonToVec2(data["world"]["velocity"], "size", { 400.f, 100.f });

		// utils
		//cfg::settings::console = data["utils"].value("console", true);
		cfg::settings::watermark = data["utils"].value("watermark", true);
		cfg::settings::streamproof = data["utils"].value("streamproof", false);
		cfg::settings::vsync = data["utils"].value("vsync", true);
		cfg::settings::free_cpu = data["utils"].value("free_cpu", true);
		//cfg::settings::open_menu_key = data["utils"].value("open_menu_key", 0);
	}
	catch (const std::exception&) {
		LOGF(FATAL, "解析配置失败");
		WriteImpl();
		return false;
	}

	LOGF(INFO, "配置解析成功");
	return true;
}

bool Config::WriteImpl() {
	// Serialize writes: the menu spawns a static write thread on every toggle,
	// so without a lock two threads could corrupt config.json.
	static std::mutex write_mtx;
	std::lock_guard<std::mutex> lock(write_mtx);

	std::ofstream f(ConfigPath());
	if (!f.good()) {
		LOGF(FATAL, "打开配置文件写入失败");
		return false;
	}

	json data;

	data["enabled"] = cfg::enabled;
	data["deathmatch"] = cfg::deathmatch;

	// esp
	data["visual"]["box"] = cfg::esp::box;
	data["visual"]["team"] = cfg::esp::team;
	data["visual"]["armor"] = cfg::esp::armor;
	data["visual"]["health"] = cfg::esp::health;
	data["visual"]["health_number"] = cfg::esp::health_number;
	data["visual"]["skeleton"] = cfg::esp::skeleton;
	data["visual"]["head_tracker"] = cfg::esp::head_tracker;
	data["visual"]["spotted"] = cfg::esp::spotted;
	data["visual"]["visible_only"] = cfg::esp::visible_only;
	data["visual"]["tracers"] = cfg::esp::tracers;
	data["visual"]["bomb"] = cfg::esp::bomb;

	// aimbot
	data["aim"]["enabled"] = cfg::aimbot::enabled;
	data["aim"]["key"] = cfg::aimbot::key;
	data["aim"]["fov"] = cfg::aimbot::fov;
	data["aim"]["smoothing"] = cfg::aimbot::smoothing;
	data["aim"]["bone"] = cfg::aimbot::bone;
	data["aim"]["show_fov"] = cfg::aimbot::show_fov;
	data["aim"]["visible_only"] = cfg::aimbot::visible_only;
	data["aim"]["smart_bone"] = cfg::aimbot::smart_bone;

	// triggerbot
	data["fire"]["enabled"] = cfg::triggerbot::enabled;
	data["fire"]["key"] = cfg::triggerbot::key;
	data["fire"]["visible_only"] = cfg::triggerbot::visible_only;

	// flags
	data["visual"]["flags"]["name"] = cfg::esp::flags::name;
	data["visual"]["flags"]["ping"] = cfg::esp::flags::ping;
	data["visual"]["flags"]["money"] = cfg::esp::flags::money;
	data["visual"]["flags"]["scoped"] = cfg::esp::flags::scoped;
	data["visual"]["flags"]["weapon"] = cfg::esp::flags::weapon;
	data["visual"]["flags"]["ammo"] = cfg::esp::flags::ammo;
	data["visual"]["flags"]["reloading"] = cfg::esp::flags::reloading;
	data["visual"]["flags"]["flashed"] = cfg::esp::flags::flashed;
	data["visual"]["flags"]["defusing"] = cfg::esp::flags::defusing;
	data["visual"]["flags"]["has_c4"] = cfg::esp::flags::has_c4;

	// world
	// spectator list
	data["world"]["spectators"]["enabled"] = cfg::world::spectators::enabled;
	data["world"]["spectators"]["detailed"] = cfg::world::spectators::detailed;
	data["world"]["spectators"]["self_only"] = cfg::world::spectators::self_only;
	Vec2ToJson(data["world"]["spectators"], "pos", cfg::world::spectators::pos);

	// bomb
	data["world"]["bomb"]["location"] = cfg::world::bomb::location;
	data["world"]["bomb"]["timer"] = cfg::world::bomb::timer;
	Vec2ToJson(data["world"]["bomb"], "pos", cfg::world::bomb::pos);

	// crosshair
	data["world"]["crosshair"]["enabled"] = cfg::world::crosshair::enabled;

	// radar
	data["world"]["radar"]["enabled"] = cfg::world::radar::enabled;
	data["world"]["radar"]["no_rotate"] = cfg::world::radar::no_rotate;
	data["world"]["radar"]["range"] = cfg::world::radar::range;
	Vec2ToJson(data["world"]["radar"], "pos", cfg::world::radar::pos);
	Vec2ToJson(data["world"]["radar"], "size", cfg::world::radar::size);

	// velocity
	data["world"]["velocity"]["enabled"] = cfg::world::velocity::enabled;
	data["world"]["velocity"]["sample_rate"] = cfg::world::velocity::sample_rate;
	data["world"]["velocity"]["sample_length"] = cfg::world::velocity::sample_length;
	Vec2ToJson(data["world"]["velocity"], "pos", cfg::world::velocity::pos);
	Vec2ToJson(data["world"]["velocity"], "size", cfg::world::velocity::size);

	// colors
	auto& col = data["visual"]["colors"];
	ColorToJson(col, "box_team", cfg::esp::colors::box_team);
	ColorToJson(col, "box_enemy", cfg::esp::colors::box_enemy);

	ColorToJson(col, "skeleton_team", cfg::esp::colors::skeleton_team);
	ColorToJson(col, "skeleton_enemy", cfg::esp::colors::skeleton_enemy);

	ColorToJson(col, "tracker_team", cfg::esp::colors::tracker_team);
	ColorToJson(col, "tracker_enemy", cfg::esp::colors::tracker_enemy);

	ColorToJson(col, "tracer_team", cfg::esp::colors::tracer_team);
	ColorToJson(col, "tracer_enemy", cfg::esp::colors::tracer_enemy);

	ColorToJson(col, "bomb", cfg::esp::colors::bomb);

	// flag colors
	auto& fcol = col["flags"];

	ColorToJson(fcol, "flashed_team", cfg::esp::colors::flags::flashed_team);
	ColorToJson(fcol, "flashed_enemy", cfg::esp::colors::flags::flashed_enemy);

	ColorToJson(fcol, "reloading_team", cfg::esp::colors::flags::reloading_team);
	ColorToJson(fcol, "reloading_enemy", cfg::esp::colors::flags::reloading_enemy);

	ColorToJson(fcol, "defusing_team", cfg::esp::colors::flags::defusing_team);
	ColorToJson(fcol, "defusing_enemy", cfg::esp::colors::flags::defusing_enemy);

	ColorToJson(fcol, "scoped_team", cfg::esp::colors::flags::scoped_team);
	ColorToJson(fcol, "scoped_enemy", cfg::esp::colors::flags::scoped_enemy);

	ColorToJson(fcol, "c4_team", cfg::esp::colors::flags::c4_team);
	ColorToJson(fcol, "c4_enemy", cfg::esp::colors::flags::c4_enemy);

	// utils
	//data["utils"]["console"] = cfg::settings::console;
	data["utils"]["watermark"] = cfg::settings::watermark;
	data["utils"]["streamproof"] = cfg::settings::streamproof;
	data["utils"]["vsync"] = cfg::settings::vsync;
	data["utils"]["free_cpu"] = cfg::settings::free_cpu;
	//data["utils"]["open_menu_key"] = cfg::settings::open_menu_key;

	f << std::setw(4) << data << std::endl;
	f.close();

	LOGF(VERBOSE, "正在写入配置到文件");

	return true;
}


// 使用 nlohmann::json 的内置转换，支持 get<color_t>() 和 get<Vec2_t>()
// 依赖 color_t/Vec2_t 有适配的 from_json/to_json 或隐式构造

color_t Config::JsonToColor(const json& parent, const std::string& key, const color_t& def) {
    if (!parent.contains(key))
        return def;
    try {
        return parent[key].get<color_t>();
    } catch (...) {
        return def;
    }
}

void Config::ColorToJson(json& parent, const std::string& key, const color_t& color) {
    parent[key] = json::array({ color.r, color.g, color.b, color.a });
}

Vec2_t Config::JsonToVec2(const json& parent, const std::string& key, const Vec2_t& def) {
    if (!parent.contains(key))
        return def;
    try {
        return parent[key].get<Vec2_t>();
    } catch (...) {
        return def;
    }
}

void Config::Vec2ToJson(json& parent, const std::string& key, const Vec2_t& vec) {
    parent[key] = json::array({ vec.x, vec.y });
}