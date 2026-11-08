// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFScalingSpec.h"
#include "Misc/Crc.h"
#include "UObject/CoreNet.h"
#include "UObject/UnrealType.h"

// SP/MP DIVERGENCE MAP
// [MP-SEAM] Only the representation of a conduit entry changes. Decode restores the
// complete entry before the existing admission, pricing, replay and wiring paths.
// SP capture and all endpoint/geometry decisions are unchanged. No quantization.
namespace SFConduitEncoding
{
constexpr uint8 Version = 1;
constexpr uint32 MaxArray = 64; // Existing spline ceiling; Cost has its tighter bound below.

// Compare reflected leaves, not UScriptStruct::Identical: native transform equality
// may be tolerant. Numeric comparison preserves even signed zero and low-order bits.
bool Equal(const FProperty* P, const void* A, const void* B)
{
    if (const auto* S = CastField<FStructProperty>(P))
    {
        if (!S->Struct->PropertyLink) return false; // Opaque structs require an explicit codec.
        for (TFieldIterator<FProperty> It(S->Struct); It; ++It)
            for (int32 I = 0; I < It->ArrayDim; ++I)
                if (!Equal(*It, It->ContainerPtrToValuePtr<void>(A, I), It->ContainerPtrToValuePtr<void>(B, I))) return false;
        return true;
    }
    if (const auto* Array = CastField<FArrayProperty>(P))
    {
        FScriptArrayHelper X(Array, A), Y(Array, B);
        if (X.Num() != Y.Num()) return false;
        for (int32 I = 0; I < X.Num(); ++I)
            if (!Equal(Array->Inner, X.GetRawPtr(I), Y.GetRawPtr(I))) return false;
        return true;
    }
    if (CastField<FNumericProperty>(P)) return FMemory::Memcmp(A, B, P->GetElementSize()) == 0;
    return P->Identical(A, B, 0);
}

uint32 SchemaProperty(const FProperty* P, uint32 Hash)
{
    Hash = FCrc::StrCrc32(*P->GetName(), Hash);
    Hash = FCrc::StrCrc32(*P->GetCPPType(), Hash);
    Hash = FCrc::StrCrc32(*FString::FromInt(P->ArrayDim), Hash);
    if (const auto* S = CastField<FStructProperty>(P))
        for (TFieldIterator<FProperty> It(S->Struct); It; ++It) Hash = SchemaProperty(*It, Hash);
    if (const auto* A = CastField<FArrayProperty>(P)) Hash = SchemaProperty(A->Inner, Hash);
    return Hash;
}

uint32 Schema()
{
    static const uint32 Hash = []
    {
        uint32 Value = 0;
        for (TFieldIterator<FProperty> It(FSFConduitPlanEntry::StaticStruct()); It; ++It)
            Value = SchemaProperty(*It, Value);
        return Value;
    }();
    return Hash;
}

bool Field(FArchive& Ar, UPackageMap* Map, FProperty* P, void* Data, const void* Default)
{
    if (Ar.IsError()) return false;
    uint8 Present = Ar.IsSaving() && !Equal(P, Data, Default);
    Ar.SerializeBits(&Present, 1);
    if (Ar.IsError() || !Present) return !Ar.IsError(); // Receiver starts with initialized defaults.
    bool Mapped = true;
    if (auto* S = CastField<FStructProperty>(P))
    {
        if (!S->Struct->PropertyLink) { Ar.SetError(); return false; }
        // Recurse rather than invoking a native transform/vector compressor. Every
        // non-default numeric leaf keeps its full native numeric representation.
        for (TFieldIterator<FProperty> It(S->Struct); It && !Ar.IsError(); ++It)
            for (int32 I = 0; I < It->ArrayDim && !Ar.IsError(); ++I)
                Mapped &= Field(Ar, Map, *It, It->ContainerPtrToValuePtr<void>(Data, I), It->ContainerPtrToValuePtr<void>(Default, I));
    }
    else if (auto* Array = CastField<FArrayProperty>(P))
    {
        FScriptArrayHelper Values(Array, Data);
        uint32 Count = Values.Num();
        Ar.SerializeIntPacked(Count);
        if (Ar.IsError() || Count > MaxArray) { Ar.SetError(); return false; }
        if (Ar.IsLoading()) Values.Resize(Count);
        void* ElementDefault = FMemory::Malloc(Array->Inner->GetSize(), Array->Inner->GetMinAlignment());
        Array->Inner->InitializeValue(ElementDefault);
        for (uint32 I = 0; I < Count && !Ar.IsError(); ++I)
            Mapped &= Field(Ar, Map, Array->Inner, Values.GetRawPtr(I), ElementDefault);
        Array->Inner->DestroyValue(ElementDefault);
        FMemory::Free(ElementDefault);
    }
    else
    {
        if (!CastField<FNumericProperty>(P) && !CastField<FEnumProperty>(P)
            && !CastField<FBoolProperty>(P) && !CastField<FNameProperty>(P)
            && !CastField<FObjectPropertyBase>(P) && !CastField<FStrProperty>(P))
        {
            Ar.SetError(); // Do not enter deprecated map/set/opaque property network paths.
            return false;
        }
        // Keep Unreal's object/name package-map serialization and unmapped-reference
        // reporting. Never replace actor identity with a path search or raw pointer.
        Mapped = P->NetSerializeItem(Ar, Map, Data);
    }
    return Mapped && !Ar.IsError();
}
}

bool FSFConduitPlanEntry::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
    using namespace SFConduitEncoding;
    bOutSuccess = false;
    uint8 WireVersion = Version;
    uint32 WireSchema = Schema();
    Ar << WireVersion << WireSchema;
    if (Ar.IsError() || WireVersion != Version || WireSchema != Schema()) { Ar.SetError(); return false; }
    if (Ar.IsSaving() && (SplinePoints.Num() > 64 || Cost.Num() > 16)) { Ar.SetError(); return false; }
    const FSFConduitPlanEntry Default;
    FSFConduitPlanEntry Decoded;
    FSFConduitPlanEntry* Value = Ar.IsLoading() ? &Decoded : this;
    bool Mapped = true;
    for (TFieldIterator<FProperty> It(StaticStruct()); It && !Ar.IsError(); ++It)
        for (int32 I = 0; I < It->ArrayDim && !Ar.IsError(); ++I)
            Mapped &= Field(Ar, Map, *It, It->ContainerPtrToValuePtr<void>(Value, I), It->ContainerPtrToValuePtr<void>(&Default, I));
    if (Ar.IsError() || Value->Cost.Num() > 16) { Ar.SetError(); return false; }
    if (Ar.IsLoading()) *this = MoveTemp(Decoded);
    bOutSuccess = true;
    return Mapped;
}
