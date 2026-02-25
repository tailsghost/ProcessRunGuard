#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <thread>

struct ProcessRunGuardResult {
	std::wstring stdoutText;
	std::wstring stderrText;
	std::wstring command;
	int code = -1;
	bool success = false;
};



class ProcessRunGuard {
public:
	ProcessRunGuard() = default;
	~ProcessRunGuard() = default;

	void RunCommand(const std::wstring& command, ProcessRunGuardResult& outResult) {
		outResult.command = command;

		SECURITY_ATTRIBUTES sa{};
		sa.nLength = sizeof(sa);
		sa.bInheritHandle = TRUE;

		HANDLE outRead = nullptr, outWrite = nullptr;
		HANDLE errRead = nullptr, errWrite = nullptr;

		if (!CreatePipe(&outRead, &outWrite, &sa, 0))
			Fail(outResult, L"CreatePipe stdout");

		if (!CreatePipe(&errRead, &errWrite, &sa, 0))
			Fail(outResult, L"CreatePipe stderr");

		SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);
		SetHandleInformation(errRead, HANDLE_FLAG_INHERIT, 0);

		STARTUPINFOW si{};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdOutput = outWrite;
		si.hStdError = errWrite;

		PROCESS_INFORMATION pi{};

		std::wstring fullCmd = L"cmd.exe /C " + command;
		std::vector<wchar_t> buf(fullCmd.begin(), fullCmd.end());
		buf.push_back(0);

		BOOL ok = CreateProcessW(
			nullptr,
			buf.data(),
			nullptr,
			nullptr,
			TRUE,
			CREATE_NO_WINDOW,
			nullptr,
			nullptr,
			&si,
			&pi
		);

		CloseHandle(outWrite);
		CloseHandle(errWrite);

		if (!ok)
			Fail(outResult, L"CreateProcessW failed");

		std::string outA, errA;
		outA = ReadPipe(outRead);
		errA = ReadPipe(errRead);

		WaitForSingleObject(pi.hProcess, INFINITE);
		GetExitCodeProcess(pi.hProcess, (DWORD*)&outResult.code);

		CloseHandle(outRead);
		CloseHandle(errRead);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		outResult.stdoutText = Utf8ToWide(outA);
		outResult.stderrText = Utf8ToWide(errA);
		outResult.success = (outResult.code == 0);
	}

private:
	static void Fail(ProcessRunGuardResult& r, const std::wstring& msg) {
		r.stderrText = msg;
		r.success = false;
	}

	static std::string ReadPipe(HANDLE h) {
		std::string s;
		char buf[4096];
		DWORD read = 0;
		while(ReadFile(h, buf, sizeof(buf), &read, nullptr) && read > 0)
			s.append(buf, read);
		return s;
	}

	static std::wstring Utf8ToWide(const std::string& s) {
		if (s.empty()) return {};
		int size = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
		std::wstring out(size, 0);
		MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), size);
		return out;
	}
};