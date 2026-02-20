#pragma once

#include "core/logger.h"
#include <memory>

namespace stv::infra {

/// Factory for the default console logger implementation.
std::unique_ptr<stv::core::ILogger> create_console_logger();

} // namespace stv::infra
