#include "HackManager.h"
#include "fh_constants.h"
#include "./../common/klib/Asm6502.h"
#include <algorithm>
#include <cassert>
#include <set>
#include <stdexcept>

// shared helpers for script library hacks

// reads the next script operand as a flag number, stores the byte number (relative to start of flags block)
// in X, and the bit number (0-7) in that byte in Y
word fh::HackManager::apply_helper_DecodeScriptFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = flag no

	code.pha(); // save original flag
	code.lsr_a(3); // A = flag no / 8
	code.tax(); // X = byte index

	code.pla();
	code.and_imm(0x07); // A = bit number
	code.tay();

	code.rts();

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// reads the next script operand as a quest flag number (0-7)
// and stores the corresponding bit number in Y
word fh::HackManager::apply_helper_DecodeQuestFlag(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = quest flag no
	code.and_imm(0x07); // A = bit number
	code.tay();

	code.rts();

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// if A equals the operand, jump - otherwise continue script execution
word fh::HackManager::apply_helper_IfAEquals(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr) const {
	klib::Asm6502 code;

	code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = operand (comparison value)
	code.sta_zp(RAM::ZP_Temp07);
	code.pla();
	code.cmp_zp(RAM::ZP_Temp07);
	code.beq("@equal");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	code.label("@equal");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// Converts the player's current pixel position to a block position.
// Returns: X = (Y_block << 4) | X_block
word fh::HackManager::apply_helper_GetPlayerBlockPos(std::vector<byte>& p_rom,
	word cpu_addr) const {
	klib::Asm6502 code;

	code.lda_zp(RAM::ZP_PlayerPosX);
	code.clc();
	code.adc_imm(0x07);                 // normalize to player center
	code.sta_zp(RAM::ZP_PlayerPosArgX);

	code.lda_zp(RAM::ZP_PlayerPosY);
	code.sta_zp(RAM::ZP_PlayerPosArgY);

	code.jsr(ROM::Area_ConvertPixelsToBlockPos);

	code.rts();

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// script library hacks
word fh::HackManager::apply_SetFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, word flag_decode_helper_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	code.jsr(flag_decode_helper_addr);

	code.lda_abs_x(RAM::Flags);
	code.ora_abs_y(bitmask_table_addr);
	code.sta_abs_x(RAM::Flags);

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_ClearFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, word flag_decode_helper_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	code.jsr(flag_decode_helper_addr);

	code.lda_abs_x(RAM::Flags);
	code.sta_zp(RAM::ZP_Temp07);

	code.lda_abs_y(bitmask_table_addr);
	code.eor_imm(0xff); // invert mask

	code.and_zp(RAM::ZP_Temp07);
	code.sta_abs_x(RAM::Flags);

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_IfFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, word flag_decode_helper_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	code.jsr(flag_decode_helper_addr);

	code.lda_abs_x(RAM::Flags);
	code.and_abs_y(bitmask_table_addr);
	code.bne("@set");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	code.label("@set");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_SelectFlag(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	const word SelectedFlagRamAddr{ cfg_word(p_config, c::ID_HACK_SCRIPT_SELECTED_FLAG_RAM_ADDR) };

	// A = flag number
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE));
	// remember the selected flag number
	code.sta_mem(SelectedFlagRamAddr);
	// continue executing the script
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_SetSelectedFlag(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	const word SelectedFlagRamAddr{ cfg_word(p_config, c::ID_HACK_SCRIPT_SELECTED_FLAG_RAM_ADDR) };

	// A = selected flag number
	code.lda_mem(SelectedFlagRamAddr);

	// if no flag has been selected, do nothing
	code.cmp_imm(0xff);
	code.beq("@done");

	// decode flag number -> X = byte index, Y = bit index
	code.pha();
	code.lsr_a(3);
	code.tax();

	code.pla();
	code.and_imm(0x07);
	code.tay();

	// set the bit
	code.lda_abs_x(RAM::Flags);
	code.ora_abs_y(bitmask_table_addr);
	code.sta_abs_x(RAM::Flags);

	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_ClearSelectedFlag(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	const word SelectedFlagRamAddr{ cfg_word(p_config, c::ID_HACK_SCRIPT_SELECTED_FLAG_RAM_ADDR) };

	// A = selected flag number
	code.lda_mem(SelectedFlagRamAddr);

	// if no flag has been selected, do nothing
	code.cmp_imm(0xff);
	code.beq("@done");

	// decode flag number -> X = byte index, Y = bit index
	code.pha();
	code.lsr_a(3);
	code.tax();

	code.pla();
	code.and_imm(0x07);
	code.tay();

	// clear the bit
	code.lda_abs_x(RAM::Flags);
	code.sta_zp(RAM::ZP_Temp07);

	code.lda_abs_y(bitmask_table_addr);
	code.eor_imm(0xff);

	code.and_zp(RAM::ZP_Temp07);
	code.sta_abs_x(RAM::Flags);

	code.label("@done");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_IfSelectedFlag(const fe::Config& p_config,
	std::vector<byte>& p_rom, word cpu_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	const word SelectedFlagRamAddr{ cfg_word(p_config, c::ID_HACK_SCRIPT_SELECTED_FLAG_RAM_ADDR) };

	// A = selected flag number
	code.lda_mem(SelectedFlagRamAddr);

	// if no flag has been selected, treat it as clear
	code.cmp_imm(0xff);
	code.beq("@clear");

	// decode flag number -> X = byte index, Y = bit index
	code.pha();
	code.lsr_a(3);
	code.tax();

	code.pla();
	code.and_imm(0x07);
	code.tay();

	// test the bit
	code.lda_abs_x(RAM::Flags);
	code.and_abs_y(bitmask_table_addr);
	code.bne("@set");

	code.label("@clear");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	code.label("@set");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_SetQuestFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, word quest_flag_decode_helper_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	code.jsr(quest_flag_decode_helper_addr);

	code.lda_abs(RAM::QuestFlags);
	code.ora_abs_y(bitmask_table_addr);
	code.sta_abs(RAM::QuestFlags);

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_ClearQuestFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, word quest_flag_decode_helper_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	code.jsr(quest_flag_decode_helper_addr);

	code.lda_abs(RAM::QuestFlags);
	code.sta_zp(RAM::ZP_Temp07);

	code.lda_abs_y(bitmask_table_addr);
	code.eor_imm(0xff); // invert mask

	code.and_zp(RAM::ZP_Temp07);
	code.sta_abs(RAM::QuestFlags);

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_IfQuestFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, word quest_flag_decode_helper_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	code.jsr(quest_flag_decode_helper_addr);

	code.lda_abs(RAM::QuestFlags);
	code.and_abs_y(bitmask_table_addr);
	code.bne("@set");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE));

	code.label("@set");
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_JUMPTONEXTADDR));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// runs screen handler 3 - data-driven tilemap changes
// we can certainly assume that RAM::CurrentScreen_SpecialEventID is 0xff when this is invoked
// so we are not storing and restoring it
word fh::HackManager::apply_RunScreenHandler(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr) const {
	klib::Asm6502 code;

	code.lda_imm(0x03);
	code.sta_abs(RAM::CurrentScreen_SpecialEventID);

	code.jsr(ROM::GameLoop_RunScreenEventHandlers);

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_GetXP(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = xp lo byte
	code.sta_zp(RAM::ZP_Temp_Int24_L);
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = xp hi byte
	code.sta_zp(RAM::ZP_Temp_Int24_M);
	code.jsr(cfg_word(p_config, c::ID_ROM_PLAYER_UPDATEEXPERIENCE));

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_IfWorld(std::vector<byte>& p_rom, word cpu_addr, word helper_if_a_equals_addr) const {
	klib::Asm6502 code;

	code.lda_zp(RAM::ZP_CurrentWorld);
	code.jmp(helper_if_a_equals_addr);

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_IfScreen(std::vector<byte>& p_rom, word cpu_addr, word helper_if_a_equals_addr) const {
	klib::Asm6502 code;

	code.lda_zp(RAM::ZP_CurrentScreen);
	code.jmp(helper_if_a_equals_addr);

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_IfStage(std::vector<byte>& p_rom, word cpu_addr, word helper_if_a_equals_addr) const {
	klib::Asm6502 code;

	code.lda_abs(RAM::CurrentStage);
	code.jmp(helper_if_a_equals_addr);

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_Die(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	code.lda_imm(0x01);
	code.sta_abs(RAM::PlayerIsDead);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_JSR(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	const word ScriptReturn_Lo_ram_addr{ cfg_word(p_config, c::ID_HACK_SCRIPT_JSR_RAM_ADDR_LO) };
	const word ScriptReturn_Hi_ram_addr{ cfg_word(p_config, c::ID_HACK_SCRIPT_JSR_RAM_ADDR_HI) };

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = target lo
	code.pha();
	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = target hi
	code.pha();

	// compute return address = base + offset
	code.clc();
	code.lda_zp(RAM::ZP_ScriptAddr);
	code.adc_zp(RAM::ZP_ScriptOffset);
	code.sta_mem(ScriptReturn_Lo_ram_addr);

	code.lda_zp(RAM::ZP_ScriptAddrU);
	code.adc_imm(0x00);
	code.sta_mem(ScriptReturn_Hi_ram_addr);

	// restore target
	code.pla();
	code.sta_zp(RAM::ZP_ScriptAddrU);
	code.pla();
	code.sta_zp(RAM::ZP_ScriptAddr);
	code.lda_imm(0x00);
	code.sta_zp(RAM::ZP_ScriptOffset);

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_Return(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	const word ScriptReturn_Lo_ram_addr{ cfg_word(p_config, c::ID_HACK_SCRIPT_JSR_RAM_ADDR_LO) };
	const word ScriptReturn_Hi_ram_addr{ cfg_word(p_config, c::ID_HACK_SCRIPT_JSR_RAM_ADDR_HI) };

	// retrieve lo addr
	code.lda_mem(ScriptReturn_Lo_ram_addr);
	code.sta_zp(RAM::ZP_ScriptAddr);

	// retrieve hi addr
	code.lda_mem(ScriptReturn_Hi_ram_addr);
	code.sta_zp(RAM::ZP_ScriptAddrU);

	// set offset to 0 - as the JSR normalized the target address
	code.lda_imm(0x00);
	code.sta_zp(RAM::ZP_ScriptOffset);

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_ForceDoor(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr) const {
	klib::Asm6502 code;

	// the requirement check has already failed and invoked this script
	// override that result so the caller proceeds through the door afterall
	code.lda_imm(0x00);
	code.sta_abs(RAM::DoorKeyRequirement);

	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_IfYX(std::vector<byte>& p_rom, word cpu_addr,
	word helper_get_player_block_pos_addr, word helper_if_a_equals_addr) const {
	klib::Asm6502 code;

	code.jsr(helper_get_player_block_pos_addr); // X = packed YX
	code.txa();                                 // A = packed YX
	code.jmp(helper_if_a_equals_addr);          // compare with script operand

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_IfDoorYX(std::vector<byte>& p_rom, word cpu_addr,
	word helper_if_a_equals_addr) const {
	klib::Asm6502 code;

	code.lda_zp(RAM::ZP_DoorBlockPos);
	code.jmp(helper_if_a_equals_addr);

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

// main orchestrator - injects the script routines specified by users through the configuration xml
// and extends the scripting language itself
std::size_t fh::HackManager::apply_script_library(const fe::Config& p_config, std::vector<byte>& p_rom,
	std::size_t p_file_offset, const std::vector<HackLib>& p_lib, std::size_t p_base_opcode_count) const {

	const std::set<HackLib> FLAG_REQUIRED{ HackLib::SetFlag, HackLib::ClearFlag, HackLib::IfFlag,
	HackLib::SelectFlag, HackLib::SetSelectedFlag, HackLib::ClearSelectedFlag, HackLib::IfSelectedFlag };
	const std::set<HackLib> QUEST_FLAG_REQUIRED{ HackLib::SetQuestFlag, HackLib::ClearQuestFlag, HackLib::IfQuestFlag };
	const std::set<HackLib> RAM_CHECK_REQUIRED{ HackLib::IfWorld, HackLib::IfScreen, HackLib::IfStage, HackLib::IfYX, HackLib::IfDoorYX };
	const std::set<HackLib> BLOCK_POS_REQUIRED{ HackLib::IfYX };
	// flag functions need access to the bitmask lookup table
	std::set<HackLib> BITMASK_TABLE_REQUIRED{ FLAG_REQUIRED };
	BITMASK_TABLE_REQUIRED.insert(begin(QUEST_FLAG_REQUIRED), end(QUEST_FLAG_REQUIRED));

	std::vector<word> script_impl_addresses{ read_script_opcode_addrs(p_rom, p_base_opcode_count) };

	const auto rom_addr_start{ klib::Asm6502::get_rom_address(p_file_offset) };
	assert(rom_addr_start.Bank == 12);

	word cpu_addr{ rom_addr_start.CpuAddr };
	// cpu addresses of optional helpers
	word bitmask_table_addr{ 0 };
	word flag_decode_helper_addr{ 0 };
	word quest_flag_decode_helper_addr{ 0 };
	word ram_check_helper_addr{ 0 };
	word block_pos_helper_addr{ 0 };

	// check if the bitmask lookup table needs to be installed
	if (requires_any(p_lib, BITMASK_TABLE_REQUIRED)) {
		const std::vector<byte> BITMASK_TABLE{ 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
		bitmask_table_addr = cpu_addr;
		cpu_addr = get_next_cpu_addr(bitmask_table_addr, klib::Asm6502::apply_bytes(p_rom, BITMASK_TABLE, 12, bitmask_table_addr));
	}

	// check if the flag-check helper must be installed
	if (requires_any(p_lib, FLAG_REQUIRED)) {
		// install hack which clears flag-memory since vanilla does not
		install_hack_clear_flag_memory(p_config, p_rom);
		// install wram to sram for flag data for en-transl and derivatives
		if (p_config.boolean_or(c::ID_FLAGS_WRAM_TO_SRAM, false))
			install_static_hack_flags_to_sram(p_config, p_rom);
		// install the persistent flag decoder helper
		flag_decode_helper_addr = cpu_addr;
		cpu_addr = apply_helper_DecodeScriptFlag(p_config, p_rom, flag_decode_helper_addr);
	}

	// check if the quest flag-check helper must be installed
	if (requires_any(p_lib, QUEST_FLAG_REQUIRED)) {
		quest_flag_decode_helper_addr = cpu_addr;
		cpu_addr = apply_helper_DecodeQuestFlag(p_config, p_rom, cpu_addr);
	}

	// check if the ram checker helper must be installed
	if (requires_any(p_lib, RAM_CHECK_REQUIRED)) {
		ram_check_helper_addr = cpu_addr;
		cpu_addr = apply_helper_IfAEquals(p_config, p_rom, cpu_addr);
	}

	// check if the block normalizer helper helper must be installed
	if (requires_any(p_lib, BLOCK_POS_REQUIRED)) {
		block_pos_helper_addr = cpu_addr;
		cpu_addr = apply_helper_GetPlayerBlockPos(p_rom, cpu_addr);
	}

	for (HackLib llib : p_lib) {
		script_impl_addresses.push_back(cpu_addr - 1);

		switch (llib) {

		case HackLib::SetFlag: {
			cpu_addr = apply_SetFlag(p_config, p_rom, cpu_addr, flag_decode_helper_addr, bitmask_table_addr);
			break;
		}
		case HackLib::ClearFlag: {
			cpu_addr = apply_ClearFlag(p_config, p_rom, cpu_addr, flag_decode_helper_addr, bitmask_table_addr);
			break;
		}
		case HackLib::IfFlag: {
			cpu_addr = apply_IfFlag(p_config, p_rom, cpu_addr, flag_decode_helper_addr, bitmask_table_addr);
			break;
		}
		case HackLib::SelectFlag: {
			cpu_addr = apply_SelectFlag(p_config, p_rom, cpu_addr);
			break;
		}
		case HackLib::SetSelectedFlag: {
			cpu_addr = apply_SetSelectedFlag(p_config, p_rom, cpu_addr, bitmask_table_addr);
			break;
		}
		case HackLib::ClearSelectedFlag: {
			cpu_addr = apply_ClearSelectedFlag(p_config, p_rom, cpu_addr, bitmask_table_addr);
			break;
		}
		case HackLib::IfSelectedFlag: {
			cpu_addr = apply_IfSelectedFlag(p_config, p_rom, cpu_addr, bitmask_table_addr);
			break;
		}
		case HackLib::SetQuestFlag: {
			cpu_addr = apply_SetQuestFlag(p_config, p_rom, cpu_addr, quest_flag_decode_helper_addr, bitmask_table_addr);
			break;
		}
		case HackLib::ClearQuestFlag: {
			cpu_addr = apply_ClearQuestFlag(p_config, p_rom, cpu_addr, quest_flag_decode_helper_addr, bitmask_table_addr);
			break;
		}
		case HackLib::IfQuestFlag: {
			cpu_addr = apply_IfQuestFlag(p_config, p_rom, cpu_addr, quest_flag_decode_helper_addr, bitmask_table_addr);
			break;
		}
		case HackLib::RunScreenHandler: {
			cpu_addr = apply_RunScreenHandler(p_config, p_rom, cpu_addr);
			break;
		}
		case HackLib::GetXP: {
			cpu_addr = apply_GetXP(p_config, p_rom, cpu_addr);
			break;
		}
		case HackLib::IfWorld: {
			cpu_addr = apply_IfWorld(p_rom, cpu_addr, ram_check_helper_addr);
			break;
		}
		case HackLib::IfScreen: {
			cpu_addr = apply_IfScreen(p_rom, cpu_addr, ram_check_helper_addr);
			break;
		}
		case HackLib::IfStage: {
			cpu_addr = apply_IfStage(p_rom, cpu_addr, ram_check_helper_addr);
			break;
		}
		case HackLib::Die: {
			cpu_addr = apply_Die(p_config, p_rom, cpu_addr);
			break;
		}
		case HackLib::JSR: {
			cpu_addr = apply_JSR(p_config, p_rom, cpu_addr);
			break;
		}
		case HackLib::Return: {
			cpu_addr = apply_Return(p_config, p_rom, cpu_addr);
			break;
		}
		case HackLib::ForceDoor: {
			cpu_addr = apply_ForceDoor(p_config, p_rom, cpu_addr);
			break;
		}
		case HackLib::IfYX: {
			cpu_addr = apply_IfYX(p_rom, cpu_addr, block_pos_helper_addr, ram_check_helper_addr);
			break;
		}
		case HackLib::IfDoorYX: {
			cpu_addr = apply_IfDoorYX(p_rom, cpu_addr, ram_check_helper_addr);
			break;
		}

		default:
			throw std::runtime_error("Unsupported script library routine.");
		}
	}

	// recreate the script jump table and update references
	cpu_addr += static_cast<word>(write_script_opcode_table(p_rom, cpu_addr, script_impl_addresses));

	return klib::Asm6502::get_file_offset(rom_addr_start.Bank, cpu_addr);
}

// given the extended library, check if any helpers are required
bool fh::HackManager::requires_any(const std::vector<HackLib>& p_lib, const std::set<HackLib>& p_required) const {
	for (HackLib llib : p_required)
		if (std::find(begin(p_lib), end(p_lib), llib) != end(p_lib))
			return true;

	return false;
}

// tilemap change subsystem
std::size_t fh::HackManager::apply_tilemap_change_subsystem(const fe::Config& p_config, std::vector<byte>& p_rom,
	const fh::TilemapChanges& tm_changes) const {

	static const std::vector<byte> BITMASK_TABLE_DATA{ 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
	const word cpu_addr{ cfg_word(p_config, c::ID_TM_CHANGE_CPU_ADDR) };
	const byte data_bank{ static_cast<byte>(p_config.constant(c::ID_TM_CHANGE_BANK)) };

	const word data_table_addr{ cpu_addr };
	const word bitmask_table_addr{ get_next_cpu_addr(data_table_addr,
		klib::Asm6502::apply_bytes(p_rom, tm_changes.to_bytes(data_table_addr), data_bank, data_table_addr)) };
	const word flag_helper_addr{ get_next_cpu_addr(bitmask_table_addr,
		klib::Asm6502::apply_bytes(p_rom, BITMASK_TABLE_DATA, data_bank, bitmask_table_addr)) };
	const word tilemap_changer_addr{ install_hack_tm_flag_helper(p_rom, data_bank, flag_helper_addr, bitmask_table_addr) };
	const word descriptor_handler_addr{ install_hack_tm_tilemap_changer(p_config, p_rom, data_bank, tilemap_changer_addr) };
	const word tm_lookup_addr{ install_hack_tm_descriptor_handler(p_rom, data_bank, descriptor_handler_addr,
		flag_helper_addr, tilemap_changer_addr) };
	const word tm_subsystem_end{ install_hack_tm_lookup(p_rom, data_bank, tm_lookup_addr, descriptor_handler_addr,
			static_cast<word>(data_table_addr + fh::TilemapChanges::EOE_TILEMAP_CHANGE_HEADER.size())) };

	// install screen event handler
	const word tm_event_handler_end{ install_hack_tm_event_handler(p_config, p_rom,
		data_bank, tm_lookup_addr) };

	return static_cast<std::size_t>(tm_subsystem_end - cpu_addr);
}

word fh::HackManager::install_hack_tm_event_handler(const fe::Config& p_config, std::vector<byte>& p_rom,
	byte tm_lookup_bank, word tm_lookup_cpu_addr) const {
	// read the jump table of the three canonical event handlers
	std::vector<word> event_handlers{ read_screen_event_handler_addrs(p_rom) };

	klib::Asm6502 code;

	const word Hack_ScreenEventHandler{ cfg_word(p_config, c::ID_TM_CHANGE_HANDLER_CPU_ADDR) };
	const word Hack_ScreenEventHandlerTable{ cfg_word(p_config, c::ID_TM_CHANGE_HANDLER_TABLE_CPU_ADDR) };
	const word MMC1_UpdateROMBank{ cfg_word(p_config, c::ID_ROM_MMC1_UPDATEROMBANK) };

	// Save the currently mapped switchable bank.
	code.lda_abs(RAM::CurrentROMBank);
	code.pha();
	// switch to the tilemap changes data bank
	code.ldx_imm(tm_lookup_bank);
	code.jsr(MMC1_UpdateROMBank);
	// call the actual tilemap change logic in the other bank
	code.jsr(tm_lookup_cpu_addr);
	// restore the bank that was mapped before this handler ran.
	code.pla();
	code.tax();
	code.jsr(MMC1_UpdateROMBank);

	code.label("@done");
	// Mark the screen event as complete.
	code.lda_imm(0xff);
	code.sta_abs(RAM::CurrentScreen_SpecialEventID);
	code.rts();

	const word result{
		get_next_cpu_addr(Hack_ScreenEventHandler, code.apply_hack_and_clear(p_rom, 15, Hack_ScreenEventHandler),
			0x10000) };

	// remake the event handler table by copying the three original addresses
	for (word handler : event_handlers)
		code.dw(handler);
	// The dispatcher enters handlers using RTS, so entries are address - 1.
	code.dw(Hack_ScreenEventHandler - 1);
	code.apply_hack_and_clear(p_rom, 15, Hack_ScreenEventHandlerTable);

	// static change: extend the valid event-table byte count from 6 to 8
	code.cmp_imm(0x08);
	code.apply_hack_and_clear(p_rom, 15, ROM::GameLoop_RunScreenEventHandlers_CMP_06);
	// static change: dispatcher expects the high byte first, then the low byte.
	code.lda_abs_y(Hack_ScreenEventHandlerTable + 1);
	code.pha();
	code.lda_abs_y(Hack_ScreenEventHandlerTable);
	code.pha();
	code.apply_hack_and_clear(p_rom, 15, ROM::GameLoop_RunScreenEventHandlers_LDA_EventTable);

	return result;
}

word fh::HackManager::install_hack_tm_lookup(std::vector<byte>& p_rom, byte p_bank, word p_cpu_addr,
	word descriptor_handler_cpu_addr, word data_table_start_cpu_addr) const {
	klib::Asm6502 code;

	code.lda_zp(RAM::ZP_CurrentWorld);
	code.asl_a();
	code.tax();
	code.lda_abs_x(data_table_start_cpu_addr);
	code.sta_zp(RAM::ZP_e4);
	code.lda_abs_x(data_table_start_cpu_addr + 1);
	code.sta_zp(RAM::ZP_e5);
	code.jmp(descriptor_handler_cpu_addr);
	return get_next_cpu_addr(
		p_cpu_addr,
		code.apply_hack_and_clear(p_rom, p_bank, p_cpu_addr));
}

word fh::HackManager::install_hack_tm_descriptor_handler(std::vector<byte>& p_rom, byte p_bank, word p_cpu_addr,
	word flag_helper_cpu_addr, word tm_changer_cpu_addr) const {
	klib::Asm6502 code;

	code.ldy_imm(0x00);
	code.label("@loop");
	code.lda_ind_y(RAM::ZP_e4);
	code.cmp_imm(0xff);
	code.beq("@done");

	code.cmp_zp(RAM::ZP_CurrentScreen);
	code.beq("@found_screen");

	code.iny();

	code.label("@flag_clear");
	code.iny();
	code.iny();
	code.iny();

	code.bne("@loop");

	code.label("@found_screen");
	code.iny(); // y -> flag
	code.lda_ind_y(RAM::ZP_e4);
	code.jsr(flag_helper_cpu_addr);
	code.bcc("@flag_clear");

	code.iny(); // y -> ptr lo

	code.lda_ind_y(RAM::ZP_e4);
	code.sta_zp(RAM::ZP_e2);

	code.iny(); // y -> ptr hi

	code.lda_ind_y(RAM::ZP_e4);
	code.sta_zp(RAM::ZP_e3);

	code.jmp(tm_changer_cpu_addr);

	code.label("@done");
	code.lda_imm(0x00);
	code.rts();

	return get_next_cpu_addr(
		p_cpu_addr,
		code.apply_hack_and_clear(p_rom, p_bank, p_cpu_addr));
}

word fh::HackManager::install_hack_tm_tilemap_changer(const fe::Config& p_config, std::vector<byte>& p_rom, byte p_bank, word p_cpu_addr) const {
	const byte waitframes{ cfg_byte(p_config, c::ID_TM_CHANGE_HANDLER_WAIT_FRAMES) };
	const byte sound_effect{ cfg_byte(p_config, c::ID_TM_CHANGE_HANDLER_SOUND_EFFECT) };

	klib::Asm6502 code;

	code.ldy_imm(0x00);
	code.lda_ind_y(RAM::ZP_e2);
	code.sta_zp(RAM::ZP_Temp08);

	code.ldy_imm(0x01);

	code.label("@loop");

	// wait for a given number of interrupts
	for (std::size_t i{ 0 }; i < waitframes; ++i)
		code.jsr(ROM::WaitForInterrupt);

	// optionally play a sound effect
	if (sound_effect != 0xff) {
		code.lda_imm(sound_effect);
		code.jsr(ROM::Sound_PlayEffect);
	}

	code.lda_ind_y(RAM::ZP_e2);
	code.tax();
	code.iny();
	code.lda_ind_y(RAM::ZP_e2);
	code.iny();

	code.sta_abs_x(RAM::ScreenBuffer);

	code.sty_zp(RAM::ZP_Temp07);
	code.jsr(ROM::Area_SetBlocks);
	code.ldy_zp(RAM::ZP_Temp07);

	code.dec_zp(RAM::ZP_Temp08);
	code.bne("@loop");

	code.rts();

	return get_next_cpu_addr(
		p_cpu_addr,
		code.apply_hack_and_clear(p_rom, p_bank, p_cpu_addr));
}

word fh::HackManager::install_hack_tm_flag_helper(std::vector<byte>& p_rom, byte p_bank, word p_cpu_addr,
	word p_table_addr) const {
	klib::Asm6502 code;

	code.pha();
	code.lsr_a(3);
	code.tax();
	code.lda_abs_x(RAM::Flags);
	code.sta_zp(RAM::ZP_Temp08);
	code.pla();
	code.and_imm(0x07);
	code.tax();
	code.lda_abs_x(p_table_addr);
	code.and_zp(RAM::ZP_Temp08);
	code.beq("@clear");
	code.sec();
	code.rts();

	code.label("@clear");
	code.clc();
	code.rts();

	return get_next_cpu_addr(
		p_cpu_addr,
		code.apply_hack_and_clear(p_rom, p_bank, p_cpu_addr));
}

// other hacks
word fh::HackManager::install_hack_clear_flag_memory(const fe::Config& p_config, std::vector<byte>& p_rom) const {
	const word Hack_ClearPersistentFlags{ cfg_word(p_config, c::ID_HACK_CLEAR_PERSISTENT_FLAGS) };

	klib::Asm6502 code;

	// Helper at $D005.
	code.ldx_imm(c::FlagsByteCount);      // #$1F

	code.label("@loop");
	code.sta_abs_x(RAM::Flags - 1);     // STA $0100,X
	code.dex();
	code.bne("@loop");

	code.jsr(ROM::Game_InitMMCAndBank);
	code.jmp(ROM::Game_Init_JSR_Game_InitScreenAndMusic);
	word result{ static_cast<word>(Hack_ClearPersistentFlags + code.apply_hack_and_clear(p_rom, 15, Hack_ClearPersistentFlags)) };

	// Replace JSR Game_InitMMCAndBank
	code.jmp(Hack_ClearPersistentFlags);
	code.apply_hack_and_clear(p_rom, 15, ROM::Game_Init_JSR_Game_InitMMCAndBank);

	return result;
}

// this code ensures the flag RAM is stored and restored via SRAM for the translation hack 'en-transl' and derivatives
void fh::HackManager::install_static_hack_flags_to_sram(const fe::Config& p_config, std::vector<byte>& p_rom) const {
	const word HackStaticExtraSave{ 0x90c0 };
	const word HackStaticExtraLoad{ 0x90dd };

	const word SRAM_Save_Hook = 0x9cdf;
	const word SRAM_Load_Hook = 0x9121;
	const word SRAM_Save_Continue = 0x9ce3;

	const word SRAM_FlagsBlock_Start{ 0x6171 };

	klib::Asm6502 code;

	// *** extra save routine ***
	code.ldx_imm(c::FlagsByteCount);
	code.label("@next");
	// save $0101-$011f -> $6171-$618f
	code.lda_abs_x(RAM::Flags - 1);
	code.sta_abs_x(SRAM_FlagsBlock_Start - 1);
	code.dex();
	code.bne("@next");

	code.lda_imm(0x14);
	code.sta_zp(0xe9);
	code.jmp(SRAM_Save_Continue);

	code.apply_hack_and_clear(p_rom, 12, HackStaticExtraSave);

	// *** extra load routine ***
	// If the original loop wasn't finished, continue it.
	code.bne("@continue");

	code.ldx_imm(c::FlagsByteCount);
	// load $6171-$618f -> $0101-$011f
	code.label("@next");
	code.lda_abs_x(SRAM_FlagsBlock_Start - 1);
	code.sta_abs_x(RAM::Flags - 1);
	code.dex();
	code.bne("@next");

	code.rts();
	code.label("@continue");
	code.jmp(0x910d);

	code.apply_hack_and_clear(p_rom, 12, HackStaticExtraLoad);

	// install save hook
	code.jmp(HackStaticExtraSave);
	code.apply_hack_and_clear(p_rom, 12, SRAM_Save_Hook);

	// install load hook
	code.jmp(HackStaticExtraLoad);
	code.apply_hack_and_clear(p_rom, 12, SRAM_Load_Hook);
}

word fh::HackManager::cfg_word(const fe::Config& p_config, const std::string& p_id) const {
	return static_cast<word>(p_config.constant(p_id));
}

byte fh::HackManager::cfg_byte(const fe::Config& p_config, const std::string& p_id) const {
	return static_cast<byte>(p_config.constant(p_id));
}

word fh::HackManager::get_next_cpu_addr(word cpu_addr, std::size_t hack_size, std::size_t max_addr) const {
	auto next_addr{ cpu_addr + hack_size };
	if (next_addr > max_addr)
		throw std::runtime_error("Hack overflow");
	return static_cast<word>(next_addr);
}

std::vector<word> fh::HackManager::read_script_opcode_addrs(const std::vector<byte>& p_rom, std::size_t p_opcode_count) const {
	std::vector<word> result;

	word ptrs_hi{ klib::Asm6502::read_word(p_rom, 12, ROM::IScripts_JumpTable_Ref_U) };
	word ptrs_lo{ klib::Asm6502::read_word(p_rom, 12, ROM::IScripts_JumpTable_Ref_L) };

	std::size_t offset_hi{ klib::Asm6502::get_file_offset(12, ptrs_hi) };
	std::size_t offset_lo{ klib::Asm6502::get_file_offset(12, ptrs_lo) };

	for (std::size_t i{ 0 }; i < p_opcode_count; ++i)
		result.push_back(static_cast<word>(p_rom.at(offset_lo + i) | p_rom.at(offset_hi + i) << 8));

	return result;
}

std::size_t fh::HackManager::write_script_opcode_table(std::vector<byte>& p_rom, word table_cpu_addr,
	const std::vector<word>& p_jump_table) const {

	word ref_lo{ table_cpu_addr };
	word ref_hi{ static_cast<word>(table_cpu_addr + p_jump_table.size()) };

	klib::Asm6502::apply_word(p_rom, ref_hi, 12, ROM::IScripts_JumpTable_Ref_U);
	klib::Asm6502::apply_word(p_rom, ref_lo, 12, ROM::IScripts_JumpTable_Ref_L);

	return klib::Asm6502::apply_words_as_split_table(p_rom, p_jump_table, 12, table_cpu_addr);
}

std::vector<word> fh::HackManager::read_screen_event_handler_addrs(const std::vector<byte>& p_rom) const {
	const word dispatcher_addr{ ROM::GameLoop_RunScreenEventHandlers_LDA_EventTable };

	// TODO: sanity check that the two LDA operands differ by exactly one
	const word table_ref{ klib::Asm6502::read_word(p_rom, 15, static_cast<word>(dispatcher_addr + 5)) };

	std::vector<word> result;

	for (std::size_t i{ 0 }; i < 3; ++i)
		result.push_back(klib::Asm6502::read_word(p_rom, 15, static_cast<word>(table_ref + 2 * i)));

	return result;
}
