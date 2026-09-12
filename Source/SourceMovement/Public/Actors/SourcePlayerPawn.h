#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "SourcePlayerPawn.generated.h"

class UCameraComponent;
class USourceHullComponent;
class USourceMovementComponent;
class USourceMovementAudioComponent;

UCLASS(Blueprintable)
class SOURCEMOVEMENT_API ASourcePlayerPawn : public APawn
{
	GENERATED_BODY()

public:
	ASourcePlayerPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual UPawnMovementComponent* GetMovementComponent() const override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source Movement|Input")
	bool bInvertMouseY = false;

	// -1..1, +1 = forward
	UFUNCTION(BlueprintCallable, Category = "Source Movement|Input")
	void SetForwardInput(float Value);

	// -1..1, +1 = right
	UFUNCTION(BlueprintCallable, Category = "Source Movement|Input")
	void SetSideInput(float Value);

	// -1..1, +1 = up
	UFUNCTION(BlueprintCallable, Category = "Source Movement|Input")
	void SetUpInput(float Value);

	UFUNCTION(BlueprintCallable, Category = "Source Movement|Input")
	void SetJumpHeld(bool bHeld);

	UFUNCTION(BlueprintCallable, Category = "Source Movement|Input")
	void SetDuckHeld(bool bHeld);

	// The CS:GO walk (shift) modifier
	UFUNCTION(BlueprintCallable, Category = "Source Movement|Input")
	void SetWalkHeld(bool bHeld);

	UFUNCTION(BlueprintCallable, Category = "Source Movement|Input")
	void AddViewInput(float PitchDelta, float YawDelta);

	UFUNCTION(BlueprintCallable, Category = "Source Movement|Input")
	void SetSourceViewAngles(float Pitch, float Yaw, float Roll = 0.f);

	UFUNCTION(BlueprintPure, Category = "Source Movement")
	USourceMovementComponent* GetSourceMovement() const { return SourceMovement; }

	UFUNCTION(BlueprintPure, Category = "Source Movement")
	USourceMovementAudioComponent* GetMovementAudio() const { return MovementAudio; }

	UFUNCTION(BlueprintPure, Category = "Source Movement")
	USourceHullComponent* GetHullProxy() const { return HullProxy; }

	UFUNCTION(BlueprintPure, Category = "Source Movement")
	UCameraComponent* GetViewCamera() const { return ViewCamera; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void UpdateView();

	static void ClampViewAngles(struct FSrcAngles& Angles);

	void Input_Turn(float Value);
	void Input_LookUp(float Value);

	void OnAnyKeyPressed(FKey Key);
	void OnAnyKeyReleased(FKey Key);

private:
	// Plain scene root: no collision body, no physics
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Source Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Source Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USourceHullComponent> HullProxy;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Source Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> ViewCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Source Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USourceMovementComponent> SourceMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Source Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USourceMovementAudioComponent> MovementAudio;
};
