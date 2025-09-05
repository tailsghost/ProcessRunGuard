#pragma once

#include <windows.h>
#include <string>
#include <mutex>
#include <atomic>

struct ProcessRunGuardResult {
	PROCESS_INFORMATION pi{};
	HANDLE out_read{ nullptr };
	HANDLE err_read{ nullptr };
	bool seccess{ false };
	DWORD last_error{ 0 };
	std::wstring message;
	int32_t code;
};

class ProcessRunGuard {
public:
	ProcessRunGuard(std::atomic<HANDLE>& currentProcess, std::mutex& processMutex, std::atomic<bool>& cancelRequested) : g_currentProcess(currentProcess), g_processMutex(processMutex), g_cancelRequested(cancelRequested) {}
	~ProcessRunGuard() {
		CloseHandlers();
	}
	ProcessRunGuardResult& StartProcessWithRedirect(const std::wstring& commandLine, const std::wstring& workDir) {
		SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
		HANDLE outRead = nullptr, outWrite = nullptr;
		HANDLE errRead = nullptr, errWrite = nullptr;

		if (!CreatePipe(&outRead, &outWrite, &sa, 0)) {
			CloseHandlers();
			_current.last_error = GetLastError();
			_current.message = L"Failed to create stdout pipe";
			_current.seccess = false;
			return _current;
		}

		SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);

		if (!CreatePipe(&errRead, &errWrite, &sa, 0)) {
			CloseHandlers();
			_current.last_error = GetLastError();
			_current.message = L"Failed to create stderr pipe";
			_current.seccess = false;
			return _current;
		}

		SetHandleInformation(errRead, HANDLE_FLAG_INHERIT, 0);

		STARTUPINFOW si{};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdOutput = outWrite;
		si.hStdError = errWrite;

		PROCESS_INFORMATION pi{};
		auto cmdCopy = commandLine;
		auto cmdBuf = &cmdCopy[0];

		auto ok = CreateProcessW(nullptr, cmdBuf, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, workDir.empty() ? nullptr : (LPWSTR)workDir.c_str(), &si, &pi);

		if (!ok) {
			CloseHandlers();
			_current.last_error = GetLastError();
			_current.message = L"CreateProcess failed";
			_current.seccess = false;
			return _current;
		}

		CloseHandle(outWrite);
		CloseHandle(errWrite);


		std::lock_guard<std::mutex> lk(g_processMutex);
		g_currentProcess = pi.hProcess;
		g_cancelRequested = false;

		_current.pi = pi;
		_current.out_read = outRead;
		_current.err_read = errRead;
		_current.seccess = true;
		return _current;
	};

private:
	bool closeHProcess;
	bool closeHThread;
	bool closeOutRead;
	bool closeErrRead;
	std::atomic<HANDLE>& g_currentProcess;
	std::mutex& g_processMutex;
	std::atomic<bool>& g_cancelRequested;
	ProcessRunGuardResult _current;

	void CloseHandlers() {
		if (_current.pi.hProcess && !closeHProcess) {
			CloseHandle(_current.pi.hProcess);
			closeHProcess = true;
		}
		if (_current.pi.hThread && !closeHThread) {
			CloseHandle(_current.pi.hThread);
			closeHThread = true;
		}
		if (_current.out_read && !closeOutRead) {
			CloseHandle(_current.out_read);
			closeOutRead = true;
		}
		if (_current.err_read && !closeErrRead) {
			CloseHandle(_current.err_read);
			closeErrRead = true;
		}
	}
};