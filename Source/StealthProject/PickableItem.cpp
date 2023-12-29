// Fill out your copyright notice in the Description page of Project Settings.


#include "PickableItem.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayTags.h"

// Sets default values
APickableItem::APickableItem()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->SetupAttachment(RootComponent);
}

void APickableItem::DisablePhysics()
{
	if (Mesh)
	{
		Mesh->SetSimulatePhysics(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void APickableItem::EnablePhysics()
{
	if (Mesh)
	{
		// Set simulate physics
		Mesh->SetSimulatePhysics(true);
		// Set collision behavior (optional, depending on your needs)
		Mesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		// Disable collisions with the specified channel
		Mesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	}
}

// Called when the game starts or when spawned
void APickableItem::BeginPlay()
{
	Super::BeginPlay();

	EnablePhysics();

	// Add tags to the container
	Tags.AddUnique(TEXT("Pickup"));
}

// Called every frame
void APickableItem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
