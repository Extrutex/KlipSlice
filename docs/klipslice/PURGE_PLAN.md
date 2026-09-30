# KLIPSLICE: removal plan in order

Repo `Extrutex/KlipSlice` (origin), branch `klipslice`, HEAD `e7f2383`. The tree is clean apart from the untracked `docs/design/`. All line numbers are from this commit, so run the grep again before each edit.

## 0. Facts checked in the code that change the brief

| Claim | What the code shows | Effect |
|---|---|---|
| "SLIC3R_GUI=OFF builds the engine" | Only true for `libslic3r` and its tests. `src/OrcaSlicer.cpp:84-99` includes `wx/*`, `slic3r/GUI/{PartPlate,GLCanvas3D,Plater,…}.hpp` and GLFW with no guard. `src/CMakeLists.txt:16,187` gates the GUI lib. | The headless gate covers only libslic3r and the kernel tests. The CLI and the app always need the full GUI build. |
| CI can act as a gate | `build_all.yml:6` runs only on `workflow_dispatch` | Commit A1 adds a push gate. Until then every gate is a manual dispatch. |
| BBL profiles are gone | `tests/cli/CMakeLists.txt:21` still points at `resources/profiles/BBL` | This is already broken. A1 fixes it. |
| `is_bbl_vendor()` is dead | `PresetBundle.cpp:942` returns `Marlin_BBL` only when the vendor is literally named `"BBL"` | Every `is_bbl_vendor` branch is already dead at runtime, so folding them is mechanical. |
| Agent default | `AppConfig.cpp:676-679` sets `use_printer_agents=false`, and `GUI_App.cpp:4026` falls back to `ORCA_PRINTER_AGENT_ID` | Klipper uploads today go through PrintHost→`Moonraker.cpp`, not through NetworkAgent. |
| Moonraker agent depends on Bambu device code | `MoonrakerPrinterAgent.cpp:6-10` includes DeviceManager, DevFilaSystem, DevStorage and DevFirmware | DeviceManager and DeviceCore stay for this whole series. |
| Profiles use other agents and hosts | printer_agent: crealityprint 26, qidi 8, snapmaker 1, moonraker 1. host_type: octoprint 107, crealityprint 26, elegoolink 9. | Creality and Elegoo are owner decisions (§3). OctoPrint gets mapped to Moonraker. |
| Upstream catalog | `.mo` output is `OrcaSlicer.mo` (`CMakeLists.txt:942`), but lookup uses `SLIC3R_APP_KEY` (`GUI_App.cpp:7985,8154`) | Changing the key without adding a domain macro turns the German UI into English. |
| Windows installer | `CPACK_PACKAGE_INSTALL_REGISTRY_KEY "OrcaSlicer"` together with `UNINSTALL_BEFORE_INSTALL ON` (`CMakeLists.txt:1335-1336`) | A KLIPSLICE installer would uninstall the user's OrcaSlicer, so the rebrand must land before any installer is built. |

## 1. Conflicts between the specialists and how they are resolved

1. **BBLTopbar.** Build area: delete (src/slic3r/CMakeLists.txt:46-53). Device area: keep. **Keep.** `MainFrame.cpp` and `MainFrame.hpp` use it and it hosts the Calibration menu. Only the unused includes at `BBLTopbar.hpp:6-7` go.
2. **PrintJob/SendJob.** Build area: delete as the "BBL send path". Network and device areas: keep. **Keep.** `Plater.hpp:24-25` includes both, and the Moonraker agent implements `start_local_print` and `start_send_gcode_to_sdcard`.
3. **`bambu_networking.hpp`** (CMake line 704). Build area: delete. Network area: keep the types. **Keep the file.** Only the library/ABI block (:99, :410-553) goes, in E1.
4. **ConfigWizard.** Flavor area: edit PageFirmware. Consumer area: delete the class. **Delete it.** The only `new ConfigWizard` calls are commented out (`Plater.cpp:2704`, `GUI_App.cpp:9463`), so editing PageFirmware is pointless.
5. **Creality.** Hosts area: delete `CrealityPrint.*`. Network area: keep `CrealityPrintAgent`. **Keep both** until the maintainer decides (§3 D2). `CrealityPrintAgent.cpp:2` includes `CrealityPrint`, and 26 profiles use both keys.
6. **UpgradeNetworkJob / DownloadProgressDialog ordering.** `UpgradeNetworkJob.cpp:61,89` calls `download_plugin` and `install_plugin`. DownloadProgressDialog is used only for plugin downloads (`GUI_App.cpp:2007,2054,4777`, `Preferences.cpp:1361`) and as the base of `MediaPlayCtrl`. **Decision:** the camera goes first (D2) and leaves both in place. The plugin commit (E1) then deletes both. This way nothing is orphaned.
7. **Wizard vs plugin.** `WebGuideDialog::DownloadPlugin/InstallPlugin` (:1917-1940) call `download_plugin`. **Decision:** trim the wizard (F1) **before** E1.
8. **UserManager.** Build area puts it in the plugin commit. It is cloud login: only `GUI_App.*` and `test_user_manager.cpp` use it. **Decision:** it goes in E4 (BBL login).
9. **deps_src/mdns.** Its only users are the Creality discovery files. **Keep it** while Creality stays.
10. **FFmpeg.** The source and link blocks go with the camera (D2). The `deps/FFMPEG` removal is batched into a single deps commit (H5), so the deps cache is invalidated only once.

## 2. Commit sequence

Gate legend:
- **L** = Linux x86_64 build plus `libslic3r_tests` (automatic after A1)
- **LH** = headless `-DSLIC3R_GUI=OFF` libslic3r_tests
- **W/M** = manual Windows x64 / macOS arm64 dispatch
- **grep0** = the listed symbols must have zero hits in `src/ tests/`

Every commit that removes files also removes their `src/slic3r/CMakeLists.txt` entries in the same commit.

### Phase A: foundation and rebrand (so KLIPSLICE never overwrites OrcaSlicer's data)

**A1 `ci: add per-push Linux gate, fix stale BBL test path`**
- Edits:
  - `build_all.yml`: add a push/PR trigger for `klipslice` that runs one `ubuntu-24.04` x64 job (`build_linux.sh -sr` + `libslic3r_tests` + slice check). aarch64, flatpak, Windows arm64, macOS x86_64 and universal become opt-in. Move the slice check (:159) off `ubuntu-24.04-arm`.
  - `tests/cli/CMakeLists.txt:21`: remove the BBL path.
  - `check_profiles.yml:7-9`: add branch `klipslice` and drop the `bambu_filament_ids.json` validation (:12).
- Buildable because only CI and one test path change.
- Platforms: CI only.
- Gate: first green L run. **Size:** 3 files, about 80 lines.

**A2 `assets: add KLIPSLICE icon set`**
- Adds `resources/images/KLIPSLICE_{32..192}px.png`, `KLIPSLICE.ico` and `KLIPSLICE.icns`. Placeholder copies of the Orca icons are fine for now.
- Buildable because nothing references the files yet.
- Platforms: prepares all three.
- Gate: L. **Size:** about 8 binary files.

**A3 `i18n: fixed gettext domain independent of app key`**
- Adds `#define SLIC3R_L10N_DOMAIN "OrcaSlicer"` (in `libslic3r.h`). Use it at `GUI_App.cpp:7985,8154`, `Preferences.cpp:377` and `CAD/DesignPanel.cpp:6467`.
- No behaviour change. Buildable because it only adds a macro.
- Gate: L, plus grep that `SLIC3R_APP_KEY` is no longer used for catalogs. **Size:** 4 files, about 10 lines.

**A4 `rebrand: KLIPSLICE app key, data dir, binaries, bundle`** (atomic: the key, the macOS executable and the AppImage names are coupled)
- Core:
  - `version.inc:4-5`: `SLIC3R_APP_NAME`/`KEY` → `KLIPSLICE`. This moves the data dir to `%APPDATA%\KLIPSLICE`, `~/Library/Application Support/KLIPSLICE` and `~/.config/KLIPSLICE`, and the config file to `KLIPSLICE.conf`.
  - `libslic3r.h:5-7`: app full name.
  - `AppConfig.cpp:43-44`: update URLs blank (or pointing at the KlipSlice releases, see D5).
- Windows:
  - `src/CMakeLists.txt:222`: wrapper exe name `klipslice.exe`.
  - `OrcaSlicer.rc.in:9,16,24`.
  - `Process.cpp:43,106`.
  - `build_win.bat:797-798`.
- macOS:
  - `src/CMakeLists.txt`: `OUTPUT_NAME KLIPSLICE` on APPLE, plus the `.app` paths at :321, :336-345.
  - `cmake/modules/MacOSXBundleInfo.plist.in:14`: bundle id `de.3dwindt.KLIPSLICE`. Also the icon names at :68/85/102.
  - `CMakeLists.txt:280`.
  - `build_release_macos.sh`: 17 `.app` references.
  - `build_orca.yml`: artifact paths.
  - Delete the dead `src/dev-utils/platform/osx/Info.plist.in` and its configure line at `src/CMakeLists.txt:135`.
- Linux:
  - `src/CMakeLists.txt:166`: binary `klipslice`.
  - `CMakeLists.txt:1253` (APP_CMD), :1268 (`share/KLIPSLICE`), :1275-1287.
  - `build_appimage.sh.in:19-41`: prefix `de.3dwindt.` and the renamed metainfo.
  - Rename `scripts/flatpak/*.metainfo.xml`.
- Buildable because every name is changed at every place that consumes it in the same commit. Target names (`OrcaSlicer`, `OrcaSlicer_dep`) stay, so the caches are not invalidated.
- Gate: L, W and M. On each platform, start the app next to an installed OrcaSlicer and check that its data dir is untouched and German is still shown. **Size:** about 20 files, about 150 lines.

**A5 `rebrand: remove OrcaSlicer collisions (installer, ProgID, URL schemes, D-Bus)`**
- Windows:
  - CPack `CMakeLists.txt:1311-1343`: name, vendor, registry key `KLIPSLICE`, executables, desktop links, new `CPACK_WIX_UPGRADE_GUID`, icons.
  - `GUI_App.cpp:286,9796-9797,9823-9824`: ProgID `KLIPSLICE.1`.
  - `GUI_App.cpp:3200,8592`: `associate_url(L"klipslice")`.
  - Either drop `scripts/msix` or rename its identity, ExcludedDirectory and protocols.
- macOS:
  - plist:35-38: scheme `klipslice`. Keep `LSHandlerRank Alternate`.
  - `InstanceCheckMac.mm:16,18,59,68`.
  - Add `NSLocalNetworkUsageDescription`.
- Linux:
  - `InstanceCheck.cpp:236-607`: D-Bus names.
  - Rename the `.desktop` to `de.3dwindt.KLIPSLICE.desktop`, plus `DesktopIntegrationDialog.cpp:212,345-384`.
  - Rename the flatpak manifest (app-id, own/talk names, `share/KLIPSLICE`) and fix the paths in `unit_tests_flatpak.yml`.
- Shared:
  - `Utils.hpp:274-277`: `is_klipslice_open`. Drop the bambustudio scheme at `InstanceCheck.cpp:500`, `Downloader.cpp:146` and `Preferences.cpp:2205`.
  - `GUI_App.cpp:465`: drop the softfever flatpak migration.
- Buildable because these are strings and identifiers only.
- Gate: L, W, M. grep0 for `Orca.Slicer.1`, `L"orcaslicer"`, `com.orcaslicer.OrcaSlicer.InstanceCheck` and `bambustudioopen`. **Size:** about 15 files, about 120 lines.

### Phase B: lock the config (libslic3r only, headless-testable)

**B1 `config: lock gcode_flavor to klipper`**
- Edits:
  - `PrintConfig.cpp:4332-4357`: enum_values and labels become klipper only.
  - `PrintConfig.cpp:4360`: default `gcfKlipper`.
  - `handle_legacy` (:9097): map every other value to `klipper`.
  - Tests: `test_gcodewriter.cpp:111,158,855` use the enum directly. `test_wipe.cpp:131,182` set `"klipper"`.
  - `Tab.cpp:5109`: remove the option line.
- The enum and `s_keys_map_GCodeFlavor` stay unchanged.
- Buildable because no symbol is removed. The narrowed enum values and the legacy mapping land together, or validation at `PrintConfig.cpp:11761` fails.
- Gate: LH, then L. Watch the golden output, e.g. `tests/data/wipe_tower_temperature_trace_main.txt`. **Size:** 4 files, about 40 lines.

**B2 `config: map every host_type to moonraker, default htMoonraker`**
- Edits: in `PrintConfig.cpp`, the handle_legacy mapping and the host_type default (:5424-5465). Fix the tooltip. The enum stays intact.
- Buildable because no symbol changes.
- Gate: LH and L. The 107 octoprint profiles load as moonraker. **Size:** 1 file, about 10 lines.

**B3 `libslic3r: fold is_bbl_vendor to false, drop Preset::has_lidar`**
- Edits:
  - `PresetBundle.hpp:357`: `return false`.
  - Delete `Preset.hpp:417` and `Preset.cpp:1010-1027`, which have no callers.
  - GUI folds from the §6 edit list: `Plater.cpp:1823-1829,3913` (`is_skip_high_flow_printer`), the PartPlate `dual_bbl` code, Tab and PresetComboBoxes.
  - The network sites stay as they are.
- Gate: L. **Size:** about 12 files, about 150 lines removed.

**B4 `config: remove scan_first_layer and bbl_calib_mark_logo`**
- Edits:
  - PrintConfig definitions: :4142-4147 and :6162-6166.
  - `PrintConfig.hpp:1603,1692`.
  - `Preset.cpp:1490`.
  - `Tab.cpp:5132,6146`.
  - `GLCanvas3D.cpp:6138-6140`.
  - `ArrangeJob.cpp:533`, `FillBedJob.cpp:235`.
  - `GCode.cpp:3527,5840-5848`.
  - Add both keys to the ignore set at :9324.
- Buildable because the definition, the macro field and every string user change in one commit.
- Gate: LH and L. grep0 for `scan_first_layer|bbl_calib_mark_logo` in `src/`. **Size:** 9 files, about 60 lines.

### Phase C: decouple Moonraker (before any network code is deleted)

**C1 `agents: make NetworkAgentFactory.hpp independent of BBL headers, default agent moonraker`**
- Edits:
  - Add a direct `#include "OrcaCloudServiceAgent.hpp"` in `NetworkAgentFactory.cpp` (the dynamic_cast at :209).
  - Local agent-id literal at `DeviceManager.cpp:498` and `DevManager.cpp:37`.
  - Add explicit includes wherever `BBLCloudServiceAgent`/`BBL_CLOUD_PROVIDER` were only reached transitively (`GUI_App.cpp:3919`, `WebViewDialog.cpp:424`).
  - `GUI_App.cpp:4026`: the fallback becomes `"moonraker"`.
- The BBL registration stays for now.
- Buildable because includes are only added.
- Gate: L and W. W catches MSVC's different transitive-include behaviour. **Size:** 5 files, about 20 lines.

**C2 `moonraker: .local resolve, Bonjour discovery`**
- Edits:
  - Port the WIN32 double-resolve (`OctoPrint.cpp:35-99,137-194`) and the non-WIN32 Bonjour fallback (:266-295) into `Moonraker.cpp`.
  - `BonjourDialog.cpp:127`: `Bonjour("moonraker")`. Change the header at :83.
  - `Moonraker.hpp:42`: `has_auto_discovery` → true.
  - macOS plist: `NSBonjourServices` → `_moonraker._tcp`.
- **Hardware test on the Voron:** confirm the service name with `dns-sd -B _moonraker._tcp`, and test an upload to `raspberrypi.local` on all three OSes.
- Gate: L and W, plus the real Voron test. **Size:** 4 files, about 200 lines.

### Phase D: Bambu device UI leaves (DeviceManager/DeviceCore stay)

**D1 `gui: move UpdateVersionDialog/ConfirmBeforeSendDialog out of ReleaseNote`**
- Adds a new device-free header. `GUI_App.cpp:146,3233`, `MainFrame.cpp:1645-1646` (`EVT_SECONDARY_CHECK_CONFIRM`) and `Preferences.cpp:2408` include it instead.
- Gate: L. **Size:** 5 files, about 300 moved lines.

**D2 `gui: remove Bambu camera, liveview and SD browser (+FFmpeg link)`**
- Deletes:
  - `MediaPlayCtrl.*`, `MediaFilePanel.*`, `ImageGrid.*`, `wxMediaCtrl3.*`, `AVVideoDecoder.*`
  - `Printer/{PrinterFileSystem.*,BambuTunnel.h}`, `CameraPopup.*`, `PartSkipDialog.*`
- Edits:
  - `MonitorBasePanel.h/.cpp`, `Monitor.hpp/.cpp`, `StatusPanel.*`: this is the riskiest hand edit.
  - Remove the BambuSource copy at `GUI_App.cpp:3642-3672` and `PresetUpdater.cpp:795-803`.
  - FFmpeg link block at `src/slic3r/CMakeLists.txt:975-1010`, DLL/.so copy at `CMakeLists.txt:1117-1174`, `tests/CMakeLists.txt:38-61`.
  - macOS AVFoundation/AVKit/CoreMedia/VideoToolbox at `src/CMakeLists.txt:177`.
- `deps/FFMPEG` stays until H5. DownloadProgressDialog stays until E1.
- Buildable because the sources and their link blocks go in one commit. Without that, macOS stops at the configure-time FATAL_ERROR.
- Platforms: all three link blocks.
- Gate: L, W, M. grep0 for `AVVideoDecoder|wxMediaCtrl3|MediaPlayCtrl|PrinterFileSystem|BambuTunnel|avcodec`. **Size:** about 20 files deleted, about 8 edited, about −9k lines.

**D3 `gui: remove bind, HMS and firmware upgrade UI`**
- Deletes: `BindDialog.*`, `Jobs/BindJob.*`, `BBLStatusBarBind.*`, `HMS.*`, `HMSPanel.*`, `UpgradePanel.*`, `DeviceTab/uiDeviceUpdateVersion.*`, `DeviceErrorDialog.*`, and the rest of ReleaseNote's Bambu dialogs.
- Edits:
  - Includes and handlers in GUI_App (:7, :9, :966 HMSQuery), MainFrame:53, Monitor, SelectMachine(+Pop), AmsMappingPopup, UserManager, Auxiliary.hpp, DeviceManager.cpp/.hpp.
  - `DeviceTab/CMakeLists.txt`.
- `DeviceCore/DevHMS` and `DevUpgrade` stay because MachineObject owns them.
- Gate: L and M, where `-Werror` catches unused members. **Size:** about 20 files deleted, about 15 edited, about −8k lines.

**D4 `gui: remove Bambu-only hardware dialogs and stale includes`**
- Deletes: `AMSDryControl.*`, `uiAmsHumidityPopup.*`, `uiAMSBestPositionPopup.*`, `wgtDeviceNozzleRack*`/`Select`, `SafetyOptionsDialog.*`, `ThermalPreconditioningDialog.*`, `ExtrusionCalibration.*`, `Calibration.cpp/.hpp` (**not** calib_dlg) and `CaliHistoryDialog.*`.
- Edits: StatusPanel (:4493, :5177). Remove the unused includes at `BBLTopbar.hpp:6-7` and `GUI_ObjectTable.cpp:4`.
- Gate: L, W, M. **Size:** about 18 files deleted, about −6k lines.

### Phase F1 (runs before E): wizard

**F1 `guide: start wizard at printer selection, drop region/stealth/plugin steps`**
- `WebGuideDialog.cpp`:
  - :121-125 and :191: start at `target=21`. The mapping lives in SetStartPage (:216-257).
  - Delete the handlers at :444-460 and :669-692, and the NetworkAgent block at :641-660.
  - Delete :797-798, :1637-1646 and `DownloadPlugin/InstallPlugin/ShowPluginStatus` (:1917-1940).
- `WebGuideDialog.hpp:107-109,141-142`.
- `guide/22/22.js:8-46`: `AcceptBtn` must always be visible, or the user gets stuck in the wizard.
- `22/index.html:108`, `21/index.html:12,84`, `21/common.js:272-295`.
- Decide `check_for_new_printers` here (§3 D5).
- Gate: L, plus a **manual** run of the first-run flow (0→21→22→finish) and the pages "Add printer" (24) and "Add filament" (23) on W and M. CI cannot see blank web views. **Size:** 6 files, about −250 lines.

### Phase E: network stack

**E1 `network: remove Bambu network plug-in loader and BBL agents`**
- Deletes:
  - `Utils/BBLNetworkPlugin.*`, `BBLPrinterAgent.*`, `BBLCloudServiceAgent.*`
  - `NetworkPluginDialog.*`, `WebDownPluginDlg.*`, `Jobs/UpgradeNetworkJob.*`, `DownloadProgressDialog.*`
  - `resources/web/guide/{4orca,5,6}`
  - `tests/libslic3r/test_bambu_networking.cpp`, `tests/slic3rutils/test_network_versions.cpp`, `test_bambu_filament_ids.cpp`
- Edits:
  - `NetworkAgent.hpp:16,22-42,60` and `NetworkAgent.cpp:9,36-83,136`.
  - `NetworkAgentFactory.hpp:8-9,18,146-158` and `.cpp:4,179-191,214-224`.
  - `GUI_App.cpp`: 152-153, 1259-1730 (keep the `switch_printer_agent` call at 1726), 1834-2110, 2413, 3733-3860 DLL part, 3913-3931, 3950-3966, 4058/4079/4108 BBL branches.
  - `Preferences.cpp:1331-1361`, `Monitor.cpp:207,570`, `Plater.cpp:13425,13631-13665,7412,7698`.
  - NotificationManager `BBLPluginInstallHint` and `BBLPluginUpdateAvailable`. Also `PresetUpdater.cpp:939,944`.
  - `bambu_networking.hpp`: the library/ABI block only.
  - `src/OrcaSlicer.cpp:108-114,7237-7250,7268-7271`. This part is guarded by `SLIC3R_GUI`, so the headless build is unaffected.
  - Test CMakeLists: `tests/libslic3r/CMakeLists.txt:12`, `tests/slic3rutils/CMakeLists.txt:4,12` and `slic3rutils_tests_main.cpp:4,31-84`.
- Windows: the LoadLibrary code and the DLL-rename logic (`GUI_App.cpp:1545-1563,3733-3751`) go.
- Keep: `-ldl` (`src/CMakeLists.txt:199`), `IPrinterAgent` unchanged, `ICloudServiceAgent`, OrcaCloud.
- Gate: L, W, M, headless LH. grep0 for `BBLNetworkPlugin|BBLPrinterAgent|BBLCloudServiceAgent|UpgradeNetworkJob|DownloadProgressDialog|download_plugin|install_plugin|get_bambu_source_entry|NetworkLibraryVersion`.
- Test afterwards that the Moonraker agent still connects with `use_printer_agents=true`.
- **Size:** about 20 files deleted, about 25 edited, about −7k lines.

**E2 `network: remove telemetry (track_*)`**
- Edits: `NetworkAgent.hpp:120-133,189`, `.cpp:583-631`. Callers: GUI_App:5750, MainFrame:1252, GLCanvas3D:6971, DeviceManager:3057, CalibUtils:1062-1064, SelectMachine.
- Gate: L and M. grep0 for `track_`. **Size:** about 10 files, about −200 lines.

**E3 `mall: remove MakerWorld / model mall`**
- Deletes: `ModelMall.*` and `mall_control_{back,forward}.svg`. Keep `mall_control_refresh.svg`, which `SendMultiMachinePage.cpp:1316` still uses.
- Edits:
  - NetworkAgent `model_mall_*`, `mw_*`, `get_design_staffpick`, `start_publish` (:633-695).
  - `GUI_App.cpp:1193-1215,9222-9336`.
  - `MainFrame.cpp:654,2769-2796`, `BBLTopbar.cpp:486`, `StatusPanel.cpp:118-130`, `DeviceManager.cpp:4786`.
  - `WebViewDialog.cpp:468-483,543-566`.
  - `homepage/home.js:533-615`, `index.html:112-118`.
- Gate: L. **Size:** about 12 files, about −1.2k lines.

**E4 `cloud: remove Bambu login, cloud provider and user manager`**
- Deletes: `WebUserLoginDialog.*`, `UserManager.*`, `test_user_manager.cpp`, `guide/11`.
- Edits:
  - `BBL_CLOUD_PROVIDER` (`CloudProvider.hpp:9`) and all its users.
  - Preferences: :590-603, :1244-1252, :2388-2406.
  - `GUI_App.cpp:4791,5033-5110,5165-5204,5421`.
  - homepage `#BambuCloudSection`, `WebViewDialog.cpp:418-540`.
  - DevManager cloud calls (465, 505, 818, 872, 945, 1075), `DeviceManager.cpp:2686,4899,4975`.
  - CalibrationWizard:288,372, `CalibrationWizardPresetPage.cpp:1716,1726`, `CalibrationPanel.cpp:254,591`.
  - `CreatePresetsDialog.cpp:3932,4051` and `ExportPresetBundleDialog.cpp:345`: use a constant version, not an empty string.
  - `BBLUserPresetExceedLimit`.
- Gate: L, W, M. grep0 for `BBL_CLOUD_PROVIDER|ZUserLogin|UserManager`. **Size:** about 25 files, about −3k lines.

**E5 (only after owner decision D3) `cloud: remove Orca cloud`.** Removes `OrcaCloudServiceAgent`, HttpServer, preset sync (`GUI_App.cpp:6594-7520,7813-7841`), `OrcaSyncConflict`, and makes `CloudPluginService` work with a null agent. The Linux `libsecret` requirement goes only after checking what wx itself needs. **Size:** about 15 files, about −5k lines.

**E6 `network: shrink NetworkAgent to printer facade`**
- Removes the unused cloud methods (:64-119). IPrinterAgent and its signatures stay unchanged, because they are the pybind trampoline ABI.
- Gate: L, W, M. **Size:** 2–4 files, about −500 lines.

### Phase F (rest): consumer web

- **F2 `guide: delete dead pages and assets`.** Deletes `guide/{1,3,31,swiper}`, the test assets and the unused homepage images. Check the hardcoded `../N/` links by hand. About −6 MB, 0 C++.
- **F3 `wizard: WizardTypes.hpp, delete ConfigWizard`.** Moves the enums. Adds the missing includes to `GUI_App.hpp:10` (`GUI_Utils.hpp`, `wx/dialog.h`). Switches `PresetComboBoxes.cpp:38,957-986,1094,1119,1653-1662`, `Plater.cpp:126,2728,3251` and `GUI_App.cpp:9438-9476,9641`. Removes the includes in PresetUpdater, UpdateDialogs, MainFrame and WebGuideDialog. Deletes `ConfigWizard.cpp` and `ConfigWizard_private.hpp` plus CMake lines 100-102. Gate: L, W, M. About −4k lines.
- **F4 `content: Klipper hints, KLIPSLICE splash`.** Covers `hints.ini:106,169,175` and `splash_logo*.svg`. About 3 files.

### Phase G: print hosts (after B2 and C2)

- **G1 `hosts: remove LAN backends`.** Duet, UltiMaker, FlashAir, AstroBox, Repetier, MKS, ESP3D, TCPConsole, `Serial*` and `SerialMessage*`. Removes their factory cases and PhysicalPrinterDialog branches (268-300, 327+). Keep the Setupapi and IOKit links. About −4k lines.
- **G2 `hosts: remove cloud hosts`.** Obico, SimplyPrint, 3DPrinterOS, `OAuthDialog`, `Jobs/OAuthJob`, `PrinterCloudAuthDialog`. Edits PhysicalPrinterDialog:229-255, 303-325, 480-538, and the `htSimplyPrint` checks at `MainFrame.cpp:2066,4391` and `Plater.cpp:3702`. Also `PresetBundle.cpp` `use_bbl_network` via `SimplyPrint.cpp:94`. About −3k lines.
- **G3 `hosts: remove Flashforge`** (see D2 in §3). The factory case, `FlashforgePrintHostSendDialog`, the Plater flashforge branch, `test_printhost.cpp:286+`, and the `flashforge_serial_number` definition, which goes into the ignore set.
- **G4 `hosts: remove OctoPrint family + ElegooLink`.** Deletes `OctoPrint.*` and `ElegooLink.*` together. The non-FFF branch at `PrintHost.cpp:84` returns `nullptr`, and the defaults at :61 and :95 become htMoonraker. Removes `ElegooPrintHostSendDialog` and `ElegooPrinterWebViewHandler`, plus the Elegoo `lan_service_web`. grep0 for `OctoPrint|PrusaLink|PrusaConnect|SL1Host|ElegooLink`.
- **G5 `hosts: remove CrealityPrint host`.** Only if D2 = drop, and together with `CrealityPrintAgent`, `CrealityHostDiscovery`, `CrealityDiscoveryDialog`, `test_creality_cfs_match.cpp` and `deps_src/mdns`.
- **G6 `hosts: shrink PrintHostType to {htMoonraker}`.** Covers `PrintConfig.hpp:102-103`, the keys map, `Field.cpp:1795,1914` and `PhysicalPrinterDialog::update_host_type`. Gate: L, W, M.

### Phase H: cleanup

- **H1 `profiles: hygiene`.** Sets `gcode_flavor=klipper` in the 37 base JSONs. Rewrites host_type to moonraker (following D2). Strips `scan_first_layer` (184 files) and `bbl_use_printhost` (100 files). Gate: `check_profiles`.
- **H2 `config: remove bbl_use_printhost, use_bbl_network/device_tab, Marlin_BBL, is_bbl_vendor`.** Covers `PresetBundle.*`, `Preset.cpp:1493,4250`, `PrintConfig.cpp:998-1003`, `Tab.cpp:5112`, the MainFrame/Plater callers and `profile_validator.cpp:310`.
- **H3 `engine: remove BBL G-code branches`.** Covers GCodeWriter, GCode, ToolOrdering, Print and WipeTowerEstimate, plus the tests `test_gcodewriter:513-719`, `test_print:64,553` and `test_gcode_timing:50`. Keep `s_IsBBLPrinter`, the 3MF `BambuStudio-` writer (`bbs_3mf.cpp:7025`, test_3mf:1233) and `is_all_bbl_filament`.
- **H4 (optional) `engine: remove non-Klipper flavor emission`.** Done file by file, with an M gate each time (AppleClang `-Werror`). Keep every `default:` and the enum.
- **H5 `deps: remove FFmpeg`.** One commit: `deps/FFMPEG`, `deps/CMakeLists.txt:399,484`, flatpak :328-332 and `build_linux_image.sh.in:111-114`. This forces a deps rebuild on every leg, so warm the caches once afterwards.
- **H6 `ci: minimal Windows/macOS legs`.** Windows x64 `build_win.bat -s` and macOS-14 arm64 `-s -n -x -a arm64`, both on dispatch or tags only.

**Stage 2 (not part of this series; depends on D1):** retire `use_printer_agents` and delete DeviceManager, DeviceCore, DeviceTab, Monitor, StatusPanel, SelectMachine, SendToPrinter, the AMS widgets, CalibrationPanel/Wizard, CalibUtils, PrintJob/SendJob and MoonrakerPrinterAgent. That is roughly −35k lines.

## 3. Decisions only the maintainer can make

1. **D1: native Moonraker device tab vs. web view only.** Keep the agent mode (MoonrakerPrinterAgent feeds Bambu UI through DeviceManager, and Happy Hare/AFC filament sync works), or rely only on PrintHost upload plus PrinterWebView/HORIZON and remove about 35k lines in Stage 2. The agent path has never run on his hardware (`use_printer_agents=false` by default).
2. **D2: Klipper vendors without Moonraker.**
   - Creality K-series: 26 profiles, own agent and host.
   - Elegoo: 9 profiles on `elegoolink`.
   - Flashforge, Qidi and Snapmaker agents.

   Keep them, or map them to Moonraker (the brief says Moonraker is the only print host)?
3. **D3: Orca cloud** (login, preset sync, cloud plugin repo). Remove it (E5) or keep it. It is tied to the Python plugin system.
4. **D4: identity.** Is the bundle id `de.3dwindt.KLIPSLICE` and the binary name `klipslice` right? He also needs to supply the logo/icon set (A2) and decide whether the gettext domain stays `OrcaSlicer`, which makes upstream `.po` merges easier.
5. **D5: updates.** Leave the update check blank or point it at KlipSlice GitHub releases? Should the wizard's "Check for new printers" button, which downloads online vendors, stay?

## 4. Overall size and risk

- About 40 commits and roughly −60k to −70k lines, not counting Stage 2.
- Riskiest commits: D2 and D3 (StatusPanel/Monitor hand edits), E1 (GUI_App), F3 (ConfigWizard.hpp sits in GUI_App.hpp) and H4 (macOS `-Werror`).
- There is no local compiler, so every commit needs at least a green L run. Commits that touch structure (A4, A5, C1, D2, E1, E4, F3, G6, H5) also need W and M.
## 5. Owner decisions (2026-09-30)

- **Name:** KLIPSLICE stays the product name (A4 uses `KLIPSLICE` / `klipslice` / `de.3dwindt.KLIPSLICE`).
- **D2 vendors:** keep the Creality/Elegoo/Qidi/Snapmaker/Flashforge Klipper printers, but route them all through Moonraker. Their own agents and hosts go (G3–G5 run; H1 rewrites host_type to moonraker).
- **D1 device tab:** no HORIZON embedding. Keep the native Moonraker agent path for now; Stage 2 is not scheduled.
- **D3 Orca cloud:** keep for now. E5 is not scheduled.
