// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/SI/SI_Device.h"

namespace SerialInterface
{
class CSIDevice_Baseboard : public ISIDevice
{
public:
  // Constructor
  CSIDevice_Baseboard(SIDevices device, int device_number);

  int RunBuffer(u8* buffer, int request_length) override;
  bool GetData(u32& hi, u32& low) override;
  void SendCommand(u32 command, u8 poll) override;

private:
  enum EBufferCommands
  {
    CMD_RESET = 0x00,
    CMD_GCAM = 0x70 // fix naming
  };
};
}  // namespace SerialInterface