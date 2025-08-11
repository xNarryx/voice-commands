#include "opera_control.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <sstream>
#include <thread>
#include <nlohmann/json.hpp>
#include <boost/beast/core.hpp>

namespace beast = boost::beast;
using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;
namespace websocket = boost::beast::websocket;
using json = nlohmann::json;

static std::string g_host;
static int g_port;

bool operaInit(const std::string& host, int port) {
	g_host = host;
	g_port = port;
	return true;
}
static std::string httpPUT(const std::string& target) {
	try {
		boost::asio::io_context ioc;
		tcp::resolver resolver(ioc);
		tcp::socket socket(ioc);
		auto const results = resolver.resolve(g_host, std::to_string(g_port));
		boost::asio::connect(socket, results.begin(), results.end());

		http::request<http::string_body> req{ http::verb::put, target, 11 };
		req.set(http::field::host, g_host);
		req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

		http::write(socket, req);

		boost::beast::flat_buffer buffer;
		http::response<http::dynamic_body> res;
		http::read(socket, buffer, res);

		std::ostringstream ss;
		ss << boost::beast::buffers_to_string(res.body().data());
		return ss.str();
	}
	catch (std::exception& e) {
		std::cerr << "HTTP PUT error: " << e.what() << "\n";
		return "";
	}
}

static std::string httpGET(const std::string& target) {
	try {
		boost::asio::io_context ioc;
		tcp::resolver resolver(ioc);
		tcp::socket socket(ioc);
		auto const results = resolver.resolve(g_host, std::to_string(g_port));
		boost::asio::connect(socket, results.begin(), results.end());

		http::request<http::string_body> req{ http::verb::get, target, 11 };
		req.set(http::field::host, g_host);
		req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

		http::write(socket, req);

		boost::beast::flat_buffer buffer;
		http::response<http::dynamic_body> res;
		http::read(socket, buffer, res);

		std::ostringstream ss;
		ss << boost::beast::buffers_to_string(res.body().data());
		return ss.str();
	}
	catch (std::exception& e) {
		std::cerr << "HTTP error: " << e.what() << "\n";
		return "";
	}
}

std::vector<TabInfo> listTabs() {
	std::vector<TabInfo> tabs;
	std::string resp = httpGET("/json");
	if (resp.empty()) return tabs;

	auto arr = json::parse(resp);
	for (auto& t : arr) {
		if (t["type"] == "page") {
			TabInfo info;
			info.id = t["id"];
			info.title = t["title"];
			info.url = t["url"];
			info.wsUrl = t["webSocketDebuggerUrl"];
			tabs.push_back(info);
		}
	}
	return tabs;
}

static bool sendWSCommand(const std::string& wsUrl, const json& cmd) {
	try {
		// Разбор wsUrl: "ws://host:port/..."
		auto pos = wsUrl.find("ws://");
		if (pos != 0) {
			std::cerr << "Unsupported WS URL: " << wsUrl << "\n";
			return false;
		}
		auto path_pos = wsUrl.find('/', 5);
		std::string host_port = wsUrl.substr(5, path_pos - 5);
		std::string path = wsUrl.substr(path_pos);

		auto colon_pos = host_port.find(':');
		std::string host = host_port.substr(0, colon_pos);
		std::string port = colon_pos != std::string::npos ? host_port.substr(colon_pos + 1) : "80";

		boost::asio::io_context ioc;

		tcp::resolver resolver(ioc);
		websocket::stream<tcp::socket> ws(ioc);

		auto const results = resolver.resolve(host, port);
		boost::asio::connect(ws.next_layer(), results.begin(), results.end());

		ws.handshake(host, path);

		ws.write(boost::asio::buffer(cmd.dump()));

		boost::beast::flat_buffer buffer;
		ws.read(buffer);

		std::cout << "WS Response: " << beast::buffers_to_string(buffer.data()) << "\n";

		ws.close(websocket::close_code::normal);

		return true;
	}
	catch (std::exception& e) {
		std::cerr << "WS error: " << e.what() << "\n";
		return false;
	}
}

bool openTab(const std::string& url) {
	std::string resp = httpPUT("/json/new?" + url);
	std::cout << "openTab response: " << resp << std::endl;
	return !resp.empty();
}

bool closeTab(const std::string& id) {
	std::string resp = httpGET("/json/close/" + id);
	return !resp.empty();
}

bool navigateTab(const std::string& id, const std::string& url) {
	auto tabs = listTabs();
	for (auto& t : tabs) {
		if (t.id == id) {
			json cmd = {
				{"id", 1},
				{"method", "Page.navigate"},
				{"params", {{"url", url}}}
			};
			return sendWSCommand(t.wsUrl, cmd);
		}
	}
	return false;
}