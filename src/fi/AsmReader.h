#ifndef FI_ASM_READER_H
#define FI_ASM_READER_H

#include <map>
#include <string>
#include <vector>
#include "FaxString.h"
#include "Shop.h"
#include "Opcode.h"

namespace fi {

	enum class SectionType { Defines, Strings, Shops, IScript };

	class AsmReader {

		std::vector<fi::FaxString> m_strings;
		std::map<std::string, std::size_t> m_defines;
		std::map<std::size_t, fi::Shop> m_shops;
		std::vector<fi::Instruction> m_instructions;
		// map from entrypoint no to offset
		std::map<std::size_t, std::size_t> m_ptr_table;

		std::map<SectionType, std::vector<std::string>> m_sections;

		std::string strip_comment(const std::string& line) const;
		std::string trim(const std::string& line) const;

		int parse_numeric(const std::string& token) const;

		void parse_section_strings(void);
		void parse_section_defines(void);
		void parse_section_shops(void);
		void parse_section_iscript(bool p_use_region_2);
		void parse_section_iscript(void);

		std::size_t resolve_token(const std::string& token) const;
		
		bool contains_label(const std::string& p_line) const;
		std::string extract_label(const std::string& p_line) const;

		bool contains_entrypoint(const std::string& p_line) const;
		std::size_t extract_entrypoint(const std::string& p_line) const;

		bool contains_textbox(const std::string& p_line) const;
		byte extract_textbox(const std::string& p_line) const;
		bool str_begins_with(const std::string& p_line, const std::string& p_start) const;
		std::vector<std::string> split_whitespace(const std::string& p_line) const;

		std::string to_lower(const std::string& str);

	public:
		AsmReader(void) = default;
		void read_asm_file(const std::string& p_filename, bool p_use_region_2);

		std::pair<std::vector<byte>, std::vector<byte>> get_script_bytes(void) const;
		std::pair<std::vector<byte>, std::vector<byte>> get_bytes(void) const;
		std::vector<byte> get_string_bytes(void) const;
	};

}

#endif
