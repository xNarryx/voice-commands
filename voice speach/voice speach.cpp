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
#include <mpg123.h>
#include <out123.h>
#include <curl/curl.h>
#include <SFML/Audio.hpp>
#include <random>
#include <nlohmann/json.hpp>
#include <cctype>
#include <regex>

#define VK_MEDIA_NEXT_TRACK 0xB0
#define VK_MEDIA_PREV_TRACK 0xB1
#define KEYEVENTF_KEYUP 0x0002
#define VK_MEDIA_PLAY_PAUSE 0xB3

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
	SetConsoleOutputCP(CP_UTF8);
	setlocale(LC_ALL, "ru_RU.UTF-8");
	std::string line, lineprev;
	std::wstring pythonExe = L"D:\\DEV\\phyton\\WPy64-312101\\python\\python.exe";
	std::wstring scriptFile = L"speech_recognition.py";
	std::ofstream file1("mic_1.txt", std::ios::trunc);
	file1.close();
	std::ofstream file2("mic_2.txt", std::ios::trunc);
	file2.close();
	if (!startPythonScript(pythonExe, scriptFile)) {
		return 1;
	}
	std::unordered_set<std::string> commands;
	while (true)
	{
		std::ifstream file("mic_1.txt");
		std::getline(file, line);
		line = to_utf8(string_to_wstring(line));
		if (line != lineprev) {
			std::cout << line << "\n";
			commands = { to_utf8(L"морс следующий"), to_utf8(L"морс предыдущий"), to_utf8(L"морс пауза"), to_utf8(L"морс стоп") };
			for (auto& cmd : commands) {
				double match = similarity(line, cmd);
				if (match >= threshold && cmd == to_utf8(L"морс следующий")) {
					keybd_event(VK_MEDIA_NEXT_TRACK, 0, 0, 0);
					keybd_event(VK_MEDIA_NEXT_TRACK, 0, KEYEVENTF_KEYUP, 0);
					std::cout << "compare: \"" << cmd << "\" (" << match << "%)\n";
					break;
				}
				else if (match >= threshold && cmd == to_utf8(L"морс предыдущий")) {
					keybd_event(VK_MEDIA_PREV_TRACK, 0, 0, 0);
					keybd_event(VK_MEDIA_PREV_TRACK, 0, KEYEVENTF_KEYUP, 0);
					std::cout << "compare: \"" << cmd << "\" (" << match << "%)\n";
					break;
				}
				else if (match >= threshold && cmd == to_utf8(L"морс пауза")) {
					keybd_event(VK_MEDIA_PLAY_PAUSE, 0, 0, 0);
					keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_KEYUP, 0);
					std::cout << "compare: \"" << cmd << "\" (" << match << "%)\n";
					break;
				}
				else if (match >= threshold && cmd == to_utf8(L"морс стоп")) {
					keybd_event(VK_MEDIA_PLAY_PAUSE, 0, 0, 0);
					keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_KEYUP, 0);
					std::cout << "compare: \"" << cmd << "\" (" << match << "%)\n";
					break;
				}
				else if (match >= threshold && cmd == to_utf8(L"морс выключись")) {
					stopPythonScript();
					std::cout << "compare: \"" << cmd << "\" (" << match << "%)\n";
					break;
				}
			}

			lineprev = line;
		}
		file.close();
		Sleep(1000);
	}
}