// Client-side zombie voices. Audio I/O and device operations run off the game
// thread; bounded object-array slices avoid a large periodic scan on that thread.
#include <mmsystem.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
namespace ZombieVoice {
static constexpr float MaxDistance=2000.f;
struct ActorRef { int index; UObject* object; };
static std::vector<ActorRef> candidates;
static UWorld* cachedWorld=nullptr;
static int scanCursor=-1;
static ULONGLONG nextScan=0,nextVoice=0;
static unsigned sequence=0;
static std::atomic<bool> audible{false},playing{false};
static std::atomic<int> playbackError{0};
static std::atomic<float> leftGain{0.f},rightGain{0.f};
static ActorRef source{-1,nullptr};
struct Request {std::filesystem::path path;};
static float Volume(float value){if(!std::isfinite(value))return 0.f;return (std::max)(0.f,(std::min)(1.f,value/100.f));}
static DWORD WINAPI PlayWorker(void* value){
 std::unique_ptr<Request> request(static_cast<Request*>(value));
 std::ifstream file(request->path,std::ios::binary|std::ios::ate);
 if(!file){playbackError.store(-1);playing.store(false);return 0;}
 const auto bytes=file.tellg();if(bytes<=0||bytes>2*1024*1024||bytes%2!=0){playbackError.store(-2);playing.store(false);return 0;}
 file.seekg(0);std::vector<int16_t> mono(static_cast<size_t>(bytes)/2);
 if(!file.read(reinterpret_cast<char*>(mono.data()),bytes)){playbackError.store(-3);playing.store(false);return 0;}
 if(!audible.load()){playing.store(false);return 0;}
 WAVEFORMATEX fmt{};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=2;fmt.nSamplesPerSec=22050;fmt.wBitsPerSample=16;fmt.nBlockAlign=4;fmt.nAvgBytesPerSec=fmt.nSamplesPerSec*fmt.nBlockAlign;
 HWAVEOUT output=nullptr;auto result=waveOutOpen(&output,WAVE_MAPPER,&fmt,0,0,CALLBACK_NULL);
 if(result!=MMSYSERR_NOERROR){playbackError.store(static_cast<int>(result));playing.store(false);return 0;}
 // A small queue lets each following chunk use the latest direction without
 // changing the system or device-wide volume of other audio streams.
 struct Chunk {WAVEHDR header{};std::vector<int16_t> stereo;bool prepared=false;};
 Chunk chunks[3];size_t cursor=0;bool failed=false;
 while(audible.load()){
  bool active=false;
  for(auto& chunk:chunks){
   if(chunk.prepared && (chunk.header.dwFlags&WHDR_DONE)){waveOutUnprepareHeader(output,&chunk.header,sizeof(chunk.header));chunk.prepared=false;}
   if(chunk.prepared){active=true;continue;}
   if(cursor>=mono.size())continue;
   const size_t frames=(std::min)(size_t(2205),mono.size()-cursor);chunk.stereo.resize(frames*2);
   const float left=leftGain.load(),right=rightGain.load();
   for(size_t i=0;i<frames;i++){chunk.stereo[i*2]=static_cast<int16_t>(mono[cursor+i]*left);chunk.stereo[i*2+1]=static_cast<int16_t>(mono[cursor+i]*right);}
   cursor+=frames;chunk.header={};chunk.header.lpData=reinterpret_cast<LPSTR>(chunk.stereo.data());chunk.header.dwBufferLength=static_cast<DWORD>(chunk.stereo.size()*sizeof(int16_t));
   if(waveOutPrepareHeader(output,&chunk.header,sizeof(chunk.header))!=MMSYSERR_NOERROR){failed=true;break;}
   chunk.prepared=true;if(waveOutWrite(output,&chunk.header,sizeof(chunk.header))!=MMSYSERR_NOERROR){failed=true;break;}active=true;
  }
  if(failed || (!active && cursor>=mono.size()))break;
  Sleep(10);
 }
 if(failed)playbackError.store(-4);
 waveOutReset(output);for(auto& chunk:chunks)if(chunk.prepared)waveOutUnprepareHeader(output,&chunk.header,sizeof(chunk.header));
 waveOutClose(output);
 playing.store(false);return 0;
}
static void Scan(UWorld* world,ULONGLONG now){
 auto* objects=UObject::GObjects.GetTypedPtr();if(!objects)return;
 if(cachedWorld!=world){cachedWorld=world;candidates.clear();scanCursor=-1;nextScan=0;}
 if(scanCursor<0){if(now<nextScan)return;scanCursor=objects->Num()-1;}
 // Recent spawned characters are at the end of the array. Pick them up first.
 for(int count=0;count<8192 && scanCursor>=0;count++,scanCursor--){
  if(scanCursor>=objects->Num())continue;auto* obj=objects->GetByIndex(scanCursor);
  if(!obj||obj->IsDefaultObject()||!obj->IsA(ATslCharacter::StaticClass()))continue;
  auto* outer=obj->GetOuter();bool belongs=false;for(int depth=0;outer&&depth<8;depth++,outer=outer->GetOuter())if(outer==world){belongs=true;break;}
  if(!belongs)continue;
  auto it=std::find_if(candidates.begin(),candidates.end(),[&](const ActorRef& ref){return ref.index==scanCursor;});
  if(it==candidates.end())candidates.push_back({scanCursor,obj});else it->object=obj;
 }
 if(scanCursor<0)nextScan=now+3000;
}
static void Tick(UWorld* world,ULONGLONG now){
 auto* state=world&&!world->AuthorityGameMode?UGameplayStatics::GetGameState(world):nullptr;
 auto* zombieState=state&&state->IsA(ATslZombieGameState::StaticClass())?static_cast<ATslZombieGameState*>(state):nullptr;
 auto* pc=zombieState?UGameplayStatics::GetPlayerController(world,0):nullptr;
 auto* pawn=pc?pc->K2_GetPawn():nullptr;
 auto* player=pawn&&pawn->IsA(ATslCharacter::StaticClass())?static_cast<ATslCharacter*>(pawn):nullptr;
 if(!player||player->Health<=0.f||player->GetZombieModePlayerState()!=EZombieModePlayerState::Human||zombieState->MatchState.ToString()!="InProgress") {audible.store(false);return;}
 if(UTslGameOption::IsMasterSoundMute()||UTslGameOption::IsEffectSoundMute()){audible.store(false);return;}
 const float master=Volume(UTslGameOption::GetMasterSoundVolume()),effect=Volume(UTslGameOption::GetEffectSoundVolume());
 if(master<=0.f||effect<=0.f){audible.store(false);return;}
 Scan(world,now);auto* objects=UObject::GObjects.GetTypedPtr();if(!objects){audible.store(false);return;}
 const FVector position=player->K2_GetActorLocation();FVector nearest{};float best=MaxDistance*MaxDistance;ActorRef nearestRef{-1,nullptr};
 const bool following=playing.load();
 for(auto it=candidates.begin();it!=candidates.end();){
  const auto ref=*it;if(ref.index<0||ref.index>=objects->Num()||objects->GetByIndex(ref.index)!=ref.object||!ref.object->IsA(ATslCharacter::StaticClass())){it=candidates.erase(it);continue;}++it;
  if(ref.object==player || (following && (ref.index!=source.index||ref.object!=source.object)))continue;
  auto* z=static_cast<ATslCharacter*>(ref.object);
  if(z->bActorIsBeingDestroyed||z->Health<=0.f||z->GetZombieModePlayerState()!=EZombieModePlayerState::Zombie)continue;
  const FVector p=z->K2_GetActorLocation();const float x=p.X-position.X,y=p.Y-position.Y,zDist=p.Z-position.Z;
  const float squared=x*x+y*y+zDist*zDist;if(std::isfinite(squared)&&squared<best){best=squared;nearest=p;nearestRef=ref;}
 }
 if(best>=MaxDistance*MaxDistance){audible.store(false);return;}
 audible.store(true);const int error=playbackError.exchange(0);if(error)Log<<"Zombie voice playback failed: "<<error<<std::endl;
 const float dist=std::sqrt(best),dx=nearest.X-position.X,dy=nearest.Y-position.Y;
 const float yaw=(pc->PlayerCameraManager?pc->PlayerCameraManager->GetCameraRotation().Yaw:player->K2_GetActorRotation().Yaw)*3.14159265358979323846f/180.f;
 const float pan=dist>1.f?(std::max)(-1.f,(std::min)(1.f,(-std::sin(yaw)*dx+std::cos(yaw)*dy)/dist)):0.f;
 const float gain=0.5f*master*effect*(1.f-dist/MaxDistance);
 leftGain.store(gain*(1.f-(std::max)(0.f,pan)));rightGain.store(gain*(1.f+(std::min)(0.f,pan)));
 if(following||now<nextVoice)return;
 source=nearestRef;auto* request=new Request{BinaryDir/L"ZombieVoice"/(L"voice"+std::to_wstring((sequence++%4)+1)+L".pcm")};
 playing.store(true);HANDLE thread=CreateThread(nullptr,0,PlayWorker,request,0,nullptr);
 if(thread)CloseHandle(thread);else{delete request;playing.store(false);}
 nextVoice=now+10000;
}
}
