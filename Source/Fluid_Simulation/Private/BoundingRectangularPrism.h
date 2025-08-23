#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h" // Required for DrawDebugBox
#include "BoundingRectangularPrism.generated.h"

// Forward declaration of the AParticle class
class AParticle;

UCLASS()
class ABoundingRectangularPrism : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABoundingRectangularPrism();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called when an instance of this class is placed (in editor) or spawned.
	virtual void OnConstruction(const FTransform &Transform) override;

#if WITH_EDITOR
	// Called when properties are changed in the editor
	virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
#endif


public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	FVector BoxExtent; // Half size of the rectangular bounding prism along X, Y, Z

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	bool bDrawBoundingBox; // Whether to draw the debug bounding box visualization

	// Gravity affecting particle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	float Gravity;

	// Particle related factors

	// Number of particles to spawn per axis (x, y and z)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation", meta = (ClampMin = "1"))
	int ParticleCountPerAxis;

	// The radius of each particle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	float ParticleGridSpacing;

	// The jitter applied to the spacing of particles
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	float JitterFactor;

	// The radius of each particle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	float ParticleRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	float ParticleMass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	float TargetDensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	float PressureFactor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Simulation")
	float SmoothingRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounding Box", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Restitution; // Measure of the elasticity of a collision particles interacting with this box (0.0 = no bounce, 1.0 = perfect bounce)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Particle Properties")
	float MinSpeedForColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Particle Properties")
	float MaxSpeedForColor;

private:
	TSubclassOf<AParticle> ParticleClass;
	TArray<AParticle *> ManagedParticles; // Array to hold particle instances
	TArray<float> DensitiesAroundParticle; // Array to hold particle densities // TODO store this on the particles themselves??

	void DrawBoundingRectangularPrism(); // Function to generate the mesh (if needed, similar to AParticle)

	void SpawnParticles(); // Function to spawn particles within the bounding box

	void ResolveBoundingBoxCollisions(float DeltaTime); // Function to update particle velocities based on gravity and other forces

	void DestroyAllParticles(); // Function to destroy all particles in the level; this is to avoid having any leftover particles from previous runs

	/* Methods to calculate particle forces on each other */
	float CalculateDensity(const FVector &SamplePoint); // Function to calculate the density at a given position based on particle positions

	float DensityToPressure(float Density); // Function to convert density to pressure based on target density and pressure factor

	float CalculateSharedPressure(float Density1, float Density2); // Function to calculate shared pressure between two particles based on their densities

	FVector CalculatePressureForce(int ParticleIndex); // Function to calculate the pressure force on a particle based on its density and position relative to other particles
};
