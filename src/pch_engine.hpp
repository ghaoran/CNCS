// Engine Module Precompiled Header - Engine-specific heavy headers
// Include this in engine module source files only

#pragma once

#include "pch_core.hpp"

// Engine types
#include "core/engine/types/Vec2.hpp"
#include "core/engine/types/Vec3.hpp"
#include "core/engine/types/Matrix.hpp"
#include "core/engine/types/Color.hpp"
#include "core/engine/types/Types.hpp"

// Engine classes
#include "core/engine/classes/Bones.hpp"
#include "core/engine/classes/Game.hpp"
#include "core/engine/classes/Player.hpp"
#include "core/engine/classes/Bomb.hpp"
#include "core/engine/classes/Weapon.hpp"
#include "core/engine/classes/Globals.hpp"
#include "core/engine/classes/ObserverServices.hpp"

// Engine core
#include "core/engine/Engine.hpp"
#include "core/engine/cache/Cache.hpp"

// Offsets & Memory
#include "core/offsets/Offsets.hpp"
#include "core/offsets/Dumper.hpp"
#include "core/memory/Memory.hpp"
#include "core/kernel/KdLoader.hpp"

// Visibility
#include "core/visibility/Visibility.hpp"
#include "core/visibility/CollisionMesh.hpp"

// Config
#include "config/Current.hpp"
#include "config/Config.hpp"

// Logger
#include "core/logger/LogHelper.hpp"