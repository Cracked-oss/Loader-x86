#include <windows.h>
#include <iostream>
#include <vector>
#include <string>


struct Patch {
    DWORD offset;
    BYTE value;
};

// List of patches to apply (change or add more)
std::vector<Patch> patches = {
    {0x5B775, 0x90},
    {0x5B776, 0x90},
    {0x5B777, 0x90},
    {0x5B778, 0x90},
    {0x5B779, 0x90},
    {0x5E26A, 0xEB},
    {0x5E2B9, 0x84},
};


void DisplayError(const std::string& message) {
    DWORD errorCode = GetLastError();
    char errorMessage[256] = {0};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, errorCode, 0, errorMessage, sizeof(errorMessage), NULL);
    std::cerr << message << " Error code: " << errorCode << " - " << errorMessage << std::endl;
}

// Get base address using direct method
DWORD_PTR GetDirectBaseAddress(HANDLE hProcess) {
    // Windows executables typically load at their preferred base address
    // For 32-bit applications, this is usually 0x400000
    return 0x400000;
}

int main() {
    const char* fileName = "test.exe"; //Change with your executable

    // Verify file existence
    DWORD fileAttributes = GetFileAttributesA(fileName);
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        std::cerr << "Error: File " << fileName << " not found.\n";
        return 1;
    }

    std::cout << "Launching process " << fileName << "...\n";

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    // Create suspended process
    if (!CreateProcessA(fileName, NULL, NULL, NULL, FALSE,
                       CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        DisplayError("Process creation failed:");
        return 1;
    }

    std::cout << "Process created successfully. PID: " << pi.dwProcessId << "\n";

    // Allow time for full process initialization
    Sleep(1000);

    // Get base address
    DWORD_PTR baseAddress = GetDirectBaseAddress(pi.hProcess);
    std::cout << "Using base address: 0x" << std::hex << baseAddress << std::dec << "\n";

    // Apply patches
    int successCount = 0;
    for (const auto& patch : patches) {
        DWORD_PTR virtualAddress = baseAddress + patch.offset;
        DWORD oldProtection;

        std::cout << "Applying patch at: 0x" << std::hex << virtualAddress << std::dec << "\n";

        // Modify memory protection
        if (!VirtualProtectEx(pi.hProcess, (LPVOID)virtualAddress, 1, PAGE_EXECUTE_READWRITE, &oldProtection)) {
            DisplayError("Memory protection change failed at offset 0x" + std::to_string(patch.offset));
            continue;
        }

        // Write patch
        SIZE_T bytesWritten = 0;
        if (!WriteProcessMemory(pi.hProcess, (LPVOID)virtualAddress, &patch.value, 1, &bytesWritten) || bytesWritten != 1) {
            DisplayError("Memory write failed at offset 0x" + std::to_string(patch.offset));
        } else {
            // Restore protection
            VirtualProtectEx(pi.hProcess, (LPVOID)virtualAddress, 1, oldProtection, &oldProtection);
            successCount++;
            std::cout << "Patch applied successfully at offset 0x" << std::hex << patch.offset << std::dec << "\n";
        }
    }

    std::cout << successCount << " of " << patches.size() << " patches applied successfully.\n";

    // Resume process execution
    if (ResumeThread(pi.hThread) == (DWORD)-1) {
        DisplayError("Process resumption failed:");
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 1;
    }

    std::cout << "Process execution resumed successfully.\n";

    // Clean up handles
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    std::cout << "Loader completed successfully!\n";
    return 0;
}