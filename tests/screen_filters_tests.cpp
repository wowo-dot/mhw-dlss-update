// The approved screen-filter table (screen_filters.cpp) is well formed: exact
// SHA-256 identities, census binding words within the route's limits, and a
// tBaseMap (global ID 193) SRV at the scene register.
#include "../src/scaled_temporal/screen_filters.hpp"
#include <cstdio>
#include <set>
#include <string>

namespace {
int failures{};
void check(bool ok,const std::string &what){if(!ok){std::printf("FAIL: %s\n",what.c_str());++failures;}}
bool hex64(std::string_view s){if(s.size()!=64)return false;for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
}
int main(){
    namespace sf=mhw::scaled_temporal;std::set<std::string_view> seen;
    for(const auto &f:sf::screen_filters()){
        const std::string n(f.technique);
        check(hex64(f.ps_sha256)&&hex64(f.vs_sha256)&&f.ps_size>=52&&f.vs_size>=52,n+": identities");
        check(seen.insert(f.ps_sha256).second,n+": unique pixel shader");
        check(f.word_count&&f.word_count<=f.words.size(),n+": word count");
        unsigned scene_srvs{};
        for(unsigned i=0;i<f.word_count;++i){const auto w=f.words[i],kind=w&15,owner=(w>>4)&255,id=(w>>12)&4095,reg=w>>24;
            check(kind<=2&&owner<f.word_count,n+": word kind/owner");
            if(kind==2){check(reg<5,n+": SRV register within the route's five");if(reg==f.scene_register){check(id==193,n+": scene is tBaseMap");++scene_srvs;}}}
        check(scene_srvs==1,n+": one scene SRV");
        check((sf::screen_filter_textures(f)>>f.scene_register)&1,n+": scene in the texture mask");
        check(sf::screen_filter(f.ps_sha256)==&f,n+": lookup");
        check(f.position_reads+f.scene_samples+f.scene_loads>0,n+": generator edits recorded");
    }
    check(sf::screen_filters().size()==6,"six approved filters");
    check(!sf::screen_filter(std::string_view("0000000000000000000000000000000000000000000000000000000000000000")),"unknown hash");
    if(failures)return 1;std::printf("screen filter table tests passed\n");return 0;
}
