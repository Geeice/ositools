#include "stdafx.h"

#include "DxgiWrapper.h"
#include "HttpFetcher.h"
#include "ErrorUtils.h"

#include <Shlwapi.h>
#include <Shlobj.h>
#include <comdef.h>

#include <vector>
#include <thread>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

HRESULT UnzipToFolder(PCWSTR pszZipFile, PCWSTR pszDestFolder, std::string & reason);

#define UPDATER_HOST L"d1ov7wr93ghrd7.cloudfront.net"
#define UPDATER_PATH_PREFIX L"/"
#define UPDATER_PATH_POSTFIX L"/Latest.zip"

std::string trim(std::string const & s)
{
	size_t first = s.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) {
		first = 0;
	}

	size_t last = s.find_last_not_of(" \t\r\n");
	return s.substr(first, (last - first + 1));
}

class OsiLoader
{
public:
	void Launch()
	{
		UpdatePaths();
		std::string reason;
		bool updated = TryToUpdate(reason);

		if (!updated && !ExtenderDLLExists()) {
			DEBUG("Update failed; reason: %s", reason.c_str());
			completed_ = true;
			auto msg = FromUTF8(reason);
			gErrorUtils->ShowError(msg.c_str());
			return;
		}

		// Make sure that we can load dependencies from the extension directory
		// (protobuf, etc.)
		DEBUG("SetDllDirectoryW(%s)", ToUTF8(extensionPath_).c_str());
		SetDllDirectoryW(extensionPath_.c_str());

		DEBUG("Loading extension: %s", ToUTF8(dllPath_).c_str());
		HMODULE handle = LoadLibraryW(dllPath_.c_str());
		// Wait a bit for extender startup to complete
		Sleep(300);
		completed_ = true;

		if (handle == NULL) {
			auto errc = GetLastError();
			DEBUG("Extension load failed; error code %d", errc);
			std::wstring errmsg = L"Script Extender DLL load failed.\r\n"
				"Error code: ";
			errmsg += std::to_wstring(errc);
			gErrorUtils->ShowError(errmsg.c_str());
		}
	}

	bool TryToUpdate(std::string & reason)
	{
		HttpFetcher fetcher(UPDATER_HOST);

		std::wstring packageUri = UPDATER_PATH_PREFIX + updateChannel_ + UPDATER_PATH_POSTFIX;

		std::string etag;
		DEBUG("Fetching ETag");
		if (!fetcher.FetchETag(packageUri.c_str(), etag)) {
			reason = "Something went wrong while checking for Script Extender updates. Please make sure you're connected to the internet and try again\r\n";
			reason += fetcher.GetLastError();
			return false;
		}

		DEBUG("Checking currently downloaded ETag");
		std::string currentETag = ReadETag();
		if (currentETag == etag) {
			return true;
		}

		DEBUG("Fetching update package: %s", ToUTF8(packageUri).c_str());
		std::vector<uint8_t> response;
		if (!fetcher.Fetch(packageUri.c_str(), response)) {
			reason = "Something went wrong while downloading Script Extender updates. Please make sure you're connected to the internet and try again\r\n";
			reason += fetcher.GetLastError();
			return false;
		}

		auto zipPath = extensionPath_ + L"\\Update.zip";
		DEBUG("Saving update to: %s", ToUTF8(zipPath).c_str());
		SaveFile(zipPath, response);
		
		std::string unzipReason;
		DEBUG("Unpacking update to %s", ToUTF8(extensionPath_).c_str());
		HRESULT hr = UnzipToFolder(zipPath.c_str(), extensionPath_.c_str(), unzipReason);
		if (hr == S_OK) {
			DEBUG("Saving updated ETag");
			SaveETag(etag);
			return true;
		} else {
			if (hr == S_FALSE) {
				reason = "Unable to extract Script Extender update package.\r\n";
				reason += unzipReason;
			} else {
				_com_error err(hr);
				LPCTSTR errMsg = err.ErrorMessage();

				std::stringstream ss;
				ss << "Unable to extract Script Extender update package.\r\n"
					<< "HRESULT 0x"
					<< std::hex << std::setw(8) << std::setfill('0') << hr;
				if (errMsg != nullptr) {
					ss << ": " << ToUTF8(errMsg);
				}

				reason = ss.str();
			}

			return false;
		}
	}

	inline bool IsCompleted() const
	{
		return completed_;
	}

private:
	std::wstring appDataPath_;
	std::wstring extensionPath_;
	std::wstring dllPath_;
	std::wstring updateChannel_;
	bool isGame_{ false };
	bool completed_{ false };

	bool ExtenderDLLExists()
	{
		return PathFileExists(dllPath_.c_str());
	}

	std::string ReadETag()
	{
		if (!ExtenderDLLExists()) {
			// Force update if extension DLL is missing
			return "";
		}

		auto etagPath = extensionPath_ + L"\\.etag";
		std::ifstream f(etagPath, std::ios::binary | std::ios::in);
		if (!f.good()) {
			return "";
		}

		f.seekg(0, std::ios::end);
		auto size = f.tellg();
		f.seekg(0, std::ios::beg);

		std::string s;
		s.resize(size);
		f.read(s.data(), size);
		DEBUG("ETag loaded: %s", s.c_str());
		return s;
	}

	void SaveETag(std::string const & etag)
	{
		auto etagPath = extensionPath_ + L"\\.etag";
		std::ofstream f(etagPath, std::ios::binary | std::ios::out);
		if (!f.good()) {
			return;
		}

		f.write(etag.c_str(), etag.size());
	}

	void SaveFile(std::wstring const & path, std::vector<uint8_t> const & body)
	{
		std::ofstream f(path, std::ios::binary | std::ios::out);
		if (!f.good()) {
			return;
		}

		f.write(reinterpret_cast<char const *>(body.data()), body.size());
	}

	void UpdatePaths()
	{
		isGame_ = (GetModuleHandleW(L"EoCApp.exe") != NULL);

		TCHAR appDataPath[MAX_PATH];
		if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appDataPath)))
		{
			appDataPath_ = appDataPath;
		}

		extensionPath_ = appDataPath_ + L"\\OsirisExtender";
		if (!PathFileExists(extensionPath_.c_str())) {
			CreateDirectory(extensionPath_.c_str(), NULL);
		}

		if (isGame_) {
			dllPath_ = extensionPath_ + L"\\OsiExtenderEoCApp.dll";
		} else {
			dllPath_ = extensionPath_ + L"\\OsiExtenderEoCPlugin.dll";
		}

		DEBUG("Determined DLL path: %s", ToUTF8(dllPath_).c_str());

		updateChannel_ = L"Release";
		std::ifstream channelFile("OsiUpdateChannel.txt", std::ios::in | std::ios::binary);
		if (channelFile.good()) {
			std::string channel;
			channelFile.seekg(0, std::ios::end);
			channel.resize(channelFile.tellg());
			channelFile.seekg(0, std::ios::beg);
			channelFile.read(channel.data(), channel.size());

			channel = trim(channel);
			if (!channel.empty()) {
				updateChannel_ = FromUTF8(channel);
			}
		}

		DEBUG("Update channel: %s", ToUTF8(updateChannel_).c_str());
	}
};

std::unique_ptr<OsiLoader> gLoader;

bool ShouldLoad()
{
	return GetModuleHandleW(L"EoCApp.exe") != NULL
		|| GetModuleHandleW(L"DivinityEngine2.exe") != NULL;
}

// This thread is responsible for polling and suspending/resuming
// the client init thread if the update is still pending during client init.
// The goal is to prevent the client from loading modules before the extender is loaded.
DWORD WINAPI ClientWorkerSuspenderThread(LPVOID param)
{
	bool suspended{ false };
	for (;;) {
		auto state = gErrorUtils->GetState();
		if (state) {
			bool completed = gLoader->IsCompleted();
			if (!suspended && !completed && (*state == GameState::LoadModule || *state == GameState::Init)) {
				DEBUG("Suspending client thread (pending update)");
				gErrorUtils->SuspendClientThread();
				suspended = true;
			}

			if (completed || *state == GameState::Menu) {
				DEBUG("Resuming client thread");
				gErrorUtils->ResumeClientThread();
				// No update takes place once we reach the menu, exit thread
				break;
			}
		}

		Sleep(1);
	}

	DEBUG("Client suspend worker exiting");
	return 0;
}

DWORD WINAPI UpdaterThread(LPVOID param)
{
	DEBUG("Init loader");
	gErrorUtils = std::make_unique<ErrorUtils>();
	gLoader = std::make_unique<OsiLoader>();
	CreateThread(NULL, 0, &ClientWorkerSuspenderThread, NULL, 0, NULL);
	DEBUG("Launch loader");
	gLoader->Launch();
	DEBUG("Extender launcher thread exiting");
	return 0;
}

void StartUpdaterThread()
{
	CreateThread(NULL, 0, &UpdaterThread, NULL, 0, NULL);
}


BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		if (ShouldLoad()) {
#if defined(_DEBUG)
			AllocConsole();
			SetConsoleTitleW(L"Script Extender Updater Debug Console");
			DisableThreadLibraryCalls(hModule);
			FILE * reopenedStream;
			freopen_s(&reopenedStream, "CONOUT$", "w", stdout);
#endif

			// Allow loading graphics mods that work via DXGI.dll (ReShade, etc.)
			// DXGI.dll should be renamed to DxgiNext.dll, and the updater will load it automatically.
			DEBUG("Chain-loading DXGINext");
			LoadLibraryW(L"DxgiNext.dll");

			DEBUG("Creating DXGI wrapper");
			gDxgiWrapper = std::make_unique<DxgiWrapper>();
			DEBUG("Start updater thread");
			StartUpdaterThread();
		}
		break;

	case DLL_PROCESS_DETACH:
		if (gDxgiWrapper) {
			DEBUG("Shutting down wrapper");
			gDxgiWrapper.reset();
		}
		break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

