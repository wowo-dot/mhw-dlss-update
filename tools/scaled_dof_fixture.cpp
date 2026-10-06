#include "dof_fixture_support.hpp"
namespace {
using namespace dof_fixture;
struct Fixture : Gpu {
    ComPtr<ID3D12RootSignature> root;
    std::vector<std::byte> vs,ps;
    Fixture(bool warp,const std::filesystem::path &dir):Gpu(warp){
        D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,5,0,0,0};std::array<D3D12_ROOT_PARAMETER,3> params{};
        for(unsigned i=0;i<2;++i){params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;params[i].Descriptor.ShaderRegister=i?3:1;params[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
        params[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[2].DescriptorTable={1,&range};params[2].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        std::array<D3D12_STATIC_SAMPLER_DESC,2> samplers{};for(unsigned i=0;i<2;++i){auto &s=samplers[i];s.Filter=i?D3D12_FILTER_MIN_MAG_MIP_LINEAR:D3D12_FILTER_MIN_MAG_MIP_POINT;
            s.AddressU=s.AddressV=s.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;s.MaxLOD=0;s.ShaderRegister=i;s.ComparisonFunc=D3D12_COMPARISON_FUNC_NEVER;s.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
        D3D12_ROOT_SIGNATURE_DESC rd{3,params.data(),2,samplers.data(),D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};ComPtr<ID3DBlob> b,e;
        hr(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&b,&e),"serialize");hr(device->CreateRootSignature(0,b->GetBufferPointer(),b->GetBufferSize(),IID_PPV_ARGS(&root)),"root");
        vs=read(dir/L"730b7d23bcdca40477ed973c29c8fff8de7ab7244b05d142d5f9458fbfec5fdd.vs.dxbc");
        ps=read(dir/L"9f04832cb0a0ff800169964919f7ebecba353e7e3c147c2b314e76af7372e3aa.ps.dxbc");
    }
    // Reference runs the unmodified shader with expanded t2 storage and
    // independently transformed constants. No adapter is used in the oracle.
    std::vector<float> draw(sf::FilterGeometry g,unsigned scenario,unsigned mode,bool packed=false){
        const unsigned w=g.output_width,h=g.output_height,scale=w/g.render_width;const bool reference=mode==0,production=mode==3;
        hr(allocator->Reset(),"allocator reset");hr(list->Reset(allocator.Get(),nullptr),"list reset");
        D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=5;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ComPtr<ID3D12DescriptorHeap> heap,rtv;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)),"SRVs");hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=1;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv)),"RTV");
        std::array<ComPtr<ID3D12Resource>,5> inputs;std::vector<Pixels> uploads;uploads.reserve(5);
        for(unsigned t=0;t<5;++t){const unsigned tw=t==2?(reference?w*scale:w):t==3?8U:w/2,th=t==2?(reference?h*scale:h):t==3?6U:h/2;
            auto d=texture(tw,th,DXGI_FORMAT_R32G32B32A32_FLOAT);inputs[t]=resource(device.Get(),d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);uploads.emplace_back(device.Get(),d,D3D12_HEAP_TYPE_UPLOAD);
            std::vector<float> data(std::size_t(tw)*th*4,std::numeric_limits<float>::quiet_NaN());
            for(unsigned y=0;y<th;++y)for(unsigned x=0;x<tw;++x){if(t==2&&(x>=w||y>=h))continue;
                auto p=data.data()+(std::size_t(y)*tw+x)*4;
                for(unsigned k=0;k<4;++k)p[k]=.1F+.8F*float((x*7+y*13+k*5+t*3)%29)/28.F;
                if(t==0)p[0]=scenario==0?0.F:.2F;
                if(t==3)p[1]=scenario==0?0.F:scenario==1?2.F:.7F;
                if(t==4)p[0]=scenario==1?1.F:.4F;
            }
            uploads.back().write(std::as_bytes(std::span(data)));uploads.back().copy(list.Get(),inputs[t].Get(),true);barrier(list.Get(),inputs[t].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=d.Format;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sd.Texture2D.MipLevels=1;
            device->CreateShaderResourceView(inputs[t].Get(),&sd,{heap->GetCPUDescriptorHandleForHeapStart().ptr+SIZE_T(t)*device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)});
        }
        auto od=texture(w,h,packed?DXGI_FORMAT_R11G11B10_FLOAT:DXGI_FORMAT_R32G32B32A32_FLOAT);od.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;auto output=resource(device.Get(),od,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RENDER_TARGET);
        device->CreateRenderTargetView(output.Get(),nullptr,rtv->GetCPUDescriptorHandleForHeapStart());Pixels readback(device.Get(),od,D3D12_HEAP_TYPE_READBACK);
        std::array<float,128> constants{};const float ratio=reference?1.F/scale:1.F;
        constants[6]=ratio/w;constants[7]=ratio/h;constants[12]=scenario>=3?3.F/ratio:0;constants[13]=scenario>=3?2.F/ratio:0;
        constants[14]=float(g.render_width)/ratio;constants[15]=float(g.render_height)/ratio;constants[16]=ratio/g.render_width;constants[17]=ratio/g.render_height;constants[26]=std::bit_cast<float>(scenario&1U);
        auto blur=constants.data()+64;blur[2]=.8F;blur[9]=2.F;blur[15]=.15F;blur[16]=1.6F;blur[17]=std::bit_cast<float>(scenario>=2?1U:0U);blur[18]=.6F;blur[20]=.2F;blur[21]=.3F;blur[22]=.4F;
        auto cb=buffer(device.Get(),sizeof(constants),D3D12_HEAP_TYPE_UPLOAD);void *mapped{};D3D12_RANGE none{};hr(cb->Map(0,&none,&mapped),"CB map");std::memcpy(mapped,constants.data(),sizeof(constants));cb->Unmap(0,nullptr);
        const auto vertices=sf::dependency_vertices(g,false);auto vb=buffer(device.Get(),sizeof(vertices),D3D12_HEAP_TYPE_UPLOAD);hr(vb->Map(0,&none,&mapped),"VB map");std::memcpy(mapped,vertices.data(),sizeof(vertices));vb->Unmap(0,nullptr);
        const std::array<D3D12_INPUT_ELEMENT_DESC,4> layout{{{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,24,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}}};
        auto pixel=mode==1?sf::make_dof_shader(ps,g):ps;
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};d.pRootSignature=root.Get();d.VS={vs.data(),vs.size()};d.PS={pixel.data(),pixel.size()};d.InputLayout={layout.data(),4};d.SampleMask=UINT_MAX;d.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;d.NumRenderTargets=1;d.RTVFormats[0]=od.Format;d.SampleDesc.Count=1;
        d.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;d.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;d.RasterizerState.FrontCounterClockwise=TRUE;
        d.BlendState.IndependentBlendEnable=TRUE;for(auto &b:d.BlendState.RenderTarget){b.SrcBlend=b.SrcBlendAlpha=D3D12_BLEND_ONE;b.DestBlend=b.DestBlendAlpha=D3D12_BLEND_ZERO;b.BlendOp=b.BlendOpAlpha=D3D12_BLEND_OP_ADD;b.RenderTargetWriteMask=15;}
        d.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_NEVER;d.DepthStencilState.FrontFace=d.DepthStencilState.BackFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_NEVER};
        ComPtr<ID3D12PipelineState> pipeline;hr(device->CreateGraphicsPipelineState(&d,IID_PPV_ARGS(&pipeline)),"PSO");
        const D3D12_VIEWPORT view{0,0,float(production?g.render_width:w),float(production?g.render_height:h),0,1};const D3D12_RECT scissor{0,0,LONG(view.Width),LONG(view.Height)};
        const D3D12_VERTEX_BUFFER_VIEW vertex{vb->GetGPUVirtualAddress(),sizeof(vertices),28};const auto handle=rtv->GetCPUDescriptorHandleForHeapStart();ID3D12DescriptorHeap *heaps[]{heap.Get()};
        list->SetDescriptorHeaps(1,heaps);list->SetGraphicsRootSignature(root.Get());list->SetGraphicsRootConstantBufferView(0,cb->GetGPUVirtualAddress());list->SetGraphicsRootConstantBufferView(1,cb->GetGPUVirtualAddress()+256);list->SetGraphicsRootDescriptorTable(2,heap->GetGPUDescriptorHandleForHeapStart());
        list->OMSetRenderTargets(1,&handle,FALSE,nullptr);list->SetPipelineState(pipeline.Get());list->IASetVertexBuffers(0,1,&vertex);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);list->RSSetViewports(1,&view);list->RSSetScissorRects(1,&scissor);
        std::unique_ptr<sf::Effect> effect;
        if(production){sf::EffectRecipe recipe;recipe.stage=sf::EffectStage::dof_composite;recipe.geometry=g;recipe.root=root.Get();recipe.vertex_shader=vs;recipe.pixel_shader=ps;recipe.input_layout=layout;recipe.dependency_vertices=std::as_bytes(std::span(vertices));recipe.blend=d.BlendState;recipe.raster=d.RasterizerState;recipe.depth=d.DepthStencilState;
            effect=std::make_unique<sf::Effect>(device.Get(),recipe);sf::EffectBindings b{list.Get(),pipeline.Get(),root.Get(),output.Get(),vb.Get(),vertex,view,scissor,D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,4,0};require(effect->record(b),"production recording");}
        else list->DrawInstanced(4,1,0,0);
        barrier(list.Get(),output.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);readback.copy(list.Get(),output.Get(),false);hr(list->Close(),"close");ID3D12CommandList *lists[]{list.Get()};queue->ExecuteCommandLists(1,lists);wait();auto data=readback.read();
        hr(allocator->Reset(),"retirement allocator");hr(list->Reset(allocator.Get(),nullptr),"retirement reset");hr(list->Close(),"retirement close");if(effect)effect->retire(fence.Get(),sequence,true);messages();return data;
    }
};
double difference(const std::vector<float> &a,const std::vector<float> &b){require(a.size()==b.size(),"image sizes");double delta=0;for(unsigned i=0;i<a.size();++i){require(std::isfinite(a[i])&&std::isfinite(b[i]),"finite pixels");delta=(std::max)(delta,std::abs(double(a[i])-b[i]));}return delta;}
}
int wmain(int argc,wchar_t **argv){try{
    require(argc==4,"usage: warp|hardware shader-directory output-directory");const std::filesystem::path out=argv[3];require(!std::filesystem::exists(out),"preserve output");std::filesystem::create_directories(out);
    Fixture f(std::wstring_view(argv[1])==L"warp",argv[2]);unsigned cases=0,negative=0;double maximum=0;
    for(unsigned scale:{1U,2U})for(unsigned scenario=0;scenario<4;++scenario){sf::FilterGeometry g{96/scale,64/scale,96,64};
        const auto native=f.draw(g,scenario,0),adapted=f.draw(g,scenario,1);const auto delta=difference(native,adapted);std::cout<<"case "<<scale<<":"<<scenario<<" delta="<<delta<<std::endl;require(delta<0.00001,"native reference mismatch");maximum=(std::max)(maximum,delta);
        const auto packed=f.draw(g,scenario,0,true),production=f.draw(g,scenario,3,true);require(packed==production,"production packed pixels differ");
        if(scale==2&&scenario>0){const auto bad=f.draw(g,scenario,2);require(difference(native,bad)>.01,"negative control sensitivity");++negative;}++cases;
    }
    auto adapted=sf::make_dof_shader(f.ps,{48,32,96,64});reflection(f.ps,adapted);save(out/L"adapted.dxbc",adapted);disassemble(out/L"adapted.asm",adapted);
    auto bad=f.ps;bad.back()^=std::byte{1};rejects([&]{sf::make_dof_shader(bad,{48,32,96,64});});rejects([&]{sf::make_dof_shader(f.ps,{47,32,96,64});});
    std::ofstream report(out/L"result.json");report<<"{\"accepted\":true,\"cases\":"<<cases<<",\"negative_controls\":"<<negative<<",\"maximum_delta\":"<<maximum<<",\"production_packed_equal\":true,\"gpu_validation\":true,\"vendor\":"<<f.vendor<<"}";
    require(bool(report),"report");std::cout<<"PASS "<<cases<<" DOF oracle/production cases, "<<negative<<" negative controls, delta "<<maximum<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
