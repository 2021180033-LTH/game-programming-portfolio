// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Misc/AutomationTest.h"

#include "Camera/GlobeCameraMath.h"

namespace EverPlanetGlobeCameraMathTests
{
	constexpr double VectorTolerance = 1.0e-4;

	bool IsOrthonormal(const FGlobeCameraFrame& Frame)
	{
		return Frame.ViewForward.IsNormalized()
			&& Frame.ViewRight.IsNormalized()
			&& Frame.ViewUp.IsNormalized()
			&& FMath::Abs(FVector::DotProduct(Frame.ViewForward, Frame.ViewRight)) <= VectorTolerance
			&& FMath::Abs(FVector::DotProduct(Frame.ViewForward, Frame.ViewUp)) <= VectorTolerance
			&& FMath::Abs(FVector::DotProduct(Frame.ViewRight, Frame.ViewUp)) <= VectorTolerance
			&& FVector::CrossProduct(Frame.ViewForward, Frame.ViewRight)
				.Equals(Frame.ViewUp, VectorTolerance);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGlobeCameraMathTest,
	"EverPlanet.Camera.GlobeCameraMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGlobeCameraMathTest::RunTest(const FString& Parameters)
{
	using namespace EverPlanetGlobeCameraMathTests;

	struct FFrameCase
	{
		FVector LocalUp;
		FVector TangentForward;
	};
	const TArray<FFrameCase> FrameCases = {
		{FVector::UpVector, FVector::ForwardVector},
		{-FVector::UpVector, FVector::ForwardVector},
		{FVector::ForwardVector, FVector::RightVector},
		{-FVector::ForwardVector, FVector::RightVector},
		{FVector::RightVector, FVector::UpVector},
		{-FVector::RightVector, FVector::UpVector},
		{FVector(1.0, 2.0, 3.0).GetSafeNormal(), FVector(-2.0, 1.0, 0.0).GetSafeNormal()},
	};

	constexpr double DownPitchDegrees = 38.0;
	const double ExpectedDownDot = -FMath::Sin(FMath::DegreesToRadians(DownPitchDegrees));
	for (int32 CaseIndex = 0; CaseIndex < FrameCases.Num(); ++CaseIndex)
	{
		FGlobeCameraFrame Frame;
		const bool bBuilt = FGlobeCameraMath::TryBuildViewFrame(
			FrameCases[CaseIndex].LocalUp,
			FrameCases[CaseIndex].TangentForward,
			DownPitchDegrees,
			Frame);
		TestTrue(FString::Printf(TEXT("Frame case %d builds"), CaseIndex), bBuilt);
		if (!bBuilt)
		{
			continue;
		}

		TestTrue(
			FString::Printf(TEXT("Frame case %d is orthonormal and right-handed"), CaseIndex),
			IsOrthonormal(Frame));
		TestTrue(
			FString::Printf(TEXT("Frame case %d keeps screen right tangent"), CaseIndex),
			FMath::Abs(FVector::DotProduct(Frame.ViewRight, Frame.LocalUp)) <= VectorTolerance);
		TestTrue(
			FString::Printf(TEXT("Frame case %d applies the requested down pitch"), CaseIndex),
			FMath::IsNearlyEqual(
				static_cast<double>(FVector::DotProduct(Frame.ViewForward, Frame.LocalUp)),
				ExpectedDownDot,
				VectorTolerance));
		TestTrue(
			FString::Printf(TEXT("Frame case %d rotation X matches view forward"), CaseIndex),
			Frame.ViewRotation.GetAxisX().Equals(Frame.ViewForward, VectorTolerance));
		TestTrue(
			FString::Printf(TEXT("Frame case %d rotation Y matches view right"), CaseIndex),
			Frame.ViewRotation.GetAxisY().Equals(Frame.ViewRight, VectorTolerance));
	}

	FGlobeCameraFrame InvalidFrame;
	TestFalse(
		TEXT("A zero local up cannot build a camera frame"),
		FGlobeCameraMath::TryBuildViewFrame(
			FVector::ZeroVector,
			FVector::ForwardVector,
			DownPitchDegrees,
			InvalidFrame));
	TestFalse(
		TEXT("A non-finite pitch cannot build a camera frame"),
		FGlobeCameraMath::TryBuildViewFrame(
			FVector::UpVector,
			FVector::ForwardVector,
			std::numeric_limits<double>::infinity(),
			InvalidFrame));
	TestFalse(
		TEXT("A pitch outside the camera contract is rejected"),
		FGlobeCameraMath::TryBuildViewFrame(
			FVector::UpVector,
			FVector::ForwardVector,
			90.0,
			InvalidFrame));

	FVector PreviousUp = FVector::UpVector;
	FVector PreviousTangent = FVector::ForwardVector;
	for (int32 Step = 1; Step <= 720; ++Step)
	{
		const double AngleRadians = FMath::DegreesToRadians(
			static_cast<double>(Step) * 0.5);
		const FVector CurrentUp(
			FMath::Sin(AngleRadians),
			0.0,
			FMath::Cos(AngleRadians));
		const FVector ExpectedTangent(
			FMath::Cos(AngleRadians),
			0.0,
			-FMath::Sin(AngleRadians));
		FVector TransportedTangent = FVector::ZeroVector;
		const bool bTransported = FGlobeCameraMath::TryParallelTransportTangent(
			PreviousUp,
			CurrentUp,
			PreviousTangent,
			TransportedTangent);
		TestTrue(FString::Printf(TEXT("Transport step %d succeeds"), Step), bTransported);
		if (!bTransported)
		{
			break;
		}
		TestTrue(
			FString::Printf(TEXT("Transport step %d follows the analytic tangent"), Step),
			TransportedTangent.Equals(ExpectedTangent, VectorTolerance));
		TestTrue(
			FString::Printf(TEXT("Transport step %d remains tangent"), Step),
			FMath::Abs(FVector::DotProduct(TransportedTangent, CurrentUp)) <= VectorTolerance);
		TestTrue(
			FString::Printf(TEXT("Transport step %d does not flip"), Step),
			FVector::DotProduct(TransportedTangent, PreviousTangent) > 0.999);
		PreviousUp = CurrentUp;
		PreviousTangent = TransportedTangent;
	}
	TestTrue(
		TEXT("A full transported lap closes its tangent heading"),
		PreviousTangent.Equals(FVector::ForwardVector, VectorTolerance));

	FVector AmbiguousTransport = FVector::ZeroVector;
	TestFalse(
		TEXT("An antipodal normal jump requires an explicit camera reseed"),
		FGlobeCameraMath::TryParallelTransportTangent(
			FVector::UpVector,
			-FVector::UpVector,
			FVector::ForwardVector,
			AmbiguousTransport));

	return true;
}

#endif
