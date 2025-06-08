// Fill out your copyright notice in the Description page of Project Settings.


#include "BoundingRectangularPrism.h"

#include "Particle.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
ABoundingRectangularPrism::ABoundingRectangularPrism()
{
 	// Set this actor to call Tick() every frame.
	PrimaryActorTick.bCanEverTick = true;

    // Set default values for bounding box
    BoxExtent = FVector(200.0f, 200.0f, 200.0f);

    // Set environment params
    Gravity = 200.0f;
	ParticleCountPerAxis = 3; // Default number of particles per axis
    ParticleGridSpacing = 50.0f;
    JitterFactor = 1.0f;
	ParticleRadius = 10.0f; // Default radius of each particle

    // Debugging
    bDrawBoundingBox = true;

     ConstructorHelpers::FClassFinder<AParticle> ParticleBPClass(TEXT("/Game/Blueprints/BP_Particle"));
     if (ParticleBPClass.Class != nullptr)
     {
         ParticleClass = ParticleBPClass.Class;
     }
     else
     {
         UE_LOG(LogTemp, Error, TEXT("Can't find Particle class or invalid world."));
     }
}

// Called when the game starts or when spawned
void ABoundingRectangularPrism::BeginPlay()
{
	Super::BeginPlay();

	// Clear any existing particles before spawning new ones
	DestroyAllParticles();
    SpawnParticles();
}

void ABoundingRectangularPrism::OnConstruction(const FTransform &Transform)
{
    Super::OnConstruction(Transform);

    // We want to visualize the bounding box and the particles before the game starts
    DrawBoundingRectangularPrism();

    // Clear any existing particles before spawning new ones
    DestroyAllParticles();
    SpawnParticles();
    UpdateParticles(0.0f); // Update particles immediately after spawning
}

void ABoundingRectangularPrism::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
    if (PropertyName == GET_MEMBER_NAME_CHECKED(ABoundingRectangularPrism, BoxExtent) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(ABoundingRectangularPrism, bDrawBoundingBox) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(ABoundingRectangularPrism, Gravity) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(ABoundingRectangularPrism, ParticleCountPerAxis) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(ABoundingRectangularPrism, ParticleGridSpacing) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(ABoundingRectangularPrism, JitterFactor) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(ABoundingRectangularPrism, ParticleRadius))
    {
        // We want to visualize the bounding box and the particles before the game starts
        DrawBoundingRectangularPrism();

        // Clear any existing particles before spawning new ones
        DestroyAllParticles();
        SpawnParticles();
		UpdateParticles(0.0f); // Update particles immediately after spawning
    }
}


// Called every frame
void ABoundingRectangularPrism::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    
	// Draw the bounding box every frame, it will clear out otherwise
    DrawBoundingRectangularPrism();

    UpdateParticles(DeltaTime);
}
void ABoundingRectangularPrism::DrawBoundingRectangularPrism()
{
    // Draw the debug bounding box if enabled
    if (bDrawBoundingBox && GetWorld())
    {
        DrawDebugBox(
            GetWorld(),
            GetActorLocation(), // World location of the box center
            BoxExtent,
            FColor::Red,
            false, // Persistent lines
            -1.0f, // Duration (-1.0f for infinite during runtime if bPersistentLines is true, or for 1 frame if false and called every tick)
            0, // Depth priority
            2.0f // Thickness
        );
    }
}

void ABoundingRectangularPrism::SpawnParticles()
{
    // Ensure ParticleCountPerAxis is at least 1 to avoid division by zero or negative extents
    if (ParticleCountPerAxis < 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("ParticleCountPerAxis must be at least 1. Adjusting to 1."));
        ParticleCountPerAxis = 1;
    }

    // Calculate the total span of the grid along one axis
    // For N particles, there are (N-1) gaps between them.
    float TotalSpanPerAxis = (ParticleCountPerAxis > 1) ? (ParticleCountPerAxis - 1.0f) * ParticleGridSpacing : 0.0f;

    for (int x = 0; x < ParticleCountPerAxis; x++)
    {
        for (int y = 0; y < ParticleCountPerAxis; y++)
        {
            for (int z = 0; z < ParticleCountPerAxis; z++)
            {
                // Calculate normalized position (0 to 1) for each particle along its axis
                // If ParticleCountPerAxis is 1, tx/ty/tz will be 0.0f, which is handled correctly below.
                float tx = (ParticleCountPerAxis > 1) ? (float)x / (ParticleCountPerAxis - 1.0f) : 0.5f; // Center if only one particle
                float ty = (ParticleCountPerAxis > 1) ? (float)y / (ParticleCountPerAxis - 1.0f) : 0.5f;
                float tz = (ParticleCountPerAxis > 1) ? (float)z / (ParticleCountPerAxis - 1.0f) : 0.5f;

                // Calculate the raw position relative to the grid's bottom-left-front corner
                // This would create a grid starting at GetActorLocation().X - TotalSpanPerAxis/2.0f, etc.
                float px_raw = tx * TotalSpanPerAxis;
                float py_raw = ty * TotalSpanPerAxis;
                float pz_raw = tz * TotalSpanPerAxis;

                // Adjust position to center the grid around BoxCenter
                float px = px_raw - (TotalSpanPerAxis / 2.0f) + GetActorLocation().X;
                float py = py_raw - (TotalSpanPerAxis / 2.0f) + GetActorLocation().Y;
                float pz = pz_raw - (TotalSpanPerAxis / 2.0f) + GetActorLocation().Z;

                FVector ParticleLocation(px, py, pz);

                // Jitter: Add random offset to each particle's position
                if (JitterFactor > KINDA_SMALL_NUMBER) // Check if jitter is significant
                {
                    ParticleLocation += FMath::VRand() * JitterFactor;
                }

                FActorSpawnParameters SpawnParams;
                SpawnParams.Owner = this;
                SpawnParams.Instigator = GetInstigator();
                //SpawnParams.SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn; // Recommended for particles

                // Ensure ParticleClass is set in the editor before playing.
                AParticle *NewParticle = GetWorld()->SpawnActor<AParticle>(ParticleClass, ParticleLocation, FRotator::ZeroRotator, SpawnParams);

                if (NewParticle)
                {
                    NewParticle->Radius = ParticleRadius; // Pass the desired visual radius
                    NewParticle->Velocity = FVector::ZeroVector; // Initialize velocity to zero
                    Particles.Add(NewParticle);
                    UE_LOG(LogTemp, Warning, TEXT("Spawning particle with radius: %f (Set from BoundingRectangularPrism's ParticleRadius: %f)"), NewParticle->Radius, ParticleRadius);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Failed to spawn particle at location: %s"), *ParticleLocation.ToString());
                }
            }
        }
    }
}

void ABoundingRectangularPrism::UpdateParticles(float DeltaTime)
{
	// Update particle velocities based on gravity
	for (AParticle *Particle : Particles)
	{
		FVector GravityForce = FVector::DownVector * Gravity * DeltaTime; // Apply gravity in the negative Z direction
		Particle->Velocity += GravityForce; // Update velocity
		Particle->UpdatePosition(DeltaTime); // Update particle position
		Particle->GenerateSphereMesh(); // Regenerate the mesh to reflect the new position
	}
}

void ABoundingRectangularPrism::DestroyAllParticles()
{
	// Find all Particles, even those not in the Particles array, to ensure we clean up everything
    TArray<AActor *> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AParticle::StaticClass(), FoundActors);

    for (AActor *Actor : FoundActors)
    {
        if (Actor)
        {
            Actor->Destroy(); // Mark the actor for destruction
        }
    }

	Particles.Empty(); // Clear the array of particles
}


