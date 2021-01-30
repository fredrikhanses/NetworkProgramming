#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FGRocket.generated.h"

class UStaticMeshComponent;

UCLASS()
class NETWORKPROGRAMMING_API AFGRocket : public AActor
{
	GENERATED_BODY()
public:	
	AFGRocket();
protected:
	virtual void BeginPlay() override;
public:	
	virtual void Tick(float DeltaTime) override;
	void StartMoving(const FVector& Forward, const FVector& InStartLocation);
	void ApplyCorrection(const FVector& Forward);
	bool IsFree() const { return bIsFree; }
	void Explode();
	void MakeFree();
private:
	void SetRocketVisibility(bool bVisible);
	FCollisionQueryParams CachedCollisionQueryParams;
	UPROPERTY(EditAnywhere, Category = VFX)
	UParticleSystem* Explosion = nullptr;
	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMeshComponent* MeshComponent = nullptr;
	UPROPERTY(EditAnywhere, Category = Debug)
	bool bDebugDrawCorrection = false;
	FVector OrigianlFacingDirection = FVector::ZeroVector;
	FVector FacingRotationStart = FVector::ZeroVector;
	FQuat FacingRotationCorrection = FQuat::Identity;
	FVector RocketStartLocation = FVector::ZeroVector;
	float LifeTime = 2.0f;
	float LifeTimeElapsed = 0.0f;
	float DistanceMoved = 0.0f;
	UPROPERTY(EditAnywhere, Category = VFX)
	float MovementVelocity = 1300.0f;
	bool bIsFree = true;
};
