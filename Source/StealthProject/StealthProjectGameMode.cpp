// Copyright Epic Games, Inc. All Rights Reserved.

#include "StealthProjectGameMode.h"
#include "StealthProjectCharacter.h"
#include "UObject/ConstructorHelpers.h"

AStealthProjectGameMode::AStealthProjectGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
