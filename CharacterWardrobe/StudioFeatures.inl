// Only character customization parameters exposed by the active model.
static bool FeatureGroup(const std::wstring& name){
 return name==L"Hair"||name==L"Face"||name==L"NudeBody"||name==L"Makeup"||name==L"SkinColor"||name==L"BodyType"||name==L"BodyTattoos";
}
static bool FeatureId(const std::string& id){
 if(id.rfind("Item_",0)!=0||id.size()>100)return false;
 std::string lower=id;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});
 if(lower.find("zombie")!=std::string::npos||lower.find("ghillie")!=std::string::npos)return false;
 for(unsigned char c:id)if(!std::isalnum(c)&&c!='_')return false;
 return true;
}
static std::unordered_map<std::wstring,std::string> ReadStudioFeatures(){
 std::unordered_map<std::wstring,std::string> result;std::ifstream input("studio-features.txt");std::string line;
 while(std::getline(input,line)){
  if(!line.empty()&&line.back()=='\r')line.pop_back();auto tab=line.find('\t');if(tab==std::string::npos)continue;
  auto group=Wide(line.substr(0,tab));auto id=line.substr(tab+1);
  if(FeatureGroup(group)&&FeatureId(id))result[group]=id;
 }
 return result;
}
static void WriteStudioFeatureCatalog(const std::vector<ParamEntry>& entries,bool female){
 std::ofstream file("studio-features-supported.tmp",std::ios::trunc);
 if(!file)return;file<<"#gender="<<(female?"female":"male")<<"\n";
 for(const auto& entry:entries){
  if(!FeatureGroup(entry.name))continue;
  std::string group(entry.name.begin(),entry.name.end());
  for(const auto& option:entry.options)if(FeatureId(option.first))file<<group<<"\t"<<option.first<<"\n";
 }
 file.close();MoveFileExW(L"studio-features-supported.tmp",L"studio-features-supported.tsv",MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
