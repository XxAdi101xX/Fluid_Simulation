// Fill out your copyright notice in the Description page of Project Settings.


#include "Particle.h"

// Sets default values
AParticle::AParticle()
{
 	// We won't tick here, we will do the calculation in the rectangular prism class
	PrimaryActorTick.bCanEverTick = false;

    // Create and attach the ProceduralMeshComponent
    ProceduralMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = ProceduralMeshComponent; // Set it as the root component

	// Set default values for properties, these will mostly be overriden when these are spawned in by BoundingRectangularPrism
    Radius = 20.0f;
    NumLatitudeSegments = 32; // Default for a reasonably smooth sphere
    NumLongitudeSegments = 32; // Default for a reasonably smooth sphere
    Color = FLinearColor::Blue;
    Position = FVector::ZeroVector; // Default to 0, 0, 0
    Velocity = FVector::ZeroVector; // Default to no velocity

    // We don't want to use UE5's default collision system for this procedural mesh
	ProceduralMeshComponent->ContainsPhysicsTriMeshData(false);
}

void AParticle::OnConstruction(const FTransform &Transform)
{
	Super::OnConstruction(Transform);
	// Generate the circle mesh when the actor is constructed
    GenerateSphereMesh();
}

// Called when the game starts or when spawned
void AParticle::BeginPlay()
{
	Super::BeginPlay();
}

// Not used; disabled tick
void AParticle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AParticle::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
    FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
    if (PropertyName == GET_MEMBER_NAME_CHECKED(AParticle, Radius) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AParticle, NumLatitudeSegments) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AParticle, NumLongitudeSegments) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AParticle, Color)
    ) {
        GenerateSphereMesh();
    }
}

void AParticle::GenerateSphereMesh()
{
    // Clear any existing mesh data
    ProceduralMeshComponent->ClearAllMeshSections();

    // Arrays to hold mesh data
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FLinearColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    // Calculate number of vertices
    // Top pole + (NumLatitudeSegments-1) rings + bottom pole
    int32 NumPoles = 2; // Top and bottom poles
    int32 VertsPerRing = NumLongitudeSegments + 1; // +1 to close the loop
    int32 NumBodyRings = FMath::Max(0, NumLatitudeSegments - 1); // Exclude actual poles in rings calculation

    // --- Vertices ---
    // Top Pole (Index 0)
    Vertices.Add(Position + FVector(0.0f, 0.0f, Radius));
    Normals.Add(FVector::UpVector);
    UV0.Add(FVector2D(0.5f, 1.0f)); // Top of texture
    VertexColors.Add(Color);

    // Body Vertices (Rings)
    for (int32 LatIdx = 0; LatIdx < NumLatitudeSegments; ++LatIdx) // 0 to NumLatitudeSegments-1 (goes through all rings)
    {
        float Phi = PI * (float)(LatIdx + 1) / (float)(NumLatitudeSegments + 1); // Latitude angle from 0 to PI
        float SinPhi = FMath::Sin(Phi);
        float CosPhi = FMath::Cos(Phi);

        for (int32 LongIdx = 0; LongIdx <= NumLongitudeSegments; ++LongIdx) // 0 to NumLongitudeSegments (inclusive for seamless wrap)
        {
            float Theta = 2.0f * PI * (float)LongIdx / (float)NumLongitudeSegments; // Longitude angle from 0 to 2*PI
            float SinTheta = FMath::Sin(Theta);
            float CosTheta = FMath::Cos(Theta);

            FVector Vertex = FVector(Radius * SinPhi * CosTheta, Radius * SinPhi * SinTheta, Radius * CosPhi);
            Vertices.Add(Position + Vertex); // Apply position offset

            FVector Normal = Vertex.GetSafeNormal(); // Normal points from center to vertex
            Normals.Add(Normal);

            FVector2D UV = FVector2D(1.0f - (float)LongIdx / NumLongitudeSegments, (float)(LatIdx + 1) / (float)(NumLatitudeSegments + 1));
            UV0.Add(UV);

            VertexColors.Add(Color);
        }
    }

    // Bottom Pole (Last Index)
    Vertices.Add(Position + FVector(0.0f, 0.0f, -Radius));
    Normals.Add(FVector::DownVector);
    UV0.Add(FVector2D(0.5f, 0.0f)); // Bottom of texture
    VertexColors.Add(Color);

    // Top Cap Triangles (connecting top pole to first ring)
    for (int32 LongIdx = 0; LongIdx < NumLongitudeSegments; ++LongIdx)
    {
        Triangles.Add(0); // Top pole
        Triangles.Add(1 + LongIdx); // Current vertex on first ring
        Triangles.Add(1 + (LongIdx + 1)); // Next vertex on first ring
    }

    // Body Triangles (connecting rings)
    for (int32 LatIdx = 0; LatIdx < NumLatitudeSegments - 1; ++LatIdx) // Iterate through latitude bands
    {
        for (int32 LongIdx = 0; LongIdx < NumLongitudeSegments; ++LongIdx)
        {
            int32 CurrentRingStartIdx = 1 + (LatIdx * VertsPerRing); // Start index of current ring
            int32 NextRingStartIdx = 1 + ((LatIdx + 1) * VertsPerRing); // Start index of next ring

            int32 V0 = CurrentRingStartIdx + LongIdx;
            int32 V1 = CurrentRingStartIdx + LongIdx + 1;
            int32 V2 = NextRingStartIdx + LongIdx + 1;
            int32 V3 = NextRingStartIdx + LongIdx;

            // First triangle of the quad
            Triangles.Add(V0);
            Triangles.Add(V1);
            Triangles.Add(V2);

            // Second triangle of the quad
            Triangles.Add(V0);
            Triangles.Add(V2);
            Triangles.Add(V3);
        }
    }

    // Bottom Cap Triangles (connecting last ring to bottom pole)
    int32 BottomPoleIdx = Vertices.Num() - 1; // Index of the bottom pole
    int32 LastRingStartIdx = 1 + ((NumLatitudeSegments - 1) * VertsPerRing);

    for (int32 LongIdx = 0; LongIdx < NumLongitudeSegments; ++LongIdx)
    {
        Triangles.Add(BottomPoleIdx); // Bottom pole
        Triangles.Add(LastRingStartIdx + LongIdx + 1); // Next vertex on last ring
        Triangles.Add(LastRingStartIdx + LongIdx); // Current vertex on last ring
    }

    // Create the mesh section
    ProceduralMeshComponent->CreateMeshSection_LinearColor(
        0,
        Vertices,
        Triangles,
        Normals,
        UV0,
        VertexColors,
        Tangents,
        true // Enable collision for this mesh section
    );

    // Assign the material M_VertexColorCircle located under Content/Materials
    const FString MaterialPath = TEXT("/Game/Materials/M_VertexColorCircle.M_VertexColorCircle");
    UMaterialInterface *BaseMaterial = Cast<UMaterialInterface>(
        StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialPath)
    );

    if (BaseMaterial)
    {
        UMaterialInstanceDynamic *DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);

        if (DynamicMaterial && ProceduralMeshComponent)
        {
            ProceduralMeshComponent->SetMaterial(0, DynamicMaterial);
        }
    }
}

void AParticle::UpdatePosition(float DeltaTime)
{
    Position += Velocity * DeltaTime;
}
