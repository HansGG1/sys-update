#include "Memory.hpp"
#include <algorithm>
#include <cwctype>

// Cached function pointers resolved once via custom PEB walker (PebWalk.hpp).
// Resolved from kernel32 — same functions as before but no IAT entry,
// no LazyImporter binary patterns, different byte fingerprint entirely.
namespace {
    using PFN_RPM  = BOOL  (WINAPI*)(HANDLE, LPCVOID, LPVOID,  SIZE_T, SIZE_T*);
    using PFN_WPM  = BOOL  (WINAPI*)(HANDLE, LPVOID,  LPCVOID, SIZE_T, SIZE_T*);
    using PFN_OP   = HANDLE(WINAPI*)(DWORD, BOOL, DWORD);
    using PFN_CH   = BOOL  (WINAPI*)(HANDLE);
    using PFN_VAEx = LPVOID(WINAPI*)(HANDLE, LPVOID, SIZE_T, DWORD, DWORD);

    static PFN_RPM  s_RPM  = nullptr;
    static PFN_WPM  s_WPM  = nullptr;
    static PFN_OP   s_OP   = nullptr;
    static PFN_CH   s_CH   = nullptr;
    static PFN_VAEx s_VAEx = nullptr;

    static void EnsureFnPtrs() noexcept
    {
        if (s_RPM) return;

        // All hashes are compile-time constants — the optimizer replaces fnhash/modhash
        // calls on string literals with their computed integer values before codegen.
        static constexpr uint32_t h_k32  = PW::modhash(L"kernel32.dll");
        static constexpr uint32_t h_RPM  = PW::fnhash("ReadProcessMemory");
        static constexpr uint32_t h_WPM  = PW::fnhash("WriteProcessMemory");
        static constexpr uint32_t h_OP   = PW::fnhash("OpenProcess");
        static constexpr uint32_t h_CH   = PW::fnhash("CloseHandle");
        static constexpr uint32_t h_VAEx = PW::fnhash("VirtualAllocEx");

        void* k32 = PW::get_module(h_k32);
        s_RPM  = (PFN_RPM) PW::get_proc(k32, h_RPM);
        s_WPM  = (PFN_WPM) PW::get_proc(k32, h_WPM);
        s_OP   = (PFN_OP)  PW::get_proc(k32, h_OP);
        s_CH   = (PFN_CH)  PW::get_proc(k32, h_CH);
        s_VAEx = (PFN_VAEx)PW::get_proc(k32, h_VAEx);
    }
}

struct TGetWindowHandleData
{
	DWORD Pid;
	std::wstring WindowName;
	HWND hWnd;
};

HANDLE AttachedProcessHandle;
DWORD AttachedProcessPid;

BOOL CALLBACK EnumWindowsCallback(HWND Handle, LPARAM lParam)
{
	TGetWindowHandleData& Data = *(TGetWindowHandleData*)lParam;

	if (Data.Pid == 0)
	{
		int Length = SafeCall(GetWindowTextLength)(Handle);
		if (Length == 0)
			return true;

		std::wstring Buffer(Length + 1, L'\0');
		SafeCall(GetWindowText)(Handle, &Buffer[0], Length + 1);

		if (Data.WindowName != Buffer)
			return true;

		Data.hWnd = Handle;
		return false;
	}
	else
	{
		DWORD Pid;
		SafeCall(GetWindowThreadProcessId)(Handle, &Pid);

		if (Data.Pid != Pid)
			return true;

		Data.hWnd = Handle;
		return false;
	}

	return true;
}

namespace FrameWork
{
	HWND Memory::GetWindowHandleByPID(int Pid)
	{
		TGetWindowHandleData HandleData;
		HandleData.Pid = Pid;
		HandleData.WindowName = XorStr(L"");
		HandleData.hWnd = NULL;

		SafeCall(EnumWindows)(EnumWindowsCallback, (LPARAM)&HandleData);
		return HandleData.hWnd;
	}

	HWND Memory::GetWindowHandleByName(std::wstring WindowName)
	{
		TGetWindowHandleData HandleData;
		HandleData.Pid = 0;
		HandleData.WindowName = WindowName;
		HandleData.hWnd = NULL;

		SafeCall(EnumWindows)(EnumWindowsCallback, (LPARAM)&HandleData);
		return HandleData.hWnd;
	}

	DWORD Memory::GetProcessPidByName(std::wstring ProcessName)
	{
		HANDLE hSnapshot = SafeCall(CreateToolhelp32Snapshot)(TH32CS_SNAPPROCESS, 0);
		if (!hSnapshot || hSnapshot == INVALID_HANDLE_VALUE || hSnapshot == ((HANDLE)(LONG_PTR)ERROR_BAD_LENGTH))
		{
#ifdef _DEBUG
			std::cout << XorStr("[ERROR : FrameWork::Memory::GetProcessPidByName::CreateToolhelp32Snapshot] Error:") << SafeCall(GetLastError)() << std::endl;
#endif
			return 0;
		}

		DWORD Pid;
		PROCESSENTRY32 ProcessEntry;
		ProcessEntry.dwSize = sizeof(ProcessEntry);
		if (SafeCall(Process32First)(hSnapshot, &ProcessEntry))
		{
			while (_wcsicmp(ProcessEntry.szExeFile, ProcessName.c_str()))
			{
				if (!SafeCall(Process32Next)(hSnapshot, &ProcessEntry))
				{
					SafeCall(CloseHandle)(hSnapshot);
					return 0;
				}
			}
			Pid = ProcessEntry.th32ProcessID;
		}
		else
		{
#ifdef _DEBUG
			std::cout << XorStr("[ERROR : FrameWork::Memory::GetProcessPidByName::Process32First] Error:") << SafeCall(GetLastError)() << std::endl;
#endif
			SafeCall(CloseHandle)(hSnapshot);
			return 0;
		}

		SafeCall(CloseHandle)(hSnapshot);
		return Pid;
	}

	uint64_t Memory::GetModuleBaseByName(DWORD Pid, std::wstring ModuleName)
	{
		HANDLE hSnapshot = SafeCall(CreateToolhelp32Snapshot)(TH32CS_SNAPMODULE, Pid);
		if (!hSnapshot || hSnapshot == INVALID_HANDLE_VALUE || hSnapshot == ((HANDLE)(LONG_PTR)ERROR_BAD_LENGTH))
		{
#ifdef _DEBUG
			std::cout << XorStr("[ERROR : FrameWork::Memory::GetModuleBaseByName::CreateToolhelp32Snapshot] Error:") << SafeCall(GetLastError)() << std::endl;
#endif
			return 0;
		}

		uint64_t ModuleBase;
		MODULEENTRY32 ModuleEntry;
		ModuleEntry.dwSize = sizeof(ModuleEntry);
		if (SafeCall(Module32First)(hSnapshot, &ModuleEntry))
		{
			while (_wcsicmp(ModuleEntry.szModule, ModuleName.c_str()))
			{
				if (!SafeCall(Module32Next)(hSnapshot, &ModuleEntry))
				{
					SafeCall(CloseHandle)(hSnapshot);
					return 0;
				}
			}
			ModuleBase = (uint64_t)ModuleEntry.modBaseAddr;
		}
		else
		{
			SafeCall(CloseHandle)(hSnapshot);
			return 0;
		}

		SafeCall(CloseHandle)(hSnapshot);
		return ModuleBase;
	}

	DWORD Memory::GetProcessPidByNameContains(std::wstring Substring, std::wstring& OutName)
	{
		HANDLE hSnapshot = SafeCall(CreateToolhelp32Snapshot)(TH32CS_SNAPPROCESS, 0);
		if (!hSnapshot || hSnapshot == INVALID_HANDLE_VALUE)
			return 0;

		std::transform(Substring.begin(), Substring.end(), Substring.begin(), ::towlower);

		DWORD Result = 0;
		PROCESSENTRY32 ProcessEntry;
		ProcessEntry.dwSize = sizeof(ProcessEntry);
		if (SafeCall(Process32First)(hSnapshot, &ProcessEntry))
		{
			do
			{
				std::wstring FileName = ProcessEntry.szExeFile;
				std::wstring Lower = FileName;
				std::transform(Lower.begin(), Lower.end(), Lower.begin(), ::towlower);

				if (Lower.find(Substring) != std::wstring::npos)
				{
					Result = ProcessEntry.th32ProcessID;
					OutName = FileName;
					break;
				}
			} while (SafeCall(Process32Next)(hSnapshot, &ProcessEntry));
		}

		SafeCall(CloseHandle)(hSnapshot);
		return Result;
	}

	uint64_t Memory::PatternScan(uint64_t Base, size_t Size, const char* Signature)
	{
		if (!AttachedProcessHandle || !Base || !Size)
			return 0;

		struct PatternByte { uint8_t Value; bool Wildcard; };
		std::vector<PatternByte> Pattern;

		for (const char* p = Signature; *p; )
		{
			while (*p == ' ')
				p++;
			if (!*p)
				break;

			if (*p == '?')
			{
				Pattern.push_back({ 0, true });
				p++;
				if (*p == '?')
					p++;
			}
			else
			{
				char Hex[3] = { p[0], p[1], 0 };
				Pattern.push_back({ (uint8_t)strtoul(Hex, nullptr, 16), false });
				p += 2;
			}
		}

		if (Pattern.empty())
			return 0;

		const size_t Chunk = 0x4000;
		std::vector<uint8_t> Buffer(Chunk);
		size_t PatternSize = Pattern.size();
		size_t Step = Chunk > PatternSize ? Chunk - PatternSize : 1;

		for (size_t Offset = 0; Offset + PatternSize < Size; Offset += Step)
		{
			size_t Remaining = Size - Offset;
			size_t ToRead = Chunk < Remaining ? Chunk : Remaining;
			SIZE_T BytesRead = 0;
			if (!s_RPM || !s_RPM(AttachedProcessHandle, (LPCVOID)(Base + Offset), Buffer.data(), ToRead, &BytesRead) || BytesRead < PatternSize)
				continue;

			for (size_t i = 0; i + PatternSize <= BytesRead; i++)
			{
				bool Match = true;
				for (size_t j = 0; j < PatternSize; j++)
				{
					if (!Pattern[j].Wildcard && Buffer[i + j] != Pattern[j].Value)
					{
						Match = false;
						break;
					}
				}

				if (Match)
					return Base + Offset + i;
			}
		}

		return 0;
	}

	uint64_t Memory::ResolveRip(uint64_t Address, int RelativeOffset, int InstructionLength)
	{
		if (!Address)
			return 0;

		int32_t Relative = ReadMemory<int32_t>(Address + RelativeOffset);
		return (uint64_t)((int64_t)(Address + InstructionLength) + Relative);
	}

	size_t Memory::GetModuleImageSize(uint64_t ModuleBase)
	{
		if (!ModuleBase)
			return 0;

		IMAGE_DOS_HEADER Dos = ReadMemory<IMAGE_DOS_HEADER>(ModuleBase);
		if (Dos.e_magic != IMAGE_DOS_SIGNATURE)
			return 0;

		IMAGE_NT_HEADERS64 Nt = ReadMemory<IMAGE_NT_HEADERS64>(ModuleBase + Dos.e_lfanew);
		if (Nt.Signature != IMAGE_NT_SIGNATURE)
			return 0;

		return Nt.OptionalHeader.SizeOfImage;
	}

	void Memory::AttachProces(DWORD Pid)
	{
		EnsureFnPtrs();
		if (AttachedProcessHandle && s_CH)
			s_CH(AttachedProcessHandle);
		AttachedProcessHandle = s_OP ? s_OP(PROCESS_ALL_ACCESS, false, Pid) : nullptr;
		AttachedProcessPid = Pid;
	}

	void Memory::DetachProcess()
	{
		if (s_CH) s_CH(AttachedProcessHandle);
		AttachedProcessHandle = nullptr;
		AttachedProcessPid = 0;
	}

	void Memory::ReadProcessMemoryImpl(uint64_t ReadAddress, LPVOID Read, SIZE_T Size)
	{
		if (AttachedProcessHandle && AttachedProcessPid && s_RPM)
			s_RPM(AttachedProcessHandle, (LPCVOID)ReadAddress, Read, Size, nullptr);
	}

	bool Memory::WriteProcessMemoryImpl(uint64_t WriteAddress, LPVOID Value, SIZE_T Size)
	{
		if (AttachedProcessHandle && AttachedProcessPid && s_WPM)
			if (s_WPM(AttachedProcessHandle, (LPVOID)WriteAddress, (LPCVOID)Value, Size, nullptr))
				return true;
		return false;
	}

	uint64_t Memory::AllocateProcessMemory(size_t Size, DWORD Protection, uint64_t NearAddress)
	{
		if (!AttachedProcessHandle || !s_VAEx)
			return 0;

		if (NearAddress)
		{
			for (uint64_t Offset = 0x10000; Offset < 0x70000000; Offset += 0x10000)
			{
				for (int Dir = 0; Dir < 2; Dir++)
				{
					if (Dir == 1 && NearAddress < Offset)
						continue;

					LPVOID Candidate = (LPVOID)(Dir == 0 ? NearAddress + Offset : NearAddress - Offset);
					void* Result = s_VAEx(AttachedProcessHandle, Candidate, Size, MEM_COMMIT | MEM_RESERVE, Protection);
					if (Result)
						return (uint64_t)Result;
				}
			}
		}

		return (uint64_t)s_VAEx(AttachedProcessHandle, nullptr, Size, MEM_COMMIT | MEM_RESERVE, Protection);
	}

	HANDLE Memory::GetHandle()
	{
		return AttachedProcessHandle;
	}

	std::string Memory::ReadProcessMemoryString(uint64_t ReadAddress, SIZE_T StringSize)
	{
		const int BufferSize = 256;
		char Buffer[BufferSize];
		int BytesRead = 0;

		while (BytesRead < BufferSize && BytesRead < (int)StringSize)
		{
			char Character;
			ReadProcessMemoryImpl((uint64_t)ReadAddress + BytesRead, &Character, sizeof(char));
			Buffer[BytesRead] = Character;
			if (Character == '\0') break;
			BytesRead++;
		}

		return std::string(Buffer);
	}
}
