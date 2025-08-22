#pragma once

#include <string>
#include <vector>
#include <format>
#include <concepts>

// Windows headers
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
//#include <devpkey.h> // TODO: Use new property names

#include "../us4oem/Us4OemAPI.h"

// Standard size units
const size_t KiB = 1024;
const size_t MiB = 1024 * KiB;
const size_t GiB = 1024 * MiB;