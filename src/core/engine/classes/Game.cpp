#include "Game.hpp"

#include "core/engine/Engine.hpp"
#include "core/offsets/Dumper.hpp"

bool Game::Update() {
	if (!Engine::GetProcess())
		return false;	
	
	if (!UpdateMatrix()) {
		LOGF(FATAL, "更新视图矩阵失败");
		return false;
	}

	// No need to be updated along with the view matrix
	//if (!UpdateEntityList()) {
	//	LOGF(FATAL, "更新实体列表失败");
	//	return false;
	//}

	return true;
}

bool Game::UpdateMatrix() {
	auto p = Engine::GetProcess();
	auto client = Engine::GetClient();

	this->view_matrix = p->read<view_matrix_t>(client.base + offsets::viewMatrix);

	// 读取失败时 view_matrix 为全零，检查其有效性。
	for (const auto& row : this->view_matrix.matrix)
		for (float v : row)
			if (v != 0.f)
				return true;
	return false;
}

bool Game::UpdateEntityList() {
	auto p = Engine::GetProcess();
	auto client = Engine::GetClient();

	this->entity_list = p->read<DWORD64>(client.base + offsets::entityList);
	if (!this->entity_list)
		return false;

	this->list_entry = p->read<DWORD64>(this->entity_list + offsets::EntityListOffset);
	if (!this->list_entry)
		return false;

	return true;
}