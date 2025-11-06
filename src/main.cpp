#include <iostream>
#include <vector>
#include "./common/klib/Kfile.h"
#include "./fi/IScriptLoader.h"
#include "./fi/Opcode.h"
#include "./fi/AsmWriter.h"

constexpr std::size_t ISCRIPT_ADDR_LO{ 0x31f7b };
constexpr std::size_t ISCRIPT_ADDR_HI{ 0x32013 };
constexpr std::size_t ISCRIPT_COUNT{ 152 };
constexpr std::size_t ISCRIPT_PTR_ZERO_ADDR{ 0x28010 };

int main(int argc, char** argv) {

	std::vector<std::size_t> script_ptrs;

	fi::IScriptLoader loader(klib::file::read_file_as_bytes("c:/temp/Faxanadu (U).nes"));

	loader.parse_rom();

	fi::AsmWriter asmw;

	asmw.generate_asm_file("c:/temp/out.iscript",
		loader.m_instructions, loader.ptr_table, loader.m_jump_targets,
		loader.m_strings, loader.m_shops);

}
