// Copyright Epic Games, Inc. All Rights Reserved.

#include "StealthProjectCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "PickableItem.h"
#include "Engine/SkeletalMeshSocket.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AStealthProjectCharacter

AStealthProjectCharacter::AStealthProjectCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 230.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm
	CrouchedCapsuleExpand = 1.3f;

	PickAnimationDelay = 0.1;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AStealthProjectCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	// Get initial radius
	fCapsuleInitialRadius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void AStealthProjectCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		
		// Crouching
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AStealthProjectCharacter::TriggerCrouch);

		// Pickup item
		EnhancedInputComponent->BindAction(PickupAction, ETriggerEvent::Completed, this, &AStealthProjectCharacter::Pickup);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AStealthProjectCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AStealthProjectCharacter::Look);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AStealthProjectCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AStealthProjectCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AStealthProjectCharacter::FaceLocation(const FVector& TargetCoordinate)
{
// Calculate the direction vector from the character's location to the target location
        FVector DirectionToTarget = TargetCoordinate - GetActorLocation();
        DirectionToTarget.Z = 0; // Ignore Z component if you want to keep the character level

        // Calculate the rotation needed to face the target location
        FRotator TargetRotation = DirectionToTarget.Rotation();

        // Set the actor's rotation
        SetActorRotation(TargetRotation);
}

void AStealthProjectCharacter::Pickup()
{
	if (HeldItem)
	{
		DropItem();
	} else {
		TSet<AActor*> OverlappingActors;
		GetOverlappingActors(OverlappingActors);

		for (AActor* OverlappingActor : OverlappingActors)
		{
			UE_LOG(LogTemp, Warning, TEXT("Found actor %s"), *OverlappingActor->GetActorNameOrLabel());
			if (OverlappingActor->ActorHasTag(TEXT("Pickup")))
			{
				FaceLocation(OverlappingActor->GetActorLocation());
				PickItem(OverlappingActor);
				break;
			}
		}
	}
}

void AStealthProjectCharacter::PickItem(AActor* Item)
{
	APickableItem* item = Cast<APickableItem>(Item);
	if (item)
	{
		item->DisablePhysics();
	}
	this->HeldItem = Item;
	// Set up the timer to call HoldItem after a specific time (e.g., 1 second)
    GetWorldTimerManager().SetTimer(AttachTimerHandle, this, &AStealthProjectCharacter::HoldItem, PickAnimationDelay, false);
}

void AStealthProjectCharacter::HoldItem()
{
	if (HeldItem == nullptr) 
	{
		UE_LOG(LogTemp, Error, TEXT("Held item is empty"));
		return;
	}
	FAttachmentTransformRules rules = FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true);
	bool result = HeldItem->AttachToComponent(GetMesh(), rules, TEXT("WeaponSocket"));
	if (result) UE_LOG(LogTemp, Warning, TEXT("Got weapon")) else UE_LOG(LogTemp, Error, TEXT("Unable to hold"));
}

void AStealthProjectCharacter::DropItem()
{
	if (HeldItem == nullptr) return;
	HeldItem->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	APickableItem* item = Cast<APickableItem>(HeldItem);
	if (item) item->EnablePhysics();
	HeldItem = nullptr;
}

void AStealthProjectCharacter::TriggerCrouch()
{
	UCharacterMovementComponent* charMovement = GetCharacterMovement();
	if (charMovement)
	{
		charMovement->bWantsToCrouch = !charMovement->bWantsToCrouch;
	}
}

void AStealthProjectCharacter::SetCapsuleProperties(float Radius = -1.f, float HalfHeight = -1.f)
{
	UCapsuleComponent* capsule = GetCapsuleComponent();
	if (capsule)
	{
		if (Radius >= 0) capsule->SetCapsuleRadius(Radius);
		if (HalfHeight >= 0) capsule->SetCapsuleHalfHeight(HalfHeight);
	}
}

void AStealthProjectCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	SetCapsuleProperties(fCapsuleInitialRadius);
}

void AStealthProjectCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	SetCapsuleProperties(fCapsuleInitialRadius * CrouchedCapsuleExpand);
}

bool AStealthProjectCharacter::IsHoldingItem() const
{
	return HeldItem != nullptr;
}