#include "ObserverServices.hpp"
#include "core/engine/Engine.hpp"
#include "core/offsets/Dumper.hpp"

bool ObserverServices::Update() {
	auto p = Engine::GetProcess();

	if (!p)
		return false;

	if (!this->address) 
		return false;

	this->mode = p->read<ObserverMode>(this->address + offsets::observerServices::m_iObserverMode);
	this->target = p->read<int>(this->address + offsets::observerServices::m_hObserverTarget);

	return true;
}

void ObserverServices::SetAddress(DWORD64 address) {
	this->address = address;
}

const char* ObserverServices::ToString() const {
	switch (this->mode)
	{
	case ObserverMode::Alive:   return "本人";
	case ObserverMode::Unknown:   return "未知";
	case ObserverMode::First: return "第一人称";
	case ObserverMode::Third: return "第三人称";
	case ObserverMode::Free: return "自由视角";
	default:      return "未知";
	}
}