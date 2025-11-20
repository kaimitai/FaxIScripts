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
#ifdef _WIN32
#include <windows.h>
#endif

void fi::Cli::print_header(void) const {
	std::cout << fi::appc::APP_NAME << " v" << fi::appc::APP_VERSION << " - Faxanadu IScript Assembler and Disassembler\n";
	std::cout << "Author: Kai E. Fr";
	output_oe_on_windows();
	std::cout << "land (https://github.com/kaimitai/FaxIScripts)\n";
	std::cout << "Build date: " << __DATE__ << " " << __TIME__ << " CET\n\n";
}

void fi::Cli::print_help(void) const {
	std::cout << "Usage:\n";
	std::cout << "  faxiscripts <[b]uild|e[x]tract> <input file> <output file> [options]\n\n";

	std::cout << "Options:\n";
	std::cout << "  -p, --no-shop-comments       Disable shop comment extraction (enabled by default)\n";
	std::cout << "  -o, --original-size          Only patch original ROM location (disabled by default)\n";
	std::cout << "  -f, --force                  Force assembly-file overwrite when extracting (disabled by default)\n";
	std::cout << "  -s, --source-rom             The ROM file to be used as a source when assembling (by default the output file itself)\n";
	std::cout << "  -r, --region                 ROM region which must be defined in the configuration xml (auto-detected by default)\n";
}

fi::Cli::Cli(int argc, char** argv) :
	m_build_mode{ false },
	m_strict{ false },
	m_shop_comments{ true },
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

	m_config.load_definitions(appc::CONFIG_XML);

	// we have the info we need to execute
	if (m_build_mode) {
		asm_to_nes(m_in_file, m_out_file,
			m_source_rom.empty() ? m_out_file : m_source_rom,
			m_strict);
	}
	else
		nes_to_asm(m_in_file, m_out_file, m_shop_comments, m_overwrite);
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
	bool p_strict) {

	if (p_strict)
		std::cout << "Using strict mode - Only original ROM data region will be used\n";

	fi::AsmReader reader;

	std::cout << "Attempting to read ROM contents from " << p_source_rom_filename << "\n";
	auto rom{ klib::file::read_file_as_bytes(p_source_rom_filename) };

	if (m_region.empty()) {
		m_config.determine_region(rom);
		std::cout << "ROM region resolved to '" << m_config.get_region() << "'\n";
	}
	else {
		m_config.set_region(m_region);
		std::cout << "ROM region specified as '" << m_region << "'\n";
	}

	m_config.load_config_data(appc::CONFIG_XML);

	std::cout << "Attempting to parse assembly file " << p_asm_filename << "\n";
	reader.read_asm_file(m_config, p_asm_filename);

	// we use different methods to get the ROM bytes if the smart linker is used
	auto bytes{ reader.get_script_bytes(m_config) };
	auto strbytes{ reader.get_string_bytes(m_config) };

	std::cout << std::format("Using {} unique strings out of a maximum of 255\n",
		reader.get_string_count());

	// extract constants we need from config
	std::size_t l_size_strings{ m_config.constant(c::ID_STRING_DATA_END) - m_config.constant(c::ID_STRING_DATA_START) };
	std::size_t l_iscript_count{ m_config.constant(c::ID_ISCRIPT_COUNT) };
	std::size_t l_iscript_rg1_size{ m_config.constant(c::ID_ISCRIPT_RG1_SIZE) };
	std::size_t l_iscript_rg2_start{ m_config.constant(c::ID_ISCRIPT_RG2_START) };
	std::size_t l_iscript_rg2_size{ m_config.constant(c::ID_ISCRIPT_RG2_END) - l_iscript_rg2_start };
	auto l_iscript_ptr{ m_config.pointer(c::ID_ISCRIPT_PTR_LO) };
	std::size_t l_iscript_string_start{ m_config.constant(c::ID_STRING_DATA_START) };
	std::size_t l_iscript_string_size{ m_config.constant(c::ID_STRING_DATA_END) - l_iscript_string_start };

	try_patch_msg("strings", strbytes.size(), l_size_strings);
	try_patch_msg("script data (region 1)",
		bytes.first.size() - l_iscript_count * 2,
		l_iscript_rg1_size);

	try_patch_msg("script data (region 2)",
		bytes.second.size(),
		l_iscript_rg2_size);

	if (p_strict && !bytes.second.empty())
		throw std::runtime_error("Strict mode was enabled but the original ROM region could not fit all data");

	for (std::size_t i{ 0 }; i < bytes.first.size(); ++i)
		rom.at(i + l_iscript_ptr.first) = bytes.first[i];

	for (std::size_t i{ 0 }; i < bytes.second.size(); ++i)
		rom.at(i + (
			!p_strict ?
			l_iscript_rg2_start : l_iscript_ptr.first + bytes.first.size()
			)) = bytes.second[i];

	for (std::size_t i{ 0 }; i < strbytes.size(); ++i)
		rom.at(i + l_iscript_string_start) = strbytes[i];
	// make the rest of the string section unparseable so we don't
	// accidentally import any garbage strings from the file we emit
	for (std::size_t i{ strbytes.size() }; i < l_iscript_string_size; ++i)
		rom.at(i + l_iscript_string_start) = 0x00;

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
	const std::string& p_asm_filename, bool p_shop_comments, bool p_overwrite) {

	// show params
	if (p_shop_comments)
		std::cout << "Will show shop data as comments where they are referenced\n";
	if (p_overwrite)
		std::cout << "Will overwrite output assembly file if it already exists\n";

	// fail early if asm file already exists and we do not overwrite
	if (!p_overwrite && klib::file::file_exists(p_asm_filename))
		throw std::runtime_error(std::format("Assembly file {} exists, and overwrite-flag is not set", p_asm_filename));

	std::cout << "Attempting to read " << p_nes_filename << "\n";
	const auto& rom_data{ klib::file::read_file_as_bytes(p_nes_filename) };

	if (m_region.empty()) {
		m_config.determine_region(rom_data);
		std::cout << "ROM region resolved to '" << m_config.get_region() << "'\n";
	}
	else {
		m_config.set_region(m_region);
		std::cout << "ROM region specified as '" << m_region << "\n";
	}

	m_config.load_config_data(appc::CONFIG_XML);

	fi::IScriptLoader loader(rom_data);

	std::cout << "Attempting to parse ROM scripting layer\n";
	loader.parse_rom(m_config);

	fi::AsmWriter asmw;

	std::cout << "Generating output file " << p_asm_filename << "\n";
	asmw.generate_asm_file(m_config, p_asm_filename,
		loader.m_instructions, loader.ptr_table, loader.m_jump_targets,
		loader.m_strings, loader.m_shops, m_shop_comments);

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
		else if (argvi == appc::CLI_REGION.first ||
			argvi == appc::CLI_REGION.second) {
			if (i + 1 >= argc)
				throw std::runtime_error("Region option was used, but no ROM region was specified");
			else
				m_region = argv[++i];
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
		m_shop_comments = !m_shop_comments;
	else if (p_flag_idx == 1)
		m_strict = !m_strict;
	else if (p_flag_idx == 2)
		m_overwrite = !m_overwrite;
}

// sad that this is needed in 2025
void fi::Cli::output_oe_on_windows(void) const {

#ifdef _WIN32
	UINT old_cp = GetConsoleOutputCP();
	SetConsoleOutputCP(CP_UTF8);
#endif

	std::cout << "ø";

#ifdef _WIN32
	SetConsoleOutputCP(old_cp);
#endif
}
