#pragma once

#include <FrameWork/FrameWork.hpp>

namespace FrameWork
{
	namespace Memory
	{
		HWND GetWindowHandleByPID(int Pid);
		HWND GetWindowHandleByName(std::wstring WindowName);
		DWORD GetProcessPidByName(std::wstring ProcessName);
		uint64_t GetModuleBaseByName(DWORD Pid, std::wstring ModuleName);

		void AttachProces(DWORD Pid);
		void DetachProcess();

		// Finds a process whose exe name *contains* Substring (case-insensitive),
		// e.g. matching "FiveM_b3258_GTAProcess.exe" against L"GTAProcess.exe"
		// without needing the exact build number hardcoded. Writes the full
		// matched exe name to OutName so it can be passed to GetModuleBaseByName.
		DWORD GetProcessPidByNameContains(std::wstring Substring, std::wstring& OutName);

		// Byte-pattern scan of the target process' memory, e.g. "48 8B 05 ?? ?? ?? ?? 33 D2".
		// '?' or '??' are wildcard bytes. Returns the absolute address of the first
		// match, or 0 if not found.
		uint64_t PatternScan(uint64_t Base, size_t Size, const char* Signature);

		// Resolves a `mov/lea reg, [rip+rel32]` style instruction at Address into the
		// absolute address it references (RelativeOffset = byte offset of the rel32,
		// InstructionLength = total instruction length).
		uint64_t ResolveRip(uint64_t Address, int RelativeOffset = 3, int InstructionLength = 7);

		// Reads the PE header of a module already mapped in the attached process to
		// get its in-memory image size (needed to bound a PatternScan).
		size_t GetModuleImageSize(uint64_t ModuleBase);

		void ReadProcessMemoryImpl(uint64_t ReadAddress, LPVOID Read, SIZE_T Size);
		bool WriteProcessMemoryImpl(uint64_t WriteAddress, LPVOID Write, SIZE_T Size);

		// Allocates Size bytes of executable+writable memory inside the attached process.
		// When NearAddress is non-zero, searches within 2 GB of that address first so that
		// 32-bit relative JMP instructions can reach the allocation.
		uint64_t AllocateProcessMemory(size_t Size, DWORD Protection = PAGE_EXECUTE_READWRITE, uint64_t NearAddress = 0);

		std::string ReadProcessMemoryString(uint64_t ReadAddress, SIZE_T StringSize = 256);

		HANDLE GetHandle();

		template <typename T, typename B>
		T ReadMemory(B ReadAddress)
		{
			T Read;
			ReadProcessMemoryImpl((uint64_t)ReadAddress, &Read, sizeof(T));
			return Read;
		}

		template <typename T, typename B>
		bool WriteMemory(B WriteAddress, T Value)
		{
			return WriteProcessMemoryImpl((uint64_t)WriteAddress, &Value, sizeof(T));
		}
	}
}