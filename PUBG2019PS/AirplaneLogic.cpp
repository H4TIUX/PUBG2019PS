#include "SDK.hpp"

using namespace SDK;

struct FPlaneSettings
{
    // [Plane]
    bool  PlaneRoute = true;
    float MapSize = 816000.f;       // Erangel, cm
    float CrossRadius = 120000.f;   // the route passes within this of the map centre (cm)
    float EdgeMin = 30000.f;        // route starts / ends at this box (just off the coast)
    float EdgeMax = 786000.f;
    float Altitude = 150000.f;      // cm (1500 m)
    float SpeedKmh = 0.f;           // 0 = keep the plane's own speed
    bool  EjectAtEnd = true;
    float EjectBeforeEnd = 80000.f; // cm before the end of the route
    bool  OpenJump = true;          // make sure the plane's jump (F) is open once it's on the new route
    float PlaneNetRate = 30.f;      // plane network updates per second (0 = leave the game's own)
    bool  JumpAssist = true;        // a player in the plane presses F (interact) and is still aboard -> jump them out
    float JumpAssistDelay = 0.3f;   // seconds to let the game do it itself first
    bool  JumpLog = true;           // log what players' games send the server while in the plane
    bool  PlaneLog = true;
    bool  SplineAbsolute = true;    // pin the path to the map, not to the (moving) plane
};
FPlaneSettings S;

struct FPlane1
{
    ATransportAircraftVehicle* Ptr = nullptr;
    int32   Index = -1;
    bool    Routed = false;
    bool    Ejected = false;
    bool    JumpLogged = false;
    FVector Start{}, End{};
    float   RouteLen = 0.f;
    double  NextLog = 0;
    FVector LastPos{};
    double  LastPosAt = 0;
    int     Rewrites = 0;
};

std::vector<FPlane1> GPlanes;
double GNextPlaneScan = 0;
unsigned GRand = 0;

constexpr float kPi = 3.14159265f;

double NowSec() { return GetTickCount64() / 1000.0; }

FVector V(float X, float Y, float Z) { FVector R; R.X = X; R.Y = Y; R.Z = Z; return R; }
float Len2D(float X, float Y) { return std::sqrt(X * X + Y * Y); }
std::string F0(float F) { return std::to_string((int)std::lround(F)); }
std::string Pos(const FVector& P) { return F0(P.X) + "," + F0(P.Y) + "," + F0(P.Z); }

void Log(std::string LOG)
{
    std::cerr << "[PUBG2019PS] " << LOG << std::endl;
}

void CUSTOMLOG1(std::string LOG)
{
    Log(LOG);
}

float Rand01()
{
    GRand = GRand * 1664525u + 1013904223u;
    return (GRand >> 8) / 16777216.f;
}

ATransportAircraftVehicle* GLOB_AircraftVehicle = nullptr;
UMatchPreparer* GLOB_OLD_MatchPreparer = nullptr;

class AActor* SpawnActorFromClass(
    class UObject* WorldContextObject, TSubclassOf < class AActor > ActorClass,
    const struct FTransform& SpawnTransform,
    ESpawnActorCollisionHandlingMethod CollisionHandlingOverride,
    class AActor* Owner) {
    auto Spawned = UGameplayStatics::BeginDeferredActorSpawnFromClass(
        WorldContextObject, ActorClass, SpawnTransform, CollisionHandlingOverride,
        Owner);

    Spawned = UGameplayStatics::FinishSpawningActor(Spawned, SpawnTransform);

    return Spawned;
}

void StartAirplane()
{
    // Access the Engine and World instances
    SDK::UEngine* Engine = SDK::UEngine::GetEngine();
    SDK::UWorld* World = SDK::UWorld::GetWorld();

    if (!World || !Engine)
    {
        CUSTOMLOG1("World or Engine is invalid.");
        return;
    }

    SDK::APlayerController* MyController = SDK::UGameplayStatics::GetPlayerController(World, 0);

    if (!MyController || !MyController->HasAuthority())
    {
        CUSTOMLOG1("Controller is invalid or lacks authority.");
        return;
    }

    auto MyGamemode = SDK::UGameplayStatics::GetGameMode(World);
    if (!MyGamemode || !MyGamemode->IsA(ATslGameMode::StaticClass()))
    {
        CUSTOMLOG1("GameMode is invalid or not ATslGameMode.");
        return;
    }

    ATslGameMode* TslGameMode = static_cast<ATslGameMode*>(MyGamemode);

    if (!TslGameMode->MatchPreparerClasses.IsValidIndex(1) || !TslGameMode->MatchPreparerClasses[1].Class)
    {
        CUSTOMLOG1("AirborneMatchPreparer class not available.");
        return;
    }

    UClass* AirborneMatchPreparerClass = TslGameMode->MatchPreparerClasses[1].Class;
    UObject* Transient = UObject::FindObjectFast("Transient");

    // Save old Match Preparer and set up Airborne Match Preparer
    GLOB_OLD_MatchPreparer = TslGameMode->MatchPreparer;
    TslGameMode->MatchPreparer = static_cast<UMatchPreparer*>(SDK::UGameplayStatics::SpawnObject(AirborneMatchPreparerClass, Transient));

    if (!TslGameMode->MatchPreparer || !TslGameMode->MatchPreparer->IsA(AirborneMatchPreparerClass))
    {
        CUSTOMLOG1("Failed to set MatchPreparer to AirborneMatchPreparer.");
        return;
    }

    // Cast to AirborneMatchPreparer to access aircraft properties
    UAirborneMatchPreparer* AirbornePrep = static_cast<UAirborneMatchPreparer*>(TslGameMode->MatchPreparer);
    if (!AirbornePrep)
    {
        CUSTOMLOG1("Airborne Match Preparer casting failed.");
        return;
    }

    // Set up aircraft altitude and class
    float Degree = AirbornePrep->EndThetaDegree;
    float AircraftAltitude = AirbornePrep->AircraftAltitude;

    // Spawn the aircraft
    FTransform AircraftTransform;
    AircraftTransform.Translation = FVector(0, 0, AircraftAltitude);
    AircraftTransform.Scale3D = FVector(1, 1, 1);
    AircraftTransform.Rotation = FQuat(0, 0, 0, 1);

    ATransportAircraftVehicle* _ARC = static_cast<ATransportAircraftVehicle*>(SpawnActorFromClass(World, ATransportAircraftVehicle::StaticClass(), AircraftTransform, ESpawnActorCollisionHandlingMethod::AlwaysSpawn, nullptr));

    if (!_ARC)
    {
        CUSTOMLOG1("Failed to spawn aircraft.");
        return;
    }

    // Store the global reference to the aircraft
    GLOB_AircraftVehicle = _ARC;

    // Kill server cus player stuck in plane forever.
    ATslCharacter* server_character = static_cast<ATslCharacter*>(UGameplayStatics::GetPlayerCharacter(World, 0));
    ATslPlayerController* Controller = server_character->GetTslPlayerController();
    //Controller->Suicide();
    //Controller->ServerSuicide();
    //CUSTOMLOG1("Killed server");

    // Get all pawns and assign them to the aircraft
    TArray<APawn*> AllPawns;
    TslGameMode->GetAllPawns(&AllPawns);
    int i = 0;
    for (APawn* Pawn : AllPawns)
    {
        if (!Pawn)
        {
            CUSTOMLOG1("Pawn not exists.");
            continue;
        }

        ATslCharacter* TslPawn = static_cast<ATslCharacter*>(Pawn);
        if (!Pawn->Controller)
        {
            CUSTOMLOG1("Controller is invalid.");
            continue;
        }

        if (!TslPawn->IsAlive())
        {
            CUSTOMLOG1("Pawn not alive.");
            continue;
        }

        APlayerController* PlayerController = static_cast<APlayerController*>(Pawn->Controller);
        int NetIndex = PlayerController->NetPlayerIndex;

        if (_ARC->VehicleSeatComponent)
        {
            UVehicleSeatInteractionComponent* PlayerSeat = _ARC->VehicleSeatComponent->GetSeats()[i];

            // Important function calls (Get players in the plane and move it to the end of the map)
            if (PlayerSeat)
            {
                // todo check if 0 , 0, 0 and set back to it
                FVector TargetLocation = FVector(150000, 15000, 150000);
                FVector SpawnLocation = FVector(0, 0, 0);

                _ARC->VehicleSeatComponent->Ride(TslPawn, PlayerSeat);
                try
                {
                    PlayerSeat->Rider = TslPawn;
                }
                catch (std::exception e)
                {
                    CUSTOMLOG1("Failed to set rider!");
                }

                _ARC->SplineComponent->AddSplinePoint(SpawnLocation, ESplineCoordinateSpace::World, true);
                _ARC->SplineComponent->AddSplinePoint(TargetLocation, ESplineCoordinateSpace::World, true);
                //_ARC->MovementComponent->bUseSafeInterpolation = true;
                _ARC->MovementComponent->Activate(false);
                _ARC->EnterAtEjectionArea();
                _ARC->AllowInteractBy(TslPawn);
                AirbornePrep->AllowDealDamage();
                PlayerSeat->Activate(true);
                _ARC->InteractionComponent->Activate(true);

                CUSTOMLOG1("Player seated! NetIndex: " + std::to_string(NetIndex) + " i: " + std::to_string(i));
            }
            else
            {
                CUSTOMLOG1("Player seat is invalid.");
            }
        }
        else
        {
            CUSTOMLOG1("Invalid NetIndex for seating assignment.");
        }
        i++;
    }

    FText NotifyMessage = UKismetTextLibrary::Conv_StringToText(FString(L"WELCOME TO OG BATTLEGROUNDS! HTTPS://T.ME/OGBATTLEGROUNDS"));
    TslGameMode->SendSystemMessageToAll(ESystemMessageType::Important, NotifyMessage, 5.0);

    // Restore the old Match Preparer
    TslGameMode->MatchPreparer = GLOB_OLD_MatchPreparer;
}

// clip the infinite line P + t*D to the box [Lo,Hi]^2 -> entry / exit points
bool ClipLine(float PX, float PY, float DX, float DY, float Lo, float Hi, FVector& A, FVector& B)
{
    float T0 = -1e12f, T1 = 1e12f;
    const float P[2] = { PX, PY }, D[2] = { DX, DY };
    for (int k = 0; k < 2; ++k)
    {
        if (std::fabs(D[k]) < 1e-6f)
        {
            if (P[k] < Lo || P[k] > Hi) return false;
            continue;
        }
        float Ta = (Lo - P[k]) / D[k], Tb = (Hi - P[k]) / D[k];
        if (Ta > Tb) std::swap(Ta, Tb);
        T0 = (std::max)(T0, Ta);
        T1 = (std::min)(T1, Tb);
    }
    if (T0 >= T1) return false;
    A = V(PX + DX * T0, PY + DY * T0, S.Altitude);
    B = V(PX + DX * T1, PY + DY * T1, S.Altitude);
    return true;
}

void DescribePath(USplineComponent* Spl, const char* What)
{
    if (!Spl) return;
    const int32 N = Spl->GetNumberOfSplinePoints();
    std::string Pts;
    for (int32 i = 0; i < N && i < 8; ++i)
        Pts += " [" + Pos(Spl->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World)) + "]";
    if (N > 8) Pts += " ...";
    Log(std::string("plane: ") + What + " path " + std::to_string(N) + " points, " +
        F0(Spl->GetSplineLength() / 100.f) + " m long, spline Duration " + std::to_string(Spl->Duration) + ":" + Pts);
}

void WriteStraightPath(USplineComponent* Spl, const FVector& St, const FVector& En)
{
    if (S.SplineAbsolute)
    {
        // path in map coordinates: the component no longer follows the plane around
        Spl->bAbsoluteLocation = 1;
        Spl->bAbsoluteRotation = 1;
        Spl->bAbsoluteScale = 1;
        FHitResult Hit{};
        Spl->K2_SetWorldLocationAndRotation(V(0, 0, 0), FRotator{}, false, &Hit, true);
    }
    Spl->ClearSplinePoints(false);
    Spl->AddSplinePoint(St, ESplineCoordinateSpace::World, false);
    Spl->AddSplinePoint(En, ESplineCoordinateSpace::World, false);
    Spl->SetSplinePointType(0, ESplinePointType::Linear, false);
    Spl->SetSplinePointType(1, ESplinePointType::Linear, false);
    Spl->SetClosedLoop(false, false);
    Spl->UpdateSpline();
}

void RoutePlane(FPlane1& P)
{
    ATransportAircraftVehicle* A = P.Ptr;
    USplineComponent* Spl = A->SplineComponent;
    if (!Spl) { Log("plane: no spline component - leaving it alone"); P.Routed = true; return; }

    const float OldLen = Spl->GetSplineLength();
    const float OldFly = A->TotalFlyingTime;
    DescribePath(Spl, "original");
    {
        USceneComponent* Par = Spl->AttachParent;
        Log(std::string("plane: path component is ") + (Par ? "attached to " + Par->GetName() : std::string("not attached")) +
            ", absolute location " + (Spl->bAbsoluteLocation ? "yes" : "no") + ", at " + Pos(Spl->K2_GetComponentLocation()) +
            " (plane at " + Pos(A->K2_GetActorLocation()) + ")");
    }
    Log("plane: TotalFlyingTime " + std::to_string(OldFly) + " s, jump open " +
        (A->bIsEnteredAtEjectionArea ? "yes" : "no") + ", EjectPredictionTime " + std::to_string(A->EjectPredictionTime) +
        ", MaxEjectSpeed " + std::to_string(A->MaxEjectSpeed) + ", net rate " + F0(A->NetUpdateFrequency) + "/s (min " +
        F0(A->MinNetUpdateFrequency) + "), priority " + std::to_string(A->NetPriority).substr(0, 4) +
        ", always relevant " + (A->bAlwaysRelevant ? "yes" : "no"));

    // straight line, random direction, through a random point near the middle of the map
    const float Mid = S.MapSize * 0.5f;
    FVector St{}, En{};
    bool bOk = false;
    for (int Try = 0; Try < 20 && !bOk; ++Try)
    {
        const float Ang = Rand01() * 2.f * kPi;
        const float R = std::sqrt(Rand01()) * S.CrossRadius, RA = Rand01() * 2.f * kPi;
        const float CX = Mid + std::cos(RA) * R, CY = Mid + std::sin(RA) * R;
        bOk = ClipLine(CX, CY, std::cos(Ang), std::sin(Ang), S.EdgeMin, S.EdgeMax, St, En);
        if (bOk && Len2D(En.X - St.X, En.Y - St.Y) < S.MapSize * 0.6f) bOk = false;   // too short (clipped a corner)
    }
    if (!bOk) { Log("plane: couldn't pick a route - leaving it alone"); P.Routed = true; return; }

    const float NewLen = Len2D(En.X - St.X, En.Y - St.Y);
    // speed: the plane always takes its TotalFlyingTime for the whole path, so it is left alone (v2 changed it
    // and that closed the jump). Only SpeedKmh (if set) changes it.
    float Speed = 0.f;   // cm/s
    if (S.SpeedKmh > 1.f) Speed = S.SpeedKmh / 0.036f;
    (void)OldLen;

    // put it at the start, facing along the route (riders are attached and come with it) - first, so a
    // path that is still attached to the plane is written where the plane really is
    FRotator Rot{};
    Rot.Yaw = std::atan2(En.Y - St.Y, En.X - St.X) * 180.f / kPi;
    FHitResult Hit{};
    A->K2_SetActorLocationAndRotation(St, Rot, false, &Hit, true);

    WriteStraightPath(Spl, St, En);
    if (Speed > 1.f)
    {
        const float Fly = NewLen / Speed;
        A->TotalFlyingTime = Fly;
        Spl->Duration = Fly;
    }
    // smoother on players' screens: more frequent position updates, sent first, to everyone
    if (S.PlaneNetRate > 0.f)
    {
        A->NetUpdateFrequency = S.PlaneNetRate;
        A->MinNetUpdateFrequency = (std::max)(A->MinNetUpdateFrequency, (std::min)(10.f, S.PlaneNetRate));
        A->NetPriority = (std::max)(A->NetPriority, 3.f);
        A->bAlwaysRelevant = 1;
    }
    // F to jump
    if (S.OpenJump && !A->bIsEnteredAtEjectionArea)
    {
        A->EnterAtEjectionArea();
        if (!A->bIsEnteredAtEjectionArea) A->bIsEnteredAtEjectionArea = true;
        Log("plane: jump (F) opened");
    }
    A->ForceNetUpdate();

    P.Start = St; P.End = En; P.RouteLen = NewLen; P.Routed = true;
    P.LastPos = St; P.LastPosAt = NowSec();
    Log("plane: new route " + Pos(St) + " -> " + Pos(En) + ", " + F0(NewLen / 100.f) + " m, heading " +
        F0(Rot.Yaw < 0 ? Rot.Yaw + 360.f : Rot.Yaw) + " deg" +
        (Speed > 1.f ? ", " + F0(Speed * 0.036f) + " km/h, about " + F0(NewLen / Speed) + " s to cross" :
            ", flying time unchanged (" + F0(A->TotalFlyingTime) + " s, about " + F0(A->TotalFlyingTime > 1.f ? NewLen / A->TotalFlyingTime * 0.036f : 0.f) + " km/h)"));
    DescribePath(Spl, "new");
}

void PlaneThink(UWorld* World)
{
    if (!S.PlaneRoute && !S.PlaneLog) return;
    const double T = NowSec();
    if (T < GNextPlaneScan) return;
    //GNextPlaneScan = T + 0.5;
    GNextPlaneScan = T + 1;
    if (!World) return;
    
    // new planes
    for (int i = 0; i < UObject::GObjects->Num(); ++i)
    {
        UObject* Obj = UObject::GObjects->GetByIndex(i);
        if (!Obj || Obj->IsDefaultObject() || !Obj->IsA(ATransportAircraftVehicle::StaticClass())) continue;
        ATransportAircraftVehicle* A = (ATransportAircraftVehicle*)Obj;
        //if (!A || !A->IsA(ATransportAircraftVehicle::StaticClass())) continue;
        bool bKnown = false;
        for (FPlane1& P : GPlanes) if (P.Ptr == A && P.Index == A->GetIndex()) bKnown = true;
        if (bKnown) continue;
        FPlane1 P;
        P.Ptr = static_cast<ATransportAircraftVehicle*>(A);
        P.Index = A->GetIndex();
        Log("plane: spotted at " + Pos(A->K2_GetActorLocation()));
        if (S.PlaneRoute) RoutePlane(P);
        GPlanes.push_back(P);
    }

    for (FPlane1& P : GPlanes)
    {
        ATransportAircraftVehicle* A = P.Ptr;
        const FVector Now = A->K2_GetActorLocation();

        // someone else rewrote the path after us? put ours back (and say so)
        if (S.PlaneRoute && P.Routed && P.RouteLen > 0.f && A->SplineComponent && P.Rewrites < 20)
        {
            USplineComponent* Spl = A->SplineComponent;
            const int32 N = Spl->GetNumberOfSplinePoints();
            const FVector E = N > 0 ? Spl->GetLocationAtSplinePoint(N - 1, ESplineCoordinateSpace::World) : FVector{};
            if (N != 2 || Len2D(E.X - P.End.X, E.Y - P.End.Y) > 100.f)
            {
                ++P.Rewrites;
                Log("plane: path changed again (" + std::to_string(N) + " points, end now " + Pos(E) +
                    ") - restoring the straight route");
                WriteStraightPath(Spl, P.Start, P.End);
            }
        }

        // speed / position log (shows any slow-downs or wandering)
        if (S.PlaneLog && T >= P.NextLog)
        {
            P.NextLog = T + 3.0;
            const double Dt = T - P.LastPosAt;
            const float Sp = Dt > 0.1 ? Len2D(Now.X - P.LastPos.X, Now.Y - P.LastPos.Y) / (float)Dt * 0.036f : 0.f;
            float Off = 0.f;
            if (P.RouteLen > 0.f)
            {
                const float DX = P.End.X - P.Start.X, DY = P.End.Y - P.Start.Y;
                Off = std::fabs((Now.X - P.Start.X) * DY - (Now.Y - P.Start.Y) * DX) / P.RouteLen;   // distance from the line
            }
            Log("plane: at " + Pos(Now) + "  " + F0(Sp) + " km/h" +
                (P.RouteLen > 0.f ? "  " + F0(Off / 100.f) + " m off the route" : std::string()) +
                "  jump " + (A->bIsEnteredAtEjectionArea ? "open" : "CLOSED") + ", in jump area " + (A->IsInEjectionArea() ? "yes" : "no"));
            P.LastPos = Now; P.LastPosAt = T;
        }

        if (S.PlaneRoute && S.OpenJump && P.Routed && !P.Ejected && !A->bIsEnteredAtEjectionArea && !P.JumpLogged)
        {
            P.JumpLogged = true;
            Log("plane: jump was closed again by something - opening it");
            A->EnterAtEjectionArea();
            A->bIsEnteredAtEjectionArea = true;
            A->ForceNetUpdate();
        }

        // eject everyone as it reaches the far coast
        if (S.EjectAtEnd && P.Routed && P.RouteLen > 0.f && !P.Ejected)
        {
            const float DX = P.End.X - P.Start.X, DY = P.End.Y - P.Start.Y;
            const float Along = ((Now.X - P.Start.X) * DX + (Now.Y - P.Start.Y) * DY) / P.RouteLen;
            if (Along >= P.RouteLen - S.EjectBeforeEnd)
            {
                P.Ejected = true;
                Log("plane: reached the far coast - ejecting everyone still aboard");
                A->EjectAll();
            }
        }
    }
}