#include <io.h>
#include <memory>
#include <vector>
#include <string>
#include <codecvt>
#include <fcntl.h>
#include <stdio.h>
#include <tchar.h>
#include <iostream>
#include <strsafe.h>
#include <Windows.h>
#include <winrt/Windows.Foundation.h>

#include "./json.hpp"
#include "./cppcodec/base64_url_unpadded.hpp"

#pragma comment(lib, "WindowsApp.lib")

using namespace std;
using namespace winrt;
using namespace nlohmann;
using namespace cppcodec;
using namespace Windows::Foundation;

wstring utf8_to_wstring(const string& str) {
	return wstring_convert<codecvt_utf8<wchar_t>>().from_bytes(str);
}

string wstring_to_utf8(const wstring& str) {
	return wstring_convert<codecvt_utf8<wchar_t>>().to_bytes(str);
}

struct registry_traits
{
	using type = HKEY;

	static void close(type value) noexcept
	{
		WINRT_VERIFY_(ERROR_SUCCESS, ::RegCloseKey(value));
	}

	static constexpr type invalid() noexcept
	{
		return nullptr;
	}
};

using registry_key = winrt::handle_type<registry_traits>;

wstring get_module_path()
{
	wstring path(100, L'?');
	uint32_t path_size{};
	DWORD actual_size{};

	do
	{
		path_size = static_cast<uint32_t>(path.size());
		actual_size = ::GetModuleFileName(nullptr, path.data(), path_size);

		if (actual_size + 1 > path_size)
		{
			path.resize(path_size * 2, L'?');
		}
	} while (actual_size + 1 > path_size);

	path.resize(actual_size);
	return path;
}

void ErrorExit(LPCWSTR lpszFunction, DWORD dwError)
{
	// Retrieve the system error message for the last-error code

	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;

	::FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dwError,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&lpMsgBuf,
		0, NULL);

	// Display the error message and exit the process

	lpDisplayBuf = (LPVOID)::LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCWSTR)lpMsgBuf) + lstrlen((LPCWSTR)lpszFunction) + 40) * sizeof(WCHAR));
	::StringCchPrintfW((LPWSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(WCHAR),
		L"%s failed with error %d: %s",
		lpszFunction, dwError, lpMsgBuf);
	::MessageBoxW(NULL, (LPCWSTR)lpDisplayBuf, L"Error", MB_ICONERROR | MB_OK);

	::LocalFree(lpMsgBuf);
	::LocalFree(lpDisplayBuf);
	::ExitProcess(dwError);
}

void ErrorExit(LPCWSTR lpszFunction)
{
	ErrorExit(lpszFunction, ::GetLastError());
}

void
ArgvQuote(
	const wstring& Argument,
	wstring& CommandLine,
	bool Force
)

/*++

Routine Description:

	This routine appends the given argument to a command line such
	that CommandLineToArgvW will return the argument string unchanged.
	Arguments in a command line should be separated by spaces; this
	function does not add these spaces.

Arguments:

	Argument - Supplies the argument to encode.

	CommandLine - Supplies the command line to which we append the encoded argument string.

	Force - Supplies an indication of whether we should quote
			the argument even if it does not contain any characters that would
			ordinarily require quoting.

Return Value:

	None.

Environment:

	Arbitrary.

--*/

{
	//
	// Unless we're told otherwise, don't quote unless we actually
	// need to do so --- hopefully avoid problems if programs won't
	// parse quotes properly
	//

	if (Force == false &&
		Argument.empty() == false &&
		Argument.find_first_of(L" \t\n\v\"") == Argument.npos)
	{
		CommandLine.append(Argument);
	}
	else {
		CommandLine.push_back(L'"');

		for (auto It = Argument.begin(); ; ++It) {
			unsigned NumberBackslashes = 0;

			while (It != Argument.end() && *It == L'\\') {
				++It;
				++NumberBackslashes;
			}

			if (It == Argument.end()) {

				//
				// Escape all backslashes, but let the terminating
				// double quotation mark we add below be interpreted
				// as a metacharacter.
				//

				CommandLine.append(NumberBackslashes * 2, L'\\');
				break;
			}
			else if (*It == L'"') {

				//
				// Escape all backslashes and the following
				// double quotation mark.
				//

				CommandLine.append(NumberBackslashes * 2 + 1, L'\\');
				CommandLine.push_back(*It);
			}
			else {

				//
				// Backslashes aren't special here.
				//

				CommandLine.append(NumberBackslashes, L'\\');
				CommandLine.push_back(*It);
			}
		}

		CommandLine.push_back(L'"');
	}
}



int registerProtocol() {
	LSTATUS status;
	registry_key key;

	const WCHAR regVAL1[] = LR"(URL:sudo-mpv-launcher Protocol)";

	status = ::RegCreateKeyExW(
		HKEY_CLASSES_ROOT,
		LR"(sudo-mpv-launcher)",
		0,
		nullptr,
		0,
		KEY_WRITE,
		nullptr,
		key.put(),
		nullptr
	);

	if (status == ERROR_ACCESS_DENIED) {
		::MessageBoxW(NULL, L"Please run as Administrator and try again!", L"Access denied!", MB_ICONERROR | MB_OK);
		return status;
	}

	if (status != ERROR_SUCCESS) {
		ErrorExit(L"RegCreateKeyExW", status);
	}

	status = ::RegSetValueExW(
		key.get(),
		nullptr,
		0,
		REG_SZ,
		reinterpret_cast<BYTE const*>(regVAL1),
		static_cast<uint32_t>(sizeof(regVAL1))
	);

	if (status != ERROR_SUCCESS) {
		ErrorExit(L"RegSetValueExW", status);
	}

	status = ::RegSetValueExW(
		key.get(),
		LR"(URL Protocol)",
		0,
		REG_SZ,
		nullptr,
		0
	);

	if (status != ERROR_SUCCESS) {
		ErrorExit(L"RegSetValueExW", status);
	}

	key.close();

	status = ::RegCreateKeyExW(
		HKEY_CLASSES_ROOT,
		LR"(sudo-mpv-launcher\shell)",
		0,
		nullptr,
		0,
		KEY_WRITE,
		nullptr,
		key.put(),
		nullptr
	);

	if (status != ERROR_SUCCESS) {
		ErrorExit(L"RegCreateKeyExW", status);
	}

	key.close();

	status = ::RegCreateKeyExW(
		HKEY_CLASSES_ROOT,
		LR"(sudo-mpv-launcher\shell\open)",
		0,
		nullptr,
		0,
		KEY_WRITE,
		nullptr,
		key.put(),
		nullptr
	);

	if (status != ERROR_SUCCESS) {
		ErrorExit(L"RegCreateKeyExW", status);
	}

	key.close();

	status = ::RegCreateKeyExW(
		HKEY_CLASSES_ROOT,
		LR"(sudo-mpv-launcher\shell\open\command)",
		0,
		nullptr,
		0,
		KEY_WRITE,
		nullptr,
		key.put(),
		nullptr
	);

	if (status != ERROR_SUCCESS) {
		ErrorExit(L"RegCreateKeyExW", status);
	}

	wstring mypath = get_module_path();
	wstring regVAL2 = LR"(")" + mypath + LR"(" "%1")";

	status = ::RegSetValueExW(
		key.get(),
		nullptr,
		0,
		REG_SZ,
		reinterpret_cast<BYTE const*>(regVAL2.c_str()),
		static_cast<uint32_t>((regVAL2.size() + 1) * sizeof(WCHAR))
	);

	if (status != ERROR_SUCCESS) {
		ErrorExit(L"RegSetValueExW", status);
	}

	::MessageBoxW(NULL, L"Success!", L"sudo-mpv-launcher", MB_ICONINFORMATION | MB_OK);

	return 0;
}

int launchMPV(json args) {
	WCHAR mpvPath[_MAX_PATH];
	DWORD result = ::SearchPathW(NULL, L"mpv", L".exe", _MAX_PATH, mpvPath, NULL);

	if (result == 0) {

		DWORD error = GetLastError();
		if (error == ERROR_FILE_NOT_FOUND) {
			::MessageBoxW(NULL, L"mpv.exe is not in your system PATH environment variable. Please add it and try again!", L"sudo-mpv-launcher", MB_ICONERROR | MB_OK);
		}

		ErrorExit(L"SearchPathW");
	}

	wstring CommandLine;

	for (auto& arg : args) {
		CommandLine.append(L" ");
		ArgvQuote(utf8_to_wstring(arg), CommandLine, false);
	}

	LPWSTR CommandLineBuf = new WCHAR[CommandLine.size() + 1];
	lstrcpynW(CommandLineBuf, CommandLine.c_str(), CommandLine.size() + 1);

	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	BOOL success = ::CreateProcessW(
		mpvPath,
		(LPWSTR)CommandLine.c_str(),
		NULL,
		NULL,
		false,
		DETACHED_PROCESS,
		NULL,
		NULL,
		&si,
		&pi
	);

	delete[] CommandLineBuf;

	if (!success) {
		ErrorExit(L"CreateProcessW");
	}

	return 0;
}

int wmain(int argc, wchar_t* argv[], wchar_t* envp[])
{
	int status = 0;

	_setmode(_fileno(stdout), _O_U16TEXT);
	_setmode(_fileno(stderr), _O_U16TEXT);

	::ShowWindow(::GetConsoleWindow(), SW_HIDE);

	if (argc == 1) {

		if (::MessageBoxW(NULL, L"Register protocol handler sudo-mpv-launcher://?", L"sudo-mpv-launcher", MB_ICONQUESTION | MB_YESNO) == IDYES)
		{
			return registerProtocol();
		}
		else
		{
			return 0;
		}
	}

	if (argc != 2)
	{
		return 1;
	}

	json params;

	try {
		Uri uri{ argv[1] };

		vector<uint8_t> decoded = base64_url_unpadded::decode(wstring_to_utf8(wstring(uri.Host())));

		params = json::parse(decoded);

		if (!params.is_object() || !params["args"].is_array()) {
			::MessageBoxW(NULL, argv[1], L"Invalid URI!", MB_ICONERROR | MB_OK);
			return 1;
		}

		if (params["player"] != "mpv") {
			::MessageBoxW(NULL, params["player"].get<wstring>().c_str(), L"Unsupported player!", MB_ICONERROR | MB_OK);
			return 1;
		}

		status = launchMPV(params["args"]);
	}
	catch (winrt::hresult_error const& ex) {
		::MessageBoxW(NULL, argv[1], L"Invalid URI!", MB_ICONERROR | MB_OK);
		return 1;
	}

	if (status != 0) {
		return status;
	}

	return status;
}
