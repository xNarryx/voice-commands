#pragma once
#include <string>
#include <vector>

struct TabInfo {
	std::string id;
	std::string title;
	std::string url;
	std::string wsUrl;
};

bool operaInit(const std::string& host = "localhost", int port = 9222);
std::vector<TabInfo> listTabs();
bool openTab(const std::string& url);
bool closeTab(const std::string& id);
bool navigateTab(const std::string& id, const std::string& url);
