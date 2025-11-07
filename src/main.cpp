#include <format>
#include <iostream>
#include "./fi/IScriptLoader.h"
#include "./fi/AsmWriter.h"
#include "./fi/AsmReader.h"
#include "./common/klib/Kfile.h"
#include "./fi/fi_constants.h"

static void try_patch_msg(const std::string& p_data_type,
	std::size_t p_data_size, std::size_t p_data_max_size) {
	std::cout << std::format("\nTrying to patch {}: Using {} of {} available bytes ({:.2f}%)\n",
		p_data_type, p_data_size, p_data_max_size,
		100.0f * static_cast<float>(p_data_size) / static_cast<float>(p_data_max_size));
	if (p_data_size > p_data_max_size)
		throw std::runtime_error(std::format("Size limits exceeded for {}",
			p_data_type));
}

static void asm_to_nes(const std::string& p_asm_filename,
	const std::string& p_nes_filename, bool p_use_region_2) {
	fi::AsmReader reader;
	reader.read_asm_file(p_asm_filename, p_use_region_2);
	auto bytes{ reader.get_bytes() };
	auto strbytes{ reader.get_string_bytes() };

	try_patch_msg("strings", strbytes.size(), fi::c::SIZE_STRINGS);

	auto rom{ klib::file::read_file_as_bytes("c:/temp/faxanadu (u).nes") };

	for (std::size_t i{ 0 }; i < bytes.first.size(); ++i)
		rom.at(i + fi::c::ISCRIPT_ADDR_LO) = bytes.first[i];

	for (std::size_t i{ 0 }; i < bytes.second.size(); ++i)
		rom.at(i + (
			p_use_region_2 ?
			fi::c::ISCRIP_DATA_ROM_OFFSET_REGION_2 : fi::c::ISCRIPT_ADDR_LO + bytes.first.size()
			)) = bytes.second[i];

	for (std::size_t i{ 0 }; i < strbytes.size(); ++i)
		rom.at(i + fi::c::OFFSET_STRINGS) = strbytes[i];

	// simplest form of static analysis - see if we can reparse the output
	try {
		fi::IScriptLoader staticanalysisread(rom);
	}
	catch (const std::runtime_error& ex) {
		std::cerr << "Invalid ROM generated. Ensure all code paths end, and that each entrypoint has a textbox context\n" << ex.what();
		return;
	}

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

int main(int argc, char** argv) try {
	// nes_to_asm("c:/temp/faxanadu (u).nes", "c:/temp/out.asm");
	asm_to_nes("c:/temp/out.asm", "c:/temp/faxscripts.nes", true);
	// nes_to_asm("c:/temp/faxscripts.nes", "c:/temp/out2.asm");
	//asm_to_nes("c:/temp/out2.asm", "c:/temp/faxscripts2.nes", true);
}
catch (const std::runtime_error& ex) {
	std::cerr << "Runtime error: " << ex.what() << "\n";
}
catch (const std::exception& ex) {
	std::cerr << "Runtime exception: " << ex.what() << "\n";
}
catch (...) {
	std::cerr << "Unknown runtime error:\n";
}
