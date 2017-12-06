// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/swap.h"
#include "core/core.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
/// NCCH header (Note: "NCCH" appears to be a publicly unknown acronym)

struct NCCH_Header {
    u8 signature[0x100];
    u32_le magic;
    u32_le content_size;
    u8 partition_id[8];
    u16_le maker_code;
    u16_le version;
    u8 reserved_0[4];
    u64_le program_id;
    u8 reserved_1[0x10];
    u8 logo_region_hash[0x20];
    u8 product_code[0x10];
    u8 extended_header_hash[0x20];
    u32_le extended_header_size;
    u8 reserved_2[4];
    u8 flags[8];
    u32_le plain_region_offset;
    u32_le plain_region_size;
    u32_le logo_region_offset;
    u32_le logo_region_size;
    u32_le exefs_offset;
    u32_le exefs_size;
    u32_le exefs_hash_region_size;
    u8 reserved_3[4];
    u32_le romfs_offset;
    u32_le romfs_size;
    u32_le romfs_hash_region_size;
    u8 reserved_4[4];
    u8 exefs_super_block_hash[0x20];
    u8 romfs_super_block_hash[0x20];
};

static_assert(sizeof(NCCH_Header) == 0x200, "NCCH header structure size is wrong");

////////////////////////////////////////////////////////////////////////////////////////////////////
// ExeFS (executable file system) headers

struct ExeFs_SectionHeader {
    char name[8];
    u32 offset;
    u32 size;
};

struct ExeFs_Header {
    ExeFs_SectionHeader section[8];
    u8 reserved[0x80];
    u8 hashes[8][0x20];
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// ExHeader (executable file system header) headers

struct ExHeader_SystemInfoFlags {
    u8 reserved[5];
    u8 flag;
    u8 remaster_version[2];
};

struct ExHeader_CodeSegmentInfo {
    u32 address;
    u32 num_max_pages;
    u32 code_size;
};

struct ExHeader_CodeSetInfo {
    u8 name[8];
    ExHeader_SystemInfoFlags flags;
    ExHeader_CodeSegmentInfo text;
    u32 stack_size;
    ExHeader_CodeSegmentInfo ro;
    u8 reserved[4];
    ExHeader_CodeSegmentInfo data;
    u32 bss_size;
};

struct ExHeader_DependencyList {
    u8 program_id[0x30][8];
};

struct ExHeader_SystemInfo {
    u64 save_data_size;
    u64_le jump_id;
    u8 reserved_2[0x30];
};

struct ExHeader_StorageInfo {
    u8 ext_save_data_id[8];
    u8 system_save_data_id[8];
    u8 reserved[8];
    u8 access_info[7];
    u8 other_attributes;
};

struct ExHeader_ARM11_SystemLocalCaps {
    u64_le program_id;
    u32_le core_version;
    u8 reserved_flags[2];
    union {
        u8 flags0;
        BitField<0, 2, u8> ideal_processor;
        BitField<2, 2, u8> affinity_mask;
        BitField<4, 4, u8> system_mode;
    };
    u8 priority;
    u8 resource_limit_descriptor[0x10][2];
    ExHeader_StorageInfo storage_info;
    u8 service_access_control[0x20][8];
    u8 ex_service_access_control[0x2][8];
    u8 reserved[0xf];
    u8 resource_limit_category;
};

struct ExHeader_ARM11_KernelCaps {
    u32_le descriptors[28];
    u8 reserved[0x10];
};

struct ExHeader_ARM9_AccessControl {
    u8 descriptors[15];
    u8 descversion;
};

struct ExHeader_Header {
    ExHeader_CodeSetInfo codeset_info;
    ExHeader_DependencyList dependency_list;
    ExHeader_SystemInfo system_info;
    ExHeader_ARM11_SystemLocalCaps arm11_system_local_caps;
    ExHeader_ARM11_KernelCaps arm11_kernel_caps;
    ExHeader_ARM9_AccessControl arm9_access_control;
    struct {
        u8 signature[0x100];
        u8 ncch_public_key_modulus[0x100];
        ExHeader_ARM11_SystemLocalCaps arm11_system_local_caps;
        ExHeader_ARM11_KernelCaps arm11_kernel_caps;
        ExHeader_ARM9_AccessControl arm9_access_control;
    } access_desc;
};

static_assert(sizeof(ExHeader_Header) == 0x800, "ExHeader structure size is wrong");

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

/**
 * Helper which implements an interface to deal with NCCH containers which can
 * contain ExeFS archives or RomFS archives for games or other applications.
 */
class NCCHContainer {
public:
    NCCHContainer(const std::string& filepath, u32 ncch_offset = 0);
    NCCHContainer() {}

    Loader::ResultStatus OpenFile(const std::string& filepath, u32 ncch_offset = 0);

    /**
     * Ensure ExeFS and exheader is loaded and ready for reading sections
     * @return ResultStatus result of function
     */
    Loader::ResultStatus Load();

    /**
     * Attempt to find overridden sections for the NCCH and mark the container as tainted
     * if any are found.
     * @return ResultStatus result of function
     */
    Loader::ResultStatus LoadOverrides();

    /**
     * Reads an application ExeFS section of an NCCH file (e.g. .code, .logo, etc.)
     * @param name Name of section to read out of NCCH file
     * @param buffer Vector to read data into
     * @return ResultStatus result of function
     */
    Loader::ResultStatus LoadSectionExeFS(const char* name, std::vector<u8>& buffer);

    /**
     * Reads an application ExeFS section from external files instead of an NCCH file,
     * (e.g. code.bin, logo.bcma.lz, icon.icn, banner.bnr)
     * @param name Name of section to read from external files
     * @param buffer Vector to read data into
     * @return ResultStatus result of function
     */
    Loader::ResultStatus LoadOverrideExeFSSection(const char* name, std::vector<u8>& buffer);

    /**
     * Get the RomFS of the NCCH container
     * Since the RomFS can be huge, we return a file reference instead of copying to a buffer
     * @param romfs_file The file containing the RomFS
     * @param offset The offset the romfs begins on
     * @param size The size of the romfs
     * @return ResultStatus result of function
     */
    Loader::ResultStatus ReadRomFS(std::shared_ptr<FileUtil::IOFile>& romfs_file, u64& offset,
                                   u64& size);

    /**
    * Get the override RomFS of the NCCH container
    * Since the RomFS can be huge, we return a file reference instead of copying to a buffer
    * @param romfs_file The file containing the RomFS
    * @param offset The offset the romfs begins on
    * @param size The size of the romfs
    * @return ResultStatus result of function
    */
    Loader::ResultStatus ReadOverrideRomFS(std::shared_ptr<FileUtil::IOFile>& romfs_file,
                                           u64& offset, u64& size);

    /**
     * Get the Program ID of the NCCH container
     * @return ResultStatus result of function
     */
    Loader::ResultStatus ReadProgramId(u64_le& program_id);

    /**
     * Checks whether the NCCH container contains an ExeFS
     * @return bool check result
     */
    bool HasExeFS();

    /**
     * Checks whether the NCCH container contains a RomFS
     * @return bool check result
     */
    bool HasRomFS();

    /**
     * Checks whether the NCCH container contains an ExHeader
     * @return bool check result
     */
    bool HasExHeader();

    NCCH_Header ncch_header;
    ExeFs_Header exefs_header;
    ExHeader_Header exheader_header;

private:
    bool has_header = false;
    bool has_exheader = false;
    bool has_exefs = false;
    bool has_romfs = false;

    bool is_tainted = false; // Are there parts of this container being overridden?
    bool is_loaded = false;
    bool is_compressed = false;

    u32 ncch_offset = 0; // Offset to NCCH header, can be 0 for NCCHs or non-zero for CIAs/NCSDs
    u32 exefs_offset = 0;

    std::string filepath;
    FileUtil::IOFile file;
    FileUtil::IOFile exefs_file;
};

} // namespace FileSys