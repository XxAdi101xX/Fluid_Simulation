// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Particle.generated.h"

class UProceduralMeshComponent;

UCLASS()
class AParticle : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AParticle();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called when an instance of this class is placed (in editor) or spawned.
	virtual void OnConstruction(const FTransform &Transform);

	// Called when properties are changed in the editor
	virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, Category = "Procedural Mesh")
	UProceduralMeshComponent* ProceduralMeshComponent;

	// Radius of the Procedural Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Mesh")
	float Radius;

	// Number of segments along the height (latitude) of the sphere
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Mesh", meta = (ClampMin = "2", ClampMax = "128"))
	int32 NumLatitudeSegments;

	// Number of segments around the circumference (longitude) of the sphere
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Mesh", meta = (ClampMin = "3", ClampMax = "128"))
	int32 NumLongitudeSegments;

	// Color of the particle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Mesh")
	FLinearColor Color;

	// Velocity of the particle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Mesh")
	FVector Velocity;

	void GenerateSphereMesh();

	void UpdatePosition(float DeltaTime);
private:
	FVector Position;
};
