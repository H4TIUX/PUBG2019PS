// Native 2019 skydive components replace legacy simulated aircraft landings.
static void UpdateNativeSkydive(UWorld* World)
{
    struct Record { ATslCharacter* Character=nullptr; bool WasInPlane=false; bool Started=false; ULONGLONG OpenAfter=0; };
    static UWorld* LastWorld=nullptr;
    static std::unordered_map<int32,Record> Records;
    static ULONGLONG NextCheck=0;
    if (!Bots::IsGameThread()) return;
    if (World!=LastWorld) { LastWorld=World;Records.clear();NextCheck=0; }
    if (!World || !GFullTerrainReady || GetTickCount64()<NextCheck || !World->AuthorityGameMode ||
        !World->AuthorityGameMode->IsA(ATslGameMode::StaticClass())) return;
    NextCheck=GetTickCount64()+100;
    auto* GM=static_cast<ATslGameMode*>(World->AuthorityGameMode);
    if (GM->MatchStartType!=EMatchStartType::Airborne) return;
    const auto State=GM->MatchState.ToString();
    if (State!="WaitingToStart" && State!="InProgress") return;
    GM->bUseParachuteAutoDeploy=true;
    TArray<APawn*> Pawns;
    GM->GetAllPawns(&Pawns);
    for(auto* Pawn:Pawns)
    {
        if(!Pawn || !Pawn->IsA(ATslCharacter::StaticClass())) continue;
        auto* C=static_cast<ATslCharacter*>(Pawn);
        if(C->Health<=0.f) continue;
        auto& Entry=Records[C->GetIndex()];
        if(Entry.Character!=C) Entry=Record{C};
        auto* Sky=C->SkydivingComponent;
        if(!Sky)
        {
            if(C->SkydivingComponentClass.AssetLongPathname.ToString().empty()) continue;
            C->CreateSkydiveComponent();
            Sky=C->SkydivingComponent;
            if(!Sky) continue;
            C->InitilizeServerSkydiveComp();
            C->ForceNetUpdate();
            CUSTOMLOG("[PUBG2019PS] Native skydive prepared: "+C->GetName());
        }
        Sky->ParachuteDeploySettings.bUseAutoDeploy=true;
        const bool InPlane=C->IsInAircraft();
        if(InPlane) { Entry.WasInPlane=true;Entry.Started=false;continue; }
        if(Entry.WasInPlane && !Entry.Started && State=="InProgress")
        {
            Entry.Started=true;
            Entry.OpenAfter=GetTickCount64()+20000;
            if(Sky->SkydiveState==ESkydiveState::Skydive_None)
                Sky->Server_SetSkydiveState(ESkydiveState::Skydive_Freefall);
            CUSTOMLOG("[PUBG2019PS] Native skydive started: "+C->GetName()+" state="+std::to_string((int)Sky->SkydiveState));
        }
        if(Entry.Started && Bots::IsBotPawn(C) && Sky->SkydiveState==ESkydiveState::Skydive_Freefall && GetTickCount64()>=Entry.OpenAfter)
        {
            Entry.OpenAfter=GetTickCount64()+2000;
            Sky->Server_RequestParachuteOpen();
            CUSTOMLOG("[PUBG2019PS] Native bot parachute request: "+C->GetName()+" state="+std::to_string((int)Sky->SkydiveState));
        }
    }
}
