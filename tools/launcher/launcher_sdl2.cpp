#include "launcher_core.h"
#include "launcher_splash.h"

#ifdef _WIN32
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include <SDL_syswm.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../runtime/ui/src/stb_truetype.h"

#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

// The game this build launches, set per launcher by CMake.
// Without one it's the combined launcher, LTCallisto, which lists every game in its Games folder
#ifndef LTLAUNCH_PROFILE
#define LTLAUNCH_COMBINED 1
#endif

// This build's stamp, <year>.<month><day>.<time>, written on every build by stamp.cmake
extern const char *g_szLauncherBuild;

namespace {

// The launcher's name, shown with its build stamp
const char *kLauncherName = "LTCallisto";

// Launcher color palette (0xRRGGBB)
const Uint32 kFace   = 0xF0F0F0;
const Uint32 kWindow = 0xFFFFFF;
const Uint32 kBorder = 0x7A7A7A;
const Uint32 kButton = 0xE1E1E1;
const Uint32 kButtonDown = 0xCCE4F7;
const Uint32 kBorderDown = 0x005499;
const Uint32 kText   = 0x000000;
const Uint32 kAlert  = 0xA00000;
const Uint32 kGrayTxt = 0x6D6D6D;
const Uint32 kCaption = 0x0078D7;
const Uint32 kCapText = 0xFFFFFF;

// The combined launcher's game list, to the left of the one game's window
#ifdef LTLAUNCH_COMBINED
const int kSideW = 210;
#else
const int kSideW = 0;
#endif
const int kMainW = 640;
const int kWinW = kSideW + kMainW, kWinH = 470;

struct Rect
{
    int x, y, w, h;
    Rect() : x(0), y(0), w(0), h(0) {}
    Rect(int a, int b, int c, int d) : x(a), y(b), w(c), h(d) {}
};

bool In(const Rect &r, int x, int y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

const Rect kPlayButton(kSideW + 406, 12, 222, 26);
const Rect kQuitButton(kSideW + 406, 330, 222, 26);
const Rect kDisplayButton(kSideW + 406, 52, 222, 26);
const Rect kCustomizeButton(kSideW + 406, 86, 222, 26);
const Rect kAddOnBox(kSideW + 12, 338, 380, 18);
const Rect kDataDirBox(kSideW + 12, 12, 300, 22);
const Rect kBrowseButton(kSideW + 318, 12, 74, 22);

// The game list's heading, and its buttons spaced like Display... and Customize...
const Rect kGamesHeading(12, 12, kSideW - 24, 26);
const int  kGameButtonY = 52, kGameButtonStep = 34;

const int kCaptionH = 22;
const int kCloseSize = 18;
const int kDlgButtonW = 80, kDlgButtonH = 24;
const int kDlgButtonGap = 8; // Gap between OK and Cancel and between Cancel and the frame's edge
const int kDlgBottomMargin = 10; // Margin below OK and Cancel

// A dialog centred over the game's side of the window, with its caption bar, close box, OK and Cancel
struct Dialog
{
    Rect frame, caption, close, ok, cancel;

    Dialog(int w, int h)
        : frame(kSideW + (kMainW - w) / 2, (kWinH - h) / 2, w, h),
          caption(frame.x + 1, frame.y + 1, w - 2, kCaptionH),
          close(caption.x + caption.w - kCloseSize - 3, caption.y + 2, kCloseSize, kCloseSize),
          ok(frame.x + w - 2 * (kDlgButtonW + kDlgButtonGap), frame.y + h - kDlgButtonH - kDlgBottomMargin,
             kDlgButtonW, kDlgButtonH),
          cancel(frame.x + w - (kDlgButtonW + kDlgButtonGap), frame.y + h - kDlgButtonH - kDlgBottomMargin,
                 kDlgButtonW, kDlgButtonH) {}
};

const Dialog kDisplayDlg(440, 320);
const Rect kSizeList(kDisplayDlg.frame.x + 14, kDisplayDlg.frame.y + 48, kDisplayDlg.frame.w - 28, 172);
const Rect kBorderlessBox(kDisplayDlg.frame.x + 14, kDisplayDlg.frame.y + 228, kDisplayDlg.frame.w - 28, 18);

const Dialog kCustomizeDlg(586, 290);
const Rect kAvailableList(kCustomizeDlg.frame.x + 14, kCustomizeDlg.frame.y + 46, 220, 160);
const Rect kChosenList(kCustomizeDlg.frame.x + 352, kCustomizeDlg.frame.y + 46, 220, 160);
const Rect kAddButton(kCustomizeDlg.frame.x + 248, kCustomizeDlg.frame.y + 90, 90, 24);
const Rect kRemoveButton(kCustomizeDlg.frame.x + 248, kCustomizeDlg.frame.y + 122, 90, 24);
const Rect kAlwaysRezBox(kCustomizeDlg.frame.x + 193, kCustomizeDlg.frame.y + 216, 200, 18);
const int  kRowH = 18;
const int  kListPad = 2;
const int  kScrollBarW = 14;
const int  kMinHandleH = 12;
const int  kCheckSize = 13;

SDL_Renderer *g_pRen  = 0;
SDL_Texture  *g_pFont = 0;

const int   kFontFirst = 32, kFontCount = 95;
const float kFontPx    = 16.0f;

stbtt_bakedchar g_Glyphs[kFontCount];
int g_nAscent = 0, g_nLineH = 0;

// Drawing

void SetColor(Uint32 c)
{
    SDL_SetRenderDrawColor(g_pRen, (Uint8)(c >> 16), (Uint8)(c >> 8), (Uint8)c, 255);
}

void Fill(const Rect &r, Uint32 c)
{
    SDL_Rect sr = { r.x, r.y, r.w, r.h };
    SetColor(c);
    SDL_RenderFillRect(g_pRen, &sr);
}

void Frame(const Rect &r, Uint32 c)
{
    SDL_Rect sr = { r.x, r.y, r.w, r.h };
    SetColor(c);
    SDL_RenderDrawRect(g_pRen, &sr);
}

int GlyphIndex(int ch)
{
    if (ch < kFontFirst || ch >= kFontFirst + kFontCount) ch = '?';
    return ch - kFontFirst;
}

int TextW(const std::string &s)
{
    float w = 0.0f;
    for (size_t i = 0; i < s.size(); ++i) w += g_Glyphs[GlyphIndex((unsigned char)s[i])].xadvance;
    return (int)(w + 0.5f);
}

void Text(int x, int y, const std::string &s, Uint32 c, int nMaxW = 100000)
{
    if (!g_pFont) return;
    SDL_SetTextureColorMod(g_pFont, (Uint8)(c >> 16), (Uint8)(c >> 8), (Uint8)c);
    float pen = (float)x;
    for (size_t i = 0; i < s.size(); ++i)
    {
        const stbtt_bakedchar &b = g_Glyphs[GlyphIndex((unsigned char)s[i])];
        if (pen + b.xadvance > (float)(x + nMaxW)) break;
        SDL_Rect src = { b.x0, b.y0, b.x1 - b.x0, b.y1 - b.y0 };
        if (src.w > 0 && src.h > 0)
        {
            SDL_Rect dst = { (int)(pen + 0.5f) + (int)b.xoff, y + g_nAscent + (int)b.yoff,
                             src.w, src.h };
            SDL_RenderCopy(g_pRen, g_pFont, &src, &dst);
        }
        pen += b.xadvance;
    }
}

void TextC(const Rect &r, const std::string &s, Uint32 c)
{
    Text(r.x + (r.w - TextW(s)) / 2, r.y + (r.h - g_nLineH) / 2, s, c, r.w);
}

// A path that's too wide keeps its end, which names the folder
std::string FitLeft(const std::string &s, int nMaxW)
{
    if (TextW(s) <= nMaxW) return s;
    size_t i = 0;
    while (i < s.size() && TextW("..." + s.substr(i)) > nMaxW) ++i;
    return "..." + s.substr(i);
}

// Tahoma from the system's fonts. Has fallback attempts on Linux in case it's not available.
bool ReadFontFile(std::vector<unsigned char> &data)
{
    std::vector<std::string> paths;
#ifdef _WIN32
    const char *pWinDir = SDL_getenv("WINDIR");
    paths.push_back(std::string(pWinDir ? pWinDir : "C:\\Windows") + "\\Fonts\\tahoma.ttf");
#else
    paths.push_back("/usr/share/wine/fonts/tahoma.ttf");
    paths.push_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    paths.push_back("/usr/share/fonts/TTF/DejaVuSans.ttf");
    paths.push_back("/usr/share/fonts/dejavu/DejaVuSans.ttf");
#endif
    for (size_t i = 0; i < paths.size(); ++i)
    {
        std::ifstream in(paths[i].c_str(), std::ios::binary | std::ios::ate);
        if (!in.good()) continue;
        const std::streamsize nSize = in.tellg();
        if (nSize <= 0) continue;
        data.resize((size_t)nSize);
        in.seekg(0);
        if (in.read((char *)&data[0], nSize)) return true;
    }
    return false;
}

SDL_Texture *MakeFontAtlas()
{
    std::vector<unsigned char> ttf;
    if (!ReadFontFile(ttf)) return 0;

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, &ttf[0], stbtt_GetFontOffsetForIndex(&ttf[0], 0))) return 0;
    const float scale = stbtt_ScaleForPixelHeight(&info, kFontPx);
    int nAscent, nDescent, nGap;
    stbtt_GetFontVMetrics(&info, &nAscent, &nDescent, &nGap);
    g_nAscent = (int)(nAscent * scale + 0.5f);
    g_nLineH  = (int)((nAscent - nDescent) * scale + 0.5f);

    const int w = 512, h = 256;
    std::vector<unsigned char> alpha((size_t)w * h);
    if (stbtt_BakeFontBitmap(&ttf[0], 0, kFontPx, &alpha[0], w, h,
                             kFontFirst, kFontCount, g_Glyphs) <= 0)
        return 0;

    SDL_Surface *pSurf = SDL_CreateRGBSurface(0, w, h, 32,
                                              0x00FF0000, 0x0000FF00, 0x000000FF,
                                              0xFF000000);
    if (!pSurf) return 0;
    SDL_LockSurface(pSurf);
    for (int y = 0; y < h; ++y)
    {
        Uint32 *pRow = (Uint32 *)((Uint8 *)pSurf->pixels + y * pSurf->pitch);
        for (int x = 0; x < w; ++x)
            pRow[x] = ((Uint32)alpha[(size_t)y * w + x] << 24) | 0x00FFFFFFu;
    }
    SDL_UnlockSurface(pSurf);
    SDL_Texture *pTex = SDL_CreateTextureFromSurface(g_pRen, pSurf);
    SDL_FreeSurface(pSurf);
    if (pTex) SDL_SetTextureBlendMode(pTex, SDL_BLENDMODE_BLEND);
    return pTex;
}

void Button(const Rect &r, const std::string &sLabel, bool bPressed = false)
{
    Fill(r, bPressed ? kButtonDown : kButton);
    Frame(r, bPressed ? kBorderDown : kBorder);
    TextC(bPressed ? Rect(r.x + 1, r.y + 1, r.w, r.h) : r, sLabel, kText);
}

void CheckBox(const Rect &hit, const std::string &sLabel, bool bOn, bool bPressed)
{
    const Rect box(hit.x, hit.y + (hit.h - kCheckSize) / 2, kCheckSize, kCheckSize);
    Fill(box, bPressed ? kButtonDown : kWindow);
    Frame(box, bPressed ? kBorderDown : kBorder);
    if (bOn)
    {
        for (int i = 0; i < 3; ++i) Fill(Rect(box.x + 3 + i, box.y + 5 + i, 1, 3), kText);
        for (int i = 0; i < 4; ++i) Fill(Rect(box.x + 6 + i, box.y + 7 - i, 1, 3), kText);
    }
    Text(box.x + kCheckSize + 6, hit.y + (hit.h - g_nLineH) / 2, sLabel, kText);
}

int VisibleRows(const Rect &r)
{
    return (r.h - 2 * kListPad) / kRowH;
}

void ListBox(const Rect &r, const std::vector<std::string> &items, int iSel, int nScroll)
{
    Fill(r, kWindow);
    Frame(r, kBorder);
    const int nVis = VisibleRows(r);
    const int nItems = (int)items.size();
    const bool bBar = nItems > nVis;
    const int nRowW = r.w - 2 * kListPad - (bBar ? kScrollBarW : 0);

    for (int n = 0; n < nVis && nScroll + n < nItems; ++n)
    {
        const int i = nScroll + n;
        const Rect row(r.x + kListPad, r.y + kListPad + n * kRowH, nRowW, kRowH);
        if (i == iSel) Fill(row, kCaption);
        Text(row.x + 4, row.y + (kRowH - g_nLineH) / 2, items[(size_t)i],
             i == iSel ? kCapText : kText, row.w - 8);
    }

    if (bBar)
    {
        const Rect bar(r.x + r.w - kScrollBarW, r.y + kListPad, kScrollBarW - kListPad, r.h - 2 * kListPad);
        Fill(bar, kFace);

        int nHandleH = bar.h * nVis / nItems;
        if (nHandleH < kMinHandleH) nHandleH = kMinHandleH;
        const int nTravel = bar.h - nHandleH;
        const int nHandleY = bar.y + nTravel * nScroll / (nItems - nVis);
        const Rect handle(bar.x, nHandleY, bar.w, nHandleH);
        Fill(handle, kButton);
        Frame(handle, kBorder);
    }
}

int ListRowAt(const Rect &list, int nItems, int nScroll, int x, int y)
{
    const int nVis = VisibleRows(list);
    const int nBar = (nItems > nVis) ? kScrollBarW : 0;
    const Rect rows(list.x + kListPad, list.y + kListPad, list.w - 2 * kListPad - nBar, nVis * kRowH);
    if (!In(rows, x, y)) return -1;
    return nScroll + (y - rows.y) / kRowH;
}

int ClampScroll(const Rect &list, int nItems, int nScroll)
{
    const int nMax = nItems - VisibleRows(list);
    if (nScroll > nMax) nScroll = nMax;
    if (nScroll < 0) nScroll = 0;
    return nScroll;
}

std::vector<ltlaunch::WindowSize> DesktopSizes()
{
    std::vector<ltlaunch::WindowSize> all;
    for (int d = 0; d < SDL_GetNumVideoDisplays(); ++d)
        for (int m = 0; m < SDL_GetNumDisplayModes(d); ++m)
        {
            SDL_DisplayMode dm;
            if (SDL_GetDisplayMode(d, m, &dm) != 0) continue;
            all.push_back({ dm.w, dm.h });
        }
    return ltlaunch::DistinctSizes(all);
}

// Asks for a folder, starting in sStart. False when cancelled, with sError set when it couldn't ask
#ifdef _WIN32
bool PickFolder(SDL_Window *pWnd, const std::string &sStart, std::string &sOut, std::string &sError)
{
    HWND hOwner = NULL;
    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    if (SDL_GetWindowWMInfo(pWnd, &wm)) hOwner = wm.info.win.window;

    const HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool bPicked = false;
    IFileOpenDialog *pDlg = NULL;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDlg))))
    {
        sError = "Could not open the folder dialog";
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return false;
    }

    DWORD dwOptions = 0;
    pDlg->GetOptions(&dwOptions);
    pDlg->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    pDlg->SetTitle(L"Choose the folder holding the game's .rez files");

    wchar_t wStart[MAX_PATH];
    IShellItem *pStart = NULL;
    if (MultiByteToWideChar(CP_ACP, 0, sStart.c_str(), -1, wStart, MAX_PATH) &&
        SUCCEEDED(SHCreateItemFromParsingName(wStart, NULL, IID_PPV_ARGS(&pStart))))
    {
        pDlg->SetFolder(pStart);
        pStart->Release();
    }

    IShellItem *pItem = NULL;
    if (SUCCEEDED(pDlg->Show(hOwner)) && SUCCEEDED(pDlg->GetResult(&pItem)))
    {
        PWSTR pPath = NULL;
        if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pPath)))
        {
            // The engine takes ANSI paths, so a name the code page can't hold is refused
            char sPath[MAX_PATH * 2];
            BOOL bLossy = FALSE;
            if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, pPath, -1, sPath, sizeof(sPath), NULL, &bLossy) &&
                !bLossy)
            {
                sOut = sPath;
                bPicked = true;
            }
            else
                sError = "That folder's name has characters the engine can't open";
            CoTaskMemFree(pPath);
        }
        pItem->Release();
    }

    pDlg->Release();
    if (SUCCEEDED(hrInit)) CoUninitialize();
    return bPicked;
}
#else
std::string ShellQuote(const std::string &s)
{
    std::string out = "'";
    for (char c : s)
    {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    return out + "'";
}

// zenity, or kdialog when it isn't installed
bool PickFolder(SDL_Window *, const std::string &sStart, std::string &sOut, std::string &sError)
{
    const std::string kCmds[] = {
        "zenity --file-selection --directory --title='Choose the game data folder' --filename=" +
            ShellQuote(ltlaunch::JoinPath(sStart, "")),
        "kdialog --getexistingdirectory " + ShellQuote(sStart),
    };
    for (const std::string &sCmd : kCmds)
    {
        FILE *pPipe = popen((sCmd + " 2>/dev/null").c_str(), "r");
        if (!pPipe) continue;
        std::string sLine;
        char buf[512];
        while (fgets(buf, sizeof(buf), pPipe)) sLine += buf;
        const int nStatus = pclose(pPipe);
        while (!sLine.empty() && (sLine.back() == '\n' || sLine.back() == '\r')) sLine.pop_back();

        if (WIFEXITED(nStatus) && WEXITSTATUS(nStatus) == 127) continue; // Not installed
        if (WIFEXITED(nStatus) && WEXITSTATUS(nStatus) == 0 && !sLine.empty())
        {
            sOut = sLine;
            return true;
        }
        return false; // Cancelled
    }
    sError = "Install zenity or kdialog to choose a folder";
    return false;
}
#endif

// Starts the engine with the game directory as its working directory
bool Spawn(const std::vector<std::string> &args, const std::string &sCwd,
           std::string &sError)
{
#ifdef _WIN32
    std::string sCmd = ltlaunch::QuoteArgs(args);
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    std::vector<char> cmd(sCmd.begin(), sCmd.end()); cmd.push_back('\0');
    if (!CreateProcessA(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL,
                        sCwd.c_str(), &si, &pi))
    { sError = "Could not start " + args[0]; return false; }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
#else
    const pid_t pid = fork();
    if (pid < 0) { sError = "Could not start " + args[0]; return false; }
    if (pid == 0)
    {
        if (chdir(sCwd.c_str()) != 0) _exit(127);
        std::vector<const char *> cargv;
        for (size_t i = 0; i < args.size(); ++i) cargv.push_back(args[i].c_str());
        cargv.push_back(0);
        execv(cargv[0], (char *const *)&cargv[0]);
        _exit(127);
    }
    return true;
#endif
}

struct App
{
    // Every game this launcher lists, the shown one copied into game and spec
    std::vector<ltlaunch::GameProfile> games;
    std::vector<Rect> gameButtons;
    int iGame = -1;
    std::string sSelectedPath; // The combined launcher's own launcher.ini, naming the selected game

    // A game's choices while another is shown, so a session's custom archives survive switching
    struct GameState
    {
        bool bSeen = false;
        std::vector<std::string> chosenRez;
        bool bAlwaysRez = false, bAddOn = false;
        std::string sDataDir;
    };
    std::vector<GameState> states;

    ltlaunch::GameProfile game;
    ltlaunch::LaunchSpec spec;
    std::string sStatus;
    bool bQuit = false;

    enum class DialogId { None, Display, Customize };
    DialogId eOpenDialog = DialogId::None;

    const Rect *pPressed = 0;
    bool bPressedOver = false;

    SDL_Texture *pSplash = 0;
    bool bSplashTried = false;

    SDL_Window *pWnd = 0; // The folder dialog's owner

    std::vector<ltlaunch::WindowSize> sizes;
    int  iSize = -1, iSizeOpen = -1;
    bool bBorderless = false, bBorderlessOpen = false;
    int  nSizeScroll = 0;

    // Extra archives loaded after the game's own
    // The 'OnOpen' copy is what Cancel puts back
    std::vector<std::string> availableRez, chosenRez, chosenRezOnOpen;
    bool bAlwaysRez = false, bAlwaysRezOnOpen = false;
    std::string sSettingsPath;
    int iAvailableSel = -1, iChosenSel = -1;
    int nAvailableScroll = 0, nChosenScroll = 0;

    // The addon's checkbox, shown only when its modules are built
    bool bHasAddOn = false, bAddOn = false;

    bool UseAddOn() const { return bHasAddOn && bAddOn; }

    // Shows a game. Each keeps its own settings in launcher.ini in its folder, which for a single
    // game's launcher is the launcher's own folder
    void SelectGame(int i)
    {
        if (i < 0 || i >= (int)games.size() || i == iGame) return;
        if (iGame >= 0)
        {
            GameState &old = states[(size_t)iGame];
            old.bSeen = true;
            old.chosenRez = chosenRez;
            old.bAlwaysRez = bAlwaysRez;
            old.bAddOn = bAddOn;
            old.sDataDir = spec.m_sDataDir;
        }

        iGame = i;
        game = games[(size_t)i];
        spec = game.m_Spec;
        bHasAddOn = ltlaunch::HasAddOn(game);
        sSettingsPath = ltlaunch::JoinPath(spec.m_sGameDir, "launcher.ini");

        const GameState &st = states[(size_t)i];
        if (st.bSeen)
        {
            chosenRez = st.chosenRez;
            bAlwaysRez = st.bAlwaysRez;
            bAddOn = st.bAddOn;
            spec.m_sDataDir = st.sDataDir;
        }
        else
        {
            chosenRez.clear();
            bAlwaysRez = bAddOn = false;
            LoadSettings();
        }

        bSplashTried = false;
        sStatus.clear();
        if (!sSelectedPath.empty()) ltlaunch::SaveSelectedGame(sSelectedPath, game.m_sKey);
    }

    // An archive the addon loads, named from its own folder.
    // Its folder is usually inside the game's, but the combined launcher's is beside it
    std::string FromAddOn(const std::string &sPath) const
    {
        if (ltlaunch::IsAbsolutePath(sPath)) return sPath;
        if (ltlaunch::IsAbsolutePath(game.m_AddOn.m_sDir)) return ltlaunch::JoinPath(spec.m_sGameDir, sPath);
        return ltlaunch::JoinPath("..", sPath);
    }

    // Every archive this launch can't start without
    std::vector<std::string> Required() const
    {
        std::vector<std::string> out = ltlaunch::RequiredRez(spec);
        if (UseAddOn()) out.insert(out.end(), game.m_AddOn.m_Rez.begin(), game.m_AddOn.m_Rez.end());
        return out;
    }

    bool HasData(const std::string &sRez) const
    {
        return ltlaunch::FileExists(ltlaunch::JoinPath(ltlaunch::DataDir(spec), sRez));
    }

    void AddRezArgs(std::vector<std::string> &rez, const std::vector<std::string> &names) const
    {
        for (const std::string &sRez : names) rez.push_back(ltlaunch::RezArg(spec, sRez));
    }

    void AddPresent(std::vector<std::string> &rez, const std::vector<std::string> &optional) const
    {
        for (const std::string &sRez : optional)
            if (HasData(sRez)) rez.push_back(ltlaunch::RezArg(spec, sRez));
    }

    // The archives a launch loads. The game's come from the data folder, and Custom's from the game folder.
    // Order: the game's archives and whichever optional ones are present, then the addon's,
    // then the Custom folder itself so loose levels are listed ingame, then the selected custom archives.
    // False when a selected archive is no longer there, with its name stored in sMissing
    bool LaunchRez(std::vector<std::string> &rez, std::string &sMissing) const
    {
        rez.clear();
        AddRezArgs(rez, spec.m_Rez);
        AddPresent(rez, spec.m_OptionalRez);

        if (UseAddOn())
        {
            AddRezArgs(rez, game.m_AddOn.m_Rez);
            AddPresent(rez, game.m_AddOn.m_OptionalRez);
        }

        if (ltlaunch::DirExists(ltlaunch::JoinPath(spec.m_sGameDir, ltlaunch::kCustomFolder)))
            rez.push_back(ltlaunch::kCustomFolder);

        for (const std::string &sRez : chosenRez)
        {
            const std::string sPath = ltlaunch::JoinPath(spec.m_sGameDir, sRez);
            if (!ltlaunch::FileExists(sPath) && !ltlaunch::DirExists(sPath))
            {
                sMissing = sRez;
                return false;
            }
            rez.push_back(sRez);
        }
        return true;
    }

    // The engine saves autoexec.cfg where it runs, so the addon keeps its own, starting from a copy of the game's
    void SeedAddOnConfig() const
    {
        const std::string sOwn = ltlaunch::JoinPath(ltlaunch::AddOnDir(game), "autoexec.cfg");
        const std::string sGame = ltlaunch::JoinPath(spec.m_sGameDir, "autoexec.cfg");
        if (!ltlaunch::FileExists(sOwn) && ltlaunch::FileExists(sGame))
            ltlaunch::WriteTextFile(sOwn, ltlaunch::ReadTextFile(sGame));
    }

    void DoPlay()
    {
        if (!ltlaunch::FirstMissing(ltlaunch::DataDir(spec), Required()).empty()) return;

        ltlaunch::LaunchSpec launch = spec;
        std::string sMissing;
        if (!LaunchRez(launch.m_Rez, sMissing))
        {
            sStatus = sMissing + " was not found";
            return;
        }

        // The addon runs from its own folder
        std::string sCwd = spec.m_sGameDir;
        if (UseAddOn())
        {
            for (std::string &sRez : launch.m_Rez) sRez = FromAddOn(sRez);
            sCwd = ltlaunch::AddOnDir(game);
            SeedAddOnConfig();
        }

        std::vector<std::string> args = ltlaunch::BuildCommandLine(launch);

        // The engine never writes display.cfg, so the addon reads the game's
        if (UseAddOn() && ltlaunch::FileExists(DisplayCfgPath()))
        {
            args.push_back("-display");
            args.push_back(FromAddOn("display.cfg"));
        }

        std::string sError;
        if (!Spawn(args, sCwd, sError))
        { sStatus = sError; return; }
        sStatus.clear();
    }

    bool Down(const Rect &r) const { return pPressed == &r && bPressedOver; }

    // The buttons a screen takes clicks on
    static const std::vector<const Rect *> &ButtonsFor(DialogId eDialog)
    {
        static const std::vector<const Rect *> kMain =
            { &kPlayButton, &kDisplayButton, &kCustomizeButton, &kQuitButton, &kAddOnBox, &kBrowseButton };
        static const std::vector<const Rect *> kDisplay =
            { &kDisplayDlg.ok, &kDisplayDlg.cancel, &kDisplayDlg.close, &kBorderlessBox };
        static const std::vector<const Rect *> kCustomize =
            { &kCustomizeDlg.ok, &kCustomizeDlg.cancel, &kCustomizeDlg.close, &kAddButton, &kRemoveButton,
              &kAlwaysRezBox };

        switch (eDialog)
        {
            case DialogId::Display:   return kDisplay;
            case DialogId::Customize: return kCustomize;
            default:                  return kMain;
        }
    }

    const Rect *ButtonAt(int x, int y) const
    {
        for (const Rect *pButton : ButtonsFor(eOpenDialog))
            if (In(*pButton, x, y) && (pButton != &kAddOnBox || bHasAddOn)) return pButton;
        if (eOpenDialog == DialogId::None)
            for (const Rect &r : gameButtons)
                if (In(r, x, y)) return &r;
        return 0;
    }

    // The game a list button shows, or -1 when it isn't one
    int GameButtonIndex(const Rect *p) const
    {
        for (size_t i = 0; i < gameButtons.size(); ++i)
            if (p == &gameButtons[i]) return (int)i;
        return -1;
    }

    static bool IsOkButton(const Rect *p) { return p == &kDisplayDlg.ok || p == &kCustomizeDlg.ok; }

    void Press(const Rect *p)
    {
        // The game list
        if (GameButtonIndex(p) >= 0) SelectGame(GameButtonIndex(p));
        // Main window
        else if (p == &kPlayButton) DoPlay();
        else if (p == &kDisplayButton) OpenDisplay();
        else if (p == &kCustomizeButton) OpenCustomize();
        else if (p == &kQuitButton) bQuit = true;
        else if (p == &kAddOnBox) ToggleAddOn();
        else if (p == &kBrowseButton) BrowseDataDir();
        // Display dialog
        else if (p == &kBorderlessBox) bBorderless = !bBorderless;
        // Customize dialog
        else if (p == &kAddButton) AddRez();
        else if (p == &kRemoveButton) RemoveRez();
        else if (p == &kAlwaysRezBox) bAlwaysRez = !bAlwaysRez;
        // OK, Cancel or a close box. Cancel and the close boxes put things back
        else CloseDialog(IsOkButton(p));
    }

    void CloseDialog(bool bAccept)
    {
        if (eOpenDialog == DialogId::Display) CloseDisplay(bAccept);
        else if (eOpenDialog == DialogId::Customize) CloseCustomize(bAccept);
        eOpenDialog = DialogId::None;
    }

    // One list in a dialog
    struct ListRef
    {
        const Rect &rect;
        int nItems;
        int &nScroll;
        int &iSel;
    };

    // The open dialog's lists
    std::vector<ListRef> OpenLists()
    {
        if (eOpenDialog == DialogId::Display)
            return { { kSizeList, (int)sizes.size(), nSizeScroll, iSize } };
        if (eOpenDialog == DialogId::Customize)
            return { { kAvailableList, (int)availableRez.size(), nAvailableScroll, iAvailableSel },
                     { kChosenList, (int)chosenRez.size(), nChosenScroll, iChosenSel } };
        return {};
    }

    // Selects the row under a click
    static bool SelectRowAt(ListRef &list, int x, int y)
    {
        const int iRow = ListRowAt(list.rect, list.nItems, list.nScroll, x, y);
        if (iRow < 0) return false;
        if (iRow < list.nItems) list.iSel = iRow;
        return true;
    }

    bool ClickList(int x, int y)
    {
        for (ListRef &list : OpenLists())
            if (SelectRowAt(list, x, y)) return true;
        return false;
    }

    // Scrolls the list under the mouse
    bool ScrollList(int x, int y, int nRows)
    {
        for (ListRef &list : OpenLists())
            if (In(list.rect, x, y))
            {
                list.nScroll = ClampScroll(list.rect, list.nItems, list.nScroll + nRows);
                return true;
            }
        return false;
    }

    std::vector<std::string> SizeRows() const
    {
        std::vector<std::string> rows;
        for (size_t i = 0; i < sizes.size(); ++i) rows.push_back(ltlaunch::SizeText(sizes[i]));
        return rows;
    }

    std::string DisplayCfgPath() const { return ltlaunch::JoinPath(spec.m_sGameDir, "display.cfg"); }

    // Takes the size and borderless setting from display.cfg
    void ReadDisplayCfg()
    {
        const std::string sCfg = ltlaunch::ReadTextFile(DisplayCfgPath());
        std::string sWidth, sHeight, sBorderless;
        ltlaunch::WindowSize fileSize = { 0, 0 };
        if (ltlaunch::GetConfigValue(sCfg, "ScreenWidth", sWidth) &&
            ltlaunch::GetConfigValue(sCfg, "ScreenHeight", sHeight))
        {
            fileSize.m_nWidth = std::atoi(sWidth.c_str());
            fileSize.m_nHeight = std::atoi(sHeight.c_str());
            std::vector<ltlaunch::WindowSize> all = sizes;
            all.push_back(fileSize);
            sizes = ltlaunch::DistinctSizes(all);
        }

        iSize = -1;
        for (size_t i = 0; i < sizes.size(); ++i)
            if (sizes[i].m_nWidth == fileSize.m_nWidth && sizes[i].m_nHeight == fileSize.m_nHeight)
                iSize = (int)i;

        bBorderless = ltlaunch::GetConfigValue(sCfg, "BorderlessWindow", sBorderless) &&
                      std::atoi(sBorderless.c_str()) != 0;
    }

    void OpenDisplay()
    {
        ReadDisplayCfg();
        iSizeOpen = iSize;
        bBorderlessOpen = bBorderless;
        nSizeScroll = ClampScroll(kSizeList, (int)sizes.size(), iSize);
        eOpenDialog = DialogId::Display;
    }

    // OK writes what changed into display.cfg
    void CloseDisplay(bool bAccept)
    {
        if (!bAccept) { iSize = iSizeOpen; bBorderless = bBorderlessOpen; return; }

        std::vector<ltlaunch::ConfigValue> values;
        if (iSize != iSizeOpen && iSize >= 0)
        {
            values.push_back({ "ScreenWidth", std::to_string(sizes[(size_t)iSize].m_nWidth) });
            values.push_back({ "ScreenHeight", std::to_string(sizes[(size_t)iSize].m_nHeight) });
        }
        if (bBorderless != bBorderlessOpen)
            values.push_back({ "BorderlessWindow", bBorderless ? "1" : "0" });
        if (values.empty()) return;

        const std::string sCfg = ltlaunch::SetConfigValues(ltlaunch::ReadTextFile(DisplayCfgPath()), values);
        if (ltlaunch::WriteTextFile(DisplayCfgPath(), sCfg)) sStatus.clear();
        else sStatus = "Could not write " + DisplayCfgPath();
    }

    void OpenCustomize()
    {
        availableRez = ltlaunch::AvailableRez(spec);
        iAvailableSel = iChosenSel = -1;
        nAvailableScroll = nChosenScroll = 0;
        chosenRezOnOpen = chosenRez;
        bAlwaysRezOnOpen = bAlwaysRez;
        eOpenDialog = DialogId::Customize;
    }

    // OK saves the checkbox, and the chosen archives with it when it's ticked
    void CloseCustomize(bool bAccept)
    {
        if (!bAccept)
        {
            chosenRez = chosenRezOnOpen;
            bAlwaysRez = bAlwaysRezOnOpen;
            return;
        }

        SaveSettings();
    }

    void SaveSettings()
    {
        ltlaunch::LauncherSettings settings;
        settings.m_bAlwaysRez = bAlwaysRez;
        settings.m_AlwaysRez = chosenRez;
        settings.m_bAddOn = bAddOn;
        settings.m_sDataDir = spec.m_sDataDir;
        if (!ltlaunch::SaveSettings(sSettingsPath, settings)) sStatus = "Could not write " + sSettingsPath;
    }

    // A ticked checkbox from an earlier run brings its archives back
    void LoadSettings()
    {
        ltlaunch::LauncherSettings settings;
        if (!ltlaunch::LoadSettings(sSettingsPath, settings)) return;
        bAlwaysRez = settings.m_bAlwaysRez;
        if (bAlwaysRez) chosenRez = settings.m_AlwaysRez;
        bAddOn = settings.m_bAddOn;
        spec.m_sDataDir = settings.m_sDataDir;
    }

    // A standard install's folder to read the archives from. The game folder goes back to the default
    void BrowseDataDir()
    {
        std::string sDir, sError;
        if (!PickFolder(pWnd, ltlaunch::DataDir(spec), sDir, sError))
        {
            if (!sError.empty()) sStatus = sError;
            return;
        }

        spec.m_sDataDir = ltlaunch::SameDir(sDir, spec.m_sGameDir) ? std::string() : sDir;
        bSplashTried = false;
        sStatus.clear();
        SaveSettings();
    }

    // The addon has its own banner, so the splash is loaded again
    void ToggleAddOn()
    {
        bAddOn = !bAddOn;
        bSplashTried = false;
        SaveSettings();
    }

    void AddRez()
    {
        if (iAvailableSel < 0 || iAvailableSel >= (int)availableRez.size()) return;
        const std::string &sRez = availableRez[(size_t)iAvailableSel];
        if (!ltlaunch::ContainsNoCase(chosenRez, sRez)) chosenRez.push_back(sRez);
    }

    void RemoveRez()
    {
        if (iChosenSel < 0 || iChosenSel >= (int)chosenRez.size()) return;
        chosenRez.erase(chosenRez.begin() + iChosenSel);

        if (iChosenSel >= (int)chosenRez.size()) iChosenSel = (int)chosenRez.size() - 1;
        nChosenScroll = ClampScroll(kChosenList, (int)chosenRez.size(), nChosenScroll);
    }

    void Draw()
    {
        Fill(Rect(0, 0, kWinW, kWinH), kFace);

        // The folder the archives are read from
        Fill(kDataDirBox, kWindow);
        Frame(kDataDirBox, kBorder);
        Text(kDataDirBox.x + 4, kDataDirBox.y + (kDataDirBox.h - g_nLineH) / 2,
             FitLeft(ltlaunch::DataDir(spec), kDataDirBox.w - 8), kText, kDataDirBox.w - 8);
        Button(kBrowseButton, "Browse...", Down(kBrowseButton));

        if (kSideW > 0) DrawGameList();

        const Rect panel(kSideW + 12, 40, 380, 290);
        Fill(panel, kWindow);
        Frame(panel, kBorder);

        const std::vector<std::string> required = Required();
        std::vector<bool> present;
        bool bMissing = false;
        for (const std::string &sRez : required)
        {
            present.push_back(HasData(sRez));
            bMissing = bMissing || !present.back();
        }
        if (bMissing)
        {
            DrawDataMissing(panel, required, present);
            bSplashTried = false; // loaded again once the archives are back
        }
        else
            DrawSplash(panel);

        // An addon without a label of its own launches under the plain one
        const bool bAddOnLabel = UseAddOn() && !game.m_AddOn.m_sButton.empty();
        Button(kPlayButton, bAddOnLabel ? game.m_AddOn.m_sButton : "Launch", Down(kPlayButton));
        Button(kDisplayButton, "Display...", Down(kDisplayButton));
        Button(kCustomizeButton, "Customize...", Down(kCustomizeButton));
        Button(kQuitButton, "Quit", Down(kQuitButton));
        if (bHasAddOn) CheckBox(kAddOnBox, game.m_AddOn.m_sLabel, bAddOn, Down(kAddOnBox));

        // A refused or failed launch
        Text(kSideW + 12, 366, sStatus, kAlert, kMainW - 24);

        // The footer
        const std::string sLine = std::string(kLauncherName) + " v" + g_szLauncherBuild;
        Text((kWinW - TextW(sLine)) / 2, 440, sLine, kText);

        if (eOpenDialog == DialogId::Display) DrawDisplayDialog();
        else if (eOpenDialog == DialogId::Customize) DrawCustomizeDialog();
    }

    // The combined launcher's games, the shown one drawn as held down, then a rule between the list and the game
    void DrawGameList()
    {
        const std::string sHead = "Games";
        const int hx = kGamesHeading.x + (kGamesHeading.w - TextW(sHead)) / 2;
        const int hy = kGamesHeading.y + (kGamesHeading.h - g_nLineH) / 2;
        Text(hx, hy, sHead, kText);
        Fill(Rect(hx, hy + g_nLineH, TextW(sHead), 1), kText);

        for (size_t i = 0; i < gameButtons.size(); ++i)
        {
            const Rect &r = gameButtons[i];
            if ((int)i == iGame && !Down(r))
            {
                Fill(r, kButtonDown);
                Frame(r, kBorderDown);
                TextC(r, games[i].m_sName, kText);
            }
            else
                Button(r, games[i].m_sName, Down(r));
        }

        Fill(Rect(kSideW - 1, 12, 1, kWinH - 24), kBorder);
    }

    void LoadSplash()
    {
        bSplashTried = true;
        if (pSplash) { SDL_DestroyTexture(pSplash); pSplash = 0; }

        // The addon's banner, or the game's when it has none
        std::vector<unsigned char> raw;
        ltlaunch::Image img;
        const bool bAddOnSplash = UseAddOn() &&
            ltlaunch::ReadRezEntry(ltlaunch::JoinPath(ltlaunch::DataDir(spec), game.m_AddOn.m_sSplashRez),
                                   game.m_AddOn.m_sSplashPath, raw) &&
            ltlaunch::DecodePcx(raw, img);
        if (!bAddOnSplash &&
            (!ltlaunch::ReadRezEntry(ltlaunch::JoinPath(ltlaunch::DataDir(spec), game.m_sSplashRez), game.m_sSplashPath, raw) ||
             !ltlaunch::DecodePcx(raw, img)))
            return;

        pSplash = SDL_CreateTexture(g_pRen, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC,
                                    img.m_nWidth, img.m_nHeight);
        if (!pSplash) return;
        SDL_UpdateTexture(pSplash, 0, &img.m_RGB[0], img.m_nWidth * 3);
        SDL_SetTextureScaleMode(pSplash, SDL_ScaleModeLinear);
    }

    void DrawSplash(const Rect &panel)
    {
        if (!bSplashTried) LoadSplash();
        if (!pSplash) return;

        int nW = 0, nH = 0;
        SDL_QueryTexture(pSplash, 0, 0, &nW, &nH);
        const Rect in(panel.x + 1, panel.y + 1, panel.w - 2, panel.h - 2);
        int w = in.w, h = nH * in.w / nW;
        if (h > in.h) { h = in.h; w = nW * in.h / nH; }
        Fill(in, 0x000000);
        SDL_Rect dst = { in.x + (in.w - w) / 2, in.y + (in.h - h) / 2, w, h };
        SDL_RenderCopy(g_pRen, pSplash, 0, &dst);
    }

    // Names each archive the game needs, whether it was found, and the folder they belong in
    void DrawDataMissing(const Rect &panel, const std::vector<std::string> &required, const std::vector<bool> &present)
    {
        const Rect in(panel.x + 3, panel.y + 3, panel.w - 6, panel.h - 6);
        const int nMaxW = in.w - 8;
        int y = in.y + 12;

        const std::string sHead = "GAME DATA MISSING";
        const int hx = in.x + (in.w - TextW(sHead)) / 2;
        Text(hx, y, sHead, kAlert);
        Text(hx + 1, y, sHead, kAlert);
        y += g_nLineH + 10;

        Text(in.x + 4, y, spec.m_sDataDir.empty() ? "This game's archives were not found. Copy them into:"
                                                  : "This game's archives were not found in:", kText, nMaxW);
        y += g_nLineH + 4;

        std::string sDir = ltlaunch::DataDir(spec);
        while (!sDir.empty())
        {
            size_t n = 1;
            while (n < sDir.size() && TextW(sDir.substr(0, n + 1)) <= nMaxW) ++n;
            if (n < sDir.size())
            {
                const size_t cut = sDir.find_last_of("\\/", n - 1);
                if (cut != std::string::npos && cut > 0) n = cut + 1;
            }
            Text(in.x + 4, y, sDir.substr(0, n), kText, nMaxW);
            sDir.erase(0, n);
            y += g_nLineH;
        }
        y += 8;

        // Wrapped at spaces to the panel
        std::string sBrowse = spec.m_sDataDir.empty()
            ? "Or press Browse... to point to your copy of the game."
            : "Press Browse... to point to your copy of the game.";
        while (!sBrowse.empty())
        {
            size_t n = sBrowse.size();
            while (n > 0 && TextW(sBrowse.substr(0, n)) > nMaxW)
            {
                const size_t cut = sBrowse.find_last_of(' ', n - 1);
                n = (cut == std::string::npos || cut == 0) ? n - 1 : cut;
            }
            if (n == 0) n = 1;
            Text(in.x + 4, y, sBrowse.substr(0, n), kText, nMaxW);
            sBrowse.erase(0, n);
            while (!sBrowse.empty() && sBrowse[0] == ' ') sBrowse.erase(0, 1);
            y += g_nLineH;
        }
        y += 8;

        for (size_t i = 0; i < required.size() && y + g_nLineH <= in.y + in.h; ++i)
        {
            Text(in.x + 12, y, present[i] ? "found" : "missing", present[i] ? kGrayTxt : kAlert);
            Text(in.x + 84, y, required[i], present[i] ? kGrayTxt : kText, in.w - 88);
            y += g_nLineH;
        }
    }

    // The frame, caption bar, close box, OK and Cancel every dialog shares
    void DrawDialog(const Dialog &d, const std::string &sTitle)
    {
        Fill(d.frame, kFace);
        Frame(d.frame, kBorder);
        Fill(d.caption, kCaption);
        Text(d.caption.x + 6, d.caption.y + (d.caption.h - g_nLineH) / 2, sTitle, kCapText, d.caption.w - 32);
        Button(d.close, "x", Down(d.close));
        Button(d.ok, "OK", Down(d.ok));
        Button(d.cancel, "Cancel", Down(d.cancel));
    }

    void DrawDisplayDialog()
    {
        DrawDialog(kDisplayDlg, "Display Settings");
        Text(kSizeList.x, kSizeList.y - 18, "Window size:", kText);
        ListBox(kSizeList, SizeRows(), iSize, nSizeScroll);
        CheckBox(kBorderlessBox, "Borderless window", bBorderless, Down(kBorderlessBox));
    }

    void DrawCustomizeDialog()
    {
        DrawDialog(kCustomizeDlg, "Customize " + game.m_sTitle);
        Text(kAvailableList.x, kAvailableList.y - 16, "Available rez files:", kText);
        ListBox(kAvailableList, availableRez, iAvailableSel, nAvailableScroll);
        Button(kAddButton, "Add >", Down(kAddButton));
        Button(kRemoveButton, "< Remove", Down(kRemoveButton));
        Text(kChosenList.x, kChosenList.y - 16, "Rez files to load:", kText);
        ListBox(kChosenList, chosenRez, iChosenSel, nChosenScroll);
        CheckBox(kAlwaysRezBox, "Always load these rez files", bAlwaysRez, Down(kAlwaysRezBox));
    }
};

} // namespace

int LauncherMain()
{
#ifdef _WIN32
    SDL_SetMainReady();
#endif

    // A click that activates the window still reaches the buttons
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

#if defined(_WIN32) && defined(LTLAUNCH_COMBINED)
    // The window and taskbar take the exe's own icon, resource 101 in ltcallisto.rc
    SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON, "101");
    SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON_SMALL, "101");
#endif

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;

    // Lithtech.exe and the game directory are found from where the launcher runs
    char *pBasePath = SDL_GetBasePath();
    const std::string sExeDir = pBasePath ? pBasePath : ".";
    if (pBasePath) SDL_free(pBasePath);

    App app;
    int iStart = 0;
#ifdef LTLAUNCH_COMBINED
    app.games = ltlaunch::CombinedProfiles(sExeDir);
    app.sSelectedPath = ltlaunch::JoinPath(sExeDir, "launcher.ini");
    const std::string sSelected = ltlaunch::LoadSelectedGame(app.sSelectedPath);
    for (size_t i = 0; i < app.games.size(); ++i)
        if (app.games[i].m_sKey == sSelected) iStart = (int)i;
    const std::string sCaption = kLauncherName;
#else
    app.games.push_back(ltlaunch::LTLAUNCH_PROFILE(sExeDir));
    const std::string sCaption = app.games[0].m_sCaption;
#endif
    app.states.resize(app.games.size());
    if (kSideW > 0)
        for (size_t i = 0; i < app.games.size(); ++i)
            app.gameButtons.push_back(Rect(12, kGameButtonY + (int)i * kGameButtonStep, kSideW - 24, 26));
    app.SelectGame(iStart);

    app.sizes = DesktopSizes();

    SDL_Window *pWnd = SDL_CreateWindow(sCaption.c_str(),
                                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        kWinW, kWinH, SDL_WINDOW_SHOWN);
    if (!pWnd) { SDL_Quit(); return 1; }
    app.pWnd = pWnd;
    g_pRen = SDL_CreateRenderer(pWnd, -1, SDL_RENDERER_SOFTWARE);
    if (!g_pRen) { SDL_DestroyWindow(pWnd); SDL_Quit(); return 1; }
    g_pFont = MakeFontAtlas();
    if (!g_pFont)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, sCaption.c_str(),
                                 "Could not load the Tahoma font.", pWnd);
        SDL_DestroyRenderer(g_pRen);
        SDL_DestroyWindow(pWnd);
        SDL_Quit();
        return 1;
    }

    bool bRedraw = true;
    while (!app.bQuit)
    {
        if (bRedraw)
        {
            app.Draw();
            SDL_RenderPresent(g_pRen);
            bRedraw = false;
        }

        SDL_Event e;
        if (!SDL_WaitEvent(&e)) break;
        do
        {
            if (e.type == SDL_QUIT)
                app.bQuit = true;
            else if (e.type == SDL_WINDOWEVENT && (e.window.event == SDL_WINDOWEVENT_EXPOSED ||
                                                   e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED))
                bRedraw = true;
            else if (e.type == SDL_KEYDOWN && app.eOpenDialog != App::DialogId::None &&
                     (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_RETURN ||
                      e.key.keysym.sym == SDLK_KP_ENTER))
            {
                const bool bAccept = e.key.keysym.sym != SDLK_ESCAPE;
                app.CloseDialog(bAccept);
                app.pPressed = 0;
                bRedraw = true;
            }
            else if (e.type == SDL_MOUSEWHEEL)
            {
                int mx = 0, my = 0;
                SDL_GetMouseState(&mx, &my);
                if (app.ScrollList(mx, my, -e.wheel.y * 3)) bRedraw = true;
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
            {
                if (app.ClickList(e.button.x, e.button.y))
                    bRedraw = true;
                else
                {
                    app.pPressed = app.ButtonAt(e.button.x, e.button.y);
                    app.bPressedOver = true;
                    if (app.pPressed) bRedraw = true;
                }
            }
            else if (e.type == SDL_MOUSEMOTION && app.pPressed)
            {
                const bool bOver = In(*app.pPressed, e.motion.x, e.motion.y);
                if (bOver != app.bPressedOver) { app.bPressedOver = bOver; bRedraw = true; }
            }
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && app.pPressed)
            {
                const Rect *p = app.pPressed;
                app.pPressed = 0;
                if (In(*p, e.button.x, e.button.y)) app.Press(p);
                bRedraw = true;
            }
        } while (SDL_PollEvent(&e));
    }

    if (app.pSplash) SDL_DestroyTexture(app.pSplash);
    if (g_pFont) SDL_DestroyTexture(g_pFont);
    SDL_DestroyRenderer(g_pRen);
    SDL_DestroyWindow(pWnd);
    SDL_Quit();
    return 0;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return LauncherMain();
}
#else
int main()
{
    return LauncherMain();
}
#endif
