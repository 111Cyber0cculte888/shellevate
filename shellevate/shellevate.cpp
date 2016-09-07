#include "stdafx.h"
#include "resource.h"

HINSTANCE g_hInst = NULL;

void ShowErrorDialogFromLastError() {
	LPTSTR errorText = NULL;

	DWORD len = FormatMessageW(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&errorText, 0, NULL
	);

	if (len) {
		MessageBoxW(NULL, errorText, L"shellevate error!", 0);
	}
	else {
		MessageBoxW(NULL, L"An Unknown Error Occurred", L"shellevate unknown error!", 0);
	}

	LocalFree(errorText);
}

INT_PTR CALLBACK UsageDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INITDIALOG: {
		HANDLE hIcon;

		hIcon = LoadImageW(g_hInst, MAKEINTRESOURCE(MAINICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
		if (hIcon) {
			SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		}
		return TRUE;
	}
	case WM_CLOSE:
		DestroyWindow(hDlg);
		return TRUE;
	case WM_DESTROY:
		PostQuitMessage(0);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
		case IDCANCEL:
			EndDialog(hDlg, 0);
			return TRUE;
		default:
			return FALSE;
		}
	default:
		return FALSE;
	}
}

void ShowUsageDialog(HINSTANCE hInstance) {
	MSG msg;
	BOOL ret;
	HWND hDialog;

	// init visual styles
	InitCommonControls();

	// show dialog
	DialogBoxW(hInstance, MAKEINTRESOURCE(SHELLEVATEABOUT), 0, UsageDialogProc);

	return;

	std::wostringstream usage;

	usage << "shellevate" << std::endl << std::endl;
	usage << "Usage: shellevate [/shell] [/elevate] [/asinvoker] [/netonly] <program> <arguments>" << std::endl << std::endl;
	usage << "/shell        Uses ShellExecute to run <program> after switching user contexts" << std::endl;
	usage << "/elevate      Force UAC elevation of <program> after switching user contexts" << std::endl;
	usage << "/asinvoker    Force <program> to run without UAC elevation after switching user contexts" << std::endl;
	usage << "/netonly      Use specified credentials for remote access only" << std::endl;

	MessageBoxW(NULL, usage.str().c_str(), L"shellevate Usage", 0);
}

struct ShellExecFlags {
	bool asinvoker;
	std::wstring operation;
	std::wstring program;
	std::wstring arguments;

	ShellExecFlags() : asinvoker(false), program(), arguments() {}

	bool parse(std::vector<std::wstring> &args, std::wstring &commandLine) {
		size_t lastIndex = 0;

		for (auto it = args.begin(); it != args.end(); ++it) {
			auto& flag = *it;

			// advance last index to the end of the current flag
			auto flagFound = commandLine.find(flag, lastIndex);
			if (flagFound != std::string::npos) {
				lastIndex = flagFound + flag.length();
			}

			// first argument should always be this program
			if (it == args.begin()) {
				continue;
			}

			if (flag == L"/asinvoker") {
				this->asinvoker = true;
				continue;
			}

			if (flag == L"/shellexec") {
				// bail if we are missing the next argument
				if (++it == args.end()) return false;

				flag = *it;

				// store program parameter
				this->program = flag;

				// advance last index to the end of the current flag
				auto flagFound = commandLine.find(flag, lastIndex);
				if (flagFound != std::string::npos) {
					lastIndex = flagFound + flag.length();
				}

				continue;
			}

			if (flag == L"/method") {
				// bail if we are missing the next argument
				if (++it == args.end()) return false;

				flag = *it;

				// store operation parameter
				this->operation = flag;

				// advance last index to the end of the current flag
				auto flagFound = commandLine.find(flag, lastIndex);
				if (flagFound != std::string::npos) {
					lastIndex = flagFound + flag.length();
				}

				continue;
			}

			if (flag == L"/shellexecparams") {
				// bail if we are missing the next argument
				if (++it == args.end()) return false;

				// find the next space character after this flag
				flagFound = commandLine.find(L" ", lastIndex);
				if (flagFound != std::string::npos) {
					// assign the remaining command line to our program arguments
					this->arguments.assign(commandLine.substr(flagFound + 1));
				}

				// shellexecparams must always be the last flag
				return true;
			}

			// unmatched params are fatal
			return false;
		}

		// we never found an unmatched flag, and didn't have shellexecparams
		return true;
	}
};

struct CommandLineFlags {
public:
	bool shell;
	bool elevate;
	bool netonly;
	bool asinvoker;
	std::wstring thisModule;
	std::wstring program;
	std::wstring programArguments;

	CommandLineFlags() : shell(false), elevate(false), netonly(false), asinvoker(false), thisModule(), program(), programArguments() {}

	bool parse(std::vector<std::wstring> &args, std::wstring &commandLine) {
		size_t lastIndex = 0;

		for (auto it = args.begin(); it != args.end(); ++it) {
			auto& flag = *it;

			// advance last index to the end of the current flag
			auto flagFound = commandLine.find(flag, lastIndex);
			if (flagFound != std::string::npos) {
				lastIndex = flagFound + flag.length();
			}

			// first argument should always be this program
			if (it == args.begin()) {
				this->thisModule.assign(flag);
				continue;
			}

			if (flag == L"/shell" || flag == L"-shell") {
				this->shell = true;
				continue;
			}

			if (flag == L"/elevate" || flag == L"-elevate") {
				this->elevate = true;
				continue;
			}

			if (flag == L"/netonly" || flag == L"-netonly") {
				this->netonly = true;
				continue;
			}

			if (flag == L"/asinvoker" || flag == L"-asinvoker") {
				this->asinvoker = true;
				continue;
			}

			// we have hit our first unmatched flag
			// store the current flag as our program to execute
			this->program.assign(flag);

			// check if there are any flags after this one
			if (++it != args.end()) {
				// find the next space character after this flag
				flagFound = commandLine.find(L" ", lastIndex);
				if (flagFound != std::string::npos) {
					// assign the remaining command line to our program arguments
					this->programArguments.assign(commandLine.substr(flagFound + 1));
				}
			}

			// break out now since we have finished parsing any flags
			return true;
		}

		// we never found an unmatched flag (IE, no program to execute)
		return false;
	}
};

struct AuthenticationBuffer {
private:
	LPVOID _buf;
	ULONG _size;
	ULONG _package;

public:
	LPVOID *buf() { return &this->_buf; }
	ULONG *size() { return &this->_size; }
	ULONG *package() { return &this->_package; }

	AuthenticationBuffer() : _buf(NULL), _size(0) {}
	~AuthenticationBuffer() {
		if (this->_buf) {
			SecureZeroMemory(this->_buf, this->_size);
			CoTaskMemFree(this->_buf);
		}
	}
};

struct AuthCredentials {
private:
	std::wstring _domain;
	std::wstring _username;
	std::wstring _password;

public:
	AuthCredentials() : _domain(), _username(), _password() {}

	void set_domain(LPCWSTR value) { this->_domain.assign(value); }
	void set_username(LPCWSTR value) { this->_username.assign(value); }
	void set_password(LPCWSTR value) { this->_password.assign(value); }

	LPCWSTR domain() { return this->_domain.empty() ? NULL : this->_domain.c_str(); }
	LPCWSTR username() { return this->_username.empty() ? NULL : this->_username.c_str(); }
	LPCWSTR password() { return this->_password.empty() ? NULL : this->_password.c_str(); }
};

bool GetCredentials(AuthCredentials &creds) {
	CREDUI_INFO info;
	AuthenticationBuffer authBuffer;

	info.cbSize = sizeof(CREDUI_INFO);
	info.hwndParent = NULL;
	info.pszMessageText = L"Message";
	info.pszCaptionText = L"Caption";
	info.hbmBanner = NULL;

	// show prompt for credentials
	auto ret = CredUIPromptForWindowsCredentialsW(&info, 0, authBuffer.package(), NULL, 0, authBuffer.buf(), authBuffer.size(), NULL, 0);

	// bail silently on cancel
	if (ret == ERROR_CANCELLED) return false;

	// show error dialog if there was an error
	if (ret != ERROR_SUCCESS) {
		ShowErrorDialogFromLastError();
		return false;
	}

	WCHAR szUserName[256];
	DWORD cchMaxUserName = ARRAYSIZE(szUserName);
	WCHAR szDomainName[256];
	DWORD cchMaxDomainname = ARRAYSIZE(szDomainName);
	WCHAR szPassword[512];
	DWORD cchMaxPassword = ARRAYSIZE(szPassword);

	// unpack credential buffer
	ret = CredUnPackAuthenticationBufferW(0, *authBuffer.buf(), *authBuffer.size(), szUserName, &cchMaxUserName, szDomainName, &cchMaxDomainname, szPassword, &cchMaxPassword);

	// show error dialog on failure
	if (ret == FALSE) {
		ShowErrorDialogFromLastError();
		return false;
	}

	// assign values to given credentials object
	creds.set_domain(szDomainName);
	creds.set_username(szUserName);
	creds.set_password(szPassword);

	return true;
}

void DoRunAs(CommandLineFlags &flags) {
	AuthCredentials credentials;

	STARTUPINFO startupInfo = { 0 };
	PROCESS_INFORMATION processInformation = { 0 };
	DWORD dwLogonFlags = LOGON_WITH_PROFILE;
	LPWSTR lpCommandLine = NULL;
	std::wostringstream argBuilder;
	std::wstring childArguments;

	// setup process start info
	startupInfo.cb = sizeof(STARTUPINFO);

	// set flag for netonly logon
	if (flags.netonly) {
		dwLogonFlags = LOGON_NETCREDENTIALS_ONLY;
	}

	auto needSecondShellevate = flags.asinvoker || flags.elevate || flags.shell;

	// always start with module name
	argBuilder << L'"' << (needSecondShellevate ? flags.thisModule : flags.program) << L'"';

	// build arguments using shellexec if we are required to
	if (needSecondShellevate) {
		// shellexec flag followed by quoted program to execute
		argBuilder << L" /shellexec " << L'"' << flags.program << L'"';

		// method flag followed by value
		argBuilder << L" /method " << (flags.elevate ? L"runas" : L"open");

		// asinvoker flag if requested
		if (flags.asinvoker) {
			argBuilder << L" /asinvoker";
		}

		// append shellexecparams if there are any
		if (!flags.programArguments.empty()) {
			argBuilder << L" /shellexecparams " << flags.programArguments;
		}
	}
	// otherwise copy arguments as is if there are any
	else if (!flags.programArguments.empty()) {
		argBuilder << " " << flags.programArguments;
	}

	// assign to child arguments
	childArguments.assign(argBuilder.str());

	// assign command line pointer if the arguments are not empty
	if (!childArguments.empty()) {
		// this pointer access of a std::wstring is supposed to be fine in c++11
		lpCommandLine = &childArguments[0];
	}

	// get credentials or bail
	if (!GetCredentials(credentials)) return;

	// start process
	auto ret = CreateProcessWithLogonW(credentials.username(), credentials.domain(), credentials.password(), dwLogonFlags, NULL, lpCommandLine, 0, NULL, NULL, &startupInfo, &processInformation);

	// show dialog on error
	if (ret == FALSE) {
		ShowErrorDialogFromLastError();
		return;
	}
}

void DoShellExecute(ShellExecFlags &flags) {
	LPCWSTR lpFile = NULL;
	LPCWSTR lpParameters = NULL;

	// store pointer to file if not empty
	if (!flags.program.empty()) {
		lpFile = flags.program.c_str();
	}

	// store pointer to parameters if not empty
	if (!flags.arguments.empty()) {
		lpParameters = flags.arguments.c_str();
	}

	// asinvoker environment variable if requested
	if (flags.asinvoker) {
		SetEnvironmentVariableW(L"__COMPAT_LAYER", L"RunAsInvoker");
	}

	// initialize OLE
	if (CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE) != S_OK) {
		ShowErrorDialogFromLastError();
		return;
	}

	// shell execute
	int rc = (int)ShellExecuteW(NULL, flags.operation.c_str(), lpFile, lpParameters, NULL, SW_SHOWNORMAL);
	if (rc <= 32 && GetLastError() != ERROR_CANCELLED) {
		ShowErrorDialogFromLastError();
	}
}

std::vector<std::wstring> GetCommandLineArguments() {
	LPCWSTR commandLine = NULL;
	LPWSTR *szArglist = NULL;
	int nArgs;

	// parse arguments
	szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	if (szArglist == NULL) {
		ShowErrorDialogFromLastError();
		return std::vector<std::wstring>();
	}

	// allocate list
	std::vector<std::wstring> args;

	// add arguments to the list
	for (int i = 0; i < nArgs; i++) {
		args.emplace_back(szArglist[i]);
	}

	// free argument list
	LocalFree(szArglist);

	return args;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	ShellExecFlags shellFlags;
	CommandLineFlags flags;
	std::wstring commandLine(GetCommandLineW());

	// store global handle for usage dialog
	g_hInst = hInstance;

	// parse arguments
	auto args = GetCommandLineArguments();
	if (args.empty()) {
		return 1;
	}

	// check for shell execute flag
	if (args.size() > 1 && args[1] == L"/shellexec") {
		if (!shellFlags.parse(args, commandLine)) {
			return 2;
		}
		DoShellExecute(shellFlags);
		return 3;
	}

	// parse flags
	if (!flags.parse(args, commandLine)) {
		// bad usage
		ShowUsageDialog(hInstance);
		return 4;
	}

	// execute runas
	DoRunAs(flags);

	return 0;
}