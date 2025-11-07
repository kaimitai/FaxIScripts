#include <iostream>
#include "./fi/IScriptLoader.h"
#include "./fi/AsmWriter.h"
#include "./fi/AsmReader.h"
#include "./common/klib/Kfile.h"
#include "./fi/fi_constants.h"

static void asm_to_nes(const std::string& p_asm_filename,
	const std::string& p_nes_filename, bool p_use_region_2) {
	fi::AsmReader reader;
	reader.read_asm_file(p_asm_filename, p_use_region_2);
	auto bytes{ reader.get_bytes() };
	auto rom{ klib::file::read_file_as_bytes("c:/temp/faxanadu (u).nes") };

	for (std::size_t i{ 0 }; i < bytes.first.size(); ++i)
		rom.at(i + fi::c::ISCRIPT_ADDR_LO) = bytes.first[i];

	for (std::size_t i{ 0 }; i < bytes.second.size(); ++i)
		rom.at(i + (
			p_use_region_2 ?
			fi::c::ISCRIP_DATA_ROM_OFFSET_REGION_2 : fi::c::ISCRIPT_ADDR_LO + bytes.first.size()
			)) = bytes.second[i];

	klib::file::write_bytes_to_file(rom, p_nes_filename);
}

static void nes_to_asm(const std::string& p_nes_filename,
	const std::string& p_asm_filename) {
	fi::IScriptLoader loader(klib::file::read_file_as_bytes(p_nes_filename));
	loader.parse_rom();
	fi::AsmWriter asmw;
	asmw.generate_asm_file(p_asm_filename,
		loader.m_instructions, loader.ptr_table, loader.m_jump_targets,
		loader.m_strings, loader.m_shops);
}

int main(int argc, char** argv) {
	// nes_to_asm("c:/temp/faxanadu (u).nes", "c:/temp/out.iscript");
	asm_to_nes("c:/temp/out.iscript", "c:/temp/faxscripts.nes", true);
	nes_to_asm("c:/temp/faxscripts.nes", "c:/temp/out-2.iscript");
}
