#include "../src/temporal_present/bindings.hpp"
#include <iostream>
#include <stdexcept>
using namespace mhw::temporal_present;
void check(bool v) { if(!v)throw std::runtime_error("binding-state contract"); }
int main() {
    try {
        Bindings s; check(!s.ready());
        s.reset(1,20); s.root(false,30); s.root(true,40);
        const std::uintptr_t heaps[]{50,60}; s.descriptor_heaps(heaps);
        s.descriptor(false,0,ArgumentKind::table,100); s.descriptor(true,0,ArgumentKind::table,200);
        s.descriptor(false,3,ArgumentKind::cbv,256); s.descriptor(false,4,ArgumentKind::srv,512);
        s.descriptor(false,5,ArgumentKind::uav,768);
        const std::uint32_t first[]{1,2}, last[]{3,4}, replace[]{9};
        s.constants(false,1,0,first); s.constants(false,1,2,last); s.constants(false,1,1,replace);
        check(s.ready() && s.compute.arguments[1].mask==15 && s.compute.arguments[1].values[1]==9 && s.compute.arguments[1].values[3]==4);
        s.root(false,30); check(s.compute.arguments[0].address==100); // Redundant signature preserves arguments.
        const std::uintptr_t reverse[]{60,50}; s.descriptor_heaps(reverse);
        check(s.compute.arguments[0].address==100 && s.graphics.arguments[0].address==200);
        const std::uintptr_t different[]{50,70}; s.descriptor_heaps(different);
        check(s.compute.arguments[0].kind==ArgumentKind::unset && s.graphics.arguments[0].kind==ArgumentKind::unset);
        check(s.compute.arguments[1].mask==15 && s.compute.arguments[3].address==256);
        s.root(false,31); check(s.compute.arguments[1].kind==ArgumentKind::unset);
        s.descriptor(true,2,ArgumentKind::cbv,1024); s.root(false,30);
        check(s.graphics.arguments[2].address==1024);
        s.reset(2); check(!s.ready() && !s.compute.signature && !s.graphics.signature && !s.heap_count);
        s.pipeline=20; s.root(false,30); check(s.ready());
        s.constants(false,63,63,replace); check(s.compute.arguments[63].mask==(std::uint64_t(1)<<63));
        auto bad=s; bad.constants(false,63,64,replace); check(!bad.ready());
        bad=s; bad.constants(false,64,0,replace); check(!bad.ready());
        bad=s; bad.descriptor(false,0,ArgumentKind::table,100); check(!bad.ready());
        bad=s; const std::uintptr_t duplicate[]{50,50}; bad.descriptor_heaps(duplicate); check(!bad.ready());
        bad=s; const std::uintptr_t too_many[]{50,60,70}; bad.descriptor_heaps(too_many); check(!bad.ready());
        bad=s; bad.invalidate(); bad.root(false,30); check(!bad.ready());
        bad.reset(0,20); bad.root(false,30); check(!bad.ready());
        // v0.23.8: `set` tracks the set arguments; clears touch only those;
        // stale constants left in `values` do not affect equality.
        Bindings a, b; a.reset(5,20); b.reset(5,20);
        a.root(true,40); b.root(true,40); a.descriptor_heaps(heaps); b.descriptor_heaps(heaps);
        const std::uint32_t seven[]{7,8};
        a.constants(true,2,0,seven); a.descriptor(true,2,ArgumentKind::table,300); b.descriptor(true,2,ArgumentKind::table,300);
        check(a.graphics.arguments[2].values[0]==7 && a.graphics.arguments[2].mask==0 && a==b);
        check(a.graphics.set==(std::uint64_t(1)<<2));
        a.constants(true,2,1,seven); check(!(a==b) && a.graphics.arguments[2].mask==6 && a.graphics.arguments[2].address==0);
        a.descriptor(true,9,ArgumentKind::srv,64); a.descriptor(true,5,ArgumentKind::table,128);
        check(a.graphics.set==((std::uint64_t(1)<<2)|(std::uint64_t(1)<<5)|(std::uint64_t(1)<<9)));
        const std::uintptr_t other[]{50,90}; a.descriptor_heaps(other);
        check(a.graphics.arguments[5].kind==ArgumentKind::unset && a.graphics.arguments[9].address==64 &&
              a.graphics.set==((std::uint64_t(1)<<2)|(std::uint64_t(1)<<9)));
        a.root(true,41);
        check(a.graphics.set==0 && a.graphics.signature==41);
        for (const auto &arg : a.graphics.arguments) check(arg.kind==ArgumentKind::unset && !arg.address && !arg.mask);
        a.reset(6,20); b.reset(6,20); check(a==b && a.graphics.set==0 && !a.graphics.signature);
        std::cout << "Generation, root/heap invalidation, partial constants, bound checks and sparse clears passed\n";
    } catch(const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
}
