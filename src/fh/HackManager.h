#ifndef FH_HACKMANAGER_H
#define FH_HACKMANAGER_H

#include "./../fe/Config.h"
#include "TilemapChanges.h"
#include <cstddef>
#include <vector>

using byte = unsigned char;
using word = uint16_t;

namespace fh {

	enum class HackLib {
		SetFlag, ClearFlag, IfFlag, RunScreenHandler, GetXP,
		IfWorld, IfScreen, IfStage, Die
	};

	class HackManager {

		// script action library
		word apply_SetFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr, word flag_decode_helper_addr, word bitmask_table_addr) const;
		word apply_ClearFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr, word flag_decode_helper_addr, word bitmask_table_addr) const;
		word apply_IfFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr, word flag_decode_helper_addr, word bitmask_table_addr) const;
		word apply_RunScreenHandler(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr) const;
		word apply_GetXP(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;
		word apply_IfWorld(std::vector<byte>& p_rom, word cpu_addr, word helper_if_a_equals_addr) const;
		word apply_IfScreen(std::vector<byte>& p_rom, word cpu_addr, word helper_if_a_equals_addr) const;
		word apply_IfStage(std::vector<byte>& p_rom, word cpu_addr, word helper_if_a_equals_addr) const;
		word apply_Die(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;

		// shared helpers for the script action library
		word apply_helper_DecodeScriptFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
			word cpu_addr) const;
		word apply_helper_IfAEquals(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const;

		// other hacks
		word install_hack_clear_flag_memory(const fe::Config& p_config, std::vector<byte>& p_rom) const;
		void install_static_hack_flags_to_sram(const fe::Config& p_config, std::vector<byte>& p_rom) const;

		// tilemap change hacks
		word install_hack_tm_flag_helper(std::vector<byte>& p_rom, byte p_bank, word p_cpu_addr,
			word p_table_addr) const;
		word install_hack_tm_tilemap_changer(std::vector<byte>& p_rom, byte p_bank, word p_cpu_addr) const;
		word install_hack_tm_descriptor_handler(std::vector<byte>& p_rom, byte p_bank, word p_cpu_addr,
			word flag_helper_cpu_addr, word tm_changer_cpu_addr) const;
		word install_hack_tm_lookup(std::vector<byte>& p_rom, byte p_bank, word p_cpu_addr,
			word descriptor_handler_cpu_addr, word data_table_start_cpu_addr) const;

		word install_hack_tm_event_handler(const fe::Config& p_config, std::vector<byte>& p_rom,
			byte tm_lookup_bank, word tm_lookup_cpu_addr) const;

		// util
		word get_next_cpu_addr(word cpu_addr, std::size_t hack_size, word max_addr = 0xc000) const;
		word cfg_word(const fe::Config& p_config, const std::string& p_id) const;
		std::vector<word> read_vanilla_script_opcode_addrs(const std::vector<byte>& p_rom) const;
		std::size_t write_script_opcode_table(std::vector<byte>& p_rom, word cpu_addr,
			const std::vector<word>& p_jump_table) const;

	public:
		HackManager(void) = default;

		std::size_t apply_tilemap_change_subsystem(const fe::Config& p_config, std::vector<byte>& p_rom,
			const fh::TilemapChanges& tm_changes) const;
		std::size_t apply_script_library(const fe::Config& p_config, std::vector<byte>& p_rom,
			std::size_t p_file_offset, const std::vector<HackLib>& p_lib) const;
	};

}

#endif
