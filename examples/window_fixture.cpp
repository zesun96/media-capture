/**
 * Copyright (c) 2026 sunze
 * SPDX-License-Identifier: Apache-2.0
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <charconv>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <string_view>

namespace {

constexpr wchar_t kWindowClass[] = L"MediaCaptureAcceptanceWindow";
constexpr UINT_PTR kPaintTimer = 1;
constexpr UINT_PTR kMinimizeTimer = 2;
constexpr UINT_PTR kRestoreTimer = 3;
constexpr UINT_PTR kCloseTimer = 4;

struct Timings {
	UINT minimize_ms = 3000;
	UINT restore_ms = 7000;
	UINT close_ms = 12000;
};

Timings g_timings;
bool g_hide_instead_of_minimize = false;
unsigned int g_paint_count = 0;

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM word, LPARAM parameter) {
	switch (message) {
	case WM_CREATE:
		::SetTimer(window, kPaintTimer, 100, nullptr);
		if (g_timings.minimize_ms > 0) {
			::SetTimer(window, kMinimizeTimer, g_timings.minimize_ms, nullptr);
		}
		if (g_timings.restore_ms > 0) {
			::SetTimer(window, kRestoreTimer, g_timings.restore_ms, nullptr);
		}
		::SetTimer(window, kCloseTimer, g_timings.close_ms, nullptr);
		return 0;
	case WM_TIMER:
		if (word == kPaintTimer) {
			++g_paint_count;
			::InvalidateRect(window, nullptr, FALSE);
		} else if (word == kMinimizeTimer) {
			::KillTimer(window, kMinimizeTimer);
			::ShowWindow(window, g_hide_instead_of_minimize ? SW_HIDE : SW_MINIMIZE);
		} else if (word == kRestoreTimer) {
			::KillTimer(window, kRestoreTimer);
			::ShowWindow(window, g_hide_instead_of_minimize ? SW_SHOW : SW_RESTORE);
			::SetForegroundWindow(window);
		} else if (word == kCloseTimer) {
			::DestroyWindow(window);
		}
		return 0;
	case WM_PAINT: {
		PAINTSTRUCT paint{};
		const HDC context = ::BeginPaint(window, &paint);
		RECT client{};
		::GetClientRect(window, &client);
		const HBRUSH background = ::CreateSolidBrush(RGB(30 + (g_paint_count * 3) % 120, 80, 180));
		::FillRect(context, &client, background);
		::DeleteObject(background);
		const wchar_t text[] = L"media-capture window minimize / restore fixture";
		::SetBkMode(context, TRANSPARENT);
		::SetTextColor(context, RGB(255, 255, 255));
		::TextOutW(context, 24, 24, text, static_cast<int>(std::size(text) - 1));
		::EndPaint(window, &paint);
		return 0;
	}
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	default:
		return ::DefWindowProcW(window, message, word, parameter);
	}
}

bool ParseSeconds(const char* text, UINT& milliseconds) {
	unsigned int seconds = 0;
	const std::string_view value(text);
	const auto result = std::from_chars(value.data(), value.data() + value.size(), seconds);
	if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || seconds > 600) {
		return false;
	}
	milliseconds = seconds * 1000;
	return true;
}

} // namespace

int main(int argc, char* argv[]) {
	if (argc > 5 || (argc > 1 && !ParseSeconds(argv[1], g_timings.close_ms)) ||
	    (argc > 2 && !ParseSeconds(argv[2], g_timings.minimize_ms)) ||
	    (argc > 3 && !ParseSeconds(argv[3], g_timings.restore_ms)) ||
	    (argc > 4 && std::string_view(argv[4]) != "minimize" &&
	     std::string_view(argv[4]) != "hide") ||
	    g_timings.close_ms == 0 ||
	    (g_timings.minimize_ms > 0 && g_timings.minimize_ms >= g_timings.close_ms) ||
	    (g_timings.restore_ms > 0 && (g_timings.restore_ms >= g_timings.close_ms ||
	                                  g_timings.restore_ms <= g_timings.minimize_ms))) {
		std::cerr << "Usage: " << argv[0]
		          << " [duration-seconds] [minimize-after-seconds] [restore-after-seconds]"
		             " [minimize|hide]\n";
		return 2;
	}
	g_hide_instead_of_minimize = argc > 4 && std::string_view(argv[4]) == "hide";

	const HINSTANCE instance = ::GetModuleHandleW(nullptr);
	WNDCLASSW window_class{};
	window_class.lpfnWndProc = WindowProcedure;
	window_class.hInstance = instance;
	window_class.hCursor = ::LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
	window_class.lpszClassName = kWindowClass;
	if (::RegisterClassW(&window_class) == 0) {
		std::cerr << "failed to register fixture window class\n";
		return 1;
	}

	const HWND window = ::CreateWindowExW(0, kWindowClass, L"media-capture acceptance fixture",
	                                      WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800,
	                                      500, nullptr, nullptr, instance, nullptr);
	if (window == nullptr) {
		std::cerr << "failed to create fixture window\n";
		return 1;
	}
	::ShowWindow(window, SW_SHOW);
	::UpdateWindow(window);
	std::cout << "window:" << reinterpret_cast<std::uintptr_t>(window) << std::endl;

	MSG message{};
	while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
		::TranslateMessage(&message);
		::DispatchMessageW(&message);
	}
	return 0;
}
