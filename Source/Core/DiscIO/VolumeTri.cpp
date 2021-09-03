// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/VolumeTri.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <mbedtls/sha1.h>

#include "Common/Assert.h"
#include "Common/ColorUtil.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "DiscIO/Blob.h"
#include "DiscIO/DiscExtractor.h"
#include "DiscIO/DiscUtils.h"
#include "DiscIO/Enums.h"
#include "DiscIO/FileSystemGCWii.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
VolumeTri::VolumeTri(std::unique_ptr<BlobReader> reader) : m_reader(std::move(reader))
{
  ASSERT(m_reader);

  m_file_system = [this]() -> std::unique_ptr<FileSystem> {
    auto file_system = std::make_unique<FileSystemGCWii>(this, PARTITION_NONE);
    return file_system->IsValid() ? std::move(file_system) : nullptr;
  };

  m_converted_banner = [this] { return LoadBannerFile(); };
}

VolumeTri::~VolumeTri() = default;

bool VolumeTri::Read(u64 offset, u64 length, u8* buffer, const Partition& partition) const
{
  if (partition != PARTITION_NONE)
    return false;

  return m_reader->Read(offset, length, buffer);
}

const FileSystem* VolumeTri::GetFileSystem(const Partition& partition) const
{
  return m_file_system->get();
}

std::string VolumeTri::GetGameTDBID(const Partition& partition) const
{
  const std::string game_id = GetGameID(partition);
  return game_id;
}

Region VolumeTri::GetRegion() const
{
  return RegionCodeToRegion(m_reader->ReadSwapped<u32>(0x458));
}

std::map<Language, std::string> VolumeTri::GetShortNames() const
{
  return m_converted_banner->short_names;
}

std::map<Language, std::string> VolumeTri::GetLongNames() const
{
  return m_converted_banner->long_names;
}

std::map<Language, std::string> VolumeTri::GetShortMakers() const
{
  return m_converted_banner->short_makers;
}

std::map<Language, std::string> VolumeTri::GetLongMakers() const
{
  return m_converted_banner->long_makers;
}

std::map<Language, std::string> VolumeTri::GetDescriptions() const
{
  return m_converted_banner->descriptions;
}

std::vector<u32> VolumeTri::GetBanner(u32* width, u32* height) const
{
  *width = m_converted_banner->image_width;
  *height = m_converted_banner->image_height;
  return m_converted_banner->image_buffer;
}

BlobType VolumeTri::GetBlobType() const
{
  return m_reader->GetBlobType();
}

u64 VolumeTri::GetSize() const
{
  return m_reader->GetDataSize();
}

bool VolumeTri::IsSizeAccurate() const
{
  return m_reader->IsDataSizeAccurate();
}

u64 VolumeTri::GetRawSize() const
{
  return m_reader->GetRawSize();
}

const BlobReader& VolumeTri::GetBlobReader() const
{
  return *m_reader;
}

Platform VolumeTri::GetVolumeType() const
{
  return Platform::Triforce;
}

bool VolumeTri::IsDatelDisc() const
{
  return false;
}

std::array<u8, 20> VolumeTri::GetSyncHash() const
{
  mbedtls_sha1_context context;
  mbedtls_sha1_init(&context);
  mbedtls_sha1_starts_ret(&context);

  AddGamePartitionToSyncHash(&context);

  std::array<u8, 20> hash;
  mbedtls_sha1_finish_ret(&context, hash.data());
  return hash;
}

VolumeTri::ConvertedGCBanner VolumeTri::LoadBannerFile() const
{
  GCBanner banner_file;
  const u64 file_size = ReadFile(*this, PARTITION_NONE, "opening.bnr",
                                 reinterpret_cast<u8*>(&banner_file), sizeof(GCBanner));
  if (file_size < 4)
  {
    WARN_LOG_FMT(DISCIO, "Could not read opening.bnr.");
    return {};  // Return early so that we don't access the uninitialized banner_file.id
  }

  constexpr u32 BNR1_MAGIC = 0x31524e42;
  constexpr u32 BNR2_MAGIC = 0x32524e42;
  bool is_bnr1;
  if (banner_file.id == BNR1_MAGIC && file_size == BNR1_SIZE)
  {
    is_bnr1 = true;
  }
  else if (banner_file.id == BNR2_MAGIC && file_size == BNR2_SIZE)
  {
    is_bnr1 = false;
  }
  else
  {
    WARN_LOG_FMT(DISCIO, "Invalid opening.bnr. Type: {:#0x} Size: {:#0x}", banner_file.id,
                 file_size);
    return {};
  }

  return ExtractBannerInformation(banner_file, is_bnr1);
}

VolumeTri::ConvertedGCBanner VolumeTri::ExtractBannerInformation(const GCBanner& banner_file,
                                                               bool is_bnr1) const
{
  ConvertedGCBanner banner;

  u32 number_of_languages = 0;
  Language start_language = Language::Unknown;

  if (is_bnr1)  // NTSC
  {
    number_of_languages = 1;
    start_language = GetRegion() == Region::NTSC_J ? Language::Japanese : Language::English;
  }
  else  // PAL
  {
    number_of_languages = 6;
    start_language = Language::English;
  }

  banner.image_width = GC_BANNER_WIDTH;
  banner.image_height = GC_BANNER_HEIGHT;
  banner.image_buffer = std::vector<u32>(GC_BANNER_WIDTH * GC_BANNER_HEIGHT);
  Common::Decode5A3Image(banner.image_buffer.data(), banner_file.image, GC_BANNER_WIDTH,
                         GC_BANNER_HEIGHT);

  for (u32 i = 0; i < number_of_languages; ++i)
  {
    const GCBannerInformation& info = banner_file.information[i];
    Language language = static_cast<Language>(static_cast<int>(start_language) + i);

    std::string description = DecodeString(info.description);
    if (!description.empty())
      banner.descriptions.emplace(language, description);

    std::string short_name = DecodeString(info.short_name);
    if (!short_name.empty())
      banner.short_names.emplace(language, short_name);

    std::string long_name = DecodeString(info.long_name);
    if (!long_name.empty())
      banner.long_names.emplace(language, long_name);

    std::string short_maker = DecodeString(info.short_maker);
    if (!short_maker.empty())
      banner.short_makers.emplace(language, short_maker);

    std::string long_maker = DecodeString(info.long_maker);
    if (!long_maker.empty())
      banner.long_makers.emplace(language, long_maker);
  }

  return banner;
}

VolumeTri::ConvertedGCBanner::ConvertedGCBanner() = default;
VolumeTri::ConvertedGCBanner::~ConvertedGCBanner() = default;
}  // namespace DiscIO