#include <Windows.h>
#include <TlHelp32.h>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "SDK.hpp"
#include "SDK/TslGame_parameters.hpp"
#include "MinHook.h"
#include "BannerImageRle.hpp"
using namespace SDK;
void CUSTOMLOG(std::string) {}
SDK::UClass* Get_PlayerPawn_Class() { return nullptr; }
using Event = void(*)(UObject*, UFunction*, void*);
static Event Next = nullptr;
static DWORD GameThread = 0;
static thread_local bool Inside = false;
static std::wstring Requested;
static std::string RequestedAscii;
static std::ofstream Log;
static ULONGLONG NextRequest = 0;
static int Attempts = 0;
static UWorld* NameWorld = nullptr;
static APlayerController* NameController = nullptr;
static APlayerState* NameState = nullptr;
static bool RequestedForState = false;
static bool NameConfirmed = false;
static ULONGLONG NextVersionNameCheck = 0;

// The bottom version widget gets its nickname independently of PlayerState.
// Change only this widget's nickname prefix and retain its version/region text.
static void UpdateVersionLabel(UWorld* World) {
    if (!World || Requested.empty() || GetTickCount64() < NextVersionNameCheck) return;
    NextVersionNameCheck = GetTickCount64() + 1000;
    auto* WidgetClass = UVersionInfoBaseWidget::StaticClass();
    if (!WidgetClass) return;
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, &Widgets, TSubclassOf<UUserWidget>(WidgetClass), false);
    bool Changed = false;
    for (int i = 0; i < Widgets.Num() && i < 16; ++i) {
        auto* Widget = Widgets[i];
        if (!Widget || !Widget->IsA(WidgetClass)) continue;
        auto* Version = static_cast<UVersionInfoBaseWidget*>(Widget);
        auto* Text = Version->VersionInfoTextBinder.CachedTextBlock;
        if (!Text || !Text->IsA(UTextBlock::StaticClass())) continue;
        const FText Current = Text->GetText();
        if (!Current.TextData) continue;
        const std::string Existing = UKismetTextLibrary::Conv_TextToString(Current).ToString();
        const size_t Separator = Existing.find(" - ");
        if (Separator == std::string::npos || Separator == 0 || Existing.substr(0, Separator) == RequestedAscii) continue;
        // Version/region is ASCII in the supported build; preserve it exactly.
        const std::string Suffix = Existing.substr(Separator);
        const std::wstring Display = Requested + std::wstring(Suffix.begin(), Suffix.end());
        Text->SetText(UKismetTextLibrary::Conv_StringToText(FString(Display.c_str())));
        Text->InvalidateLayoutAndVolatility();
        Version->InvalidateLayoutAndVolatility();
        // Retainer boxes can wrap a nested user widget and need not be
        // returned as top-level widgets. Walk the actual widget parents too.
        UWidget* Parent = Version;
        for (int Depth = 0; Parent && Depth < 16; ++Depth) {
            Parent = Parent->GetParent();
            if (Parent && Parent->IsA(URetainerBox::StaticClass())) {
                static_cast<URetainerBox*>(Parent)->RequestRender();
                Log << "Version label ancestor retainer refresh requested." << std::endl;
            }
        }
        Changed = true;
        Log << "Version label nickname updated: " << Version->GetFullName() << " / " << Text->GetFullName() << std::endl;
    }
    if (Changed) {
        // Version text can be painted into a retainer box. Request a new
        // render after changing the text so the cached image is not stale.
        auto* CanvasClass = UVersionInfoCanvasBaseWidget::StaticClass();
        TArray<UUserWidget*> Canvases;
        if (CanvasClass) UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, &Canvases, TSubclassOf<UUserWidget>(CanvasClass), false);
        int RenderRequests = 0;
        for (int i = 0; i < Canvases.Num() && i < 16; ++i) {
            auto* Canvas = Canvases[i];
            if (!Canvas || !Canvas->IsA(CanvasClass)) continue;
            auto* VersionCanvas = static_cast<UVersionInfoCanvasBaseWidget*>(Canvas);
            auto* Retainer = VersionCanvas->VersionInfoRetainerBoxBinder.CachedRetainerBox;
            if (Retainer && Retainer->IsA(URetainerBox::StaticClass())) {
                Retainer->RequestRender(); ++RenderRequests;
            }
        }
        Log << "Version label retainer render requests: " << RenderRequests << std::endl;
    }
}

// Gender itself is not a replicated property in this build. A saved human
// appearance can replace the initial pawn's randomly chosen body. Keep the
// client's gender consistent with the authoritative decoded face/body parts.
static void SyncAppearanceGender(UObject* Object){
    if(!Object||!Object->IsA(ATslCharacter::StaticClass()))return;
    auto* Character=static_cast<ATslCharacter*>(Object);auto* Instance=Character->CustomizableObjectInstance;
    if(!Instance)return;
    const auto Face=Instance->GetIntParameterSelectedOption(FString(L"Face")).ToString();
    const auto Body=Instance->GetIntParameterSelectedOption(FString(L"NudeBody")).ToString();
    if(Face.size()<2||Body.size()<2||Face[0]!=Body[0]||Face[1]!='_'||Body[1]!='_'||(Face[0]!='F'&&Face[0]!='M'))return;
    const auto Gender=Face[0]=='F'?EGender::Female:EGender::Male;
    if(Character->Gender!=Gender){Character->Gender=Gender;Log<<"Character gender synchronized from replicated appearance: "<<Character->GetName()<<std::endl;}
}

static DWORD MainThread() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 e{sizeof(e)};
    ULONGLONG oldest=~0ULL; DWORD result=0;
    if (snapshot!=INVALID_HANDLE_VALUE && Thread32First(snapshot,&e)) do {
        if(e.th32OwnerProcessID!=GetCurrentProcessId()) continue;
        HANDLE thread=OpenThread(THREAD_QUERY_LIMITED_INFORMATION,FALSE,e.th32ThreadID);
        if(!thread) continue;
        FILETIME created{},exit{},kernel{},user{};
        if(GetThreadTimes(thread,&created,&exit,&kernel,&user)) {
            ULONGLONG t=(ULONGLONG(created.dwHighDateTime)<<32)|created.dwLowDateTime;
            if(t<oldest){oldest=t;result=e.th32ThreadID;}
        }
        CloseHandle(thread);
    } while(Thread32Next(snapshot,&e));
    if(snapshot!=INVALID_HANDLE_VALUE) CloseHandle(snapshot);
    return result;
}

static ULONGLONG NextAircraftProbe = 0;
static int AircraftSnapshots = 0;
static std::ofstream AircraftLog;
static std::filesystem::path BinaryDir;
#include "ZombieVoice.inl"
static UTextureRenderTarget2D* BannerTexture = nullptr;
static uint32_t BannerAppliedSlots = 0;

static float SRGBToLinear(uint8_t Channel) {
    const float Value = static_cast<float>(Channel) / 255.0f;
    return Value <= 0.04045f ? Value / 12.92f : std::pow((Value + 0.055f) / 1.055f, 2.4f);
}

static UTextureRenderTarget2D* BuildBannerArtwork(UWorld* World, const BannerImage::Run* Runs, const uint16_t* RowRunCounts, const char* Label) {
    if (!World || !Runs || !RowRunCounts) return nullptr;
    auto* Artwork = UKismetRenderingLibrary::CreateRenderTarget2D(World, BannerImage::Width, BannerImage::Height);
    if (!Artwork) {
        AircraftLog << "BANNER CREATE failed for " << Label << std::endl;
        return nullptr;
    }
    UCanvas* Canvas = nullptr;
    FVector2D CanvasSize{};
    FDrawToRenderTargetContext Context{};
    UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, Artwork, &Canvas, &CanvasSize, &Context);
    if (!Canvas || !Canvas->DefaultTexture || CanvasSize.X < BannerImage::Width || CanvasSize.Y < BannerImage::Height) {
        AircraftLog << "BANNER CREATE invalid artwork canvas for " << Label << std::endl;
        if (Canvas) UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, Context);
        return nullptr;
    }

    size_t RunIndex = 0;
    uint32_t DrawCalls = 0;
    const FVector2D UV(0.0f, 0.0f);
    const FVector2D UVSize(1.0f, 1.0f);
    const FVector2D Pivot(0.0f, 0.0f);
    for (int32 Y = 0; Y < BannerImage::Height; ++Y) {
        float X = 0.0f;
        const uint16_t RowCount = RowRunCounts[Y];
        for (uint16_t I = 0; I < RowCount; ++I, ++RunIndex) {
            const auto& Run = Runs[RunIndex];
            if (!Run.Length) continue;
            const FVector2D Position(X, static_cast<float>(Y));
            const FVector2D Size(static_cast<float>(Run.Length), 1.0f);
            const FLinearColor Color{SRGBToLinear(Run.R), SRGBToLinear(Run.G), SRGBToLinear(Run.B), 1.0f};
            Canvas->K2_DrawTexture(Canvas->DefaultTexture, Position, Size, UV, UVSize, Color, EBlendMode::BLEND_Opaque, 0.0f, Pivot);
            X += static_cast<float>(Run.Length);
            ++DrawCalls;
        }
        if (X != static_cast<float>(BannerImage::Width)) {
            AircraftLog << "BANNER CREATE invalid RLE width for " << Label << " row=" << Y << std::endl;
            UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, Context);
            return nullptr;
        }
    }
    UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, Context);
    AircraftLog << "BANNER CREATE artwork " << Label << " " << BannerImage::Width << "x" << BannerImage::Height << " draws=" << DrawCalls << std::endl;
    return Artwork;
}

static UTextureRenderTarget2D* BuildBannerTexture(UWorld* World) {
    if (!World) return nullptr;
    auto* Artwork = BuildBannerArtwork(World, BannerImage::RunsNormal, BannerImage::RowRunCountsNormal, "normal orientation");
    if (!Artwork) return nullptr;
    // The upper source-atlas island is UV-mirrored on the aircraft mesh. Mirror
    // the artwork there so it reads normally from that side; keep the lower
    // island normal, which already reads correctly from its side.
    auto* MirroredArtwork = BuildBannerArtwork(World, BannerImage::RunsFlipX, BannerImage::RowRunCountsFlipX, "horizontal flip for mirrored UV side");
    if (!MirroredArtwork) return nullptr;
    auto* Atlas = UKismetRenderingLibrary::CreateRenderTarget2D(World, 2048, 2048);
    if (!Atlas) {
        AircraftLog << "BANNER CREATE failed to create 2048x2048 texture atlas" << std::endl;
        return nullptr;
    }
    // Match the dark edge of the supplied artwork in the unused atlas area.
    const FLinearColor Background{SRGBToLinear(1), SRGBToLinear(27), SRGBToLinear(39), 1.0f};
    UKismetRenderingLibrary::ClearRenderTarget2D(World, Atlas, Background);
    UCanvas* Canvas = nullptr;
    FVector2D CanvasSize{};
    FDrawToRenderTargetContext Context{};
    UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, Atlas, &Canvas, &CanvasSize, &Context);
    if (!Canvas || CanvasSize.X < 2048.0f || CanvasSize.Y < 2048.0f) {
        AircraftLog << "BANNER CREATE invalid 2048x2048 atlas canvas" << std::endl;
        if (Canvas) UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, Context);
        return nullptr;
    }
    const FVector2D UV(0.0f, 0.0f);
    const FVector2D UVSize(1.0f, 1.0f);
    const FLinearColor White{1.0f, 1.0f, 1.0f, 1.0f};
    // The mesh displays only a subsection of each atlas island. The previous
    // full-width tiles put artwork outside that subsection: the normal side
    // lost its ending and the mirrored side lost the P17 logo. These bounds
    // are measured from the two in-game views and retain a small edge margin.
    const FVector2D MirroredPosition(64.0f, 1127.0f);
    const FVector2D MirroredSize(1710.0f, 395.0f);
    const FVector2D NormalPosition(64.0f, 1586.0f);
    const FVector2D NormalSize(1710.0f, 415.0f);
    Canvas->K2_DrawTexture(MirroredArtwork, MirroredPosition, MirroredSize, UV, UVSize, White, EBlendMode::BLEND_Opaque, 0.0f, FVector2D(0.0f, 0.0f));
    Canvas->K2_DrawTexture(Artwork, NormalPosition, NormalSize, UV, UVSize, White, EBlendMode::BLEND_Opaque, 0.0f, FVector2D(0.0f, 0.0f));
    UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, Context);
    AircraftLog << "BANNER CREATE visible UV bounds: mirrored=(64,1127 1710x395), normal=(64,1586 1710x415)" << std::endl;
    return Atlas;
}

static void LogAircraftActor(UWorld* World, AActor* Actor) {
    if (!Actor) return;
    AircraftLog << "ACTOR " << Actor->GetFullName() << std::endl;
    auto Components = Actor->GetComponentsByClass(TSubclassOf<UActorComponent>(UMeshComponent::StaticClass()));
    for (int i=0; i<Components.Num() && i<100; ++i) {
        auto* Comp = Components[i];
        if (!Comp || !Comp->IsA(UMeshComponent::StaticClass())) continue;
        auto* Mesh = static_cast<UMeshComponent*>(Comp);
        AircraftLog << " COMPONENT " << Comp->GetFullName() << std::endl;
        if (Comp->IsA(UStaticMeshComponent::StaticClass())) {
            auto* Static = static_cast<UStaticMeshComponent*>(Comp);
            if (Static->StaticMesh) AircraftLog << "  MESH " << Static->StaticMesh->GetFullName() << std::endl;
        }
        const int Count = (std::min)(Mesh->GetNumMaterials(), 32);
        for (int j=0; j<Count; ++j) {
            auto* Material = Mesh->GetMaterial(j);
            if (!Material) continue;
            AircraftLog << "  MATERIAL " << j << " " << Material->GetFullName() << std::endl;
            if (Material->IsA(UMaterialInstance::StaticClass())) {
                auto* Inst = static_cast<UMaterialInstance*>(Material);
                if (Inst->Parent) AircraftLog << "   PARENT " << Inst->Parent->GetFullName() << std::endl;
                for (int k=0; k<Inst->TextureParameterValues.Num() && k<64; ++k) {
                    const auto& P=Inst->TextureParameterValues[k];
                    if (P.ParameterValue) {
                        AircraftLog << "   TEXTURE " << P.ParameterName.ToString() << " " << P.ParameterValue->GetFullName() << std::endl;
                        if (Comp->GetName() == "PGI_Flag_Plane001" && P.ParameterValue->GetName() == "PGI_Flag_D" &&
                            (P.ParameterName.GetRawString() == "Color") && j < 32 && !(BannerAppliedSlots & (1u << j))) {
                            if (!BannerTexture) BannerTexture = BuildBannerTexture(World);
                            if (BannerTexture) {
                                auto* Dynamic = Mesh->CreateDynamicMaterialInstance(j, Material);
                                if (Dynamic) {
                                    Dynamic->SetTextureParameterValue(P.ParameterName, BannerTexture);
                                    BannerAppliedSlots |= (1u << j);
                                    AircraftLog << "BANNER APPLIED component=" << Comp->GetName() << " slot=" << j << std::endl;
                                } else {
                                    AircraftLog << "BANNER apply failed to create dynamic material slot=" << j << std::endl;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

static void ProbeAircraft() {
    if (AircraftSnapshots >= 12 || GetTickCount64() < NextAircraftProbe) return;
    NextAircraftProbe = GetTickCount64() + 5000;
    auto* World = UWorld::GetWorld();
    if (!World || World->AuthorityGameMode) return;
    TArray<AActor*> Planes;
    UGameplayStatics::GetAllActorsOfClass(World, TSubclassOf<AActor>(ATransportAircraftVehicle::StaticClass()), &Planes);
    if (Planes.Num() == 0) return;
    AircraftLog << "AIRCRAFT DIAGNOSTIC count=" << Planes.Num() << std::endl;
    for (int i=0; i<Planes.Num() && i<8; ++i) {
        auto* Plane=Planes[i]; if (!Plane) continue;
        LogAircraftActor(World, Plane);
        TArray<AActor*> Attached; Plane->GetAttachedActors(&Attached);
        for (int j=0; j<Attached.Num() && j<32; ++j) LogAircraftActor(World, Attached[j]);
    }
    ++AircraftSnapshots;
    AircraftLog << "AIRCRAFT SNAPSHOT COMPLETE " << AircraftSnapshots << std::endl;
}

// Experimental zombie-only compatibility: the original scoreboard can be
// constructed without the timer helpers expected by its native callbacks.
// This candidate stays local until it has been independently tested.
static void EnsureZombieScoreboardTimers(UWorld* world) {
    static ULONGLONG NextScan=0;
    static UTslZombieTwoTeamScoreboardWidget* Cached=nullptr;
    static int CachedIndex=-1;
    if (!world || world->AuthorityGameMode || GetTickCount64()<NextScan) return;
    auto* gs=UGameplayStatics::GetGameState(world);
    if (!gs || !gs->IsA(ATslZombieGameState::StaticClass())) return;
    NextScan=GetTickCount64()+250;
    auto* objects=UObject::GObjects.GetTypedPtr();
    if (!objects) return;
    if (CachedIndex>=0 && CachedIndex<objects->Num() && objects->GetByIndex(CachedIndex)==Cached && Cached->TimeLimitTimerUpdater && Cached->RespawnTimerUpdater) return;
    for (int i=0;i<objects->Num();++i) {
        auto* object=objects->GetByIndex(i);
        if (!object || object->IsDefaultObject() || !object->IsA(UTslZombieTwoTeamScoreboardWidget::StaticClass())) continue;
        auto* widget=static_cast<UTslZombieTwoTeamScoreboardWidget*>(object);
        Cached=widget; CachedIndex=i;
        if (!widget->TimeLimitTimerUpdater) widget->TimeLimitTimerUpdater=static_cast<UTimerTextBlockUpdater*>(UGameplayStatics::SpawnObject(UTimerTextBlockUpdater::StaticClass(),widget));
        if (!widget->RespawnTimerUpdater) widget->RespawnTimerUpdater=static_cast<UTimerTextBlockUpdater*>(UGameplayStatics::SpawnObject(UTimerTextBlockUpdater::StaticClass(),widget));
    }
}

// Zombie BR may create the native results title while the replicated match is
// still running. Only suppress that exact result widget for a living human;
// restore its original visibility for genuine end-of-match/death results.
static void HidePrematureZombieResult(UWorld* world) {
    static ULONGLONG NextScan=0;
    static UUserWidget* Cached=nullptr; static int CachedIndex=-1;
    static bool HiddenByUs=false; static ESlateVisibility Previous=ESlateVisibility::Visible;
    if (GetTickCount64()<NextScan) return; NextScan=GetTickCount64()+200;
    auto* Objects=UObject::GObjects.GetTypedPtr(); if (!Objects) return;
    if (CachedIndex<0 || CachedIndex>=Objects->Num() || Objects->GetByIndex(CachedIndex)!=Cached) {
        Cached=nullptr; CachedIndex=-1; HiddenByUs=false;
    }
    auto* Base=world && !world->AuthorityGameMode ? UGameplayStatics::GetGameState(world) : nullptr;
    auto* PC=world && !world->AuthorityGameMode ? UGameplayStatics::GetPlayerController(world,0) : nullptr;
    auto* Z=Base && Base->IsA(ATslZombieGameState::StaticClass()) ? static_cast<ATslZombieGameState*>(Base) : nullptr;
    auto* C=PC && PC->IsA(ATslPlayerController::StaticClass()) ? static_cast<ATslPlayerController*>(PC)->GetTslCharacter() : nullptr;
    const bool Hide=Z && Z->MatchState.ToString()=="InProgress" && Z->NumAlivePlayers>0 && Z->NumAliveZombiePlayers>0 &&
        C && C->Health>0.f && C->GetZombieModePlayerState()==EZombieModePlayerState::Human;
    if (Hide && !Cached) for (int i=0;i<Objects->Num();++i) {
        auto* O=Objects->GetByIndex(i);
        if (!O || O->IsDefaultObject() || !O->IsA(UUserWidget::StaticClass()) || !O->GetClass() ||
            O->GetClass()->GetName()!="NewResultTitleWidget_C" ||
            O->GetFullName().find("Transient.TslEngine")==std::string::npos) continue;
        Cached=static_cast<UUserWidget*>(O); CachedIndex=i; break;
    }
    if (!Cached) return;
    const auto Current=Cached->GetVisibility();
    if (Hide) {
        if (Current!=ESlateVisibility::Collapsed && Current!=ESlateVisibility::Hidden) {
            if (!HiddenByUs) Previous=Current;
            Cached->SetVisibility(ESlateVisibility::Collapsed); HiddenByUs=true;
            Log << "Zombie BR: suppressed premature results title while the match is active: " << Cached->GetFullName() << std::endl;
        }
    } else if (HiddenByUs) {
        if (Current==ESlateVisibility::Collapsed || Current==ESlateVisibility::Hidden) Cached->SetVisibility(Previous);
        HiddenByUs=false;
        Log << "Zombie BR: restored the results title." << std::endl;
    }
}

// The training victim's hit-info properties are not replicated by this build.
// Process the scoped owner event and run the game's native hit feedback locally.
static bool HandleTrainingHitFeedback(UObject* Object,UFunction* Function,void* Arguments){
    if(!Object||!Function||!Arguments||Function->GetName()!="Client_DrawDebugDamage"||!Object->IsA(ATslCharacter::StaticClass()))return false;
    const auto* Args=static_cast<Params::TslCharacter_Client_DrawDebugDamage*>(Arguments);const auto Text=Args->ExtraInfo.ToString();
    const std::string Prefix="P19_TRAINING_HIT|";if(Text.rfind(Prefix,0)!=0||Text.size()>256)return false;
    auto* World=UWorld::GetWorld();if(!World||World->AuthorityGameMode)return false;
    auto* State=UGameplayStatics::GetGameState(World);auto* PC=UGameplayStatics::GetPlayerController(World,0);
    if(!State||!State->IsA(ATslTrainingGameState::StaticClass())||!PC||PC->K2_GetPawn()!=Object||
       !std::isfinite(Args->Damage)||Args->Damage<=0.f||!std::isfinite(Args->ModifiedDamage)||!std::isfinite(Args->ActualDamage))return false;
    auto* C=static_cast<ATslCharacter*>(Object);auto& Hit=C->LastDamageIndicatorInfo;
    const auto Split=Text.find('|',Prefix.size());const auto TypeName=Text.substr(Prefix.size(),Split-Prefix.size());
    UClass* Type=TypeName.empty()?nullptr:UObject::FindClassFast(TypeName.c_str());
    if(!Type||!Type->IsSubclassOf(UTslDamageType::StaticClass()))Type=UTslDamageType::StaticClass();
    Hit.ActualDamage=Args->Damage;Hit.DamageType=TSubclassOf<UDamageType>(Type);Hit.PlayerInstigator.ObjectIndex=-1;Hit.PlayerInstigator.ObjectSerialNumber=0;
    const float Pi=3.14159265358979323846f;const float Yaw=Args->ModifiedDamage*Pi/180.f,Pitch=Args->ActualDamage*Pi/180.f;
    const FVector Direction{std::cos(Pitch)*std::cos(Yaw),std::cos(Pitch)*std::sin(Yaw),std::sin(Pitch)};
    const FVector Origin=C->K2_GetActorLocation()-Direction*500.f;static_cast<FVector&>(Hit.DamageOrigin)=Origin;Hit.AttackerLocation=Origin;
    static_cast<FVector&>(Hit.RelHitLocation)=FVector{0.f,0.f,40.f};Hit.BoneName=FName{};
    if(Split!=std::string::npos&&Split+1<Text.size()){const std::string Name=Text.substr(Split+1);if(Name.size()<64){std::wstring Wide(Name.begin(),Name.end());Hit.BoneName=UKismetStringLibrary::Conv_StringToName(FString(Wide.c_str()));}}
    Hit.ShotDirYaw=(uint8)((int)std::lround(Args->ModifiedDamage*256.f/360.f)&255);Hit.ShotDirPitch=(uint8)((int)std::lround(Args->ActualDamage*256.f/360.f)&255);
    Hit.bPointDamage=1;Hit.bRadialDamage=0;Hit.bKilled=0;++Hit.EnsureReplicationByte;
    C->LastTakeHitInfo=Hit;C->OnRep_LastDamageIndicatorInfo();C->OnRep_LastTakeHitInfo();
    Log<<"Training hit feedback received, HP="<<C->Health<<std::endl;return true;
}

static void Hook(UObject* object, UFunction* function, void* params) {
    // In zombie BR the normal aircraft interaction can tear down the local
    // match while ejecting. Send the game's vehicle-leave request instead,
    // but only for the local player's aircraft seat in an active jump area.
    static thread_local bool ReroutingEject=false;
    static UFunction* AircraftInteraction=nullptr;
    if (!AircraftInteraction && object && GetCurrentThreadId()==GameThread && object->IsA(ATslPlayerController::StaticClass()))
        AircraftInteraction=object->GetClass()->GetFunction("TslPlayerController", "CastInteractionByComponentOnServer");
    if (!ReroutingEject && AircraftInteraction && function==AircraftInteraction && params && object && GetCurrentThreadId()==GameThread) {
        ReroutingEject=true;
        const bool WasInside=Inside;
        Inside=true;
        auto* World=UWorld::GetWorld();
        auto* Base=World && !World->AuthorityGameMode ? UGameplayStatics::GetGameState(World) : nullptr;
        auto* Z=Base && Base->IsA(ATslZombieGameState::StaticClass()) ? static_cast<ATslZombieGameState*>(Base) : nullptr;
        auto* PC=World && !World->AuthorityGameMode ? UGameplayStatics::GetPlayerController(World,0) : nullptr;
        auto* C=PC && PC->IsA(ATslPlayerController::StaticClass()) ? static_cast<ATslPlayerController*>(PC)->GetTslCharacter() : nullptr;
        auto* Component=*reinterpret_cast<UActorComponent**>(params);
        auto* Owner=Component ? Component->GetOwner() : nullptr;
        const bool Eject=Z && Z->ZombieRuleType==EZombieRuleType::BattleRoyale && Z->MatchState.ToString()=="InProgress" &&
            PC==object && C && C->Health>0.f && C->IsInAircraft() && Owner &&
            Owner->IsA(ATransportAircraftVehicle::StaticClass()) && static_cast<ATransportAircraftVehicle*>(Owner)->bIsEnteredAtEjectionArea;
        if (Eject) {
            Log << "Zombie BR: routed aircraft exit through ServerLeaveVehicle." << std::endl;
            static_cast<ATslPlayerController*>(PC)->ServerLeaveVehicle();
            Inside=WasInside;
            ReroutingEject=false;
            return;
        }
        Inside=WasInside;
        ReroutingEject=false;
    }
    if (Inside || GetCurrentThreadId()!=GameThread || !function) { Next(object,function,params); return; }
    Inside=true;const bool TrainingHit=HandleTrainingHitFeedback(object,function,params);Inside=false;
    if(TrainingHit)return;
    Next(object,function,params);
    Inside=true;
    if(function->GetName()=="OnRep_InstanceDescriptor")SyncAppearanceGender(object);
    static ULONGLONG NextZombieHudCheck=0;
    if (GetTickCount64()>=NextZombieHudCheck && function->GetName()=="ReceiveDrawHUD") {
        NextZombieHudCheck=GetTickCount64()+250;
        auto* ZombieWorld=UWorld::GetWorld();
        ZombieVoice::Tick(ZombieWorld,GetTickCount64());
        EnsureZombieScoreboardTimers(ZombieWorld);
        HidePrematureZombieResult(ZombieWorld);
        // Re-evaluate the native FPP camera after the local human has settled
        // into a ground-vehicle seat in Zombie BR. Do not change camera mode,
        // view target, other players, aircraft, or the player's preferences.
        static int LastPawn=-1,LastSeat=-1;static ULONGLONG SeatedAt=0;static bool Fixed=false;
        auto* Base=ZombieWorld && !ZombieWorld->AuthorityGameMode ? UGameplayStatics::GetGameState(ZombieWorld) : nullptr;
        auto* Z=Base && Base->IsA(ATslZombieGameState::StaticClass()) ? static_cast<ATslZombieGameState*>(Base) : nullptr;
        auto* PC=Z ? UGameplayStatics::GetPlayerController(ZombieWorld,0) : nullptr;
        auto* Pawn=PC ? PC->K2_GetPawn() : nullptr;
        auto* Character=Pawn && Pawn->IsA(ATslCharacter::StaticClass()) ? static_cast<ATslCharacter*>(Pawn) : nullptr;
        auto* Seat=Character && Z->ZombieRuleType==EZombieRuleType::BattleRoyale && Character->Health>0.f &&
            Character->GetZombieModePlayerState()==EZombieModePlayerState::Human && Character->IsInVehicle() &&
            !Character->IsInAircraft() && Character->IsFirstPerson() ? Character->GetVehicleSeat() : nullptr;
        if(!Seat){LastPawn=LastSeat=-1;SeatedAt=0;Fixed=false;}
        else {
            if(LastPawn!=Character->GetIndex() || LastSeat!=Seat->GetIndex()){
                LastPawn=Character->GetIndex();LastSeat=Seat->GetIndex();SeatedAt=GetTickCount64();Fixed=false;
            }
            if(!Fixed && GetTickCount64()-SeatedAt>=750){
                Fixed=true;Character->FixupCamera();Log<<"Zombie BR: refreshed local FPP vehicle camera."<<std::endl;
            }
        }
    }
    ProbeAircraft();
    if (!Requested.empty() && GetTickCount64() >= NextVersionNameCheck) {
        auto* world = UWorld::GetWorld();
        if (world && !world->AuthorityGameMode) UpdateVersionLabel(world);
        else NextVersionNameCheck = GetTickCount64() + 1000;
    }
    if ((GetAsyncKeyState(VK_F12) & 1) != 0) {
        auto* world = UWorld::GetWorld();
        if (world && !world->AuthorityGameMode) {
            auto* pc = UGameplayStatics::GetPlayerController(world, 0);
            if (pc) {
                UKismetSystemLibrary::ExecuteConsoleCommand(world, FString(L"Shot ShowUI"), pc);
                Log << "F12 requested Shot ShowUI command" << std::endl;
            }
        }
    }
    Inside=false;
    if (Requested.empty() || GetTickCount64()<NextRequest) return;
    Inside=true;
    const ULONGLONG Now = GetTickCount64();
    NextRequest=Now+5000;
    auto* world=UWorld::GetWorld();
    if(world && !world->AuthorityGameMode) {
        auto* pc=UGameplayStatics::GetPlayerController(world,0);
        if(pc && pc->PlayerState && pc->IsLocalPlayerController()) {
            auto* state = pc->PlayerState;
            if (world != NameWorld || pc != NameController || state != NameState) {
                NameWorld = world; NameController = pc; NameState = state;
                Attempts = 0; RequestedForState = false; NameConfirmed = false;
                Log << "Local player state changed; name request re-armed." << std::endl;
            }
            const bool Matches = state->PlayerName.ToString() == RequestedAscii;
            if (!RequestedForState || !Matches) {
                NameConfirmed = false;
                if (Attempts < 6) {
                    ++Attempts;
                    pc->ServerChangeName(FString(Requested.c_str()));
                    RequestedForState = true;
                    Log << "Requested local player name, attempt " << Attempts << std::endl;
                } else {
                    Attempts = 0;
                    NextRequest = Now + 30000;
                    Log << "Name not yet confirmed; retrying after a pause." << std::endl;
                }
            } else {
                if (!NameConfirmed) Log << "Local player state confirms the requested name." << std::endl;
                NameConfirmed = true; Attempts = 0; NextRequest = Now + 10000;
            }
        } else {
            NameWorld = nullptr; NameController = nullptr; NameState = nullptr;
            RequestedForState = false; NameConfirmed = false; Attempts = 0;
        }
    } else {
        NameWorld = nullptr; NameController = nullptr; NameState = nullptr;
        RequestedForState = false; NameConfirmed = false; Attempts = 0;
    }
    Inside=false;
}

static DWORD WINAPI Install(void* module) {
    wchar_t path[32768]{};
    if(!GetModuleFileNameW(static_cast<HMODULE>(module),path,32768)) return 1;
    const std::filesystem::path dir=std::filesystem::path(path).parent_path();
    BinaryDir = dir;
    Log.open(dir / L"PUBG2019ClientName.log",std::ios::app);
    AircraftLog.open(dir / L"PUBG2019AircraftDiagnostic.log", std::ios::trunc);
    std::ifstream input(dir / L"PlayerName.txt");
    std::string name;
    if (input) std::getline(input,name); // Empty or missing names still need the compatibility hook.
    else Log << "No PlayerName.txt name configured; enabling compatibility only." << std::endl;
    if (name.size() >= 3 && (unsigned char)name[0]==0xef && (unsigned char)name[1]==0xbb && (unsigned char)name[2]==0xbf) name.erase(0,3);
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.pop_back();
    size_t first=0; while(first<name.size() && std::isspace(static_cast<unsigned char>(name[first]))) ++first;
    name.erase(0,first);
    if (name.size()>24) { Log << "Name must contain 1-24 letters, digits, underscores or hyphens." << std::endl; return 2; }
    for(unsigned char c:name) {
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-')) {
            Log << "Name must contain 1-24 letters, digits, underscores or hyphens." << std::endl; return 2;
        }
    }
    Requested.assign(name.begin(),name.end());
    RequestedAscii = name;
    GameThread=MainThread();
    if(!GameThread) { Log << "Cannot find the game thread." << std::endl; return 3; }
    auto* target=reinterpret_cast<unsigned char*>(InSDKUtils::GetImageBase()+0x2647620);
    const unsigned char expected[]={0x48,0x89,0x54,0x24,0x10,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x83};
    bool ready=false;
    for (int i=0;i<120;++i) {
        unsigned char bytes[sizeof(expected)]{}; SIZE_T read=0;
        if(ReadProcessMemory(GetCurrentProcess(),target,bytes,sizeof(bytes),&read) && read==sizeof(bytes) && !std::memcmp(bytes,expected,sizeof(bytes))) { ready=true; break; }
        Sleep(1000);
    }
    if(!ready) { Log << "Unsupported or unready ProcessEvent signature; name helper disabled." << std::endl; return 4; }
    auto status=MH_Initialize();
    if(status!=MH_OK && status!=MH_ERROR_ALREADY_INITIALIZED) { Log << "Hook initialization failed." << std::endl; return 5; }
    if(MH_CreateHook(target,(void*)&Hook,(void**)&Next)!=MH_OK || MH_EnableHook(target)!=MH_OK) { Log << "Name hook installation failed." << std::endl; return 6; }
    Log << "Name helper ready for " << name << std::endl;
    return 0;
}
BOOL APIENTRY DllMain(HMODULE module,DWORD reason,void*) {
    if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);auto thread=CreateThread(nullptr,0,Install,module,0,nullptr);if(thread)CloseHandle(thread);}return TRUE;
}
