// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceBaseboard.h"

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

namespace ExpansionInterface
{
CEXIBaseboard::CEXIBaseboard() = default;
{
}

void CEXIBaseboard::SetCS(int cs)
{
  if (cs)
    m_position = 0;
}

bool CEXIBaseboard::IsPresent() const
{
  return true;
}

void CEXIBaseboard::TransferByte(u8& byte)
{
  if (m_position == 0)
  {
    m_command = byte;
  }
  else
  {
    switch (static_cast<Command>(m_command))
    {
    case Command::init:
    {
      static constexpr std::array<u8, 4> ID = {0x06, 0x04, 0x10, 0x00};
      byte = ID[(m_position - 2) & 3];
      break;
    }
    default:
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "EXI BASEBOARD: Unhandled command {:#x}", m_command);
    }
  }
  m_position++;
}

void CEXIBaseboard::DoState(PointerWrap& p)
{
  p.Do(m_position);
  p.Do(m_command);
}
}  // namespace ExpansionInterface
