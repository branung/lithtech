#include "launcher_core.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace ltlaunch {

const char *kCustomFolder = "Custom";

std::string JoinPath(const std::string &dir, const std::string &leaf)
{
    if (dir.empty()) return leaf;
    char last = dir[dir.size() - 1];
    if (last == '/' || last == '\\') return dir + leaf;
#ifdef _WIN32
    return dir + "\\" + leaf;
#else
    return dir + "/" + leaf;
#endif
}

bool FileExists(const std::string &sPath)
{
    std::ifstream f(sPath.c_str());
    return f.good();
}

GameProfile ShogoProfile(const std::string &sExeDir)
{
    GameProfile game;
    game.m_sCaption = game.m_sTitle = "Shogo: Mobile Armor Division";
    game.m_sSplashRez = "SHOGO.REZ";
    game.m_sSplashPath = "INTERFACE\\SPLASH.PCX";
    game.m_Spec.m_sEngineDir = game.m_Spec.m_sGameDir = sExeDir;
    game.m_Spec.m_Rez = { "SHOGO.REZ", "SOUND.REZ" };
    return game;
}

GameProfile LithLauncherProfile(const std::string &sExeDir)
{
    GameProfile game = ShogoProfile(sExeDir);
    game.m_sCaption = "LithLauncher";
    game.m_bTitleBox = true;
    game.m_Spec.m_sGameDir = JoinPath(sExeDir, "Shogo");
    return game;
}

GameProfile Blood2Profile(const std::string &sExeDir)
{
    GameProfile game;
    game.m_sCaption = game.m_sTitle = "Blood II: The Chosen";
    game.m_sSplashRez = "BLOOD2.REZ";
    game.m_sSplashPath = "INTERFACE\\MAINMENUS\\SPLASH.PCX";
    game.m_Spec.m_sEngineDir = game.m_Spec.m_sGameDir = sExeDir;
    game.m_Spec.m_Rez = { "BLOOD2.REZ", "SOUND.REZ" };
    game.m_Spec.m_OptionalRez = { "BLOOD2P.REZ", "BLOOD2P2.REZ" }; // The patches

    // The retail launcher adds BLOOD2NM2.REZ too, which no install here has
    game.m_AddOn.m_sLabel = "The Nightmare Levels";
    game.m_AddOn.m_sButton = "Launch Nightmares";
    game.m_AddOn.m_sDir = "Blood2NM";
    game.m_AddOn.m_sSplashRez = "BLOOD2NM.REZ";
    game.m_AddOn.m_sSplashPath = "SCREENS_AO\\TITLE.PCX";
    game.m_AddOn.m_Rez = { "BLOOD2NM.REZ" };
    game.m_AddOn.m_OptionalRez = { "BLOOD2NM2.REZ" };
    return game;
}

GameProfile NOLF2Profile(const std::string &sExeDir)
{
    GameProfile game;
    game.m_sCaption = game.m_sTitle = "No One Lives Forever 2";
    game.m_sSplashRez = "game.rez";
    game.m_sSplashPath = "INTERFACE\\MENU\\ART\\SPLASH.PCX";
    game.m_Spec.m_sEngineDir = game.m_Spec.m_sGameDir = sExeDir;
    game.m_Spec.m_EngineRez = { "engine.rez", "game.rez", "game2.rez", "sound.rez" };
    game.m_Spec.m_OptionalRez = { "Update_v1x3.rez" }; // The 1.3 patch
    return game;
}

std::vector<std::string> RequiredRez(const LaunchSpec &spec)
{
    std::vector<std::string> out = spec.m_EngineRez;
    out.insert(out.end(), spec.m_Rez.begin(), spec.m_Rez.end());
    return out;
}

std::string FirstMissing(const std::string &sDir, const std::vector<std::string> &names)
{
    for (const std::string &sName : names)
        if (!FileExists(JoinPath(sDir, sName)))
            return sName;
    return std::string();
}

std::string AddOnDir(const GameProfile &game)
{
    return JoinPath(game.m_Spec.m_sGameDir, game.m_AddOn.m_sDir);
}

bool HasAddOn(const GameProfile &game)
{
    return !game.m_AddOn.m_sDir.empty() && FileExists(JoinPath(AddOnDir(game), "CShell.dll"));
}

std::string ReadTextFile(const std::string &sPath)
{
    std::ifstream in(sPath.c_str(), std::ios::binary);
    std::ostringstream os;
    os << in.rdbuf();
    return os.str();
}

bool WriteTextFile(const std::string &sPath, const std::string &sText)
{
    std::ofstream out(sPath.c_str(), std::ios::binary);
    out << sText;
    return !out.fail();
}

namespace {

bool LargerFirst(const WindowSize &a, const WindowSize &b)
{
    if (a.m_nWidth != b.m_nWidth) return a.m_nWidth > b.m_nWidth;
    return a.m_nHeight > b.m_nHeight;
}

bool EqualNoCase(const std::string &a, const std::string &b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return true;
}

bool EndsWithNoCase(const std::string &s, const std::string &sEnd)
{
    return s.size() >= sEnd.size() && EqualNoCase(s.substr(s.size() - sEnd.size()), sEnd);
}

bool ReadWord(const std::string &line, size_t &i, std::string &out)
{
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (i >= line.size()) return false;
    if (line[i] == '"')
    {
        const size_t end = line.find('"', i + 1);
        if (end == std::string::npos) return false;
        out = line.substr(i + 1, end - i - 1);
        i = end + 1;
    }
    else
    {
        size_t end = line.find_first_of(" \t\r", i);
        if (end == std::string::npos) end = line.size();
        out = line.substr(i, end - i);
        i = end;
    }
    return true;
}

bool ParseConfigLine(const std::string &line, std::string &sName, std::string &sValue)
{
    size_t i = line.find_first_not_of(" \t");
    if (i == std::string::npos) return false;
    if (line[i] == '+' || line[i] == '-') ++i;
    return ReadWord(line, i, sName) && ReadWord(line, i, sValue);
}

std::string LineEnding(const std::string &sCfg)
{
    const bool bHasLF   = sCfg.find('\n') != std::string::npos;
    const bool bHasCRLF = sCfg.find("\r\n") != std::string::npos;
    return (!bHasLF || bHasCRLF) ? "\r\n" : "\n";
}

size_t FindValue(const std::vector<ConfigValue> &values, const std::string &sName)
{
    for (size_t k = 0; k < values.size(); ++k)
        if (EqualNoCase(values[k].m_sName, sName)) return k;
    return values.size();
}

// "Name" "Value"
std::string QuotedLine(const ConfigValue &value)
{
    return "\"" + value.m_sName + "\" \"" + value.m_sValue + "\"";
}

}

bool DirExists(const std::string &sPath)
{
    std::error_code ec;
    return std::filesystem::is_directory(sPath, ec);
}

std::vector<std::string> ListFiles(const std::string &sDir, const std::string &sExt)
{
    std::vector<std::string> out;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(sDir, ec), end; !ec && it != end; it.increment(ec))
    {
        if (!it->is_regular_file(ec)) continue;
        const std::string sName = it->path().filename().string();
        if (EndsWithNoCase(sName, sExt)) out.push_back(sName);
    }
    std::sort(out.begin(), out.end());
    return out;
}

bool ContainsNoCase(const std::vector<std::string> &list, const std::string &sName)
{
    for (const std::string &s : list)
        if (EqualNoCase(s, sName)) return true;
    return false;
}

std::vector<std::string> AvailableRez(const LaunchSpec &spec)
{
    std::vector<std::string> out;
    for (const std::string &sName : ListFiles(JoinPath(spec.m_sGameDir, kCustomFolder), ".rez"))
        out.push_back(JoinPath(kCustomFolder, sName));
    return out;
}

std::vector<WindowSize> DistinctSizes(const std::vector<WindowSize> &sizes)
{
    std::vector<WindowSize> out;
    for (size_t i = 0; i < sizes.size(); ++i)
    {
        if (sizes[i].m_nWidth <= 0 || sizes[i].m_nHeight <= 0) continue;
        bool bDup = false;
        for (size_t j = 0; j < out.size() && !bDup; ++j)
            bDup = out[j].m_nWidth == sizes[i].m_nWidth && out[j].m_nHeight == sizes[i].m_nHeight;
        if (!bDup) out.push_back(sizes[i]);
    }
    std::sort(out.begin(), out.end(), LargerFirst);
    return out;
}

std::string SizeText(const WindowSize &m)
{
    std::ostringstream os;
    os << m.m_nWidth << " x " << m.m_nHeight;
    return os.str();
}

bool GetConfigValue(const std::string &sCfg, const std::string &sName, std::string &sValue)
{
    std::istringstream in(sCfg);
    std::string line, sLineName, sLineValue;
    bool bFound = false;
    while (std::getline(in, line))
    {
        if (ParseConfigLine(line, sLineName, sLineValue) && EqualNoCase(sLineName, sName))
        {
            sValue = sLineValue;
            bFound = true;
        }
    }
    return bFound;
}

std::string SetConfigValues(const std::string &sCfg, const std::vector<ConfigValue> &values)
{
    const std::string sEol = LineEnding(sCfg);
    std::vector<bool> written(values.size(), false);
    std::ostringstream os;
    std::istringstream in(sCfg);
    std::string line, sLineName, sLineValue;
    while (std::getline(in, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        if (ParseConfigLine(line, sLineName, sLineValue))
        {
            const size_t k = FindValue(values, sLineName);
            if (k < values.size())
            {
                line = QuotedLine(values[k]);
                written[k] = true;
            }
        }
        os << line << sEol;
    }

    for (size_t k = 0; k < values.size(); ++k)
        if (!written[k]) os << QuotedLine(values[k]) << sEol;
    return os.str();
}

// One 'key=value' line per setting, and one rez= line per remembered archive
bool LoadSettings(const std::string &sPath, LauncherSettings &settings)
{
    std::ifstream in(sPath.c_str(), std::ios::binary);
    if (!in.good()) return false;

    settings = LauncherSettings();
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        const size_t eq = line.find('=');
        if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;

        const std::string sKey = line.substr(0, eq), sValue = line.substr(eq + 1);
        if (sKey == "alwaysrez") settings.m_bAlwaysRez = (sValue == "1");
        else if (sKey == "rez") settings.m_AlwaysRez.push_back(sValue);
        else if (sKey == "addon") settings.m_bAddOn = (sValue == "1");
    }
    return true;
}

bool SaveSettings(const std::string &sPath, const LauncherSettings &settings)
{
    std::ostringstream os;
    os << "# Written by LithLauncher. Safe to delete\n";
    os << "addon=" << (settings.m_bAddOn ? 1 : 0) << "\n";
    os << "alwaysrez=" << (settings.m_bAlwaysRez ? 1 : 0) << "\n";
    if (settings.m_bAlwaysRez)
        for (const std::string &sRez : settings.m_AlwaysRez) os << "rez=" << sRez << "\n";
    return WriteTextFile(sPath, os.str());
}

std::vector<std::string> BuildCommandLine(const LaunchSpec &spec)
{
    std::vector<std::string> args;
#ifdef _WIN32
    args.push_back(JoinPath(spec.m_sEngineDir, "Lithtech.exe"));
#else
    args.push_back(JoinPath(spec.m_sEngineDir, "Lithtech"));
#endif
    for (size_t i = 0; i < spec.m_Rez.size(); ++i)
    {
        args.push_back("-rez");
        args.push_back(spec.m_Rez[i]);
    }
    return args;
}

std::string QuoteArgs(const std::vector<std::string> &args)
{
    std::ostringstream os;
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (i) os << ' ';
        if (args[i].find(' ') != std::string::npos) os << '"' << args[i] << '"';
        else os << args[i];
    }
    return os.str();
}

} // namespace ltlaunch
