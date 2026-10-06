// Included inside provenance's private namespace: bounded, read-only API inputs.
// Resources are retained until all hook calls drain, never across another run.
constexpr unsigned copy_capacity=512,copy_resource_capacity=128;
constexpr unsigned copy_pin_capacity=copy_capacity*2;
struct CopyResourcePin {
  std::uintptr_t object{};
  unsigned ready{};
  std::uintptr_t identity{},device{},pin{};
  std::uint64_t first{};
  D3D12_RESOURCE_DESC desc{};
};
// Location words: resource, type (2 = whole resource), subresource/offset,
// footprint format, width, height, depth, row pitch. Unused words stay zero.
struct CopyRecord {
  std::array<std::uint64_t,8> source{},destination{};
  std::array<unsigned,3> xyz{};
  std::array<unsigned,6> box{};
  bool has_box{};
};
std::array<CopyRecord,copy_capacity> copies;
// One writer per event slot: independent COM queries and references, with no
// shared initialization, spinning, or locks across renderer threads.
std::array<CopyResourcePin,copy_pin_capacity> copy_pins;
// Built only by the worker after the hook admission count reaches zero.
std::array<CopyResourcePin,copy_resource_capacity> copy_resources;
unsigned copy_resource_count{};
std::atomic<unsigned> copy_refs_acquired{};
unsigned copy_refs_released{};
std::atomic<unsigned> copy_count{};
#ifdef MHW_BINDING_FIXTURE
std::atomic<bool> pause_first_copy_pin{},first_copy_pin_waiting{},resume_first_copy_pin{};
#endif
static_assert(sizeof(D3D12_TEXTURE_COPY_LOCATION)==48 && offsetof(D3D12_TEXTURE_COPY_LOCATION,PlacedFootprint)==16);
static_assert(sizeof(D3D12_BOX)==24 && sizeof(D3D12_RESOURCE_DESC)==56);
bool copy_pin(unsigned index,std::uintptr_t object,std::uint64_t first) noexcept {
  if(!object)return false;
  auto entry=&copy_pins[index];entry->object=object;entry->first=first;
#ifdef MHW_BINDING_FIXTURE
  if(index==0 && pause_first_copy_pin.load()) {
    first_copy_pin_waiting.store(true,std::memory_order_release);
    const auto deadline=GetTickCount64()+5000;
    while(!resume_first_copy_pin.load(std::memory_order_acquire) && GetTickCount64()<deadline)SwitchToThread();
    if(!resume_first_copy_pin.load()){fail(Error::deadline);return false;}
  }
#endif
  ID3D12Resource *held{};IUnknown *identity{};ID3D12Device *device{};bool ok=false;
  if(methods(object,{0,2,7,10})) {
    __try {
      auto p=reinterpret_cast<ID3D12Resource *>(object);
      if(SUCCEEDED(p->QueryInterface(IID_PPV_ARGS(&held))) && reinterpret_cast<std::uintptr_t>(held)==object &&
         SUCCEEDED(held->GetDevice(IID_PPV_ARGS(&device))) && reinterpret_cast<std::uintptr_t>(device)==cfg.device &&
         SUCCEEDED(held->QueryInterface(IID_PPV_ARGS(&identity))) && identity) {
        entry->desc=held->GetDesc();entry->identity=reinterpret_cast<std::uintptr_t>(identity);entry->device=cfg.device;
        const auto &d=entry->desc;
        ok=d.Dimension>=D3D12_RESOURCE_DIMENSION_BUFFER && d.Dimension<=D3D12_RESOURCE_DIMENSION_TEXTURE3D &&
          d.Width && d.Height && d.DepthOrArraySize && d.MipLevels && d.SampleDesc.Count;
      }
    } __except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
  }
  const bool a=release(identity),b=release(device);ok=ok && a && b;
  if(!ok){release(held);entry->ready=2;return false;}
  entry->pin=reinterpret_cast<std::uintptr_t>(held);entry->ready=1;
  copy_refs_acquired.fetch_add(1,std::memory_order_relaxed);return true;
}
bool same_copy_description(const D3D12_RESOURCE_DESC &a,const D3D12_RESOURCE_DESC &b) noexcept {
  // Compare fields, never ABI padding.
  return a.Dimension==b.Dimension && a.Alignment==b.Alignment && a.Width==b.Width && a.Height==b.Height &&
    a.DepthOrArraySize==b.DepthOrArraySize && a.MipLevels==b.MipLevels && a.Format==b.Format &&
    a.SampleDesc.Count==b.SampleDesc.Count && a.SampleDesc.Quality==b.SampleDesc.Quality && a.Layout==b.Layout && a.Flags==b.Flags;
}
bool merge_copy_resources() noexcept {
  bool ok=true;
  for(const auto &pin:copy_pins)if(pin.ready==1) {
    unsigned index{};
    while(index<copy_resource_count && copy_resources[index].object!=pin.object)++index;
    if(index==copy_resource_count) {
      if(index==copy_resource_capacity){fail(Error::capacity);ok=false;continue;}
      for(unsigned j=0;j<index;++j)if(copy_resources[j].identity==pin.identity){fail(Error::identity);ok=false;}
      copy_resources[index]=pin;copy_resources[index].pin=0;++copy_resource_count;
    } else {
      auto &prior=copy_resources[index];
      if(prior.identity!=pin.identity || prior.device!=pin.device || !same_copy_description(prior.desc,pin.desc)) {
        fail(Error::changed);ok=false;
      }
      prior.first=(std::min)(prior.first,pin.first);
    }
  }
  return ok;
}
bool release_copy_resources() noexcept {
  bool ok=true;
  for(auto &p:copy_pins)if(p.pin) {
    if(release(reinterpret_cast<IUnknown *>(p.pin)))++copy_refs_released;
    else {fail(Error::exception);ok=false;}
    p.pin=0;
  }
  return ok && copy_refs_acquired.load()==copy_refs_released;
}
bool copy_location(std::uintptr_t pointer,std::array<std::uint64_t,8> &out) noexcept {
  D3D12_TEXTURE_COPY_LOCATION loc{};
  if(!copy_memory(pointer,&loc,sizeof(loc)))return false;
  out[0]=reinterpret_cast<std::uintptr_t>(loc.pResource);out[1]=loc.Type;
  if(loc.Type==D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX)out[2]=loc.SubresourceIndex;
  else if(loc.Type==D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT) {
    const auto &p=loc.PlacedFootprint;out[2]=p.Offset;out[3]=p.Footprint.Format;
    out[4]=p.Footprint.Width;out[5]=p.Footprint.Height;out[6]=p.Footprint.Depth;out[7]=p.Footprint.RowPitch;
  } else return false;
  return out[0]!=0;
}
bool capture_copy(Event &e,const Frame &f) noexcept {
  e.first=copy_count.fetch_add(1);e.count=1;
  if(e.first>=copy_capacity){fail(Error::capacity);return false;}
  auto &c=copies[e.first];
  if(e.kind==copy_resource) {
    c.destination[0]=f.gpr[2];c.source[0]=f.gpr[3];c.destination[1]=c.source[1]=2;
  } else {
    if(!copy_location(f.gpr[2],c.destination) || !copy_location(f.stack[1],c.source))return false;
    c.xyz={static_cast<unsigned>(f.gpr[3]),static_cast<unsigned>(f.gpr[4]),static_cast<unsigned>(f.stack[0])};
    c.has_box=f.stack[2]!=0;
    if(c.has_box && !copy_memory(f.stack[2],c.box.data(),sizeof(D3D12_BOX)))return false;
  }
  e.resource=c.source[0];e.backing=c.destination[0];
  return copy_pin(e.first*2,e.resource,e.enter) && copy_pin(e.first*2+1,e.backing,e.enter);
}
void copies_json(std::ostream &o) {
  o<<",\"copy_refs_acquired\":"<<copy_refs_acquired.load()<<",\"copy_refs_released\":"<<copy_refs_released
   <<",\"copies_reserved\":"<<copy_count.load()<<",\"copies\":[";
  const auto words=[&o](const auto &a){o<<'[';for(unsigned i=0;i<a.size();++i){if(i)o<<',';o<<a[i];}o<<']';};
  for(unsigned i=0;i<(std::min)(copy_count.load(),copy_capacity);++i) {
    if(i)o<<',';const auto &c=copies[i];o<<"{\"source\":";words(c.source);o<<",\"destination\":";words(c.destination);
    o<<",\"xyz\":";words(c.xyz);o<<",\"has_box\":"<<c.has_box<<",\"box\":";words(c.box);o<<'}';
  }
  o<<"],\"copy_resources\":[";bool comma=false;
  for(const auto &p:copy_resources)if(p.object) {
    if(comma)o<<',';comma=true;const auto &d=p.desc;
    o<<"{\"object\":"<<p.object<<",\"identity\":"<<p.identity<<",\"device\":"<<p.device
     <<",\"first\":"<<p.first<<",\"ready\":"<<p.ready<<",\"pin\":"<<p.pin
     <<",\"desc\":["<<d.Dimension<<','<<d.Alignment<<','<<d.Width<<','<<d.Height<<','<<d.DepthOrArraySize<<','<<d.MipLevels
     <<','<<d.Format<<','<<d.SampleDesc.Count<<','<<d.SampleDesc.Quality<<','<<d.Layout<<','<<d.Flags<<"]}";
  }
  o<<']';
}
