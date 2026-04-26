// Copyright Epic Games, Inc. All Rights Reserved.

#include "SFSplineAnalyzer.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Components/SplineComponent.h"
#include "EngineUtils.h"
#include "SmartFoundations.h"

void FSFSplineAnalyzer::AnalyzeNearLocation(UWorld* World, const FVector& Location, float Radius)
{
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("🔍 SPLINE ANALYZER: Invalid world"));
		return;
	}

	UE_LOG(LogSmartFoundations, Display, TEXT("🔍 SPLINE ANALYZER: Analyzing pipes and belts within %.1fm"), Radius / 100.0f);
	
	// Analyze pipes first
	AnalyzePipesNearLocation(World, Location, Radius);
	
	// Then analyze belts
	AnalyzeBeltsNearLocation(World, Location, Radius);
}

void FSFSplineAnalyzer::AnalyzeBeltsNearLocation(UWorld* World, const FVector& Location, float Radius)
{
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("🔍 BELT ANALYZER: Invalid world"));
		return;
	}

	UE_LOG(LogSmartFoundations, Display, TEXT("🔍 BELT ANALYZER: Searching for belts within %.1fm of %s"),
		Radius / 100.0f, *Location.ToString());

	int32 BeltCount = 0;

	// Find all belts in world
	for (TActorIterator<AFGBuildableConveyorBelt> It(World); It; ++It)
	{
		AFGBuildableConveyorBelt* Belt = *It;
		if (!Belt) continue;

		// Check distance
		float Distance = FVector::Dist(Belt->GetActorLocation(), Location);
		if (Distance > Radius) continue;

		// Analyze this belt
		if (AnalyzeBeltSpline(Belt))
		{
			BeltCount++;
		}
	}

	UE_LOG(LogSmartFoundations, Display, TEXT("🔍 BELT ANALYZER: Analyzed %d belts"), BeltCount);
}

void FSFSplineAnalyzer::AnalyzePipesNearLocation(UWorld* World, const FVector& Location, float Radius)
{
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("🔍 SPLINE ANALYZER: Invalid world"));
		return;
	}

	UE_LOG(LogSmartFoundations, Display, TEXT("🔍 SPLINE ANALYZER: Searching for pipes within %.1fm of %s"),
		Radius / 100.0f, *Location.ToString());

	int32 PipeCount = 0;
	TArray<float> TangentScaleFactors;

	// Find all pipes in world
	for (TActorIterator<AFGBuildablePipeline> It(World); It; ++It)
	{
		AFGBuildablePipeline* Pipeline = *It;
		if (!Pipeline) continue;

		// Check distance
		float Distance = FVector::Dist(Pipeline->GetActorLocation(), Location);
		if (Distance > Radius) continue;

		// Analyze this pipe
		if (AnalyzePipeSpline(Pipeline))
		{
			PipeCount++;
		}
	}

	UE_LOG(LogSmartFoundations, Display, TEXT("🔍 SPLINE ANALYZER: Analyzed %d pipes"), PipeCount);
}

bool FSFSplineAnalyzer::AnalyzePipeSpline(AFGBuildablePipeline* Pipeline)
{
	if (!Pipeline)
	{
		return false;
	}

	// Get spline component
	USplineComponent* SplineComp = Pipeline->FindComponentByClass<USplineComponent>();
	if (!SplineComp)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("   ❌ Pipe has no spline component: %s"), *Pipeline->GetName());
		return false;
	}

	int32 NumPoints = SplineComp->GetNumberOfSplinePoints();
	float SplineLength = SplineComp->GetSplineLength();

	UE_LOG(LogSmartFoundations, Display, TEXT("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
	UE_LOG(LogSmartFoundations, Display, TEXT("📊 PIPE ANALYSIS: %s"), *Pipeline->GetName());
	UE_LOG(LogSmartFoundations, Display, TEXT("   Spline Points: %d"), NumPoints);
	UE_LOG(LogSmartFoundations, Display, TEXT("   Spline Length: %.1f cm"), SplineLength);

	// Analyze first and last points (most pipes are 2-point splines)
	if (NumPoints >= 2)
	{
		FVector StartPos = SplineComp->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
		FVector EndPos = SplineComp->GetLocationAtSplinePoint(NumPoints - 1, ESplineCoordinateSpace::World);
		
		FVector StartLeaveTangent = SplineComp->GetLeaveTangentAtSplinePoint(0, ESplineCoordinateSpace::World);
		FVector EndArriveTangent = SplineComp->GetArriveTangentAtSplinePoint(NumPoints - 1, ESplineCoordinateSpace::World);

		float StraightLineDistance = FVector::Dist(StartPos, EndPos);

		UE_LOG(LogSmartFoundations, Display, TEXT("   ━━━ ENDPOINTS ━━━"));
		UE_LOG(LogSmartFoundations, Display, TEXT("   Start Position: %s"), *StartPos.ToString());
		UE_LOG(LogSmartFoundations, Display, TEXT("   End Position:   %s"), *EndPos.ToString());
		UE_LOG(LogSmartFoundations, Display, TEXT("   Straight Distance: %.1f cm"), StraightLineDistance);

		UE_LOG(LogSmartFoundations, Display, TEXT("   ━━━ TANGENTS ━━━"));
		UE_LOG(LogSmartFoundations, Display, TEXT("   Start Leave Tangent:  %s (length: %.1f)"), 
			*StartLeaveTangent.ToString(), StartLeaveTangent.Size());
		UE_LOG(LogSmartFoundations, Display, TEXT("   End Arrive Tangent:   %s (length: %.1f)"), 
			*EndArriveTangent.ToString(), EndArriveTangent.Size());

		// Calculate tangent scale factor
		float ScaleFactor = CalculateTangentScaleFactor(StartPos, EndPos, StartLeaveTangent, EndArriveTangent);
		
		UE_LOG(LogSmartFoundations, Display, TEXT("   ━━━ ANALYSIS ━━━"));
		UE_LOG(LogSmartFoundations, Display, TEXT("   ⚡ TANGENT SCALE FACTOR: %.3f (%.1f%%)"), 
			ScaleFactor, ScaleFactor * 100.0f);
		UE_LOG(LogSmartFoundations, Display, TEXT("   📐 Formula: TangentLength = Distance × %.3f"), ScaleFactor);

		// Compare to our implementation (now 50cm fixed)
		float ExpectedTangent = 50.0f;  // Our fixed tangent
		float ActualAvgTangent = (StartLeaveTangent.Size() + EndArriveTangent.Size()) / 2.0f;
		float TangentDifference = FMath::Abs(ActualAvgTangent - ExpectedTangent);
		
		if (TangentDifference < 5.0f)
		{
			UE_LOG(LogSmartFoundations, Display, TEXT("   ✅ MATCH: Close to our 50cm fixed tangent (diff: %.1f cm)"), TangentDifference);
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ MISMATCH: Different from our 50cm fixed tangent (actual avg: %.1f cm, diff: %.1f cm)"), 
				ActualAvgTangent, TangentDifference);
		}

		// Check if tangents align with connector normals
		FVector Direction = (EndPos - StartPos).GetSafeNormal();
		FVector StartTangentDir = StartLeaveTangent.GetSafeNormal();
		FVector EndTangentDir = EndArriveTangent.GetSafeNormal();
		
		float StartAlignment = FVector::DotProduct(Direction, StartTangentDir);
		float EndAlignment = FVector::DotProduct(Direction, -EndTangentDir);  // Note: end tangent should oppose direction

		UE_LOG(LogSmartFoundations, Display, TEXT("   ━━━ TANGENT ALIGNMENT ━━━"));
		UE_LOG(LogSmartFoundations, Display, TEXT("   Start tangent alignment: %.3f (1.0 = parallel to direction)"), StartAlignment);
		UE_LOG(LogSmartFoundations, Display, TEXT("   End tangent alignment:   %.3f (1.0 = parallel to direction)"), EndAlignment);

		if (StartAlignment > 0.95f && EndAlignment > 0.95f)
		{
			UE_LOG(LogSmartFoundations, Display, TEXT("   ⚠️ LINEAR: Tangents align with straight direction (not using connector normals)"));
		}
		else
		{
			UE_LOG(LogSmartFoundations, Display, TEXT("   ✅ CURVED: Tangents diverge from straight direction (using connector normals)"));
		}
	}

	// Log all intermediate points if any
	if (NumPoints > 2)
	{
		UE_LOG(LogSmartFoundations, Display, TEXT("   ━━━ INTERMEDIATE POINTS ━━━"));
		for (int32 i = 1; i < NumPoints - 1; i++)
		{
			FVector Pos = SplineComp->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
			FVector ArriveTangent = SplineComp->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
			FVector LeaveTangent = SplineComp->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::World);

			UE_LOG(LogSmartFoundations, Display, TEXT("   Point %d: %s"), i, *Pos.ToString());
			UE_LOG(LogSmartFoundations, Display, TEXT("      Arrive: %s (%.1f)"), *ArriveTangent.ToString(), ArriveTangent.Size());
			UE_LOG(LogSmartFoundations, Display, TEXT("      Leave:  %s (%.1f)"), *LeaveTangent.ToString(), LeaveTangent.Size());
		}
	}

	UE_LOG(LogSmartFoundations, Display, TEXT("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));

	return true;
}

bool FSFSplineAnalyzer::AnalyzeBeltSpline(AFGBuildableConveyorBelt* Belt)
{
	if (!Belt)
	{
		return false;
	}

	// Get spline component
	USplineComponent* SplineComp = Belt->FindComponentByClass<USplineComponent>();
	if (!SplineComp)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("   ❌ Belt has no spline component: %s"), *Belt->GetName());
		return false;
	}

	int32 NumPoints = SplineComp->GetNumberOfSplinePoints();
	float SplineLength = SplineComp->GetSplineLength();

	UE_LOG(LogSmartFoundations, Display, TEXT("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
	UE_LOG(LogSmartFoundations, Display, TEXT("📦 BELT ANALYSIS: %s"), *Belt->GetName());
	UE_LOG(LogSmartFoundations, Display, TEXT("   Spline Points: %d"), NumPoints);
	UE_LOG(LogSmartFoundations, Display, TEXT("   Spline Length: %.1f cm"), SplineLength);

	// Analyze first and last points
	if (NumPoints >= 2)
	{
		FVector StartPos = SplineComp->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
		FVector EndPos = SplineComp->GetLocationAtSplinePoint(NumPoints - 1, ESplineCoordinateSpace::World);
		
		FVector StartLeaveTangent = SplineComp->GetLeaveTangentAtSplinePoint(0, ESplineCoordinateSpace::World);
		FVector EndArriveTangent = SplineComp->GetArriveTangentAtSplinePoint(NumPoints - 1, ESplineCoordinateSpace::World);

		float StraightLineDistance = FVector::Dist(StartPos, EndPos);

		UE_LOG(LogSmartFoundations, Display, TEXT("   ━━━ ENDPOINTS ━━━"));
		UE_LOG(LogSmartFoundations, Display, TEXT("   Start Position: %s"), *StartPos.ToString());
		UE_LOG(LogSmartFoundations, Display, TEXT("   End Position:   %s"), *EndPos.ToString());
		UE_LOG(LogSmartFoundations, Display, TEXT("   Straight Distance: %.1f cm"), StraightLineDistance);

		UE_LOG(LogSmartFoundations, Display, TEXT("   ━━━ TANGENTS ━━━"));
		UE_LOG(LogSmartFoundations, Display, TEXT("   Start Leave Tangent:  %s (length: %.1f)"), 
			*StartLeaveTangent.ToString(), StartLeaveTangent.Size());
		UE_LOG(LogSmartFoundations, Display, TEXT("   End Arrive Tangent:   %s (length: %.1f)"), 
			*EndArriveTangent.ToString(), EndArriveTangent.Size());

		// Calculate tangent scale factor
		float ScaleFactor = CalculateTangentScaleFactor(StartPos, EndPos, StartLeaveTangent, EndArriveTangent);
		
		UE_LOG(LogSmartFoundations, Display, TEXT("   ━━━ ANALYSIS ━━━"));
		UE_LOG(LogSmartFoundations, Display, TEXT("   ⚡ TANGENT SCALE FACTOR: %.3f (%.1f%%)"), 
			ScaleFactor, ScaleFactor * 100.0f);
		UE_LOG(LogSmartFoundations, Display, TEXT("   📐 Formula: TangentLength = Distance × %.3f"), ScaleFactor);

		// Compare to our implementation (now 50cm fixed)
		float ExpectedTangent = 50.0f;  // Our fixed tangent
		float ActualAvgTangent = (StartLeaveTangent.Size() + EndArriveTangent.Size()) / 2.0f;
		float TangentDifference = FMath::Abs(ActualAvgTangent - ExpectedTangent);
		
		if (TangentDifference < 5.0f)
		{
			UE_LOG(LogSmartFoundations, Display, TEXT("   ✅ MATCH: Close to our 50cm fixed tangent (diff: %.1f cm)"), TangentDifference);
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ MISMATCH: Different from our 50cm fixed tangent (actual avg: %.1f cm, diff: %.1f cm)"), 
				ActualAvgTangent, TangentDifference);
		}

		// Check tangent alignment
		FVector Direction = (EndPos - StartPos).GetSafeNormal();
		FVector StartTangentDir = StartLeaveTangent.GetSafeNormal();
		FVector EndTangentDir = EndArriveTangent.GetSafeNormal();
		
		float StartAlignment = FVector::DotProduct(Direction, StartTangentDir);
		float EndAlignment = FVector::DotProduct(Direction, -EndTangentDir);

		UE_LOG(LogSmartFoundations, Display, TEXT("   ━━━ TANGENT ALIGNMENT ━━━"));
		UE_LOG(LogSmartFoundations, Display, TEXT("   Start tangent alignment: %.3f (1.0 = parallel to direction)"), StartAlignment);
		UE_LOG(LogSmartFoundations, Display, TEXT("   End tangent alignment:   %.3f (1.0 = parallel to direction)"), EndAlignment);

		if (StartAlignment > 0.95f && EndAlignment > 0.95f)
		{
			UE_LOG(LogSmartFoundations, Display, TEXT("   ⚠️ LINEAR: Tangents align with straight direction (not using connector normals)"));
		}
		else
		{
			UE_LOG(LogSmartFoundations, Display, TEXT("   ✅ CURVED: Tangents diverge from straight direction (using connector normals)"));
		}
	}

	// Log intermediate points if any
	if (NumPoints > 2)
	{
		UE_LOG(LogSmartFoundations, Display, TEXT("   ━━━ INTERMEDIATE POINTS ━━━"));
		for (int32 i = 1; i < NumPoints - 1; i++)
		{
			FVector Pos = SplineComp->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
			FVector ArriveTangent = SplineComp->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
			FVector LeaveTangent = SplineComp->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::World);

			UE_LOG(LogSmartFoundations, Display, TEXT("   Point %d: %s"), i, *Pos.ToString());
			UE_LOG(LogSmartFoundations, Display, TEXT("      Arrive: %s (%.1f)"), *ArriveTangent.ToString(), ArriveTangent.Size());
			UE_LOG(LogSmartFoundations, Display, TEXT("      Leave:  %s (%.1f)"), *LeaveTangent.ToString(), LeaveTangent.Size());
		}
	}

	UE_LOG(LogSmartFoundations, Display, TEXT("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));

	return true;
}

float FSFSplineAnalyzer::CalculateTangentScaleFactor(
	const FVector& StartPos,
	const FVector& EndPos,
	const FVector& StartTangent,
	const FVector& EndTangent)
{
	float Distance = FVector::Dist(StartPos, EndPos);
	if (Distance < 1.0f)
	{
		return 0.0f;  // Too short to analyze
	}

	// Calculate scale factors for both tangents
	float StartTangentLength = StartTangent.Size();
	float EndTangentLength = EndTangent.Size();

	float StartScale = StartTangentLength / Distance;
	float EndScale = EndTangentLength / Distance;

	// Return average
	return (StartScale + EndScale) / 2.0f;
}
