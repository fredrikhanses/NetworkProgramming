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
class AFGRocket;
class AFGPickup;

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
	UFUNCTION(BlueprintPure)
	bool IsBraking() const { return bBrake; }
	UFUNCTION(BlueprintPure)
	int32 GetPing() const;
	UFUNCTION(Server, Unreliable)
	void Server_SendLocation(const FVector& LocationToSend);
	UFUNCTION(Server, Reliable)
	void Server_OnPickup(AFGPickup* Pickup);
	UFUNCTION(Client, Reliable)
	void Client_OnPickupRockets(int32 PickedUpRockets);
	UFUNCTION(Server, Unreliable)
	void Server_SendYaw(float NewYaw);
	UFUNCTION(NetMulticast, Unreliable)
	void MultiCast_SendLocation(const FVector& LocationToSend);
	UFUNCTION(Server, Unreliable)
	void Server_SendRotation(const FQuat& RotationToSend);
	UFUNCTION(NetMulticast, Unreliable)
	void MultiCast_SendRotation(const FQuat& RotationToSend);
	UFUNCTION(BlueprintPure)
	int32 GetNumRockets() const { return NumberRockets; }
	UFUNCTION(BlueprintImplementableEvent, Category = Player, meta = (DisplayName = "On Number Rockets Changed"))
	void BP_OnNumberRocketsChanged(int32 NewNumberRockets);
	void OnPickup(AFGPickup* Pickup);
	void ShowDebugMenu();
	void HideDebugMenu();
	int32 GetNumberActiveRockets() const;
	void FireRocket();
	void SpawnRockets();
public:
	UPROPERTY(EditAnywhere, Category = Settings)
	UFGPlayerSettings* PlayerSettings = nullptr;
	UPROPERTY(EditAnywhere, Category = Debug)
	TSubclassOf<UFGNetDebugWidget> DebugMenuClass;
private:
	UFUNCTION(Server, Unreliable)
	void Server_SendMovement(const FVector& ClientLocation, float TimeStamp, float ClientForward, float ClientYaw);
	UFUNCTION(NetMulticast, Unreliable)
	void MultiCast_SendMovement(const FVector& InClientLocation, float TimeStamp, float ClientForward, float ClientYaw);
	UFUNCTION(Server, Reliable)
	void Server_FireRocket(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation);
	UFUNCTION(NetMulticast, Reliable)
	void MultiCast_FireRocket(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation);
	UFUNCTION(Client, Reliable)
	void Client_RemoveRocket(AFGRocket* RocketToRemove);
	UFUNCTION(BlueprintCallable)
	void Cheat_IncreaseRockets(int32 InNumberRockets);
	void Handle_Acceleration(float Value);
	void Handle_Turn(float Value);
	void Handle_BrakePressed();
	void Handle_BrakeReleased();
	void Handle_DebugMenuPressed();
	void Handle_FirePressed();
	void CreateDebugWidget();
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const;
	FVector GetRocketStartLocation() const;
	AFGRocket* GetFreeRocket() const;
	void AddMovementVelocity(float DeltaTime);
private:
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
	UPROPERTY(Replicated, Transient)
	TArray<AFGRocket*> RocketInstances;
	UPROPERTY(EditAnywhere, Category = Weapon)
	TSubclassOf<AFGRocket> RocketClass;
	UPROPERTY(EditAnywhere, Category = Weapon)
	bool bUnlimitedRockets = false;
	UPROPERTY(Transient)
	UFGNetDebugWidget* DebugMenuInstance = nullptr;
	UPROPERTY(Replicated)
	float ReplicatedYaw = 0.0f;
	UPROPERTY(Replicated)
	FVector ReplicatedLocation;
	UPROPERTY(EditAnywhere, Category = Network)
	bool bPerformNetworkSmoothing = true;
	FVector OriginalMeshOffset = FVector::ZeroVector;
	int32 MaxActiveRockets = 3;
	float FireCooldownElapsed = 0.0f;
	int32 ServerNumberRockets = 0;
	int32 NumberRockets = 0;
	bool bShowDebugMenu = false;
	float Forward = 0.0f;
	float Turn = 0.0f;
	float MovementVelocity = 0.0f;
	float Yaw = 0.0f;
	float CurrentDeltaTime = 0.0f;
	float NetMessageTimeCount = 0.0f;
	bool bBrake = false;
	float ClientTimeStamp = 0.0f;
	float LastCorrectionDelta = 0.0f;
	float ServerTimeStamp = 0.0f;
	FVector TargetLocation = FVector::ZeroVector;
	FQuat TargetRotation = FQuat::Identity;
};