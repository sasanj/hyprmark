#pragma once

#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>
#include <hyprutils/memory/SharedPtr.hpp>

using namespace Hyprutils::Memory;

#define SP CSharedPointer
#define WP CWeakPointer
#define UP CUniquePointer

// Set by CMake; fallback matches CMake's default install prefix.
#ifndef HYPRMARK_DATADIR
#define HYPRMARK_DATADIR "/usr/local/share/hyprmark"
#endif
