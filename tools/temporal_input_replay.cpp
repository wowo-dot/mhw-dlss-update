#include "../src/temporal_gpu_inputs/replay.hpp"
#include <windows.h>
#include <DirectXPackedVector.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace ti = mhw::temporal_gpu_inputs;
namespace fs = std::filesystem;
namespace {
void require(bool value, const char *reason) { if (!value) throw std::runtime_error(reason); }
unsigned number(const std::wstring &s) {
    std::size_t used{}; const auto n = std::stoull(s, &used, 0);
    require(used == s.size() && !s.empty() && s[0] != '-' && n <= std::numeric_limits<unsigned>::max(), "Invalid unsigned argument");
    return static_cast<unsigned>(n);
}
std::vector<std::uint32_t> load(const fs::path &path, std::size_t count) {
    require(count * 4 <= ti::input_byte_limit && fs::file_size(path) == count * 4, "Input file size");
    std::vector<std::uint32_t> data(count); std::ifstream in(path, std::ios::binary);
    require(bool(in.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(count * 4))), "Input file read");
    return data;
}
void save(const fs::path &path, const void *data, std::size_t bytes) {
    require(bytes <= std::numeric_limits<DWORD>::max(), "Output size");
    const auto file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "Preserve existing output / create failure");
    DWORD written{}; const auto ok = WriteFile(file, data, static_cast<DWORD>(bytes), &written, nullptr);
    const auto closed = CloseHandle(file);
    require(ok && written == bytes && closed, "Output file write");
}
std::array<float, 3> compare(const ti::Frame &f, const ti::Result &r) {
    require(r.completed && r.allocator_retired && r.fence_value >= 1, "Incomplete GPU result");
    const auto n = std::size_t(f.width) * f.height;
    require(r.color.size() == n * 4 && r.depth.size() == n && r.motion.size() == n * 2, "Output extent");
    std::array<float, 3> errors{};
    for (std::size_t i = 0; i < n; ++i) {
        const auto c = ti::color_reference(f.color[i]);
        const auto m = ti::motion_reference(f.motion[i], f.width, f.height);
        for (unsigned j = 0; j < 4; ++j) {
            require(std::isfinite(r.color[i * 4 + j]), "Nonfinite GPU color");
            errors[0] = std::max(errors[0], std::abs(r.color[i * 4 + j] - c[j]));
        }
        require(std::isfinite(r.depth[i]), "Nonfinite GPU depth");
        errors[1] = std::max(errors[1], std::abs(r.depth[i] - std::bit_cast<float>(f.depth[i])));
        for (unsigned j = 0; j < 2; ++j) {
            require(std::isfinite(r.motion[i * 2 + j]), "Nonfinite GPU motion");
            errors[2] = std::max(errors[2], std::abs(r.motion[i * 2 + j] - m[j]));
        }
    }
    require(errors == std::array<float, 3>{}, "GPU values differ from CPU references");
    return errors;
}
std::string report(const ti::Frame &f, const ti::Result &r) {
    std::ostringstream out; out << std::boolalpha;
    out << "{\n  \"schema\":1,\"experiment\":\"owned-temporal-gpu-input-replay-v1\",\"graphics_api\":\"D3D12\",\n"
        << "  \"width\":" << f.width << ",\"height\":" << f.height
        << ",\"pixels_compared\":" << std::uint64_t(f.width) * f.height << ",\n"
        << "  \"adapter_luid_low\":" << r.adapter_low << ",\"adapter_luid_high\":" << r.adapter_high
        << ",\"vendor\":" << r.vendor << ",\"device\":" << r.device << ",\n"
        << "  \"debug_layer\":" << r.debug_layer << ",\"gpu_validation\":" << r.gpu_validation
        << ",\"debug_errors\":" << r.debug_errors << ",\"debug_warnings\":" << r.debug_warnings << ",\n"
        << "  \"input_srv_formats\":[26,41,42],\"output_uav_formats\":[2,41,16],\n"
        << "  \"srvs\":3,\"uavs\":3,\"mip_levels\":1,\"plane_slice\":0,\"component_mapping\":5768,\n"
        << "  \"gpu_dispatches\":1,\"texture_copies\":6,\"transition_barriers\":6,\n"
        << "  \"fence_value\":" << r.fence_value << ",\"completed\":" << r.completed
        << ",\"allocator_retired\":" << r.allocator_retired << ",\"allocation_bytes\":" << r.allocation_bytes << ",\n"
        << "  \"all_pixels_match_cpu\":true,\"color_max_abs_error\":0,\"depth_max_abs_error\":0,\"motion_max_abs_error\":0,\n"
        << "  \"motion_convention\":\"current-to-previous pixel offset; low packed flag bit masked\",\n"
        << "  \"game_process_access\":false,\"engine_views_verified\":false,\"sdk_calls\":0,\"dlaa_ready\":false\n}\n";
    return out.str();
}
void self_test() {
    for (const auto size : {std::array<unsigned, 2>{17, 19}, std::array<unsigned, 2>{64, 16}}) {
        const auto n = std::size_t(size[0]) * size[1];
        std::vector<std::uint32_t> color(n), depth(n), motion(n);
        constexpr std::array<std::uint16_t, 8> halves{0x0000, 0x8000, 0x0002, 0x3800, 0xb800, 0x3c00, 0xbc00, 0x4800};
        for (std::size_t i = 0; i < n; ++i) {
            DirectX::PackedVector::XMFLOAT3PK c(static_cast<float>(i % 17) * .125f,
                static_cast<float>(i % 29) * .25f, static_cast<float>(i % 13) * 4.0f);
            color[i] = c.v; depth[i] = std::bit_cast<std::uint32_t>(static_cast<float>(i % 257) / 256.0f);
            motion[i] = (std::uint32_t(halves[i % 8]) << 16) | halves[(i / 8) % 8] | static_cast<std::uint32_t>(i & 1);
        }
        const ti::Frame f{size[0], size[1], color, depth, motion};
        const auto r = ti::replay(f, {true, 0, 0}); compare(f, r); std::cout << report(f, r);
    }
}
}
int wmain(int argc, wchar_t **argv) {
    try {
        if (argc == 3 && std::wstring(argv[1]) == L"--self-test" && std::wstring(argv[2]) == L"--warp") { self_test(); return 0; }
        unsigned width{}, height{}; ti::Adapter adapter; bool selected{};
        fs::path color_path, depth_path, motion_path, output;
        for (int i = 1; i < argc; ++i) {
            const std::wstring key(argv[i]);
            if (key == L"--warp") { require(!selected, "Choose one adapter"); selected = true; adapter.warp = true; continue; }
            require(i + 1 < argc, "Argument value missing"); const std::wstring value(argv[++i]);
            if (key == L"--width") width = number(value);
            else if (key == L"--height") height = number(value);
            else if (key == L"--color") color_path = value;
            else if (key == L"--depth") depth_path = value;
            else if (key == L"--motion") motion_path = value;
            else if (key == L"--output") output = value;
            else if (key == L"--adapter-luid") {
                require(!selected && i + 1 < argc, "LUID requires low/high pair and one adapter"); selected = true;
                adapter.luid_low = number(value); adapter.luid_high = std::bit_cast<std::int32_t>(number(argv[++i]));
            } else throw std::runtime_error("Unknown argument");
        }
        require(selected && width && height && width <= 8192 && height <= 8192 && !output.empty(),
            "Use --warp or --adapter-luid LOW HIGH, --width W --height H --color FILE --depth FILE --motion FILE --output PREFIX");
        const std::array<fs::path, 4> paths{output.wstring() + L".color.f32", output.wstring() + L".depth.f32",
            output.wstring() + L".motion.f32", output.wstring() + L".json"};
        for (const auto &p : paths) require(!fs::exists(p), "Preserve existing replay output");
        const auto n = std::size_t(width) * height;
        const auto color = load(color_path, n), depth = load(depth_path, n), motion = load(motion_path, n);
        const ti::Frame f{width, height, color, depth, motion}; const auto r = ti::replay(f, adapter); compare(f, r);
        save(paths[0], r.color.data(), r.color.size() * sizeof(float));
        save(paths[1], r.depth.data(), r.depth.size() * sizeof(float));
        save(paths[2], r.motion.data(), r.motion.size() * sizeof(float));
        const auto text = report(f, r); save(paths[3], text.data(), text.size()); std::cout << text;
        return 0;
    } catch (const std::exception &e) { std::cerr << "Replay rejected: " << e.what() << '\n'; return 1; }
}
