// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Per-feature log categories for Smart!.
 *
 * Historically everything logged to the single `LogSmartFoundations` category (declared in
 * SmartFoundations.h), which makes per-feature debugging hard — you cannot raise verbosity for
 * just Extend, or silence AutoConnect, without drowning in everything else. These named
 * categories let each feature log independently (e.g. `Log LogSmartExtend Verbose` at runtime).
 *
 * `LogSmartFoundations` remains the default/core category; new and migrated feature code should
 * prefer the matching category below. Migration is incremental — both coexist.
 */

// Extend (clone/extend factories: detection, topology, wiring, manifold serialization)
SMARTFOUNDATIONS_API DECLARE_LOG_CATEGORY_EXTERN(LogSmartExtend, Log, All);

// Auto-connect families (belts, pipes, power routing between placed buildings)
SMARTFOUNDATIONS_API DECLARE_LOG_CATEGORY_EXTERN(LogSmartAutoConnect, Log, All);

// Smart Upgrade (radius/network audit, traversal, execution, chain-actor repair)
SMARTFOUNDATIONS_API DECLARE_LOG_CATEGORY_EXTERN(LogSmartUpgrade, Log, All);

// Smart Restore (preset save/apply/share)
SMARTFOUNDATIONS_API DECLARE_LOG_CATEGORY_EXTERN(LogSmartRestore, Log, All);

// Grid scaling / child-hologram lifecycle
SMARTFOUNDATIONS_API DECLARE_LOG_CATEGORY_EXTERN(LogSmartGrid, Log, All);

// Holograms (Smart hologram subclasses: factory, conveyor, pipeline, power)
SMARTFOUNDATIONS_API DECLARE_LOG_CATEGORY_EXTERN(LogSmartHologram, Log, All);

// UI / HUD (Smart Panel, Upgrade panel, HUD widget)
SMARTFOUNDATIONS_API DECLARE_LOG_CATEGORY_EXTERN(LogSmartUI, Log, All);

// Directional arrow visualization (arrow assets, orbit/label rendering)
SMARTFOUNDATIONS_API DECLARE_LOG_CATEGORY_EXTERN(LogSmartArrows, Log, All);
