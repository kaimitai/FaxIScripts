#ifndef FI_CLI_H
#define FI_CLI_H

#include <vector>
#include <string>
#include "./../../fe/Config.h"

namespace fi {

	class Cli {

		bool m_build_mode;

		std::string m_in_file, m_out_file, m_source_rom, m_region;
		bool m_strict, m_shop_comments, m_overwrite;
		fe::Config m_config;

		void set_mode(const std::string& p_mode);
		void toggle_flag(std::size_t p_flag_idx);
		void set_flag(const std::string& p_flag);
		void print_header(void) const;
		void print_help(void) const;
		void parse_arguments(int arg_start, int argc, char** argv);

		void output_oe_on_windows(void) const;

		// main logic
		void asm_to_nes(const std::string& p_asm_filename,
			const std::string& p_nes_filename,
			const std::string& p_source_rom_filename, 
			bool p_strict);
		void nes_to_asm(const std::string& p_nes_filename,
			const std::string& p_asm_filename,
			bool p_shop_comments, bool p_overwrite);

	public:
		Cli(int argc, char** argv);
	};

}

#endif
