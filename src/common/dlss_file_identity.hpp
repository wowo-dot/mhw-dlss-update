#pragma once
// 0.32.0: reads what a DLSS file is (dlss_files.hpp): its version resource and
// whether NVIDIA Corporation signed it. The signature check is offline: no
// revocation lookup and no download, so it answers the same without a network.
// Links version, wintrust and crypt32.
#include "dlss_files.hpp"
#include <windows.h>
#include <softpub.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <cwchar>
#include <string>
#include <vector>

namespace mhw {
inline std::string narrow(const wchar_t *text) {
    if (!text || !*text) return {};
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1) return {};
    std::string out(static_cast<std::size_t>(bytes - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), bytes, nullptr, nullptr);
    return out;
}
// The file version and InternalName; false when the file has no version resource.
inline bool read_dlss_version(const wchar_t *path, DlssFile &out) {
    DWORD ignored{};
    const auto size = GetFileVersionInfoSizeW(path, &ignored);
    if (!size) return false;
    std::vector<unsigned char> data(size);
    VS_FIXEDFILEINFO *fixed{};
    UINT length{};
    if (!GetFileVersionInfoW(path, 0, size, data.data()) ||
        !VerQueryValueW(data.data(), L"\\", reinterpret_cast<void **>(&fixed), &length) || !fixed ||
        length < sizeof *fixed)
        return false;
    out.version = {HIWORD(fixed->dwFileVersionMS), LOWORD(fixed->dwFileVersionMS), HIWORD(fixed->dwFileVersionLS),
                   LOWORD(fixed->dwFileVersionLS)};
    struct Translation {
        WORD language, code_page;
    } *translation{};
    wchar_t key[64]{};
    if (VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void **>(&translation), &length) &&
        translation && length >= sizeof *translation)
        swprintf_s(key, L"\\StringFileInfo\\%04x%04x\\InternalName", translation->language, translation->code_page);
    else
        swprintf_s(key, L"\\StringFileInfo\\040904b0\\InternalName");
    wchar_t *name{};
    if (VerQueryValueW(data.data(), key, reinterpret_cast<void **>(&name), &length) && name && length)
        out.internal = narrow(name);
    return true;
}
// A valid Authenticode signature whose signer is NVIDIA Corporation.
inline bool nvidia_signature(const wchar_t *path, std::string &signer) {
    WINTRUST_FILE_INFO file{};
    file.cbStruct = sizeof file;
    file.pcwszFilePath = path;
    WINTRUST_DATA data{};
    data.cbStruct = sizeof data;
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_NONE;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.pFile = &file;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const bool valid = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, &data) == ERROR_SUCCESS;
    if (valid)
        if (auto *provider = WTHelperProvDataFromStateData(data.hWVTStateData))
            if (auto *signer_data = WTHelperGetProvSignerFromChain(provider, 0, FALSE, 0))
                if (const auto *certificate = WTHelperGetProvCertFromChain(signer_data, 0);
                    certificate && certificate->pCert) {
                    wchar_t name[256]{};
                    if (CertGetNameStringW(certificate->pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, name,
                                           static_cast<DWORD>(std::size(name))) > 1)
                        signer = narrow(name);
                }
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, &data);
    return valid && signer == "NVIDIA Corporation";
}
// The file's identity; `shipped` is the caller's (it knows the pinned hash).
// The shipped file needs no signature check (the hash already says it is).
inline DlssFile identify_dlss(const wchar_t *path, bool shipped) {
    DlssFile f;
    f.shipped = shipped;
    if (!read_dlss_version(path, f)) return f;
    if (!shipped) f.nvidia_signed = nvidia_signature(path, f.signer);
    return f;
}
} // namespace mhw
