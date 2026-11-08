// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/Net/SFScalingPayloadBudget.h"
#include "Core/Net/SFRCO.h"
#include "Features/Scaling/SFScalingSpec.h"
#include "Engine/ActorChannel.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/RepLayout.h"
#include "UObject/CoreNet.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

// SP/MP DIVERGENCE MAP — staging payload budget
// [MP-CLIENT] Size the existing reliable RPC through its native replication layout.
// SP has no staging RPC. Authority admission, endpoint selection and construction are unchanged.
FSFScalingPayloadBudget FSFScalingPayloadBudget::Measure(APlayerController* Player, const FSFScalingSpec& Spec)
{
    FSFScalingPayloadBudget Result;
    UWorld* World = IsValid(Player) ? Player->GetWorld() : nullptr;
    UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
    UNetConnection* Connection = Driver ? Driver->ServerConnection.Get() : nullptr;
    if (!World || World->GetNetMode() != NM_Client || !Connection || !Connection->PackageMap
        || Connection->IsInternalAck()) return Result;
    UActorChannel* Channel = Connection->FindActorChannelRef(Player);
    UFunction* Function = USFRCO::StaticClass()->FindFunctionByName(TEXT("Server_StageScalingSpec"));
    FStructProperty* Parameter = Function ? FindFProperty<FStructProperty>(Function, TEXT("Spec")) : nullptr;
    if (!Channel || !Parameter || Parameter->Struct != FSFScalingSpec::StaticStruct() || Function->NumParms != 1)
        return Result;
    const auto Layout = Driver->GetFunctionRepLayout(Function);
    if (!Layout.IsValid()) return Result;

    // Use the same package map and function serializer as the imminent RPC. Client
    // references retain native GUID/path encoding; no pointer-size or per-entry estimate.
    // Work on a copy so a property serializer cannot alter the captured construction plan.
    FStructOnScope Parameters(Function);
    Parameter->CopyCompleteValue(Parameter->ContainerPtrToValuePtr<void>(Parameters.GetStructMemory()), &Spec);
    FNetBitWriter Writer(Connection->PackageMap.Get(), (MaxBytes - EnvelopeReserveBytes) * 8);
    Writer.SetAllowResize(false);
    Writer.SetAllowOverflow(true); // Expected budget refusal, not a network overflow to send.
    Layout->SendPropertiesForRPC(Function, Channel, Writer, FConstRepObjectDataBuffer(Parameters.GetStructMemory()));
    Result.ParameterBits = Writer.GetNumBits();
    Result.Status = Writer.IsError() ? ESFScalingPayloadStatus::TooLarge : ESFScalingPayloadStatus::Fits;
    return Result;
}
