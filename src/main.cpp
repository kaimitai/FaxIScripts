#include <iostream>
#include "./fi/IScriptLoader.h"
#include "./fi/AsmWriter.h"
#include "./fi/AsmReader.h"
#include "./common/klib/Kfile.h"

constexpr std::size_t ISCRIPT_ADDR_LO{ 0x31f7b };
constexpr std::size_t ISCRIPT_ADDR_HI{ 0x32013 };
constexpr std::size_t ISCRIPT_COUNT{ 152 };
constexpr std::size_t ISCRIPT_PTR_ZERO_ADDR{ 0x28010 };

int main(int argc, char** argv) {

	
	fi::IScriptLoader loader(klib::file::read_file_as_bytes("c:/temp/faxscripts.nes"));
	loader.parse_rom();
	fi::AsmWriter asmw;
	asmw.generate_asm_file("c:/temp/out-2.iscript",
		loader.m_instructions, loader.ptr_table, loader.m_jump_targets,
		loader.m_strings, loader.m_shops);

	/*
	fi::AsmReader reader;
	reader.read_asm_file("c:/temp/out.iscript");
	reader.get_bytes();
	*/
}
