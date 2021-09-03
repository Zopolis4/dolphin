// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/EXI/EXI_Device.h"

class PointerWrap;

namespace ExpansionInterface
{
class CEXIBaseboard : public IEXIDevice
{
public:
  CEXIBaseboard();
  void SetCS(int CS) override;
  bool IsPresent() const override;
  void DoState(PointerWrap& p) override;

private:
  enum class Command
  {
    init = 0x00
  };

  // STATE_TO_SAVE
  u32 m_position = 0;
  u32 m_command = 0;

  void TransferByte(u8& _uByte) override;
};
}  // namespace ExpansionInterface