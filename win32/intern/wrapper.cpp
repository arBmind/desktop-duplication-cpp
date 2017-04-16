#include "wrapper.h"

#include <windows.h>

namespace win32 {
namespace intern {

bool CloseHandle(intptr_t handle) {
    return ::CloseHandle(reinterpret_cast<HANDLE>(handle));
}

}
}
