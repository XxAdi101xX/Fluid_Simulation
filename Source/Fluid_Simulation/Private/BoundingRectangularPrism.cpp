#include "BoundingRectangularPrism.h"

#include "Particle.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
ABoundingRectangularPrism::ABoundingRectangularPrism()
{
 	// Set this actor to call Tick() every frame.
	PrimaryActorTick.bCanEverTick = true;

    // Set default values
    BoxExtent = FVector(100.0f, 200.0f, 200.0f);
    Gravity = 200.0f;
	ParticleCountPerAxis = 8; 
    ParticleGridSpacing = 30.0f;
    JitterFactor = 1.0f;
	ParticleRadius = 10.0f;
	ParticleMass = 1.0f; // Default mass for particles
	TargetDensity = 3.0f; // Default target density for water
	SmoothingRadius = 25.0f; // Default smoothing radius for SPH
	PressureFactor = 500.0f; // Default pressure factor for SPH
	Restitution = 0.8f;
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
    ResolveBoundingBoxCollisions(0.0f); // Update particles immediately after spawning
}

#if WITH_EDITOR
void ABoundingRectangularPrism::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
    if (PropertyName == GET_MEMBER_NAME_CHECKED(ABoundingRectangularPrism, ParticleCountPerAxis) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(ABoundingRectangularPrism, ParticleGridSpacing) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(ABoundingRectangularPrism, ParticleRadius) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(ABoundingRectangularPrism, Restitution))
    {
        // We want to visualize the bounding box and the particles before the game starts
        DrawBoundingRectangularPrism();

        // Clear any existing particles before spawning new ones
        DestroyAllParticles();
        SpawnParticles();
		ResolveBoundingBoxCollisions(0.0f); // Update particles immediately after spawning
    }
}
#endif


// Called every frame
void ABoundingRectangularPrism::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    
	// Draw the bounding box every frame, it will clear out otherwise
    DrawBoundingRectangularPrism();

    // Pre-calculate densities around each particle; they will be used by pressure calculations
    ParallelFor(ManagedParticles.Num(), [&](int32 Index)
        {
			AParticle *Particle = ManagedParticles[Index];

            DensitiesAroundParticle[Index] = CalculateDensity(Particle->Position);
        });


	// Apply gravity to the particle's velocity, alculate pressure forces and apply them to the particles' velocities
    ParallelFor(ManagedParticles.Num(), [&](int32 Index)
        {
            AParticle *Particle = ManagedParticles[Index];

			// Apply gravity to the particle's velocity
            Particle->Velocity += FVector::DownVector * Gravity * DeltaTime;

			// Calculate pressure force based on the density of the particle and its neighbors
			FVector PressureForce = CalculatePressureForce(Index);

			// F = m * a; but instead of mass, we use the density
			FVector PressureAcceleration = PressureForce / DensitiesAroundParticle[Index];

			// Update the particle's velocity based on the pressure acceleration
			Particle->Velocity += PressureAcceleration * DeltaTime;
        });

	// Also loops over particles to update their positions and handle collisions
    // TODO: use parallel for here too?
    ResolveBoundingBoxCollisions(DeltaTime);
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
    if (ParticleClass == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("ABoundingRectangularPrism: ParticleClass is not set. Cannot spawn particles."));
        return;
    }

    float TotalSpanPerAxis = (ParticleCountPerAxis > 1) ? (ParticleCountPerAxis - 1.0f) * ParticleGridSpacing : 0.0f;
    FVector HalfSpan = FVector(TotalSpanPerAxis, TotalSpanPerAxis, TotalSpanPerAxis) / 2.0f;
    FRandomStream RandomStream;
    RandomStream.Initialize(FMath::Rand());

    ManagedParticles.Empty();

    for (int x = 0; x < ParticleCountPerAxis; x++)
    {
        for (int y = 0; y < ParticleCountPerAxis; y++)
        {
            for (int z = 0; z < ParticleCountPerAxis; z++)
            {
                FVector RelativeGridPos;
                if (ParticleCountPerAxis > 1)
                {
                    RelativeGridPos.X = (float)x * ParticleGridSpacing;
                    RelativeGridPos.Y = (float)y * ParticleGridSpacing;
                    RelativeGridPos.Z = (float)z * ParticleGridSpacing;
                }
                else
                {
                    RelativeGridPos = FVector::ZeroVector;
                }

                FVector DesiredParticleWorldLocation = GetActorLocation() + (RelativeGridPos - HalfSpan);

                if (JitterFactor > KINDA_SMALL_NUMBER)
                {
                    DesiredParticleWorldLocation.X += RandomStream.GetFraction() * 2.0f * JitterFactor - JitterFactor;
                    DesiredParticleWorldLocation.Y += RandomStream.GetFraction() * 2.0f * JitterFactor - JitterFactor;
                    DesiredParticleWorldLocation.Z += RandomStream.GetFraction() * 2.0f * JitterFactor - JitterFactor;
                }

                FActorSpawnParameters SpawnParams;
                SpawnParams.Owner = this;
                SpawnParams.Instigator = GetInstigator();

                AParticle *NewParticle = GetWorld()->SpawnActor<AParticle>(ParticleClass, DesiredParticleWorldLocation, FRotator::ZeroRotator, SpawnParams);

                if (NewParticle)
                {
                    NewParticle->Radius = ParticleRadius;
                    NewParticle->Position = DesiredParticleWorldLocation;
                    NewParticle->Velocity = FVector::ZeroVector;
                    NewParticle->GenerateSphereMesh();
                    NewParticle->SetActorLocation(NewParticle->Position);

                    ManagedParticles.Add(NewParticle);
					DensitiesAroundParticle.Add(0.0f); // Initialize density for this particle
#if 0
                    UE_LOG(LogTemp, Log, TEXT("ABoundingRectangularPrism: Spawned Particle %s at World Pos: %s, Stored Pos: %s"),
                        *NewParticle->GetName(), *NewParticle->GetActorLocation().ToString(), *NewParticle->Position.ToString());
#endif
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("ABoundingRectangularPrism: Failed to spawn particle at location: %s"), *DesiredParticleWorldLocation.ToString());
                }
            }
        }
    }
    UE_LOG(LogTemp, Log, TEXT("ABoundingRectangularPrism: Spawned %d particles."), ManagedParticles.Num());
}

void ABoundingRectangularPrism::ResolveBoundingBoxCollisions(float DeltaTime)
{
    FVector MinBounds = GetActorLocation() - BoxExtent;
    FVector MaxBounds = GetActorLocation() + BoxExtent;

    for (AParticle *Particle : ManagedParticles)
    {
        // Store the particle's previous position for comparison
        FVector PreviousPosition = Particle->Position;

        // Update particle's position (which is now its WORLD position)
        Particle->Position += Particle->Velocity * DeltaTime;

        // Perform Bounding Box Collision
        bool bPositionOrVelocityChanged = false; 

        // Check X-axis collision
        if (Particle->Position.X - Particle->Radius < MinBounds.X)
        {
            Particle->Position.X = MinBounds.X + Particle->Radius;
            Particle->Velocity.X *= -Restitution;
            bPositionOrVelocityChanged = true;
        }
        else if (Particle->Position.X + Particle->Radius > MaxBounds.X)
        {
            Particle->Position.X = MaxBounds.X - Particle->Radius;
            Particle->Velocity.X *= -Restitution;
            bPositionOrVelocityChanged = true;
        }

        // Check Y-axis collision
        if (Particle->Position.Y - Particle->Radius < MinBounds.Y)
        {
            Particle->Position.Y = MinBounds.Y + Particle->Radius;
            Particle->Velocity.Y *= -Restitution;
            bPositionOrVelocityChanged = true;
        }
        else if (Particle->Position.Y + Particle->Radius > MaxBounds.Y)
        {
            Particle->Position.Y = MaxBounds.Y - Particle->Radius;
            Particle->Velocity.Y *= -Restitution;
            bPositionOrVelocityChanged = true;
        }

        // Check Z-axis collision (floor and ceiling)
        if (Particle->Position.Z - Particle->Radius < MinBounds.Z)
        {
            Particle->Position.Z = MinBounds.Z + Particle->Radius;
            Particle->Velocity.Z *= -Restitution;
            Particle->Velocity.X *= 0.9f;
            Particle->Velocity.Y *= 0.9f;
            bPositionOrVelocityChanged = true;
        }
        else if (Particle->Position.Z + Particle->Radius > MaxBounds.Z)
        {
            Particle->Position.Z = MaxBounds.Z - Particle->Radius;
            Particle->Velocity.Z *= -Restitution;
            bPositionOrVelocityChanged = true;
        }

        // Only update if necessary
        if (bPositionOrVelocityChanged || !Particle->Velocity.IsZero() || !PreviousPosition.Equals(Particle->Position, KINDA_SMALL_NUMBER))
        {
            Particle->SetActorLocation(Particle->Position);
        }
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

    ManagedParticles.Empty(); // Clear the array of particles
}

float SmoothingKernel(float Distance, float Radius)
{
	if (Distance >= Radius)
	{
		return 0.0f; // Outside the influence radius
	}

	float Volume = PI * pow(Radius, 4) / 6.0f;

    return (Radius - Distance) * (Radius - Distance) / Volume;
}

float SmoothingKernelDerivative(float Distance, float Radius)
{
	if (Distance >= Radius)
	{
		return 0.0f; // Outside the influence radius
	}

	float Scale = 12 / (PI * pow(Radius, 4));
	return (Distance - Radius) * Scale;
}

float ABoundingRectangularPrism::CalculateDensity(const FVector &SamplePoint)
{
    float Density = 0.0f;
    const float Mass = 1.0f;

    // TODO: optimize to only look at particles within smoothing radius
    for (const AParticle *Particle : ManagedParticles)
    {
        float Distance = (Particle->Position - SamplePoint).Size();
        float Influence = SmoothingKernel(Distance, SmoothingRadius);
        Density += Mass * Influence;
    }
    return Density;
}

float ABoundingRectangularPrism::DensityToPressure(float Density)
{
	// Calculate pressure based on the difference from target density
	// This equation is better applicable for gasses but we will use it for liquids as well
    float DensityDifference = (TargetDensity - Density);
	float Pressure = PressureFactor * DensityDifference;
	return Pressure;
}

float ABoundingRectangularPrism::CalculateSharedPressure(float Density1, float Density2)
{
	// Calculate shared pressure between two particles based on their densities
	float Pressure1 = DensityToPressure(Density1);
	float Pressure2 = DensityToPressure(Density2);
	return (Pressure1 + Pressure2) / 2.0f; // Average pressure
}

FVector ABoundingRectangularPrism::CalculatePressureForce(int ParticleIndex)
{
    FVector PressureForce = FVector::ZeroVector;
    for (int CurrentParticleIndex = 0; CurrentParticleIndex < ManagedParticles.Num(); ++CurrentParticleIndex)
    {
		if (CurrentParticleIndex == ParticleIndex)
		{
			continue; // Skip the particle itself
		}
        const AParticle *CurrentParticle = ManagedParticles[CurrentParticleIndex];

		FVector OffsetBetweenParticles = (CurrentParticle->Position - ManagedParticles[ParticleIndex]->Position);
		float Distance = OffsetBetweenParticles.Size();
        FVector Direction = Distance == 0 ? FMath::VRand() : OffsetBetweenParticles / Distance;
        float Slope = SmoothingKernelDerivative(Distance, SmoothingRadius);
        float Density = DensitiesAroundParticle[CurrentParticleIndex];
		float SharedPressure = CalculateSharedPressure(Density, DensitiesAroundParticle[ParticleIndex]);
        PressureForce += SharedPressure * Slope *Direction *ParticleMass / Density;
    }
    return PressureForce;
}


