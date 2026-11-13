// Item blueprints often refer to a base mesh plus a separate named variant.
// Only expose an alias when BOTH settings exist on this exact character model.
namespace StudioClothingVariants {
struct Variant {std::wstring group, option;};
static std::unordered_map<UCustomizableObjectInstance*,std::unordered_map<std::string,Variant>> cache;
static std::wstring Slot(const std::string& category){
 if(category=="Jackets")return L"Outer";if(category=="Tops")return L"Torso";if(category=="Pants")return L"Legs";
 if(category=="Footwear")return L"Feet";if(category=="Headwear")return L"Head";if(category=="Eyewear")return L"Eyes";
 if(category=="Gloves")return L"Hands";if(category=="Masks")return L"Mask";if(category=="Belts")return L"Belt";return L"";
}
static void Build(UCustomizableObjectInstance* instance,std::vector<ParamEntry>& entries){
 auto& variants=cache[instance];auto* object=instance->CustomizableObject;if(!object)return;
 auto* find=object->GetClass()->GetFunction("CustomizableObject","FindParameter");auto* count=object->GetClass()->GetFunction("CustomizableObject","GetIntParameterNumOptions");auto* get=object->GetClass()->GetFunction("CustomizableObject","GetIntParameterAvailableOption");
 if(!find||!count||!get)return;
 std::unordered_map<std::string,std::unordered_set<std::string>> groups;
 std::ifstream catalog("studio-weapon-catalog.tsv");std::string line;
 while(std::getline(catalog,line)){
  auto tab=line.find('\t');if(tab==std::string::npos)continue;auto tab2=line.find('\t',tab+1);if(tab2==std::string::npos)continue;
  auto slot=Slot(line.substr(0,tab));auto id=line.substr(tab+1,tab2-tab-1);if(slot.empty()||id.rfind("Item_",0)!=0||id.find("Ghillie")!=std::string::npos)continue;
  auto name=id.substr(5);auto suffix=name.rfind('_');if(suffix==std::string::npos||suffix+1==name.size()||!std::all_of(name.begin()+suffix+1,name.end(),[](unsigned char c){return std::isdigit(c)!=0;}))continue;
  std::string base=name.substr(0,suffix),group=base+"_Group";ParamEntry* entry=nullptr;
  for(auto& e:entries)if(e.name==slot){entry=&e;break;}if(!entry||entry->options.count(id))continue;
  auto original=entry->options.find("Item_"+base);if(original==entry->options.end())continue;
  auto found=groups.find(group);if(found==groups.end()){
   auto& options=groups[group];auto wgroup=Wide(group);Params::CustomizableObject_FindParameter f{};f.Name_0=FString(wgroup.c_str());object->ProcessEvent(find,&f);
   if(f.ReturnValue>=0){Params::CustomizableObject_GetIntParameterNumOptions n{};n.ParamIndex=f.ReturnValue;object->ProcessEvent(count,&n);for(int i=0;i<n.ReturnValue&&i<5000;i++){Params::CustomizableObject_GetIntParameterAvailableOption p{};p.ParamIndex=f.ReturnValue;p.K=i;object->ProcessEvent(get,&p);options.insert(p.ReturnValue.ToString());}}
   found=groups.find(group);
  }
  if(!found->second.count(name))continue;
  auto model=original->second;entry->options.emplace(id,model);variants.emplace(id,Variant{Wide(group),Wide(name)});
 }
 // Some original items use the exact variant name in a differently named
 // group (e.g. streamer skins). Index those groups from the live model and
 // only accept a single matching group and base model.
 struct GroupOption{std::string group,option,base;};
 auto lower=[](std::string v){std::transform(v.begin(),v.end(),v.begin(),[](unsigned char c){return (char)std::tolower(c);});return v;};
 std::unordered_map<std::string,std::vector<GroupOption>> byOption;
 auto* countParams=object->GetClass()->GetFunction("CustomizableObject","GetParameterCount");auto* paramName=object->GetClass()->GetFunction("CustomizableObject","GetParameterName");auto* paramType=object->GetClass()->GetFunction("CustomizableObject","GetParameterType");
 if(countParams&&paramName&&paramType){Params::CustomizableObject_GetParameterCount total{};object->ProcessEvent(countParams,&total);
  for(int i=0;i<total.ReturnValue&&i<500;i++){Params::CustomizableObject_GetParameterType t{};t.ParamIndex=i;object->ProcessEvent(paramType,&t);if(t.ReturnValue!=EMutableParameterType::Int)continue;
   Params::CustomizableObject_GetParameterName n{};n.ParamIndex=i;object->ProcessEvent(paramName,&n);auto group=n.ReturnValue.ToString();if(group.size()<7||lower(group.substr(group.size()-6))!="_group")continue;
   auto base=group.substr(0,group.size()-6);Params::CustomizableObject_GetIntParameterNumOptions c{};c.ParamIndex=i;object->ProcessEvent(count,&c);
   for(int j=0;j<c.ReturnValue&&j<5000;j++){Params::CustomizableObject_GetIntParameterAvailableOption o{};o.ParamIndex=i;o.K=j;object->ProcessEvent(get,&o);auto option=o.ReturnValue.ToString();byOption[lower(option)].push_back({group,option,base});}
  }
 }
 size_t additional=0;std::ifstream extraCatalog("studio-weapon-catalog.tsv");
 while(std::getline(extraCatalog,line)){
  auto tab=line.find('\t');if(tab==std::string::npos)continue;auto tab2=line.find('\t',tab+1);if(tab2==std::string::npos)continue;
  auto slot=Slot(line.substr(0,tab));auto id=line.substr(tab+1,tab2-tab-1);if(slot.empty()||id.rfind("Item_",0)!=0||lower(id).find("ghillie")!=std::string::npos)continue;
  ParamEntry* entry=nullptr;for(auto& e:entries)if(e.name==slot){entry=&e;break;}if(!entry||entry->options.count(id))continue;
  std::wstring direct;int directCount=0;for(const auto& opt:entry->options)if(lower(opt.first)==lower(id)){if(!directCount||direct!=opt.second){direct=opt.second;directCount++;}}
  if(directCount==1){entry->options.emplace(id,direct);additional++;continue;}
  // This model option is intentionally gender-neutral in the game asset.
  if(id=="Item_Head_DM_01"&&slot==L"Head"){
   Params::CustomizableObject_FindParameter f{};f.Name_0=FString(L"Head");object->ProcessEvent(find,&f);
   if(f.ReturnValue>=0){Params::CustomizableObject_GetIntParameterNumOptions n{};n.ParamIndex=f.ReturnValue;object->ProcessEvent(count,&n);for(int i=0;i<n.ReturnValue&&i<5000;i++){Params::CustomizableObject_GetIntParameterAvailableOption o{};o.ParamIndex=f.ReturnValue;o.K=i;object->ProcessEvent(get,&o);if(o.ReturnValue.ToString()=="Head_DM_01"){entry->options.emplace(id,L"Head_DM_01");additional++;break;}}}
   if(entry->options.count(id))continue;
  }
  auto found=byOption.find(lower(id.substr(5)));if(found==byOption.end())continue;
  int matches=0;std::wstring matchedModel;Variant matchedVariant;
  for(const auto& candidate:found->second){auto base=lower(candidate.base),baseWithoutIndex=base;if(base.size()>3&&base.substr(base.size()-3)=="_01")baseWithoutIndex=base.substr(0,base.size()-3);
   for(const auto& opt:entry->options){std::string model(opt.second.begin(),opt.second.end());if(model.size()>2&&(model[0]=='F'||model[0]=='M')&&model[1]=='_')model=model.substr(2);model=lower(model);if(model==base||model==baseWithoutIndex){
     if(matches==0||matchedModel!=opt.second||matchedVariant.group!=Wide(candidate.group)||matchedVariant.option!=Wide(candidate.option)){matches++;matchedModel=opt.second;matchedVariant={Wide(candidate.group),Wide(candidate.option)};}
   }}
  }
  if(matches==1){entry->options.emplace(id,matchedModel);variants.emplace(id,matchedVariant);additional++;}
 }
 // Explicitly researched mappings whose variant names include an extra
 // model-family component. Both base and variant are still checked live.
 struct KnownAlias{const char* id;const wchar_t* slot;const char* model;const char* group;const char* option;};
 static const KnownAlias aliases[]={
  {"Item_Feet_Sacriel_01",L"Feet","Feet_OU_TBR","Feet_OU_TBR_Group","Feet_OU_Sacriel_01"},
  {"Item_Hands_Ashek_01",L"Hands","Hands_Q_TBR","Hands_Q_TBR_Group","Hands_Q_Ashek_01"},
  {"Item_Jacket_Edberg_01",L"Outer","Jacket_M_TBR","Jacket_M_TBR_Group","Jacket_M_Edberg_01"},
  {"Item_Jacket_Sacriel_01",L"Outer","Jacket_O_TBR","Jacket_O_TBR_Group","Jacket_O_Sacriel_01"},
  {"Item_Mask_Just9n_01",L"Mask","Mask_Biker_TBR","Mask_Biker_TBR_Group","Mask_Biker_Just9n_01"},
  {"Item_Mask_Sacriel_01",L"Mask","Mask_Biker_TBR","Mask_Biker_TBR_Group","Mask_Biker_Sacriel_01"},
  {"Item_Legs_fuffenz_01",L"Legs","Legs_L_TBR","Legs_L_TBR_Group","Legs_L_fuffenz_01"},
  {"Item_Jacket_NPL_01",L"Outer","Jacket_Esports_01","Jacket_Esports_Group","Jacket_NPL_01"},
  {"Item_Jacket_PCL_01",L"Outer","Jacket_Esports_01","Jacket_Esports_Group","Jacket_PCL_01"},
  {"Item_Jacket_PEL_01",L"Outer","Jacket_Esports_01","Jacket_Esports_Group","Jacket_PEL_01"},
  {"Item_Jacket_PJS_01",L"Outer","Jacket_Esports_01","Jacket_Esports_Group","Jacket_PJS_01"},
  {"Item_Jacket_PKL_01",L"Outer","Jacket_Esports_01","Jacket_Esports_Group","Jacket_PKL_01"},
  {"Item_Jacket_PML_01",L"Outer","Jacket_Esports_01","Jacket_Esports_Group","Jacket_PML_01"},
  {"Item_Body_Half_01",L"Torso","Body_half_A","Body_Half_Group","Body_half_01"},
  {"Item_Body_Half_02",L"Torso","Body_half_A","Body_Half_Group","Body_half_02"},
  {"Item_Body_Half_03",L"Torso","Body_half_A","Body_Half_Group","Body_half_03"},
  {"Item_Body_Half_04",L"Torso","Body_half_A","Body_Half_Group","Body_half_04"},
  {"Item_Body_Half_A_05",L"Torso","Body_half_A","Body_Half_Group","Body_half_05"},
  {"Item_Body_Half_A_06",L"Torso","Body_half_A","Body_Half_Group","Body_half_06"},
  {"Item_Body_Half_A_07",L"Torso","Body_half_A","Body_Half_Group","Body_half_07"},
  {"Item_Body_Half_08",L"Torso","Body_half_A","Body_Half_Group","Body_half_08"},
  {"Item_Body_Half_09",L"Torso","Body_half_A","Body_Half_Group","Body_half_09"},
  {"Item_Body_Half_11",L"Torso","Body_half_B","Body_Half_Group_B","Body_Half_11"},
  {"Item_Body_Half_12",L"Torso","Body_half_B","Body_Half_Group_B","Body_Half_12"},
  {"Item_Body_D_Dahmien7_01",L"Torso","Body_TBR_D","Body_TBR_D_01_Group","Body_Dahmien7_01"},
  {"Item_Body_D_NeoticaCH_01",L"Torso","Body_TBR_D","Body_TBR_D_01_Group","Body_NeoticaCH_01"},
  {"Item_Body_D_THZ_TV_01",L"Torso","Body_TBR_D","Body_TBR_D_01_Group","Body_thz_tv_01"},
  {"Item_Body_D_edb_potato_01",L"Torso","Body_TBR_D","Body_TBR_D_01_Group","Body_edb_potato_01"},
  {"Item_Body_TBR_D_CigaretteSTV_01",L"Torso","Body_TBR_D","Body_TBR_D_01_Group","Body_CigaretteSTV_01"},
  {"Item_Body_TBR_D_ewanng_01",L"Torso","Body_TBR_D","Body_TBR_D_01_Group","Body_ewanng_01"},
  {"Item_Body_TBR_fuffenz_01",L"Torso","Body_TBR_D","Body_TBR_D_01_Group","Body_fuffenz_01"},
  {"Item_Jacket_TG_01",L"Outer","Jacket_TG","Jakcet_TG_Group","Jacket_TG_01"},
  {"Item_Legs_FL_01",L"Legs","Legs_FL","Leg_FL_Group","Leg_FL_01"},
  {"Item_Legs_Leggings_01",L"Legs","Legs_Leggings","Leg_Leggings_Group","Leg_Leggings_01"},
  {"Item_Legs_Leggings_02",L"Legs","Legs_Leggings","Leg_Leggings_Group","Leg_Leggings_02"},
  {"Item_Legs_Leggings_04",L"Legs","Legs_Leggings","Leg_Leggings_Group","Leg_Leggings_04"},
  {"Item_Body_Top_02",L"Torso","Body_Top_A","Body_Top_Group","Body_Top_02"},
 };
 auto addKnown=[&](const std::string& id,const std::wstring& slot,const std::string& model,const std::string& group,const std::string& option){
  ParamEntry* entry=nullptr;for(auto& e:entries)if(e.name==slot){entry=&e;break;}if(!entry||entry->options.count(id))return;
  auto base=entry->options.find("Item_"+model);if(base==entry->options.end())return;
  auto wgroup=Wide(group);Params::CustomizableObject_FindParameter f{};f.Name_0=FString(wgroup.c_str());object->ProcessEvent(find,&f);if(f.ReturnValue<0)return;
  Params::CustomizableObject_GetIntParameterNumOptions n{};n.ParamIndex=f.ReturnValue;object->ProcessEvent(count,&n);bool valid=false;
  for(int i=0;i<n.ReturnValue&&i<5000;i++){Params::CustomizableObject_GetIntParameterAvailableOption o{};o.ParamIndex=f.ReturnValue;o.K=i;object->ProcessEvent(get,&o);if(o.ReturnValue.ToString()==option){valid=true;break;}}
  if(!valid)return;auto value=base->second;entry->options.emplace(id,value);variants.emplace(id,Variant{wgroup,Wide(option)});additional++;
 };
 std::unordered_set<std::string> allowlisted;std::ifstream allowedFile("studio-weapon-catalog.tsv");while(std::getline(allowedFile,line)){auto first=line.find('\t'),second=first==std::string::npos?std::string::npos:line.find('\t',first+1);if(second!=std::string::npos)allowlisted.insert(line.substr(first+1,second-first-1));}
 for(const auto& a:aliases)if(allowlisted.count(a.id))addKnown(a.id,a.slot,a.model,a.group,a.option);
 // The JennaJulien trousers use singular "Leg" in the model parameter.
 if(allowlisted.count("Item_Legs_BR06_02_JennaJulien_01"))for(auto& e:entries)if(e.name==L"Legs"&&!e.options.count("Item_Legs_BR06_02_JennaJulien_01")){auto i=e.options.find("Item_Leg_BR06_02_JennaJulien_01");if(i!=e.options.end()){auto model=i->second;e.options.emplace("Item_Legs_BR06_02_JennaJulien_01",model);additional++;}break;}
 if(allowlisted.count("Item_Feet_P4wnyhof_01"))for(auto& e:entries)if(e.name==L"Feet"&&!e.options.count("Item_Feet_P4wnyhof_01")){auto i=e.options.find("Item_Feet_BR02_P4wnyhof_01");if(i!=e.options.end()){auto model=i->second;e.options.emplace("Item_Feet_P4wnyhof_01",model);additional++;}break;}
 for(int i=1;i<=20;i++){
  auto number=std::to_string(i);auto id="Item_Jacket_Hoodie_"+(i<10?std::string("0"):std::string())+number;if(!allowlisted.count(id))continue;
  auto letter=i<=10?"A":"B";auto variant="Hoodie_Team_"+(i<10?std::string("00"):std::string("0"))+number;
  addKnown(id,L"Outer",std::string("Jacket_Hoodie_")+letter,std::string("Jacket_Hoodie_Group_")+letter,variant);
 }
 out<<"STUDIO CLOTHING VARIANTS "<<variants.size()<<" additional="<<additional<<std::endl;
}
static void Apply(UCustomizableObjectInstance* instance,const std::string& selection){
 auto found=cache.find(instance);if(found==cache.end())return;
 std::istringstream in(selection);std::string id;
 while(std::getline(in,id,',')){auto variant=found->second.find(id);if(variant!=found->second.end()){
  bool ok=SetClothing(instance,variant->second.group.c_str(),variant->second.option.c_str());out<<"STUDIO CLOTHING VARIANT "<<id<<" applied="<<ok<<std::endl;
 }}
}
static void Clear(){cache.clear();}
}
