#include "Core/Helpers/SFConveyorConnectionHelper.h"
#include "SmartFoundations.h"

bool FSFConveyorConnectionHelper::ConnectToPreviousConveyor(
    AFGBuildableConveyorBase* CurrentConveyor,
    AFGBuildableConveyorBase* PreviousConveyor)
{
    if (!CurrentConveyor || !PreviousConveyor)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔗 ConnectToPreviousConveyor: Invalid conveyor (Current=%s, Previous=%s)"),
            CurrentConveyor ? *CurrentConveyor->GetName() : TEXT("null"),
            PreviousConveyor ? *PreviousConveyor->GetName() : TEXT("null"));
        return false;
    }
    
    UFGFactoryConnectionComponent* CurrentConn0 = CurrentConveyor->GetConnection0();
    UFGFactoryConnectionComponent* PrevConn1 = PreviousConveyor->GetConnection1();
    
    return EstablishConnection(PrevConn1, CurrentConn0, TEXT("CHAIN LINK"));
}

bool FSFConveyorConnectionHelper::ConnectToDistributor(
    AFGBuildableConveyorBase* Conveyor,
    AFGBuildable* Distributor,
    bool bIsInputChain)
{
    if (!Conveyor || !Distributor)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔗 ConnectToDistributor: Invalid buildable (Conveyor=%s, Distributor=%s)"),
            Conveyor ? *Conveyor->GetName() : TEXT("null"),
            Distributor ? *Distributor->GetName() : TEXT("null"));
        return false;
    }
    
    UFGFactoryConnectionComponent* ConveyorConn0 = Conveyor->GetConnection0();
    if (!ConveyorConn0)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔗 ConnectToDistributor: Conveyor %s has no Conn0"), *Conveyor->GetName());
        return false;
    }
    
    if (ConveyorConn0->IsConnected())
    {
        UE_LOG(LogSmartFoundations, Log, TEXT("🔗 ConnectToDistributor: Conveyor %s Conn0 already connected"), *Conveyor->GetName());
        return false;
    }
    
    // For INPUT chains: Conveyor receives items FROM distributor, so we need distributor's OUTPUT
    // For OUTPUT chains: Conveyor sends items TO distributor, so we need distributor's INPUT
    EFactoryConnectionDirection NeededDir = bIsInputChain ? 
        EFactoryConnectionDirection::FCD_OUTPUT : EFactoryConnectionDirection::FCD_INPUT;
    
    UFGFactoryConnectionComponent* DistConn = FindBestConnection(
        Distributor, 
        ConveyorConn0->GetComponentLocation(), 
        NeededDir, 
        true);
    
    if (!DistConn)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔗 ConnectToDistributor: No suitable %s connection found on %s"),
            bIsInputChain ? TEXT("OUTPUT") : TEXT("INPUT"),
            *Distributor->GetName());
        return false;
    }
    
    return EstablishConnection(DistConn, ConveyorConn0, TEXT("DISTRIBUTOR"));
}

bool FSFConveyorConnectionHelper::ConnectToFactory(
    AFGBuildableConveyorBase* Conveyor,
    AFGBuildable* Factory,
    bool bConnectConn1,
    bool bNeedsInput)
{
    if (!Conveyor || !Factory)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔗 ConnectToFactory: Invalid buildable (Conveyor=%s, Factory=%s)"),
            Conveyor ? *Conveyor->GetName() : TEXT("null"),
            Factory ? *Factory->GetName() : TEXT("null"));
        return false;
    }
    
    UFGFactoryConnectionComponent* ConveyorConn = bConnectConn1 ? 
        Conveyor->GetConnection1() : Conveyor->GetConnection0();
    
    if (!ConveyorConn)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔗 ConnectToFactory: Conveyor %s has no Conn%d"), 
            *Conveyor->GetName(), bConnectConn1 ? 1 : 0);
        return false;
    }
    
    if (ConveyorConn->IsConnected())
    {
        UE_LOG(LogSmartFoundations, Log, TEXT("🔗 ConnectToFactory: Conveyor %s Conn%d already connected"), 
            *Conveyor->GetName(), bConnectConn1 ? 1 : 0);
        return false;
    }
    
    // Factory needs INPUT if we're delivering items TO it (end of INPUT chain)
    // Factory needs OUTPUT if we're taking items FROM it (start of OUTPUT chain)
    EFactoryConnectionDirection NeededDir = bNeedsInput ? 
        EFactoryConnectionDirection::FCD_INPUT : EFactoryConnectionDirection::FCD_OUTPUT;
    
    UFGFactoryConnectionComponent* FactoryConn = FindBestConnection(
        Factory,
        ConveyorConn->GetComponentLocation(),
        NeededDir,
        true);
    
    if (!FactoryConn)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔗 ConnectToFactory: No suitable %s connection found on %s"),
            bNeedsInput ? TEXT("INPUT") : TEXT("OUTPUT"),
            *Factory->GetName());
        return false;
    }
    
    return EstablishConnection(ConveyorConn, FactoryConn, TEXT("FACTORY"));
}

UFGFactoryConnectionComponent* FSFConveyorConnectionHelper::FindBestConnection(
    AFGBuildable* Buildable,
    const FVector& ReferenceLocation,
    EFactoryConnectionDirection NeededDirection,
    bool bMustBeUnconnected)
{
    if (!Buildable)
    {
        return nullptr;
    }
    
    TArray<UFGFactoryConnectionComponent*> Connections;
    Buildable->GetComponents<UFGFactoryConnectionComponent>(Connections);
    
    UFGFactoryConnectionComponent* BestConn = nullptr;
    float BestDist = FLT_MAX;
    
    for (UFGFactoryConnectionComponent* Conn : Connections)
    {
        if (!Conn)
        {
            continue;
        }
        
        // Check direction
        if (Conn->GetDirection() != NeededDirection)
        {
            continue;
        }
        
        // Check if already connected (if required)
        if (bMustBeUnconnected && Conn->IsConnected())
        {
            continue;
        }
        
        // Find closest
        float Dist = FVector::Dist(ReferenceLocation, Conn->GetComponentLocation());
        if (Dist < BestDist)
        {
            BestDist = Dist;
            BestConn = Conn;
        }
    }
    
    return BestConn;
}

bool FSFConveyorConnectionHelper::CanConnect(
    UFGFactoryConnectionComponent* Conn1,
    UFGFactoryConnectionComponent* Conn2)
{
    if (!Conn1 || !Conn2)
    {
        return false;
    }
    
    // Both must be unconnected
    if (Conn1->IsConnected() || Conn2->IsConnected())
    {
        return false;
    }
    
    // Directions must be compatible (one input, one output, or both any)
    EFactoryConnectionDirection Dir1 = Conn1->GetDirection();
    EFactoryConnectionDirection Dir2 = Conn2->GetDirection();
    
    // ANY can connect to anything
    if (Dir1 == EFactoryConnectionDirection::FCD_ANY || Dir2 == EFactoryConnectionDirection::FCD_ANY)
    {
        return true;
    }
    
    // Otherwise, need opposite directions
    return (Dir1 == EFactoryConnectionDirection::FCD_INPUT && Dir2 == EFactoryConnectionDirection::FCD_OUTPUT) ||
           (Dir1 == EFactoryConnectionDirection::FCD_OUTPUT && Dir2 == EFactoryConnectionDirection::FCD_INPUT);
}

bool FSFConveyorConnectionHelper::EstablishConnection(
    UFGFactoryConnectionComponent* FromConn,
    UFGFactoryConnectionComponent* ToConn,
    const FString& ContextDescription)
{
    if (!FromConn || !ToConn)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("🔗 EstablishConnection [%s]: Invalid connection (From=%s, To=%s)"),
            *ContextDescription,
            FromConn ? *FromConn->GetName() : TEXT("null"),
            ToConn ? *ToConn->GetName() : TEXT("null"));
        return false;
    }
    
    if (FromConn->IsConnected())
    {
        UE_LOG(LogSmartFoundations, Log, TEXT("🔗 EstablishConnection [%s]: %s already connected"),
            *ContextDescription, *FromConn->GetName());
        return false;
    }
    
    if (ToConn->IsConnected())
    {
        UE_LOG(LogSmartFoundations, Log, TEXT("🔗 EstablishConnection [%s]: %s already connected"),
            *ContextDescription, *ToConn->GetName());
        return false;
    }
    
    // Establish the connection
    FromConn->SetConnection(ToConn);
    
    // Get owner names for logging
    FString FromOwner = FromConn->GetOwner() ? FromConn->GetOwner()->GetName() : TEXT("Unknown");
    FString ToOwner = ToConn->GetOwner() ? ToConn->GetOwner()->GetName() : TEXT("Unknown");
    
    UE_LOG(LogSmartFoundations, Log, TEXT("🔗 EstablishConnection [%s]: ✅ %s.%s → %s.%s"),
        *ContextDescription,
        *FromOwner, *FromConn->GetName(),
        *ToOwner, *ToConn->GetName());
    
    return true;
}
