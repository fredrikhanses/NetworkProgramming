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
#include "Kismet/GameplayStatics.h"

const static float MaxMoveDeltaTime = 0.125f;

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
	MovementComponent->SetUpdatedComponent(CollisionComponent);
	CreateDebugWidget();
	if (DebugMenuInstance != nullptr)
	{
		DebugMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
	}
	SpawnRockets();
	BP_OnNumberRocketsChanged(NumberRockets);
	BP_OnHealthChanged(Health);
	OriginalMeshOffset = MeshComponent->GetRelativeLocation();
}

void AFGPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	FireCooldownElapsed -= DeltaTime;
	if (!ensure(PlayerSettings != nullptr))
	{
		return;
	}
	FFGFrameMovement FrameMovement = MovementComponent->CreateFrameMovement();
	if (IsLocallyControlled())
	{
		ClientTimeStamp += DeltaTime;
		const float MaxVelocity = PlayerSettings->MaxVelocity;
		const float Friction = IsBraking() ? PlayerSettings->BrakingFriction : PlayerSettings->DefaultFriction;
		const float Alpha = FMath::Clamp(FMath::Abs(MovementVelocity / (PlayerSettings->MaxVelocity * 0.75f)), 0.0f, 1.0f);
		const float TurnSpeed = FMath::InterpEaseOut(0.0f, PlayerSettings->TurnSpeedDefault, Alpha, 5.0f);
		const float MovementDirection = MovementVelocity > 0.0f ? Turn : -Turn;
		Yaw += (MovementDirection * TurnSpeed) * DeltaTime;
		FQuat WantedFacingDirection = FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw));
		MovementComponent->SetFacingRotation(WantedFacingDirection, 10.5f);
		AddMovementVelocity(DeltaTime); 
		MovementVelocity *= FMath::Pow(Friction, DeltaTime);
		MovementComponent->ApplyGravity();
		FrameMovement.AddDelta(GetActorForwardVector() * MovementVelocity * DeltaTime);
		MovementComponent->Move(FrameMovement); 
		Server_SendMovement(GetActorLocation(), ClientTimeStamp, Forward, GetActorRotation().Yaw);
	}
	else
	{
		const float Friction = IsBraking() ? PlayerSettings->BrakingFriction : PlayerSettings->DefaultFriction;
		MovementVelocity *= FMath::Pow(Friction, DeltaTime);
		FrameMovement.AddDelta(GetActorForwardVector() * MovementVelocity * DeltaTime);
		MovementComponent->Move(FrameMovement);
		if (bPerformNetworkSmoothing)
		{
			const FVector NewRelativeLocation = FMath::VInterpTo(MeshComponent->GetRelativeLocation(), OriginalMeshOffset, LastCorrectionDelta, 0.75f);
			MeshComponent->SetRelativeLocation(NewRelativeLocation, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
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
	bBrake = true;
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

void AFGPlayer::Server_SendMovement_Implementation(const FVector& ClientLocation, float TimeStamp, float ClientForward, float ClientYaw)
{
	MultiCast_SendMovement(ClientLocation, TimeStamp, ClientForward, ClientYaw);
}

void AFGPlayer::MultiCast_SendMovement_Implementation(const FVector& InClientLocation, float TimeStamp, float ClientForward, float ClientYaw)
{
	if (!IsLocallyControlled())
	{
		Forward = ClientForward;
		const float DeltaTime = FMath::Min(TimeStamp - ClientTimeStamp, MaxMoveDeltaTime);
		ClientTimeStamp = TimeStamp;
		AddMovementVelocity(DeltaTime);
		MovementComponent->SetFacingRotation(FRotator(0.0f, ClientYaw, 0.0f));
		const FVector DeltaDifference = InClientLocation - GetActorLocation();
		if (DeltaDifference.SizeSquared() > FMath::Square(40.0f))
		{
			if (bPerformNetworkSmoothing)
			{
				const FScopedPreventAttachedComponentMove PreventMeshMove(MeshComponent);
				MovementComponent->UpdatedComponent->SetWorldLocation(InClientLocation, false, nullptr, ETeleportType::TeleportPhysics);
				LastCorrectionDelta = DeltaTime;
			}
			else
			{
				SetActorLocation(InClientLocation);
			}
		}
	}
}

void AFGPlayer::AddMovementVelocity(float DeltaTime)
{
	if (!ensure(PlayerSettings != nullptr))
	{
		return;
	}
	const float MaxVelocity = PlayerSettings->MaxVelocity;
	const float Acceleration = PlayerSettings->Acceleration;
	MovementVelocity += Forward * Acceleration * DeltaTime;
	MovementVelocity = FMath::Clamp(MovementVelocity, -MaxVelocity, MaxVelocity);
}

void AFGPlayer::Server_SendLocation_Implementation(const FVector& LocationToSend)
{
	ReplicatedLocation = LocationToSend;
}

void AFGPlayer::Server_SendYaw_Implementation(float NewYaw)
{
	ReplicatedYaw = NewYaw;
}

void AFGPlayer::OnPickup(AFGPickup* Pickup)
{
	if (HasAuthority())
	{
		Server_OnPickup(Pickup);
	}
	else if (IsLocallyControlled())
	{
		Pickup->SetVisibility(false);
	}
}

void AFGPlayer::OnHit(AFGRocket* Rocket)
{
	if (HasAuthority())
	{
		Server_OnHit(Rocket->Damage);
	}
}

void AFGPlayer::Die()
{
	Explode();
	BP_OnEnd(false);
	SetActorHiddenInGame(true);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorTickEnabled(false);
}

void AFGPlayer::Explode()
{
	if (Explosion != nullptr)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Explosion, GetActorLocation(), GetActorRotation(), true);
	}
}

void AFGPlayer::Server_OnPickup_Implementation(AFGPickup* Pickup)
{
	if (Pickup->PickupType == EFGPickupType::Rocket)
	{
		ServerNumberRockets += Pickup->NumberRockets;
		MultiCast_UpdateStat(Pickup, ServerNumberRockets);
		MultiCast_OnPickup(Pickup);
	}
	else if (Pickup->PickupType == EFGPickupType::Health)
	{
		ServerHealth += Pickup->HealthValue;
		MultiCast_UpdateStat(Pickup, ServerHealth);
		MultiCast_OnPickup(Pickup);
	}
}

void AFGPlayer::Server_OnHit_Implementation(uint32 DamageToSend)
{
	ServerHealth -= DamageToSend;
	MultiCast_OnHit(DamageToSend);
}

void AFGPlayer::MultiCast_OnPickup_Implementation(AFGPickup* Pickup)
{
	if (Pickup->PickupType == EFGPickupType::Rocket)
	{
		NumberRockets += Pickup->NumberRockets;
		Pickup->RestartPickup();
	}
	else if (Pickup->PickupType == EFGPickupType::Health)
	{
		Health += Pickup->HealthValue;
		Pickup->RestartPickup();
	}
}

void AFGPlayer::MultiCast_OnHit_Implementation(uint32 DamageToSend)
{
	Health -= DamageToSend;
	BP_OnHealthChanged(Health);
	if (Health <= 0)
	{
		Die();
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
			AFGRocket* NewRocketInstance = GetWorld()->SpawnActor<AFGRocket>(RocketClass, GetActorLocation(), GetActorRotation(), SpawnParameters);
			RocketInstances.Add(NewRocketInstance);
		}
	}
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
			BP_OnNumberRocketsChanged(NumberRockets);
		}
		else
		{
			NumberRockets--;
			NewRocket->StartMoving(GetActorForwardVector(), GetRocketStartLocation());
			Server_FireRocket(NewRocket, GetRocketStartLocation(), GetActorRotation());
			BP_OnNumberRocketsChanged(NumberRockets);
		}
	}
}

void AFGPlayer::Server_FireRocket_Implementation(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation)
{
	if ((ServerNumberRockets - 1) < 0 && !bUnlimitedRockets)
	{
		Client_RemoveRocket(NewRocket, ServerNumberRockets);
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

void AFGPlayer::MultiCast_UpdateStat_Implementation(AFGPickup* Pickup, uint32 InStat)
{
	if (Pickup->PickupType == EFGPickupType::Rocket)
	{
		BP_OnNumberRocketsChanged(InStat);
	}
	else if (Pickup->PickupType == EFGPickupType::Health)
	{
		BP_OnHealthChanged(InStat);
	}
}

void AFGPlayer::Client_RemoveRocket_Implementation(AFGRocket* RocketToRemove, uint32 InNumberRockets)
{
	RocketToRemove->MakeFree();
	NumberRockets = InNumberRockets;
}

FVector AFGPlayer::GetRocketStartLocation() const
{
	const FVector StartLocation = GetActorLocation() + GetActorForwardVector() * 100.0f;
	return StartLocation;
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

void AFGPlayer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFGPlayer, ReplicatedYaw);
	DOREPLIFETIME(AFGPlayer, ReplicatedLocation);
	DOREPLIFETIME(AFGPlayer, RocketInstances);
}

int32 AFGPlayer::GetPing() const
{
	if (GetPlayerState())
	{
		return static_cast<int32>(GetPlayerState()->GetPing());
	}
	return 0;
}

AFGPlayer::~AFGPlayer()
{
}