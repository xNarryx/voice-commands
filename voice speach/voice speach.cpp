#include <dpp/dpp.h>
#include <windows.h>
#include <iostream>
#include <locale>
#include <fstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <limits>
#include <codecvt>
#include <curl/curl.h>
#include <SFML/Audio.hpp>
#include <random>
#include <nlohmann/json.hpp>
#include <cctype>
#include <regex>
#include "opera_control.h"

#define VK_MEDIA_NEXT_TRACK 0xB0
#define VK_MEDIA_PREV_TRACK 0xB1
#define KEYEVENTF_KEYUP 0x0002
#define VK_MEDIA_PLAY_PAUSE 0xB3
struct SearchData {
	std::wstring partTitle;
	HWND foundHwnd = nullptr;
};
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
	SearchData* data = reinterpret_cast<SearchData*>(lParam);

	if (!IsWindowVisible(hwnd))
		return TRUE;

	wchar_t title[256];
	GetWindowTextW(hwnd, title, sizeof(title) / sizeof(wchar_t));

	std::wstring winTitle = title;
	if (!winTitle.empty()) {
		std::wstring lowerTitle = winTitle;
		std::wstring lowerSearch = data->partTitle;
		std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(), ::towlower);
		std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::towlower);

		if (lowerTitle.find(lowerSearch) != std::wstring::npos) {
			data->foundHwnd = hwnd;
			return FALSE;
		}
	}
	return TRUE;
}

static std::string to_utf8(const std::wstring& wstr) {
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string str_to(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str_to[0], size_needed, NULL, NULL);
	return str_to;
}
std::vector<std::string> split(const std::string& str, char delim) {
	std::stringstream ss(str);
	std::string item;
	std::vector<std::string> tokens;

	while (std::getline(ss, item, delim)) {
		if (!item.empty())
			tokens.push_back(item);
	}
	return tokens;
}
std::string keep_digits(std::string str) {
	std::erase_if(str, [](unsigned char c) {
		return !std::isdigit(c);
		});
	return str;
}
static std::wstring string_to_wstring(const std::string& str) {
	int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
	if (len == 0) return L"";
	std::wstring wstr(len, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
	wstr.pop_back();
	return wstr;
}
static std::string wstring_to_string(const std::wstring& wstr) {
	int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (len == 0) return "";
	std::string str(len, '\0');
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], len, nullptr, nullptr);
	str.pop_back();
	return str;
}
int levenshtein(const std::string& s1, const std::string& s2) {
	int m = (int)s1.size();
	int n = (int)s2.size();
	std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));
	for (int i = 0; i <= m; ++i) dp[i][0] = i;
	for (int j = 0; j <= n; ++j) dp[0][j] = j;
	for (int i = 1; i <= m; ++i) {
		for (int j = 1; j <= n; ++j) {
			int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
			dp[i][j] = std::min({ dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + cost });
		}
	}
	return dp[m][n];
}
double similarity(const std::string& s1, const std::string& s2) {
	int dist = levenshtein(s1, to_utf8(string_to_wstring(s2)));
	int max_len = std::max((int)s1.size(), (int)s2.size());
	if (max_len == 0) return 100.0;
	return (1.0 - (double)dist / max_len) * 100.0;
}
bool BringWindowToFrontByPartialTitle(const std::wstring& partTitle) {
	SearchData data{ partTitle, nullptr };
	EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));

	if (!data.foundHwnd) {
		std::cout << to_utf8(L"Окно, содержащее \"") << to_utf8(partTitle) << to_utf8(L"\" не найдено.\n");
		return false;
	}

	ShowWindow(data.foundHwnd, SW_SHOWNORMAL);
	SetForegroundWindow(data.foundHwnd);
	return true;
}
bool IsWindowOpenByPartialTitle(const std::wstring& partTitle) {
	SearchData data{ partTitle, nullptr };
	EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));
	return (data.foundHwnd != nullptr);
}
bool CloseWindowByPartialTitle(const std::wstring& partTitle) {
	SearchData data{ partTitle, nullptr };
	EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));

	if (data.foundHwnd) {
		PostMessageW(data.foundHwnd, WM_CLOSE, 0, 0);
		return true;
	}
	return false;
}
bool MinimizeWindowByPartialTitle(const std::wstring& partTitle) {
	SearchData data{ partTitle, nullptr };
	EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));

	if (data.foundHwnd) {
		ShowWindow(data.foundHwnd, SW_MINIMIZE);
		return true;
	}
	return false;
}

PROCESS_INFORMATION pi;
bool startPythonScript(const std::wstring& pythonPath, const std::wstring& scriptPath) {
	std::wstring cmd = L"\"" + pythonPath + L"\" \"" + scriptPath + L"\"";

	STARTUPINFOW si = { sizeof(si) };
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	si.wShowWindow = SW_HIDE;
	si.hStdOutput = NULL;
	si.hStdError = NULL;
	si.hStdInput = NULL;

	if (CreateProcessW(
		NULL,
		cmd.data(),
		NULL, NULL, FALSE,
		CREATE_NO_WINDOW,
		NULL, NULL, &si, &pi
	)) {
		std::wcout << L"Запущен Пайтон скрипт: " << scriptPath << std::endl;
		return true;
	}
	else {
		std::wcout << L"Ошибка: " << GetLastError() << std::endl;
		return false;
	}
}
void stopPythonScript() {
	if (pi.hProcess) {
		TerminateProcess(pi.hProcess, 0);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		std::cout << "Скрипт остановлен" << std::endl;
	}
}
double threshold = 80.0;

int main()
{
	std::cout << "browser initialization trying\n";
	if (operaInit("localhost", 9222)) {
		std::cout << "sucsesfuly\n";
	}
	else {
		std::cout << "error\n";
	}

	SetConsoleOutputCP(CP_UTF8);
	setlocale(LC_ALL, "ru_RU.UTF-8");
	std::string line, lineprev;
	std::wstring pythonExe = L"D:\\DEV\\phyton\\WPy64-312101\\python\\python.exe";
	std::wstring scriptFile = L"speech_recognition.py";
	std::ofstream file1("mic_1.txt", std::ios::trunc);
	file1.close();
	std::ofstream file2("mic_2.txt", std::ios::trunc);
	file2.close();
	while (IsWindowOpenByPartialTitle(L"Phyton")) {
		std::cout << "closing phyton..\n";
		CloseWindowByPartialTitle(L"Phyton");
	}
	std::cout << "opening phyton script\n";
	startPythonScript(pythonExe, scriptFile);
	std::unordered_set<std::string> commands;
	double match, matchnext;
	std::string nextCmd;
	auto activate_time = std::chrono::system_clock::now();
	while (true)
	{
		std::ifstream file("mic_1.txt");
		std::getline(file, line);
		line = to_utf8(string_to_wstring(line));
		if (line != lineprev) {
			std::cout << line << "\n";
			std::vector<std::string> args = split(line, ' ');
			for (auto argIt = args.begin(); argIt != args.end(); ++argIt) {
				auto& arg = *argIt;

				commands = { to_utf8(L"морс"),
					to_utf8(L"следующий"), to_utf8(L"предыдущий"),
					to_utf8(L"пауза"), to_utf8(L"стоп"),
					to_utf8(L"открой"), to_utf8(L"браузер"),
					to_utf8(L"закрой"), to_utf8(L"сверни") };

				// Активация "морс"
				if (similarity(arg, to_utf8(L"морс")) >= threshold) {
					activate_time = std::chrono::system_clock::now() + std::chrono::seconds{ 15 };
					std::cout << similarity(arg, to_utf8(L"морс")) << " " << activate_time << "\n";
				}

				if (std::chrono::system_clock::now() <= activate_time) {
					for (auto cmdIt = commands.begin(); cmdIt != commands.end(); ++cmdIt) {
						auto& cmd = *cmdIt;
						match = similarity(arg, cmd);

						if (match >= threshold && cmd == to_utf8(L"следующий")) {
							keybd_event(VK_MEDIA_NEXT_TRACK, 0, 0, 0);
							keybd_event(VK_MEDIA_NEXT_TRACK, 0, KEYEVENTF_KEYUP, 0);
							std::cout << "compare: \"" << cmd << "\" (" << match << "%)\n";
							break;
						}
						else if (match >= threshold && cmd == to_utf8(L"предыдущий")) {
							keybd_event(VK_MEDIA_PREV_TRACK, 0, 0, 0);
							keybd_event(VK_MEDIA_PREV_TRACK, 0, KEYEVENTF_KEYUP, 0);
							std::cout << "compare: \"" << cmd << "\" (" << match << "%)\n";
							break;
						}
						else if (match >= threshold && cmd == to_utf8(L"пауза")) {
							keybd_event(VK_MEDIA_PLAY_PAUSE, 0, 0, 0);
							keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_KEYUP, 0);
							std::cout << "compare: \"" << cmd << "\" (" << match << "%)\n";
							break;
						}
						else if (match >= threshold && cmd == to_utf8(L"стоп")) {
							keybd_event(VK_MEDIA_PLAY_PAUSE, 0, 0, 0);
							keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_KEYUP, 0);
							std::cout << "compare: \"" << cmd << "\" (" << match << "%)\n";
							break;
						}
						else if (match >= threshold && cmd == to_utf8(L"выключись")) {
							stopPythonScript();
							std::cout << "compare: \"" << cmd << "\" (" << match << "%)\n";
							break;
						}
						else if (match >= threshold && cmd == to_utf8(L"открой")) {
							auto nextArgIt = argIt;
							if (++nextArgIt != args.end()) {
								auto& nextArg = *nextArgIt;
								if (similarity(nextArg, to_utf8(L"браузер")) >= threshold) {
									std::cout << "compare: \"" << nextArg << "\" (" << match << "%)\n";
									BringWindowToFrontByPartialTitle(L"opera");
								}
								if (similarity(nextArg, to_utf8(L"ютуб")) >= threshold) {
									std::cout << "compare: \"" << nextArg << "\" (" << match << "%)\n";
									if (openTab("https://youtube.com")) {
										std::cout << "opened youtube\n";
									}
									else {
										std::cout << "cant open\n";
									}
								}
							}
							std::cout << "compare: \"" << cmd << "\" (" << match << "%)\n";
							break;
						}
						else if (match >= threshold && cmd == to_utf8(L"закрой")) {
							auto nextArgIt = argIt;
							if (++nextArgIt != args.end()) {
								auto& nextArg = *nextArgIt;
								if (similarity(nextArg, to_utf8(L"браузер")) >= threshold) {
									CloseWindowByPartialTitle(L"opera");
								}
							}
							std::cout << "compare: \"" << cmd << "\" (" << match << "%)\n";
							break;
						}
						else if (match >= threshold && cmd == to_utf8(L"сверни")) {
							auto nextArgIt = argIt;
							if (++nextArgIt != args.end()) {
								auto& nextArg = *nextArgIt;
								if (similarity(nextArg, to_utf8(L"браузер")) >= threshold) {
									MinimizeWindowByPartialTitle(L"opera");
								}
							}
							std::cout << "compare: \"" << cmd << "\" (" << match << "%)\n";
							break;
						}
					}
				}
			}
			lineprev = line;
		}
		file.close();
		Sleep(1000);
	}
}