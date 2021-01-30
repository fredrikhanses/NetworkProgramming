#include "FGPlayer.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SphereComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/NetDriver.h"
#include "GameFramework/PlayerState.h"
#include "../Components/FGMovementComponent.h"
#include "../FGMovementStatics.h"
#include "Net/UnrealNetwork.h"
#include "FGPlayerSettings.h"
#include "../Debug/UI/FGNetDebugWidget.h"
#include "../FGPickup.h"
#include "../FGRocket.h"

AFGPlayer::AFGPlayer()
{
	PrimaryActorTick.bCanEverTick = true;
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	RootComponent = CollisionComponent;
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComponent->bInheritYaw = false;
	SpringArmComponent->SetupAttachment(CollisionComponent);
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SpringArmComponent);
	MovementComponent = CreateDefaultSubobject<UFGMovementComponent>(TEXT("MovementComponent"));
	SetReplicateMovement(false);
}

void AFGPlayer::BeginPlay()
{
	Super::BeginPlay();
	TargetLocation = GetActorLocation();
	TargetRotation = GetActorRotation().Quaternion();
	MovementComponent->SetUpdatedComponent(CollisionComponent);
	CreateDebugWidget();
	if (DebugMenuInstance != nullptr)
	{
		DebugMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
	}
	SpawnRockets();
}

void AFGPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	FireCooldownElapsed -= DeltaTime;
	if (!ensure(PlayerSettings != nullptr))
	{
		return;
	}
	if (IsLocallyControlled())
	{
		const float MaxVelocity = PlayerSettings->MaxVelocity;
		const float Acceleration = PlayerSettings->Acceleration;
		const float Friction = IsBraking() ? PlayerSettings->BrakingFriction : PlayerSettings->DefaultFriction;
		const float Alpha = FMath::Clamp(FMath::Abs(MovementVelocity / (PlayerSettings->MaxVelocity * 0.75f)), 0.0f, 1.0f);
		const float TurnSpeed = FMath::InterpEaseInOut(0.0f, PlayerSettings->TurnSpeedDefault, Alpha, 5.0f);
		const float MovementDirection = MovementVelocity > 0.0f ? Turn : -Turn;
		Yaw += (MovementDirection * TurnSpeed) * DeltaTime;
		FQuat WantedFacingDirection = FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw));
		MovementComponent->SetFacingRotation(WantedFacingDirection, 10.5f);
		FFGFrameMovement FrameMovement = MovementComponent->CreateFrameMovement();
		MovementVelocity += Forward * Acceleration * DeltaTime;
		MovementVelocity = FMath::Clamp(MovementVelocity, -MaxVelocity, MaxVelocity);
		MovementVelocity *= FMath::Pow(Friction, DeltaTime);
		MovementComponent->ApplyGravity();
		FrameMovement.AddDelta(GetActorForwardVector() * MovementVelocity * DeltaTime);
		MovementComponent->Move(FrameMovement);
		Server_SendLocation(GetActorLocation());
		Server_SendYaw(MovementComponent->GetFacingRotation().Yaw);
	}
	else
	{
		const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), ReplicatedLocation, DeltaTime, 10.0f);
		SetActorLocation(NewLocation);
		MovementComponent->SetFacingRotation(FRotator(0.0f, ReplicatedYaw, 0.0f));
		SetActorRotation(MovementComponent->GetFacingRotation());
	}
	/*const float MaxVelocity = PlayerSettings->MaxVelocity;
	const float Acceleration = PlayerSettings->Acceleration;
	const float Friction = IsBraking() ? PlayerSettings->BrakingFriction : PlayerSettings->DefaultFriction;
	const float Alpha = FMath::Clamp(FMath::Abs(MovementVelocity / (PlayerSettings->MaxVelocity * 0.75f)), 0.0f, 1.0f);
	const float TurnSpeed = FMath::InterpEaseInOut(0.0f, PlayerSettings->TurnSpeedDefault, Alpha, 5.0f);
	const float MovementDirection = MovementVelocity > 0.0f ? Turn : -Turn;
	Yaw += (MovementDirection * TurnSpeed) * DeltaTime;
	FQuat WantedFacingDirection = FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw));
	MovementComponent->SetFacingRotation(WantedFacingDirection, 10.5f);
	if (IsLocallyControlled())
	{
		MovementComponent->SetFacingRotation(WantedFacingDirection);
	}
	else
	{
		SetActorLocation(FMath::Lerp(GetActorLocation(), TargetLocation, DeltaTime * 5.0f));
		MovementComponent->SetFacingRotation(FMath::Lerp(GetActorRotation().Quaternion(), TargetRotation, DeltaTime * 5.0f));
	}
	FFGFrameMovement FrameMovement = MovementComponent->CreateFrameMovement();
	MovementVelocity += Forward * Acceleration * DeltaTime;
	MovementVelocity = FMath::Clamp(MovementVelocity, -MaxVelocity, MaxVelocity);
	MovementVelocity *= FMath::Pow(Friction, DeltaTime);
	MovementComponent->ApplyGravity();
	FrameMovement.AddDelta(GetActorForwardVector() * MovementVelocity * DeltaTime);
	MovementComponent->Move(FrameMovement);
	if (IsLocallyControlled())
	{
		Server_SendLocation(GetActorLocation());
		Server_SendRotation(GetActorRotation().Quaternion());
	}*/
}

void AFGPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindAxis(TEXT("Accelerate"), this, &AFGPlayer::Handle_Acceleration);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AFGPlayer::Handle_Turn);
	PlayerInputComponent->BindAction(TEXT("Brake"), IE_Pressed, this, &AFGPlayer::Handle_BrakePressed);
	PlayerInputComponent->BindAction(TEXT("Brake"), IE_Released, this, &AFGPlayer::Handle_BrakeReleased);
	PlayerInputComponent->BindAction(TEXT("DebugMenu"), IE_Pressed, this, &AFGPlayer::Handle_DebugMenuPressed);
	PlayerInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &AFGPlayer::Handle_FirePressed);
}

int32 AFGPlayer::GetPing() const
{
	if (GetPlayerState())
	{
		return static_cast<int32>(GetPlayerState()->GetPing());
	}
	return 0;
}

void AFGPlayer::OnPickup(AFGPickup* Pickup)
{
	if (IsLocallyControlled())
	{
		Server_OnPickup(Pickup);
	}
}

void AFGPlayer::Server_OnPickup_Implementation(AFGPickup* Pickup)
{
	ServerNumberRockets += Pickup->NumberRockets;
	Client_OnPickupRockets(Pickup->NumberRockets);
}

void AFGPlayer::Client_OnPickupRockets_Implementation(int32 PickedUpRockets)
{
	NumberRockets += PickedUpRockets;
	BP_OnNumberRocketsChanged(NumberRockets);
}

void AFGPlayer::Server_SendLocation_Implementation(const FVector& LocationToSend)
{
	//MultiCast_SendLocation(LocationToSend);
	ReplicatedLocation = LocationToSend;
}

void AFGPlayer::Server_SendYaw_Implementation(float NewYaw)
{
	ReplicatedYaw = NewYaw;
}

void AFGPlayer::MultiCast_SendLocation_Implementation(const FVector& LocationToSend)
{
	if (!IsLocallyControlled())
	{
		//TargetLocation = LocationToSend;
		SetActorLocation(LocationToSend);
	}
}

void AFGPlayer::Server_SendRotation_Implementation(const FQuat& RotationToSend)
{
	MultiCast_SendRotation(RotationToSend);
}

void AFGPlayer::MultiCast_SendRotation_Implementation(const FQuat& RotationToSend)
{
	if (!IsLocallyControlled())
	{
		TargetRotation = RotationToSend;
	}
}

void AFGPlayer::ShowDebugMenu()
{
	CreateDebugWidget();
	if (DebugMenuInstance == nullptr)
	{
		return;
	}
	DebugMenuInstance->SetVisibility(ESlateVisibility::Visible);
	DebugMenuInstance->BP_OnShowWdiget();
}

void AFGPlayer::HideDebugMenu()
{
	if (DebugMenuInstance == nullptr)
	{
		return;
	}
	DebugMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
	DebugMenuInstance->BP_OnHideWdiget();
}

int32 AFGPlayer::GetNumberActiveRockets() const
{
	int32 NumberActive = 0;
	for (AFGRocket* Rocket : RocketInstances)
	{
		if (!Rocket->IsFree())
		{
			NumberActive++;
		}
	}
	return NumberActive;
}

void AFGPlayer::FireRocket()
{
	if (FireCooldownElapsed > 0.0f)
	{
		return;
	}
	if (NumberRockets <= 0 && !bUnlimitedRockets)
	{
		return;
	}
	if (GetNumberActiveRockets() >= MaxActiveRockets)
	{
		return;
	}
	AFGRocket* NewRocket = GetFreeRocket();
	if (!ensure(NewRocket != nullptr))
	{
		return;
	}
	FireCooldownElapsed = PlayerSettings->FireCooldown;
	if (GetLocalRole() >= ROLE_AutonomousProxy)
	{
		if (HasAuthority())
		{
			Server_FireRocket(NewRocket, GetRocketStartLocation(), GetActorRotation());
		}
		else
		{
			NumberRockets--;
			NewRocket->StartMoving(GetActorForwardVector(), GetRocketStartLocation());
			Server_FireRocket(NewRocket, GetRocketStartLocation(), GetActorRotation());
		}
	}
}

void AFGPlayer::SpawnRockets()
{
	if (HasAuthority() && RocketClass != nullptr)
	{
		const int32 RocketCache = 8;
		for (int32 Index = 0; Index < RocketCache; ++Index)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			SpawnParameters.ObjectFlags = RF_Transient;
			SpawnParameters.Instigator = this;
			SpawnParameters.Owner = this;
			AFGRocket * NewRocketInstance = GetWorld()->SpawnActor<AFGRocket>(RocketClass, GetActorLocation(), GetActorRotation(), SpawnParameters);
			RocketInstances.Add(NewRocketInstance);
		}
	}
}

FVector AFGPlayer::GetRocketStartLocation() const
{
	const FVector StartLocation = GetActorLocation() + GetActorForwardVector() * 100.0f;
	return StartLocation;
}

AFGRocket* AFGPlayer::GetFreeRocket() const
{
	for (AFGRocket* Rocket : RocketInstances)
	{
		if (Rocket == nullptr)
		{
			continue;
		}
		if (Rocket->IsFree())
		{
			return Rocket;
		}
	}
	return nullptr;
}

void AFGPlayer::Cheat_IncreaseRockets(int32 InNumberRockets)
{
	if(IsLocallyControlled())
	{
		NumberRockets += InNumberRockets;
	}
}

void AFGPlayer::Server_SendMovement_Implementation(const FVector& ClientLocation, float TimeStamp, float ClientForward, float ClientYaw)
{

}

void AFGPlayer::MultiCast_SendMovement_Implementation(const FVector& InClientLocation, float TimeStamp, float ClientForward, float ClientYaw)
{

}

void AFGPlayer::Server_FireRocket_Implementation(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation)
{
	if ((ServerNumberRockets - 1) < 0 && !bUnlimitedRockets)
	{
		Client_RemoveRocket(NewRocket);
	}
	else
	{
		const float DeltaYaw = FMath::FindDeltaAngleDegrees(RocketFacingRotation.Yaw, GetActorForwardVector().Rotation().Yaw);
		const FRotator NewFacingRotation = RocketFacingRotation + FRotator(0.0f, DeltaYaw, 0.0f);
		ServerNumberRockets--;
		MultiCast_FireRocket(NewRocket, RocketStartLocation, NewFacingRotation);
	}
}

void AFGPlayer::MultiCast_FireRocket_Implementation(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation)
{
	if (!ensure(NewRocket != nullptr))
	{
		return;
	}
	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		NewRocket->ApplyCorrection(RocketFacingRotation.Vector());
	}
	else
	{
		NumberRockets--;
		NewRocket->StartMoving(RocketFacingRotation.Vector(), RocketStartLocation);
	}
	BP_OnNumberRocketsChanged(NumberRockets);
}

void AFGPlayer::Client_RemoveRocket_Implementation(AFGRocket* RocketToRemove)
{
	RocketToRemove->MakeFree();
}

void AFGPlayer::Handle_Acceleration(float Value)
{
	Forward = Value;
}

void AFGPlayer::Handle_Turn(float Value)
{
	Turn = Value;
}

void AFGPlayer::Handle_BrakePressed()
{
	bBrake = true ;
}

void AFGPlayer::Handle_BrakeReleased()
{
	bBrake = false;
}

void AFGPlayer::Handle_DebugMenuPressed()
{
	bShowDebugMenu = !bShowDebugMenu;
	if (bShowDebugMenu)
	{
		ShowDebugMenu();
	}
	else
	{
		HideDebugMenu();
	}
}

void AFGPlayer::Handle_FirePressed()
{
	FireRocket();
}

void AFGPlayer::CreateDebugWidget()
{
	if (DebugMenuClass == nullptr)
	{
		return;
	}
	if (!IsLocallyControlled())
	{
		return;
	}
	if (DebugMenuInstance == nullptr)
	{
		DebugMenuInstance = CreateWidget<UFGNetDebugWidget>(GetWorld(), DebugMenuClass);
		DebugMenuInstance->AddToViewport();
	}
}

void AFGPlayer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFGPlayer, ReplicatedYaw);
	DOREPLIFETIME(AFGPlayer, ReplicatedLocation);
	DOREPLIFETIME(AFGPlayer, RocketInstances);
}

AFGPlayer::~AFGPlayer()
{
}