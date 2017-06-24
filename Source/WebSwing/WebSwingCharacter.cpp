// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WebSwingCharacter.h"
#include "Kismet/HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"

//////////////////////////////////////////////////////////////////////////
// AWebSwingCharacter

AWebSwingCharacter::AWebSwingCharacter()
{
	PrimaryActorTick.bCanEverTick = true; //tutorial
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	WebStart = CreateDefaultSubobject<USceneComponent>(TEXT("WebStart"));
	WebStartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WebStartMesh"));
	WebStartMesh->SetupAttachment(WebStart);
	CanShoot = true;
	ControllerYaw = 0.f;
	IsWebbed = false;
	JumpCameraLength = 600;
	WebEndPoint = CreateDefaultSubobject<USceneComponent>(TEXT("WebEndPoint"));
	WebParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("WebParticles"));
	SwingReleaseBoost = 400.f;
	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
}



//////////////////////////////////////////////////////////////////////////
// Input

void AWebSwingCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAction("Shoot", IE_Pressed, this, &AWebSwingCharacter::OnShoot); //tutorial
	PlayerInputComponent->BindAction("Shoot", IE_Released, this, &AWebSwingCharacter::OnRelease); //tutorial

	PlayerInputComponent->BindAxis("MoveForward", this, &AWebSwingCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AWebSwingCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AWebSwingCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AWebSwingCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AWebSwingCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AWebSwingCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AWebSwingCharacter::OnResetVR);
}


void AWebSwingCharacter::BeginPlay() {
	Super::BeginPlay();
	thisBall = GetWorld()->SpawnActor<ATheConstrainedBall>(TestBall, AWebSwingCharacter::GetActorLocation(), FRotator(0.f));
	//thisBall->MyCollider->SetMassScale(NAME_None, 0);
	//CreateNewPhysicsConstraintBetween(SystemActor->StableMesh, SystemActor->ConstrainedMesh);

	
	//thisSystem = GetWorld()->SpawnActor<ATestConstraintSystem>(FVector(0,0,1000), FRotator(0.f));
																							//thisSystem->RootComponent->SetWorldLocation(hit.Location);

	//thisSystem->TheConstraint->SetConstrainedComponents(thisSystem->StableMesh, NAME_None, GetMesh(), NAME_None);

}

//tutorial
void AWebSwingCharacter::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
	FVector2D myforward2d = FVector2D(AWebSwingCharacter::GetActorForwardVector().X, AWebSwingCharacter::GetActorForwardVector().Y).GetSafeNormal();
	FVector2D myworldvelocity2d = FVector2D(ACharacter::GetVelocity().X, ACharacter::GetVelocity().Y).GetSafeNormal();
	float dotforward = FVector2D::DotProduct(myforward2d, myworldvelocity2d);
	if (dotforward >= .7f) {
		//GEngine->AddOnScreenDebugMessage(-1, 100, FColor::Cyan, "MovingForward");
	}
	else {
		//GEngine->AddOnScreenDebugMessage(-1, 100, FColor::Cyan, "NotMovingForward");
	}
	//GEngine->AddOnScreenDebugMessage(-1, 100, FColor::Cyan, ACharacter::GetVelocity().GetSafeNormal().ToString());
	/*****************          Jump Camera        *******************/
	if (GetCharacterMovement()->IsFalling() == true) {
		float ArmLength = FMath::Lerp(GetCameraBoom()->TargetArmLength, JumpCameraLength, 4 * DeltaTime);
		GetCameraBoom()->TargetArmLength = ArmLength;
	}
	else {
		float ArmLength = FMath::Lerp(GetCameraBoom()->TargetArmLength, 300.f, 4 * DeltaTime);
		GetCameraBoom()->TargetArmLength = ArmLength;
	}
	/********************************************************************************/
	if (IsWebbed == true) {
		AWebSwingCharacter::SetActorLocation(thisBall->GetActorLocation());
		LerpingRotation = false;
		//DrawDebugLine(GetWorld(), hit.Location, thisSystem->StableMesh->GetForwardVector() * 6, FColor::Red, false, .1f);
		WebEndPoint->SetWorldLocation(hit.Location);
		WebParticles->SetBeamSourcePoint(0, WebStart->GetComponentLocation(), 0);
		if (ACharacter::GetVelocity().Z < 0) {
			//thisBall->MyCollider->AddForce(thisBall->GetVelocity() * SwingReleaseBoost);
		}
		WebParticles->SetBeamTargetPoint(0, WebEndPoint->GetComponentLocation(), 0);
		
		//ATestConstraintSystem ConstraintSystem =  GetWorld()->SpawnActor(SystemActor);
	}
	if (IsWebbed == false && thisBall->GetVelocity().Z == 0){ //&& ACharacter::GetCharacterMovement()->IsFalling() == false) {
		thisBall->SetActorLocation(AWebSwingCharacter::GetActorLocation());

	}
	else {
		AWebSwingCharacter::SetActorLocation(thisBall->GetActorLocation());
	}
	if (ACharacter::GetCharacterMovement()->IsFalling() == false) {
		thisBall->MyCollider->SetSimulatePhysics(false);
		thisBall->SetActorLocation(AWebSwingCharacter::GetActorLocation());
	}
	else {
		GEngine->AddOnScreenDebugMessage(1, 1, FColor::Red, "FALLING");
	}
	/**/
	if (LerpingRotation == true) {
		FRotator wantedrotation = FRotator(0, ControllerYaw, 0);
		if (AWebSwingCharacter::GetActorRotation().Equals(wantedrotation, 8.f) == false) {
			FRotator C = FMath::Lerp(AWebSwingCharacter::GetActorRotation(), wantedrotation, 6 * DeltaTime);
			AWebSwingCharacter::SetActorRotation(C);
		}
		else {
			AWebSwingCharacter::SetActorRotation(wantedrotation);
			//GEngine->AddOnScreenDebugMessage(-1, 5, FColor::Blue, "Straight");
			LerpingRotation = false;
			//GetCapsuleComponent()->SetSimulatePhysics(false);
			
			
		}
	}

	//DrawDebugLine(GetWorld(), AWebSwingCharacter::GetActorLocation(), AWebSwingCharacter::GetActorLocation() + AWebSwingCharacter::GetActorUpVector() * -110, FColor::Blue);
	FCollisionQueryParams MyParams(FName(TEXT("GroundTrace")), false, this);
	MyParams.AddIgnoredActor(thisBall);
	if ((ACharacter::GetVelocity().Z <= 0 || thisBall->GetVelocity().Z <= 0) && ACharacter::GetCharacterMovement()->IsMovingOnGround() == false && (GetWorld()->LineTraceSingleByChannel(GroundTrace, AWebSwingCharacter::GetActorLocation(), AWebSwingCharacter::GetActorLocation()+ AWebSwingCharacter::GetActorUpVector() * -110, ECC_Visibility, MyParams)))
	{
		GEngine->AddOnScreenDebugMessage(1, 1, FColor::Red, "Touching");
		thisBall->MyCollider->SetSimulatePhysics(false);

		//thisBall->MyCollider->SetMassScale(NAME_None, 0);
		thisBall->SetActorLocation(AWebSwingCharacter::GetActorLocation());
	}


	if (IsWebbed == false && GetCapsuleComponent()->IsSimulatingPhysics() == true && LerpingRotation == false) {
		FRotator wantedrotation = FRotator(0,GetController()->GetControlRotation().Yaw, 0);
		AWebSwingCharacter::SetActorRotation(wantedrotation);
		/*
		FVector V = ACharacter::GetVelocity();
		FRotator R = ACharacter::GetActorRotation();
		V = R.UnrotateVector(V);

		// get forward vector
		FRotator VelocityDirection = UKismetMathLibrary::FindLookAtRotation(GetCapsuleComponent()->GetComponentLocation(), GetCapsuleComponent()->GetComponentLocation() + FVector(ACharacter::GetVelocity().X, ACharacter::GetVelocity().Y, 0).Normalize());
		//FRotator wantedrotation = UKismetMathLibrary::FindLookAtRotation(GetCapsuleComponent()->GetComponentLocation(), GetCapsuleComponent()->GetComponentLocation() + Direction);
		FRotator C = FMath::RInterpTo(GetCapsuleComponent()->GetComponentRotation(), VelocityDirection, .5, 1);
		GetCapsuleComponent()->SetWorldRotation(C);
		*/
	}

}






void AWebSwingCharacter::OnRelease()
{
	
	
	if (IsWebbed == true) {
		//ControllerYaw = GetController()->GetControlRotation().Yaw;
		//LerpingRotation = true;
		thisSystem->TheConstraint->BreakConstraint();
		//thisSystem->TheConstraint->SetConstrainedComponents(thisSystem->StableMesh, NAME_None, NULL, NAME_None);
		
		//AWebSwingCharacter::GetCapsuleComponent()->AddImpulse(ACharacter::GetVelocity() * 10);

		//ACharacter::GetCharacterMovement()->SetPlaneConstraintNormal(AWebSwingCharacter::GetActorRightVector());
		//AWebSwingCharacter::GetCapsuleComponent()->AddImpulse(FollowCamera->GetForwardVector() * SwingReleaseBoost);
		//AWebSwingCharacter::GetMesh()->SetSimulatePhysics(false);
		WebParticles->Deactivate();
		CanShoot = true;
		IsWebbed = false;
		/*
			if (IsWebbed == false && GetCapsuleComponent()->IsSimulatingPhysics() == true && LerpingRotation == false) {
		GetCapsuleComponent()->SetWorldRotation(FRotator(0.f, GetController()->GetControlRotation().Yaw, 0.f));
		*/
	
		
	}
	
	
}

void AWebSwingCharacter::OnShoot()
{

	if (CanShoot == true && IsWebbed == false && ACharacter::GetVelocity().Z <= 0 && AWebSwingCharacter::GetCharacterMovement()->IsFalling() == true) {
		FCollisionQueryParams MyParams(FName(TEXT("WebTrace")), false, this);
		if (GetWorld()->LineTraceSingleByChannel(hit, FollowCamera->GetComponentLocation(), FollowCamera->GetForwardVector() * 100000, ECC_Visibility,MyParams) == true) {
			if (hit.GetActor()->ActorHasTag("CanWeb") && hit.Location.Z > AWebSwingCharacter::GetActorLocation().Z) {
				FVector directiontopoint = FVector(hit.Location.X - AWebSwingCharacter::GetActorLocation().X, hit.Location.Y - AWebSwingCharacter::GetActorLocation().Y, hit.Location.Z - AWebSwingCharacter::GetActorLocation().Z).GetSafeNormal();
				FVector forwarddirection = FVector(ACharacter::GetVelocity().X, ACharacter::GetVelocity().Y, hit.Location.Z - AWebSwingCharacter::GetActorLocation().Z).GetSafeNormal();
				float TheAngle = FMath::RadiansToDegrees(UKismetMathLibrary::Acos(FVector::DotProduct(directiontopoint, forwarddirection)));

				IsWebbed = true;
				CanShoot = false;
				WebEndPoint->SetWorldLocation(hit.Location);
				WebParticles->Activate();
				

				if (TestSystem != NULL) {
					//GetCapsuleComponent()->SetSimulatePhysics(true);

					//EDOFMode::CustomPlane = 
					//FVector MyCurrentTransform = FVector(AWebSwingCharacter::GetActorForwardVector() + AWebSwingCharacter::GetActorRightVector() + AWebSwingCharacter::GetActorUpVector());
					//GetCapsuleComponent()->SetConstraintMode(EDOFMode::CustomPlane);
					
					thisSystem = GetWorld()->SpawnActor<ATestConstraintSystem>(TestSystem, hit.Location, hit.Normal.Rotation());
					ConstraintSystems.Add(thisSystem);

					thisBall->MyCollider->SetSimulatePhysics(true);
					thisBall->MyCollider->SetAllPhysicsLinearVelocity(GetCapsuleComponent()->GetPhysicsLinearVelocity());
					thisBall->MyCollider->SetMassScale(NAME_None, GetCapsuleComponent()->GetMassScale());

					//GEngine->AddOnScreenDebugMessage(-1, 100, FColor::Cyan, FString::FromInt(ConstraintSystems.Num()));
					//thisSystem->StableMesh->SetCollisionProfileName("NoCollision");
					thisSystem->StableMesh->SetWorldLocation(thisSystem->GetActorLocation());
					thisSystem->StableMesh->SetMobility(EComponentMobility::Stationary);
					thisSystem->TheConstraint->AttachToComponent(thisSystem->StableMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
					//thisSystem->TheConstraint->OverrideComponent1 = thisSystem->StableMesh;
					//thisSystem->TheConstraint->ConstraintActor2 = this;
					thisSystem->TheConstraint->SetConstrainedComponents(thisSystem->StableMesh, NAME_None, thisBall->MyCollider, NAME_None);
				}

			}
		}

	}


}




void AWebSwingCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AWebSwingCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void AWebSwingCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void AWebSwingCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AWebSwingCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AWebSwingCharacter::MoveForward(float Value)
{
	
	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}

}

void AWebSwingCharacter::MoveRight(float Value)
{
	if ( (Controller != NULL) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}
