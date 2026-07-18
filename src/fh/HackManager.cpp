#include "HackManager.h"
#include "fh_constants.h"
#include "./../common/klib/Asm6502.h"
#include <algorithm>
#include <cassert>
#include <set>
#include <stdexcept>

// script library hacks
word fh::HackManager::apply_SetFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = flag no
	code.sty_zp(RAM::ZP_Temp07);
	code.pha(); // save original flag

	code.lsr_a(3); // A = flag no / 8
	code.tax(); // X = byte index

	code.pla();
	code.and_imm(0x07); // A = bit number
	code.tay();

	code.lda_abs_x(RAM::Flags);
	code.ora_abs_y(bitmask_table_addr);
	code.sta_abs_x(RAM::Flags);

	code.ldy_zp(RAM::ZP_Temp07);
	code.jmp(cfg_word(p_config, c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(cpu_addr, code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

word fh::HackManager::apply_ClearFlag(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = flag no

	code.pha(); // save original flag
	code.lsr_a(3); // A = flag no / 8
	code.tax(); // X = byte index

	code.pla();
	code.and_imm(0x07); // A = bit number
	code.tay();

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
	word cpu_addr, word bitmask_table_addr) const {
	klib::Asm6502 code;

	code.jsr(cfg_word(p_config, c::ID_ROM_ISCRIPTS_LOADBYTE)); // A = flag no

	code.pha(); // save original flag
	code.lsr_a(3); // A = flag no / 8
	code.tax(); // X = byte index

	code.pla();
	code.and_imm(0x07); // A = bit number
	code.tay();

	code.lda_abs_x(RAM::Flags);
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

	code.jmp(cfg_word(
		p_config,
		c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));

	return get_next_cpu_addr(
		cpu_addr,
		code.apply_hack_and_clear(p_rom, 12, cpu_addr));
}

std::size_t fh::HackManager::apply_script_library(const fe::Config& p_config, std::vector<byte>& p_rom,
	std::size_t p_file_offset, const std::vector<HackLib>& p_lib) const {
	const std::set<HackLib> FLAG_REQUIRED{ HackLib::SetFlag, HackLib::ClearFlag, HackLib::IfFlag };
	std::vector<word> script_impl_addresses{ read_vanilla_script_opcode_addrs(p_rom) };

	klib::Asm6502 code;

	const auto rom_addr_start{ klib::Asm6502::get_rom_address(p_file_offset) };
	assert(rom_addr_start.Bank == 12);

	word cpu_addr{ rom_addr_start.CpuAddr };
	word bitmask_table_addr{ cpu_addr };

	for (HackLib llib : FLAG_REQUIRED)
		if (std::find(begin(p_lib), end(p_lib), llib) != end(p_lib)) {
			// install hack which clears flag-memory since vanilla does not
			install_hack_clear_flag_memory(p_config, p_rom);
			// install wram to sram for flag data for en-transl and derivatives
			if (p_config.boolean_or(c::ID_FLAGS_WRAM_TO_SRAM, false))
				install_static_hack_flags_to_sram(p_config, p_rom);
			// install lookup table to extract bits from flag bytes
			const std::vector<byte> BITMASK_TABLE{ 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
			cpu_addr = get_next_cpu_addr(cpu_addr, klib::Asm6502::apply_bytes(p_rom, BITMASK_TABLE, rom_addr_start.Bank, cpu_addr));
			break;
		}

	for (HackLib llib : p_lib) {
		script_impl_addresses.push_back(cpu_addr - 1);

		switch (llib) {

		case HackLib::SetFlag: {
			cpu_addr = apply_SetFlag(p_config, p_rom, cpu_addr, bitmask_table_addr);
			break;
		}

		case HackLib::ClearFlag: {
			cpu_addr = apply_ClearFlag(p_config, p_rom, cpu_addr, bitmask_table_addr);
			break;
		}

		case HackLib::IfFlag: {
			cpu_addr = apply_IfFlag(p_config, p_rom, cpu_addr, bitmask_table_addr);
			break;
		}

		case HackLib::RunScreenHandler: {
			cpu_addr = apply_RunScreenHandler(p_config, p_rom, cpu_addr);
			break;
		}

		default:
			throw std::runtime_error("Unsupported script library routine.");
		}
	}

	// recreate the script jump table and update references
	cpu_addr += static_cast<word>(write_script_opcode_table(p_rom, cpu_addr, script_impl_addresses));

	return code.get_file_offset(rom_addr_start.Bank, cpu_addr);
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

word fh::HackManager::get_next_cpu_addr(word cpu_addr, std::size_t hack_size, word max_addr) const {
	auto next_addr{ cpu_addr + hack_size };
	if (next_addr > max_addr)
		throw std::runtime_error("Hack overflow");
	return static_cast<word>(next_addr);
}

std::vector<word> fh::HackManager::read_vanilla_script_opcode_addrs(const std::vector<byte>& p_rom) const {
	std::vector<word> result;

	word ptrs_hi{ klib::Asm6502::read_word(p_rom, 12, ROM::IScripts_JumpTable_Ref_U) };
	word ptrs_lo{ klib::Asm6502::read_word(p_rom, 12, ROM::IScripts_JumpTable_Ref_L) };

	std::size_t offset_hi{ klib::Asm6502::get_file_offset(12, ptrs_hi) };
	std::size_t offset_lo{ klib::Asm6502::get_file_offset(12, ptrs_lo) };

	for (std::size_t i{ 0 }; i < c::VANILLA_SCRIPT_COUNT; ++i)
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
