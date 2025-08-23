#include "Particle.h"

// Sets default values
AParticle::AParticle()
{
    // Disable tick by default as it will be managed externally by the BoundingRectangularPrism
    PrimaryActorTick.bCanEverTick = false;

    // Create and attach the ProceduralMeshComponent
    // It's the RootComponent, so setting its World Location will move the actor.
    ProceduralMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = ProceduralMeshComponent;

    // Set default values for properties, these will mostly be overriden when these are spawned in by BoundingRectangularPrism
    Radius = 20.0f;
    NumLatitudeSegments = 32; // Default for a reasonably smooth sphere
    NumLongitudeSegments = 32; // Default for a reasonably smooth sphere
    Color = FLinearColor::Blue;
    Position = FVector::ZeroVector; // Default to world origin
    Velocity = FVector::ZeroVector;

    // We don't want to use UE5's default collision system for this procedural mesh
    ProceduralMeshComponent->ContainsPhysicsTriMeshData(false);
}

void AParticle::OnConstruction(const FTransform &Transform)
{
    Super::OnConstruction(Transform);
    // Generate the sphere mesh when the actor is constructed in editor
    GenerateSphereMesh();
    SetActorLocation(Position);
}

// Called when the game starts or when spawned
void AParticle::BeginPlay()
{
    Super::BeginPlay();

    // Generate the sphere mesh when the actor begins play and set the location. This may be redundant since OnConstruction has this logic,
    // but that's more for a visual aid prior to pressing play. This is the "proper" way to do this.
    GenerateSphereMesh();
    SetActorLocation(Position);
}

// Not used; disabled tick since particle movement is handled by the bounding rectangular prism
void AParticle::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);
}

#if WITH_EDITOR
void AParticle::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyName = PropertyChangedEvent.Property->GetFName() : NAME_None;
    if (PropertyName == GET_MEMBER_NAME_CHECKED(AParticle, Radius) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AParticle, NumLatitudeSegments) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AParticle, NumLongitudeSegments) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AParticle, Color) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AParticle, Position)) // Also respond to direct Position changes in editor
    {
        GenerateSphereMesh(); // Regenerate for shape/color changes
        SetActorLocation(Position); // Update actor location for direct Position changes
    }
}
#endif

void AParticle::GenerateSphereMesh()
{
    // Clear any existing mesh data
    ProceduralMeshComponent->ClearAllMeshSections();

    Vertices.Empty();
    Triangles.Empty();
    Normals.Empty();
    UV0.Empty();
    VertexColors.Empty();
    Tangents.Empty();

    // Calculate number of vertices
    int32 NumPoles = 2; // Top and bottom poles
    int32 VertsPerRing = NumLongitudeSegments + 1; // +1 to close the loop
    int32 NumBodyRings = FMath::Max(0, NumLatitudeSegments - 1); // Exclude actual poles in rings calculation

    // Vertices are now generated relative to the ProceduralMeshComponent's local origin (0,0,0)
    // The component itself will be moved to the particle's World Position.
    Vertices.Add(FVector(0.0f, 0.0f, Radius));
    Normals.Add(FVector::UpVector);
    UV0.Add(FVector2D(0.5f, 1.0f)); // Top of texture
    VertexColors.Add(Color);

    // Body Vertices (Rings)
    for (int32 LatIdx = 0; LatIdx < NumLatitudeSegments; ++LatIdx)
    {
        float Phi = PI * (float)(LatIdx + 1) / (float)(NumLatitudeSegments + 1);
        float SinPhi = FMath::Sin(Phi);
        float CosPhi = FMath::Cos(Phi);

        for (int32 LongIdx = 0; LongIdx <= NumLongitudeSegments; ++LongIdx)
        {
            float Theta = 2.0f * PI * (float)LongIdx / (float)NumLongitudeSegments;
            float SinTheta = FMath::Sin(Theta);
            float CosTheta = FMath::Cos(Theta);

            FVector Vertex = FVector(Radius * SinPhi * CosTheta, Radius * SinPhi * SinTheta, Radius * CosPhi);
            Vertices.Add(Vertex); // Vertices are relative to PMC local origin
            Normals.Add(Vertex.GetSafeNormal());
            UV0.Add(FVector2D(1.0f - (float)LongIdx / NumLongitudeSegments, (float)(LatIdx + 1) / (float)(NumLatitudeSegments + 1)));
            VertexColors.Add(Color);
        }
    }

    // Bottom Pole (Last Index) - Generated at local origin relative to PMC
    Vertices.Add(FVector(0.0f, 0.0f, -Radius));
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
    for (int32 LatIdx = 0; LatIdx < NumLatitudeSegments - 1; ++LatIdx)
    {
        for (int32 LongIdx = 0; LongIdx < NumLongitudeSegments; ++LongIdx)
        {
            int32 CurrentRingStartIdx = 1 + (LatIdx * VertsPerRing);
            int32 NextRingStartIdx = 1 + ((LatIdx + 1) * VertsPerRing);

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
    int32 BottomPoleIdx = Vertices.Num() - 1;
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

void AParticle::UpdateVertexColors(const FLinearColor& NewColor)
{
    // Update all vertex colors
    for (int32 i = 0; i < VertexColors.Num(); ++i)
    {
        VertexColors[i] = NewColor;
    }

    ProceduralMeshComponent->UpdateMeshSection_LinearColor(
        0,
        Vertices,      // Your array from sphere generation
        Normals,
        UV0,
        VertexColors,  // Your array of new colors
        Tangents,
        true
    );
}

void AParticle::UpdateColorBasedOnSpeed(float MinSpeed, float MaxSpeed)
{
    // Calculate the magnitude of velocity (speed)
    float Speed = Velocity.Size();

    // Clamp the speed to our min/max range
    Speed = FMath::Clamp(Speed, MinSpeed, MaxSpeed);

    // Calculate normalized speed (0 to 1)
    float NormalizedSpeed = (Speed - MinSpeed) / (MaxSpeed - MinSpeed);

    // Lerp from blue (slow) to red (fast)
    FLinearColor NewColor = FLinearColor::LerpUsingHSV(
        FLinearColor::Yellow,    // Slow color
        FLinearColor::Red,     // Fast color
        NormalizedSpeed
    );

    // Update the particle's color
    Color = NewColor;
    UpdateVertexColors(NewColor);
}

