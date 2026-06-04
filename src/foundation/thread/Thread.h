#pragma once

#include <pthread.h>
#include <string>
#include <thread>

namespace tri::foundation {

inline void setCurrentThreadName(const std::string& name) {
#if defined(__linux__)
    pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
#else
    (void)name;
#endif
}

} // namespace tri::foundation
