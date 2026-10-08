#include "launcher_core.h"
#include "launcher_splash.h"

#ifdef _WIN32
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>

#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../runtime/ui/src/stb_truetype.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

// The launcher's name and version
const char *kLauncherName    = "LithLauncher";
const char *kLauncherVersion = "0.01";
const char *kGameTitle       = "Shogo: Mobile Armor Division";
const char *kSplashRez       = "SHOGO.REZ";
const char *kSplashPath      = "INTERFACE\\SPLASH.PCX";

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

const int kWinW = 640, kWinH = 470;

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

const Rect kPlayButton(406, 12, 222, 26);
const Rect kQuitButton(406, 330, 222, 26);
const Rect kDisplayButton(406, 52, 222, 26);
const Rect kCustomizeButton(406, 86, 222, 26);

const int kCaptionH = 22;
const int kCloseSize = 18;
const int kDlgButtonW = 80, kDlgButtonH = 24;
const int kDlgButtonGap = 8; // Gap between OK and Cancel and between Cancel and the frame's edge
const int kDlgBottomMargin = 10; // Margin below OK and Cancel

// A dialog centred over the window, with its caption bar, close box, OK and Cancel
struct Dialog
{
    Rect frame, caption, close, ok, cancel;

    Dialog(int w, int h)
        : frame((kWinW - w) / 2, (kWinH - h) / 2, w, h),
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
    ltlaunch::LaunchSpec spec;
    std::string sStatus;
    bool bQuit = false;

    enum class DialogId { None, Display, Customize };
    DialogId eOpenDialog = DialogId::None;

    const Rect *pPressed = 0;
    bool bPressedOver = false;

    SDL_Texture *pSplash = 0;
    bool bSplashTried = false;

    std::vector<ltlaunch::WindowSize> sizes;
    int  iSize = -1, iSizeOpen = -1;
    bool bBorderless = false, bBorderlessOpen = false;
    int  nSizeScroll = 0;

    // Extra archives loaded after the game's own
    // The 'OnOpen' copy is what Cancel puts back
    std::vector<std::string> availableRez, chosenRez, chosenRezOnOpen;
    int iAvailableSel = -1, iChosenSel = -1;
    int nAvailableScroll = 0, nChosenScroll = 0;

    // The archives a launch loads
    // Order: the game's archives, then the Custom folder itself so loose levels are listed ingame, then the selected custom archives.
    // False when a selected archive is no longer there, with its name stored in sMissing
    bool LaunchRez(std::vector<std::string> &rez, std::string &sMissing) const
    {
        rez = spec.m_Rez;

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

    void DoPlay()
    {
        if (!ltlaunch::FirstMissing(spec).empty()) return;

        ltlaunch::LaunchSpec launch = spec;
        std::string sMissing;
        if (!LaunchRez(launch.m_Rez, sMissing))
        {
            sStatus = sMissing + " was not found";
            return;
        }

        std::string sError;
        if (!Spawn(ltlaunch::BuildCommandLine(launch), spec.m_sGameDir, sError))
        { sStatus = sError; return; }
        sStatus.clear();
    }

    bool Down(const Rect &r) const { return pPressed == &r && bPressedOver; }

    // The buttons a screen takes clicks on
    static const std::vector<const Rect *> &ButtonsFor(DialogId eDialog)
    {
        static const std::vector<const Rect *> kMain =
            { &kPlayButton, &kDisplayButton, &kCustomizeButton, &kQuitButton };
        static const std::vector<const Rect *> kDisplay =
            { &kDisplayDlg.ok, &kDisplayDlg.cancel, &kDisplayDlg.close, &kBorderlessBox };
        static const std::vector<const Rect *> kCustomize =
            { &kCustomizeDlg.ok, &kCustomizeDlg.cancel, &kCustomizeDlg.close, &kAddButton, &kRemoveButton };

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
            if (In(*pButton, x, y)) return pButton;
        return 0;
    }

    static bool IsOkButton(const Rect *p) { return p == &kDisplayDlg.ok || p == &kCustomizeDlg.ok; }

    void Press(const Rect *p)
    {
        // Main window
        if (p == &kPlayButton) DoPlay();
        else if (p == &kDisplayButton) OpenDisplay();
        else if (p == &kCustomizeButton) OpenCustomize();
        else if (p == &kQuitButton) bQuit = true;
        // Display dialog
        else if (p == &kBorderlessBox) bBorderless = !bBorderless;
        // Customize dialog
        else if (p == &kAddButton) AddRez();
        else if (p == &kRemoveButton) RemoveRez();
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
        eOpenDialog = DialogId::Customize;
    }

    void CloseCustomize(bool bAccept)
    {
        if (!bAccept) chosenRez = chosenRezOnOpen;
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

        const Rect title(12, 12, 380, 22);
        Fill(title, kWindow);
        Frame(title, kBorder);
        Text(title.x + 4, title.y + (title.h - g_nLineH) / 2, kGameTitle, kText, title.w - 8);

        const Rect panel(12, 40, 380, 290);
        Fill(panel, kWindow);
        Frame(panel, kBorder);

        std::vector<bool> present;
        bool bMissing = false;
        for (size_t i = 0; i < spec.m_Rez.size(); ++i)
        {
            present.push_back(ltlaunch::FileExists(ltlaunch::JoinPath(spec.m_sGameDir, spec.m_Rez[i])));
            bMissing = bMissing || !present.back();
        }
        if (bMissing)
        {
            DrawDataMissing(panel, present);
            bSplashTried = false; // loaded again once the archives are back
        }
        else
            DrawSplash(panel);

        Button(kPlayButton, "Launch", Down(kPlayButton));
        Button(kDisplayButton, "Display...", Down(kDisplayButton));
        Button(kCustomizeButton, "Customize...", Down(kCustomizeButton));
        Button(kQuitButton, "Quit", Down(kQuitButton));

        // A refused or failed launch
        Text(12, 366, sStatus, kAlert, kWinW - 24);

        // The footer
        const std::string sLine = std::string(kLauncherName) + " v" + kLauncherVersion;
        Text((kWinW - TextW(sLine)) / 2, 440, sLine, kText);

        if (eOpenDialog == DialogId::Display) DrawDisplayDialog();
        else if (eOpenDialog == DialogId::Customize) DrawCustomizeDialog();
    }

    void LoadSplash()
    {
        bSplashTried = true;
        if (pSplash) { SDL_DestroyTexture(pSplash); pSplash = 0; }

        std::vector<unsigned char> raw;
        ltlaunch::Image img;
        if (!ltlaunch::ReadRezEntry(ltlaunch::JoinPath(spec.m_sGameDir, kSplashRez), kSplashPath, raw) ||
            !ltlaunch::DecodePcx(raw, img))
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
    void DrawDataMissing(const Rect &panel, const std::vector<bool> &present)
    {
        const Rect in(panel.x + 3, panel.y + 3, panel.w - 6, panel.h - 6);
        const int nMaxW = in.w - 8;
        int y = in.y + 12;

        const std::string sHead = "GAME DATA MISSING";
        const int hx = in.x + (in.w - TextW(sHead)) / 2;
        Text(hx, y, sHead, kAlert);
        Text(hx + 1, y, sHead, kAlert);
        y += g_nLineH + 10;

        Text(in.x + 4, y, "This game's archives were not found. Copy them into:", kText, nMaxW);
        y += g_nLineH + 4;

        std::string sDir = spec.m_sGameDir;
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

        for (size_t i = 0; i < spec.m_Rez.size() && y + g_nLineH <= in.y + in.h; ++i)
        {
            Text(in.x + 12, y, present[i] ? "found" : "missing", present[i] ? kGrayTxt : kAlert);
            Text(in.x + 84, y, spec.m_Rez[i], present[i] ? kGrayTxt : kText, in.w - 88);
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
        DrawDialog(kCustomizeDlg, std::string("Customize ") + kGameTitle);
        Text(kAvailableList.x, kAvailableList.y - 16, "Available rez files:", kText);
        ListBox(kAvailableList, availableRez, iAvailableSel, nAvailableScroll);
        Button(kAddButton, "Add >", Down(kAddButton));
        Button(kRemoveButton, "< Remove", Down(kRemoveButton));
        Text(kChosenList.x, kChosenList.y - 16, "Rez files to load:", kText);
        ListBox(kChosenList, chosenRez, iChosenSel, nChosenScroll);
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

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;

    // Lithtech.exe and the game directory are found from where the launcher runs
    char *pBasePath = SDL_GetBasePath();
    const std::string sExeDir = pBasePath ? pBasePath : ".";
    if (pBasePath) SDL_free(pBasePath);

    App app;
    app.spec = ltlaunch::ShogoLaunch(sExeDir);

    app.sizes = DesktopSizes();

    SDL_Window *pWnd = SDL_CreateWindow(kLauncherName,
                                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        kWinW, kWinH, SDL_WINDOW_SHOWN);
    if (!pWnd) { SDL_Quit(); return 1; }
    g_pRen = SDL_CreateRenderer(pWnd, -1, SDL_RENDERER_SOFTWARE);
    if (!g_pRen) { SDL_DestroyWindow(pWnd); SDL_Quit(); return 1; }
    g_pFont = MakeFontAtlas();
    if (!g_pFont)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kLauncherName,
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
