#pragma once

#include "GameFramework/Pawn.h"
#include "FGPlayer.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UFGMovementComponent;
class UStaticMeshComponent;
class USphereComponent;

UCLASS()
class NETWORKPROGRAMMING_API AFGPlayer : public APawn
{
	GENERATED_BODY()

public:

	AFGPlayer();

protected:

	virtual void BeginPlay();

public:

	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(EditAnywhere, Category = Movement)
	float Acceleration = 500.0f;

	UPROPERTY(EditAnywhere, Category = Movement, meta = (DisplayName = "TurnSpeed"))
	float TurnSpeedDefault = 100.0f;

	UPROPERTY(EditAnywhere, Category = Movement)
	float MaxVelocity = 2000.0f;

	UPROPERTY(EditAnywhere, Category = Movement, meta = (ClampMin = 0.0f, ClampMax = 1.0f))
	float DefaultFriction = 0.75f;

	UPROPERTY(EditAnywhere, Category = Movement, meta = (ClampMin = 0.0f, ClampMax = 1.0f))
	float BrakingFriction = 0.001f;

	UFUNCTION(BlueprintPure)
	bool IsBraking() const { return bBrake; }

	UFUNCTION(BlueprintPure)
	int32 GetPing() const;

	UFUNCTION(Server, Unreliable)
	void Server_SendLocation(const FVector& LocationToSend);

	UFUNCTION(NetMulticast, Unreliable)
	void MultiCast_SendLocation(const FVector& LocationToSend);

	UFUNCTION(Server, Unreliable)
	void Server_SendRotation(const FRotator& RotationToSend);

	UFUNCTION(NetMulticast, Unreliable)
	void MultiCast_SendRotation(const FRotator& RotationToSend);

private:
	
	void Handle_Acceleration(float Value);
	void Handle_Turn(float Value);
	void Handle_BrakePressed();
	void Handle_BrakeReleased();

	float Forward = 0.0f;
	float Turn = 0.0f;

	float MovementVelocity = 0.0f;
	float Yaw = 0.0f;

	bool bBrake = false;

	UPROPERTY(EditAnywhere, Category = Collision)
	USphereComponent* CollisionComponent;
	
	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMeshComponent* MeshComponent;
	
	UPROPERTY(EditAnywhere, Category = Camera)
	USpringArmComponent* SpringArmComponent;
	
	UPROPERTY(EditAnywhere, Category = Camera)
	UCameraComponent* CameraComponent;
	
	UPROPERTY(EditAnywhere, Category = Movement)
	UFGMovementComponent* MovementComponent;
};