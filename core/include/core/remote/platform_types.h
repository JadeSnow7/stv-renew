#pragma once

namespace stv::core::remote {

enum class ClientPlatform {
  Windows,
  MacOS,
  Linux,
  Unknown
};

enum class CpuArch {
  X64,
  Arm64,
  Unknown
};

} // namespace stv::core::remote
