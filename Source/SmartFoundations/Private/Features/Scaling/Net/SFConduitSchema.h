// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Misc/Crc.h"

namespace SFConduitEncoding
{
// Reflection names have FName identity, not case-sensitive display identity.
// Shipping builds do not preserve name casing: the first interned spelling can
// differ between processes (observed Recipe/recipe and Amount/amount on Windows
// and Linux). CPP type spellings also contain reflected class/struct/enum names.
// Normalize fingerprint tokens only; field order, types, dimensions and payload
// values are unchanged. A real schema/version mismatch must still be rejected.
inline uint32 HashSchemaToken(FString Token, uint32 Hash)
{
    Token.ToLowerInline();
    return FCrc::StrCrc32(*Token, Hash);
}
}
