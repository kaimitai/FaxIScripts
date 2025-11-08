#include "Cli.h"
#include <format>
#include <iostream>
#include "application_constants.h"
#include "./../fi_constants.h"
#include <stdexcept>
#include "./../IScriptLoader.h"
#include "./../AsmReader.h"
#include "./../AsmWriter.h"
#include "./../../common/klib/Kfile.h"

void fi::Cli::print_header(void) const {
	std::cout << fi::appc::APP_NAME << " v" << fi::appc::APP_VERSION << " - Faxanadu IScript Assembler and Disassembler\n";
	std::cout << "Author: Kai E. Froeland (https://github.com/kaimitai/FaxIScripts)\n";
	std::cout << "Build date: " << __DATE__ << " " << __TIME__ << " CET\n\n";
}

void fi::Cli::print_help(void) const {
	std::cout << "Usage:\n";
	std::cout << "  faxiscripts <[b]uild|e[x]tract> <input file> <output file> [options]\n\n";

	std::cout << "Options:\n";
	std::cout << "  -c, --no-string-comments     Disable string comment extraction (enabled by default)\n";
	std::cout << "  -p, --no-shop-comments       Disable shop comment extraction (enabled by default)\n";
	std::cout << "  -o, --original-size          Only patch original ROM location (disabled by default)\n";
	std::cout << "  -n, --no-smart-linker        Disable smart linker optimization (enabled by default)\n";
	std::cout << "  -f, --force                  Force assembly-file overwrite when extracting (disabled by default)\n";
	std::cout << "  -s, --source-rom             The ROM file to be used as a source when assembling (by default the output file itself)\n\n";
}

fi::Cli::Cli(int argc, char** argv) :
	m_build_mode{ false },
	m_bridge{ true },
	m_strict{ false },
	m_shop_comments{ true },
	m_str_comments{ true },
	m_overwrite{ false }
{
	print_header();

	if (argc < 4) {
		print_help();
		return;
	}
	else {
		set_mode(argv[1]);
		m_in_file = argv[2];
		m_out_file = argv[3];
		parse_arguments(4, argc, argv);
	}

	// we have the info we need to execute
	if (m_build_mode) {
		asm_to_nes(m_in_file, m_out_file,
			m_source_rom.empty() ? m_out_file : m_source_rom,
			m_strict, m_bridge);
	}
	else
		nes_to_asm(m_in_file, m_out_file, m_str_comments, m_shop_comments, m_overwrite);
}

static void try_patch_msg(const std::string& p_data_type,
	std::size_t p_data_size, std::size_t p_data_max_size) {
	std::cout << std::format("Trying to patch {}: Using {} of {} available bytes ({:.2f}%)\n",
		p_data_type, p_data_size, p_data_max_size,
		100.0f * static_cast<float>(p_data_size) / static_cast<float>(p_data_max_size));
	if (p_data_size > p_data_max_size)
		throw std::runtime_error(std::format("Size limits exceeded for {}",
			p_data_type));
}

void fi::Cli::asm_to_nes(const std::string& p_asm_filename,
	const std::string& p_out_filename,
	const std::string& p_source_rom_filename,
	bool p_strict, bool p_smart_link) {

	if (p_strict)
		std::cout << "Using strict mode - Only original ROM data region will be used\n";
	if (p_smart_link)
		std::cout << "Using smart linker strategy to optimize for size\n";

	fi::AsmReader reader;

	std::cout << "Attempting to parse assembly file " << p_asm_filename << "\n";
	reader.read_asm_file(p_asm_filename, !p_strict, p_smart_link);

	std::cout << "Attempting to read ROM contents from " << p_source_rom_filename << "\n";
	auto rom{ klib::file::read_file_as_bytes(p_source_rom_filename) };

	// we use different methods to get the ROM bytes if the smart linker is used
	auto bytes{ p_smart_link ?
		reader.get_script_bytes() : reader.get_bytes(!p_strict) };
	auto strbytes{ reader.get_string_bytes() };

	try_patch_msg("strings", strbytes.size(), fi::c::SIZE_STRINGS);
	try_patch_msg("script data (region 1)",
		bytes.first.size() - c::ISCRIPT_COUNT * 2,
		fi::c::ISCRIPT_DATA_SIZE_REGION_1);

	try_patch_msg("script data (region 2)",
		bytes.second.size(),
		fi::c::ISCRIPT_DATA_SIZE_REGION_2);

	if (p_strict && !bytes.second.empty())
		throw std::runtime_error("Strict mode was enabled but the original ROM region could not fit all data");

	for (std::size_t i{ 0 }; i < bytes.first.size(); ++i)
		rom.at(i + fi::c::ISCRIPT_ADDR_LO) = bytes.first[i];

	for (std::size_t i{ 0 }; i < bytes.second.size(); ++i)
		rom.at(i + (
			!p_strict ?
			fi::c::ISCRIPT_DATA_ROM_OFFSET_REGION_2 : fi::c::ISCRIPT_ADDR_LO + bytes.first.size()
			)) = bytes.second[i];

	for (std::size_t i{ 0 }; i < strbytes.size(); ++i)
		rom.at(i + fi::c::OFFSET_STRINGS) = strbytes[i];

	std::cout << "Verifying generated ROM contents\n";
	try {
		fi::IScriptLoader staticanalysisread(rom);
	}
	catch (const std::runtime_error& ex) {
		std::cerr << "Invalid ROM generated. Ensure all code paths end, and that each entrypoint has a textbox context\n" << ex.what();
		return;
	}

	std::cout << "Attempting to patch file " << p_out_filename << "\n";
	klib::file::write_bytes_to_file(rom, p_out_filename);
	std::cout << "File patched\n";
}

void fi::Cli::nes_to_asm(const std::string& p_nes_filename,
	const std::string& p_asm_filename, bool p_str_comments,
	bool p_shop_comments, bool p_overwrite) {

	// show params
	if (p_str_comments)
		std::cout << "Will show string data as comments where they are referenced\n";
	if (p_shop_comments)
		std::cout << "Will show shop data as comments where they are referenced\n";
	if (p_overwrite)
		std::cout << "Will overwrite output assembly file if it already exists\n";

	// fail early if asm file already exists and we do not overwrite
	if (!p_overwrite && klib::file::file_exists(p_asm_filename))
		throw std::runtime_error(std::format("Assembly file {} exists, and overwrite-flag is not set", p_asm_filename));

	std::cout << "Attempting to read " << p_nes_filename << "\n";
	fi::IScriptLoader loader(klib::file::read_file_as_bytes(p_nes_filename));

	std::cout << "Attempting to parse ROM scripting layer\n";
	loader.parse_rom();

	fi::AsmWriter asmw;

	std::cout << "Generating output file " << p_asm_filename << "\n";
	asmw.generate_asm_file(p_asm_filename,
		loader.m_instructions, loader.ptr_table, loader.m_jump_targets,
		loader.m_strings, loader.m_shops, p_str_comments, p_shop_comments);

	std::cout << "Extraction complete!\n";
}

void fi::Cli::parse_arguments(int arg_start, int argc, char** argv) {
	for (int i{ arg_start }; i < argc; ++i) {
		std::string argvi{ argv[i] };
		if (argvi == appc::CLI_SOURCE_ROM.first ||
			argvi == appc::CLI_SOURCE_ROM.second) {
			if (i + 1 >= argc)
				throw std::runtime_error("Source ROM option was set, but no source ROM file was specified");
			else
				m_source_rom = argv[++i];
		}
		else
			set_flag(argvi);
	}
}

void fi::Cli::set_mode(const std::string& p_mode) {
	if (p_mode == appc::CMD_BUILD.first || p_mode == appc::CMD_BUILD.second)
		m_build_mode = true;
	else if (p_mode == appc::CMD_EXTRACT.first || p_mode == appc::CMD_EXTRACT.second)
		m_build_mode = false;
	else throw std::runtime_error("Unknown commad " + p_mode);
}

// TODO: Streamline flag lookup with const map
void fi::Cli::set_flag(const std::string& p_flag) {
	for (std::size_t i{ 0 }; i < appc::CLI_FLAGS.size(); ++i) {
		if (p_flag == appc::CLI_FLAGS[i].first || p_flag == appc::CLI_FLAGS[i].second) {
			toggle_flag(i);
			return;
		}
	}

	throw std::runtime_error("Unknown option " + p_flag);
}

void fi::Cli::toggle_flag(std::size_t p_flag_idx) {
	if (p_flag_idx == 0)
		m_str_comments = !m_str_comments;
	else if (p_flag_idx == 1)
		m_shop_comments = !m_shop_comments;
	else if (p_flag_idx == 2)
		m_strict = !m_strict;
	else if (p_flag_idx == 3)
		m_bridge = !m_bridge;
	else if (p_flag_idx == 4)
		m_overwrite = !m_overwrite;
}
