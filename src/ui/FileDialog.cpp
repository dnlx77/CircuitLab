#include "UI/FileDialog.h"

#ifdef _WIN32

// Questo file è l'unico che include windows.h: le sue macro (min, max, near, far...)
// darebbero fastidio al resto del codice, che usa std::min/std::max e SFML.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>

#pragma comment(lib, "comdlg32.lib")

namespace {

	std::wstring ToWide(const std::string &utf8)
	{
		if (utf8.empty())
			return std::wstring();
		const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
		std::wstring wide(static_cast<size_t>(size), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), wide.data(), size);
		return wide;
	}

	std::string ToUtf8(const wchar_t *wide)
	{
		const int size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
		if (size <= 1)
			return std::string();
		std::string utf8(static_cast<size_t>(size - 1), '\0');
		WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8.data(), size, nullptr, nullptr);
		return utf8;
	}

	// Filtri: coppie "descrizione\0pattern\0", chiuse da un \0 in più
	constexpr wchar_t FILTER[] = L"Circuiti CircuitLab (*.json)\0*.json\0Tutti i file (*.*)\0*.*\0";

	std::optional<std::string> Run(bool save, void *owner, const std::string &initialDir, const std::string &suggestedName)
	{
		wchar_t path[32768] = {};
		const std::wstring suggested = ToWide(suggestedName);
		wcsncpy_s(path, suggested.c_str(), _TRUNCATE);

		const std::wstring dir = ToWide(initialDir);

		OPENFILENAMEW ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = static_cast<HWND>(owner);
		ofn.lpstrFilter = FILTER;
		ofn.nFilterIndex = 1;
		ofn.lpstrFile = path;
		ofn.nMaxFile = static_cast<DWORD>(std::size(path));
		ofn.lpstrInitialDir = dir.empty() ? nullptr : dir.c_str();
		ofn.lpstrDefExt = L"json";
		// OFN_NOCHANGEDIR: senza, la finestra cambia la cartella di lavoro del processo e
		// da quel momento font e risorse relative non si trovano più
		ofn.Flags = OFN_NOCHANGEDIR | OFN_EXPLORER | OFN_PATHMUSTEXIST |
			(save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

		const BOOL chosen = save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
		if (!chosen)
			return std::nullopt; // annullato (o errore: in entrambi i casi non c'è nulla da fare)

		return ToUtf8(path);
	}

}

std::optional<std::string> CircuitLab::FileDialog::OpenCircuit(void *ownerWindow, const std::string &initialDir)
{
	return Run(false, ownerWindow, initialDir, std::string());
}

std::optional<std::string> CircuitLab::FileDialog::SaveCircuit(void *ownerWindow, const std::string &initialDir, const std::string &suggestedName)
{
	return Run(true, ownerWindow, initialDir, suggestedName);
}

#else

std::optional<std::string> CircuitLab::FileDialog::OpenCircuit(void *, const std::string &)
{
	return std::nullopt;
}

std::optional<std::string> CircuitLab::FileDialog::SaveCircuit(void *, const std::string &, const std::string &)
{
	return std::nullopt;
}

#endif
