// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Extend/Net/SFExtendSourceSnapshot.h"
#include "SmartFoundations.h"
#include "Buildables/FGBuildable.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

bool FSFSourceTopology::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
    TArray<uint8> Bytes;
    bOutSuccess = false;
    if (Ar.IsSaving())
    {
        if (!SFExtendSourceSnapshot::FitsReply(*this)) { Ar.SetError(); return true; }
        FMemoryWriter Writer(Bytes);
        Writer.SetWantBinaryPropertySerialization(true);
        StaticStruct()->SerializeBin(Writer, this);
        if (Writer.IsError()) { Ar.SetError(); return true; }
    }
    uint32 Size = Bytes.Num();
    Ar.SerializeIntPacked(Size);
    if (Ar.IsError() || Size > 32 * 1024) { Ar.SetError(); return true; }
    if (Ar.IsLoading()) Bytes.SetNumUninitialized(Size);
    if (Size) Ar.Serialize(Bytes.GetData(), Size);
    if (Ar.IsError()) return true;
    if (Ar.IsLoading())
    {
        FSFSourceTopology Received;
        FMemoryReader Reader(Bytes);
        Reader.SetWantBinaryPropertySerialization(true);
        StaticStruct()->SerializeBin(Reader, &Received);
        if (Reader.IsError() || Reader.Tell() != Bytes.Num() || !SFExtendSourceSnapshot::FitsReply(Received))
        {
            Ar.SetError();
            return true;
        }
        *this = MoveTemp(Received);
    }
    bOutSuccess = true;
    return true;
}

bool SFExtendSourceSnapshot::FitsReply(const FSFSourceTopology& Source)
{
    const int32 Chains = Source.BeltInputChains.Num() + Source.BeltOutputChains.Num()
        + Source.PipeInputChains.Num() + Source.PipeOutputChains.Num();
    int32 Parts = Source.PowerPoles.Num() + Source.PipePassthroughs.Num() + Source.WallHoles.Num();
    int32 Points = 0;
    if (Chains > 32 || Parts > 64) return false;
    auto CheckSegments = [&](const TArray<FSFSourceSegment>& Segments)
    {
        Parts += Segments.Num();
        if (Parts > 64) return false;
        for (const FSFSourceSegment& Segment : Segments)
        {
            Points += Segment.SplineData.Points.Num();
            if (Segment.SplineData.Points.Num() > 64 || Points > 256
                || Segment.RelatedSourceIds.Num() > 64 || Segment.LiftData.PassthroughCloneIds.Num() > 2)
                return false;
        }
        return true;
    };
    // The two decorator arrays were counted above; check their variable geometry too.
    Parts -= Source.PipePassthroughs.Num() + Source.WallHoles.Num();
    if (!CheckSegments(Source.PipePassthroughs) || !CheckSegments(Source.WallHoles)) return false;
    for (const TArray<FSFSourceChain>* Group : {&Source.BeltInputChains, &Source.BeltOutputChains,
        &Source.PipeInputChains, &Source.PipeOutputChains})
    {
        for (const FSFSourceChain& Chain : *Group)
        {
            if (++Parts > 64 || Chain.Distributor.ConnectedConnectors.Num() > 32
                || Chain.Distributor.ConnectorWorldPositions.Num() > 32 || !CheckSegments(Chain.Segments))
                return false;
        }
    }
    for (const FSFSourcePowerPole& Pole : Source.PowerPoles)
        if (Pole.PortCapacities.Num() > 32) return false;

    // All snapshot fields are values, with no UObject references. This includes every
    // string/customization field in the budget; leave room for RPC framing and owner refs.
    TArray<uint8> Bytes;
    FMemoryWriter Writer(Bytes);
    // Apply binary serialization to nested records as well. Tagged save archives
    // add field names/types which RPC property serialization does not transmit.
    Writer.SetWantBinaryPropertySerialization(true);
    FSFSourceTopology::StaticStruct()->SerializeBin(Writer, const_cast<FSFSourceTopology*>(&Source));
    return !Writer.IsError() && Bytes.Num() <= 32 * 1024;
}

void SFExtendSourceSnapshot::RemapOwners(FSFSourceTopology& Source, const TMap<FString, FString>& Owners)
{
    auto Id = [&](FString& Value) { if (const FString* Local = Owners.Find(Value)) Value = *Local; };
    auto Segment = [&](FSFSourceSegment& Value)
    {
        Id(Value.Id);
        Id(Value.Connections.ConveyorAny0.Target);
        Id(Value.Connections.ConveyorAny1.Target);
        Id(Value.PassthroughTop.Target);
        Id(Value.PassthroughBottom.Target);
        Id(Value.ConnectedPowerPoleSourceId);
        for (FString& Related : Value.RelatedSourceIds) Id(Related);
        for (FString& Hole : Value.LiftData.PassthroughCloneIds) Id(Hole);
    };
    Id(Source.Factory.Id);
    for (TArray<FSFSourceChain>* Group : {&Source.BeltInputChains, &Source.BeltOutputChains,
        &Source.PipeInputChains, &Source.PipeOutputChains})
        for (FSFSourceChain& Chain : *Group)
        {
            Id(Chain.Distributor.Id);
            for (FSFSourceSegment& Value : Chain.Segments) Segment(Value);
        }
    for (FSFSourcePowerPole& Pole : Source.PowerPoles) Id(Pole.Id);
    for (FSFSourceSegment& Value : Source.PipePassthroughs) Segment(Value);
    for (FSFSourceSegment& Value : Source.WallHoles) Segment(Value);
}

bool SFExtendSourceSnapshot::ResolveReply(FSFExtendTopology& Reply)
{
    if (!Reply.bIsValid || !Reply.bHasSourceSnapshot || !Reply.SourceBuilding.IsValid()
        || Reply.SourceOwners.Num() > 128 || !FitsReply(Reply.SourceSnapshot)) return false;
    TMap<FString, FString> Owners;
    bool bHasSource = false;
    for (const FSFExtendSourceOwner& Owner : Reply.SourceOwners)
    {
        if (!Owner.Actor.IsValid()) continue; // Value-only conveyor members retain their stable IDs.
        if (Owner.Id == Reply.SourceSnapshot.Factory.Id)
        {
            if (Owner.Actor != Reply.SourceBuilding) return false;
            bHasSource = true;
        }
        Owners.Add(Owner.Id, Owner.Actor->GetName());
    }
    if (!bHasSource) return false;
    RemapOwners(Reply.SourceSnapshot, Owners);
    return Reply.SourceSnapshot.Factory.Id == Reply.SourceBuilding->GetName();
}
