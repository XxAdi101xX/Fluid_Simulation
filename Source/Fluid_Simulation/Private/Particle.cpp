// Fill out your copyright notice in the Description page of Project Settings.


#include "Particle.h"

// Sets default values
AParticle::AParticle()
{
 	// Set this actor to call Tick() every frame.
	PrimaryActorTick.bCanEverTick = false;

    // Create and attach the ProceduralMeshComponent
    ProceduralMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = ProceduralMeshComponent; // Set it as the root component

    // Set default values for properties
    Radius = 100.0f;
    NumSegments = 64; // Default to a reasonably smooth circle
    Color = FLinearColor::Blue; // Default color

    // Ensure the procedural mesh component is set up for collision if needed
    // ProceduralMeshComponent->ContainsPhysicsTriMeshData(true); // Uncomment if you need collision from the mesh data

}

void AParticle::OnConstruction(const FTransform &Transform)
{
	Super::OnConstruction(Transform);
	// Generate the circle mesh when the actor is constructed
	GenerateCircleMesh();
}

// Called when the game starts or when spawned
void AParticle::BeginPlay()
{
	Super::BeginPlay();

	// Generate the circle mesh when the actor begins play
	GenerateCircleMesh();
	
}

// Called every frame
void AParticle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AParticle::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
    FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
    if (PropertyName == GET_MEMBER_NAME_CHECKED(AParticle, Radius) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AParticle, NumSegments) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AParticle, Color)
    ) {
        GenerateCircleMesh();
    }
}

void AParticle::GenerateCircleMesh()
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

    // Center vertex
    Vertices.Add(FVector(0.0f, 0.0f, 0.0f));
    Normals.Add(FVector(0.0f, 0.0f, 1.0f)); // Normal pointing up
    UV0.Add(FVector2D(0.5f, 0.5f)); // Center of UV space
    VertexColors.Add(Color);

    // Vertices around the circle
    for (int32 i = 0; i < NumSegments; ++i)
    {
        float Angle = (float)i * (360.0f / NumSegments);
        float Radian = FMath::DegreesToRadians(Angle);

        // Calculate vertex position
        float X = Radius * FMath::Cos(Radian);
        float Y = Radius * FMath::Sin(Radian);
        Vertices.Add(FVector(X, Y, 0.0f));

        // Add normal (all pointing up for a flat circle)
        Normals.Add(FVector(0.0f, 0.0f, 1.0f));

        // Add UV coordinates (mapping circle to a square texture)
        UV0.Add(FVector2D((X / Radius * 0.5f) + 0.5f, (Y / Radius * 0.5f) + 0.5f));

        // Add vertex color
        VertexColors.Add(Color);
    }

    // Triangles
    // The center vertex (index 0) connects to two adjacent outer vertices to form a triangle.
    for (int32 i = 0; i < NumSegments; ++i)
    {
        Triangles.Add(0); // Center vertex
        Triangles.Add(i + 1); // Current outer vertex
        Triangles.Add((i + 1) % NumSegments + 1); // Next outer vertex (wraps around for the last segment)
    }

    // Create the mesh section
    // SectionIndex 0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, Collision
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

    // Material is created on UE5 side and set there
}
