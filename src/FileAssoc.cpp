#include "FileAssoc.hpp"

#include <windows.h>
#include <shlobj.h>
#include <string>

#pragma comment(lib, "shell32.lib")

namespace fileassoc {

namespace {

const wchar_t *PROG_ID = L"CarGame3D.cgmap";

bool SetRegString(HKEY root, const std::wstring &subkey, const wchar_t *valueName, const std::wstring &value)
{
    HKEY key;
    LONG res = RegCreateKeyExW(root, subkey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr);
    if (res != ERROR_SUCCESS) return false;

    res = RegSetValueExW(key, valueName, 0, REG_SZ, (const BYTE *)value.c_str(),
                         (DWORD)((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return res == ERROR_SUCCESS;
}

std::wstring ExePath()
{
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::wstring(buf, len);
}

} // namespace

bool AssociateCgmapFiles()
{
    std::wstring exe = ExePath();
    if (exe.empty()) return false;

    // HKCU\Software\Classes\.cgmap -> (default) = ProgID
    if (!SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\.cgmap", nullptr, PROG_ID)) return false;

    // HKCU\Software\Classes\CarGame3D.cgmap -> (default) = friendly name
    if (!SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\" + std::wstring(PROG_ID),
                      nullptr, L"3D Car Game Map"))
        return false;

    // ...\shell\open\command -> (default) = "<exe>" "%1"
    std::wstring command = L"\"" + exe + L"\" \"%1\"";
    if (!SetRegString(HKEY_CURRENT_USER,
                      L"Software\\Classes\\" + std::wstring(PROG_ID) + L"\\shell\\open\\command",
                      nullptr, command))
        return false;

    // Tell Explorer the association changed so it takes effect without a
    // logoff/restart -- best-effort, the association itself is already
    // written either way by this point.
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

bool IsAssociated()
{
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.cgmap", 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }
    wchar_t value[256] = { 0 };
    DWORD size = sizeof(value);
    LONG res = RegQueryValueExW(key, nullptr, nullptr, nullptr, (BYTE *)value, &size);
    RegCloseKey(key);
    return res == ERROR_SUCCESS && std::wstring(value) == PROG_ID;
}

} // namespace fileassoc
