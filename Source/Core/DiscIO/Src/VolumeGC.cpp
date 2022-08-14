// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "VolumeGC.h"
#include "StringUtil.h"
#include "FileMonitor.h"
#include "Filesystem.h"
#include "VolumeCreator.h"

namespace DiscIO
{
CVolumeGC::CVolumeGC(IBlobReader* _pReader)
	: m_pReader(_pReader)
{}

CVolumeGC::~CVolumeGC()
{
	delete m_pReader;
	m_pReader = NULL; // I don't think this makes any difference, but anyway
}

bool CVolumeGC::Read(u64 _Offset, u64 _Length, u8* _pBuffer) const
{
	if (m_pReader == NULL)
		return false;

	FileMon::FindFilename(_Offset);

	return m_pReader->Read(_Offset, _Length, _pBuffer);
}

bool CVolumeGC::RAWRead(u64 _Offset, u64 _Length, u8* _pBuffer) const
{
	return Read(_Offset, _Length, _pBuffer);
}

std::string CVolumeGC::GetUniqueID() const
{
	static const std::string NO_UID("NO_UID");
	if (m_pReader == NULL)
		return NO_UID;

	char ID[12] = {0};
	// Game Code + Maker Code in Disk Header (at offset 0) are 6 bytes
	if (!Read(0, 6, reinterpret_cast<u8*>(ID)))
	{
		PanicAlertT("Failed to read unique ID from disc image");
		return NO_UID;
	}

	// Read boot.id at offset 0x30 (4byte ASCII real game UniqueID), and 1 byte for null terminator
	u8 bootid_buff[0x34] = {0};

	if( !m_pReader || (m_pReader && !(m_pReader->filen)) )
	{
		ERROR_LOG(DISCIO, "Volume reader or filename is invalid");
		return ID;
	}

	DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename( m_pReader->filen->c_str() );
	if(!pVolume)
	{
		ERROR_LOG(DISCIO, "Invalid volume");
		return ID;
	}

	DiscIO::IFileSystem* pFileSystem = DiscIO::CreateFileSystem(pVolume);
	if(!pFileSystem)
	{
		ERROR_LOG(DISCIO, "Invalid file system");
		return ID;
	}

	u64 res = pFileSystem->ReadFile("boot.id", bootid_buff, sizeof(bootid_buff));
	if(res == sizeof(bootid_buff))
	{
		u8 * game_uuid = bootid_buff + 0x30;

		// Check for String ASCII sanity
		bool game_uuid_sane = true;
		for(int i=0 ; i<4 ; i++)
		{
			u8 c = game_uuid[i];
			if( (c < 'A' || c > 'Z') && (c < '0' || c > '9') )
			{
				game_uuid_sane = false;
				break;
			}
		}

		// Append game UUID if sane
		if(game_uuid_sane)
		{
			snprintf(ID + 6, sizeof(ID)-6, "-%s", game_uuid);
			ID[11] = 0;
		}
	}

	return ID;
}

std::string CVolumeGC::GetRevisionSpecificUniqueID() const
{
	char rev[16];
	sprintf(rev, "r%d", GetRevision());
	return GetUniqueID() + rev;
}

IVolume::ECountry CVolumeGC::GetCountry() const
{
	if (!m_pReader)
		return COUNTRY_UNKNOWN;

	u8 CountryCode;
	m_pReader->Read(3, 1, &CountryCode);

	return CountrySwitch(CountryCode);
}

std::string CVolumeGC::GetMakerID() const
{
	if (m_pReader == NULL)
		return std::string();

	char makerID[3];
	if (!Read(0x4, 0x2, (u8*)&makerID))
		return std::string();
	makerID[2] = '\0';

	return makerID;
}

int CVolumeGC::GetRevision() const
{
	if (!m_pReader)
		return 0;

	u8 Revision;
	if (!Read(7, 1, &Revision))
		return 0;

	return Revision;
}

std::vector<std::string> CVolumeGC::GetNames() const
{
	std::vector<std::string> names;
	
	auto const string_decoder = GetStringDecoder(GetCountry());

	char name[0x60 + 1] = {};
	if (m_pReader != NULL && Read(0x20, 0x60, (u8*)name))
		names.push_back(string_decoder(name));

	return names;
}

u32 CVolumeGC::GetFSTSize() const
{
	if (m_pReader == NULL)
		return 0;

	u32 size;
	if (!Read(0x428, 0x4, (u8*)&size))
		return 0;

	return Common::swap32(size);
}

std::string CVolumeGC::GetApploaderDate() const
{
	if (m_pReader == NULL)
		return std::string();

	char date[16];
	if (!Read(0x2440, 0x10, (u8*)&date))
		return std::string();
	// Should be 0 already, but just in case
	date[10] = '\0';

	return date;
}

u64 CVolumeGC::GetSize() const
{
	if (m_pReader)
		return m_pReader->GetDataSize();
	else
		return 0;
}

u64 CVolumeGC::GetRawSize() const
{
	if (m_pReader)
		return m_pReader->GetRawSize();
	else
		return 0;
}

bool CVolumeGC::IsDiscTwo() const
{
	bool discTwo;
	Read(6,1, (u8*) &discTwo);
	return discTwo;
}

auto CVolumeGC::GetStringDecoder(ECountry country) -> StringDecoder
{
	return (COUNTRY_JAPAN == country || COUNTRY_TAIWAN == country) ?
		SHIFTJISToUTF8 : CP1252ToUTF8;
}

} // namespace
