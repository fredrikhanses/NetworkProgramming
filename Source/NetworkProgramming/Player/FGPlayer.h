#pragma once

#include "GameFramework/Pawn.h"
#include "FGPlayer.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UFGMovementComponent;
class UStaticMeshComponent;
class USphereComponent;
class UFGPlayerSettings;
class UFGNetDebugWidget;

UCLASS()
class NETWORKPROGRAMMING_API AFGPlayer : public APawn
{
	GENERATED_BODY()

public:

	AFGPlayer();
	~AFGPlayer();

protected:

	virtual void BeginPlay();

public:

	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(EditAnywhere, Category = Settings)
	UFGPlayerSettings* PlayerSettings = nullptr;

	UFUNCTION(BlueprintPure)
	bool IsBraking() const { return bBrake; }

	UFUNCTION(BlueprintPure)
	int32 GetPing() const;

	UPROPERTY(EditAnywhere, Category = Debug)
	TSubclassOf<UFGNetDebugWidget> DebugMenuClass;

	UFUNCTION(Server, Unreliable)
	void Server_SendLocation(const FVector& LocationToSend);

	UFUNCTION(NetMulticast, Unreliable)
	void MultiCast_SendLocation(const FVector& LocationToSend);

	UFUNCTION(Server, Unreliable)
	void Server_SendRotation(const FQuat& RotationToSend);

	UFUNCTION(NetMulticast, Unreliable)
	void MultiCast_SendRotation(const FQuat& RotationToSend);

	void ShowDebugMenu();
	void HideDebugMenu();

private:
	
	void Handle_Acceleration(float Value);
	void Handle_Turn(float Value);
	void Handle_BrakePressed();
	void Handle_BrakeReleased();

	void Handle_DebugMenuPressed();

	void CreateDebugWidget();

	UPROPERTY(Transient)
	UFGNetDebugWidget* DebugMenuInstance = nullptr;

	bool bShowDebugMenu = false;

	float Forward = 0.0f;
	float Turn = 0.0f;

	float MovementVelocity = 0.0f;
	float Yaw = 0.0f;

	float CurrentDeltaTime = 0.0f;
	float NetMessageTimeCount = 0.0f;

	bool bBrake = false;

	FVector TargetLocation = FVector::ZeroVector;
	FQuat TargetRotation = FQuat::Identity;

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