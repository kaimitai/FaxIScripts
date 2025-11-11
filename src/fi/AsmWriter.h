#ifndef FI_ASMWRITER_H
#define FI_ASMWRITER_H

#include "IScriptLoader.h"
#include "FaxString.h"
#include "Shop.h"

namespace fi {

	class AsmWriter {

	public:

		AsmWriter(void) = default;

		void append_defines_section(std::string& p_asm) const;
		void append_strings_section(std::string& p_asm,
			const std::vector<fi::FaxString>& p_strings) const;
		void append_shops_section(std::string& p_asm,
			const std::vector<fi::Shop>& p_shops) const;
		std::string serialize_shop_as_string(const fi::Shop& p_shop) const;

		void generate_asm_file(const std::string& p_filename,
			const std::map<std::size_t, fi::Instruction>& p_instructions,
			const std::vector<std::size_t>& p_entrypoints,
			const std::set<std::size_t>& p_jump_targets,
			const std::vector<fi::FaxString>& p_strings,
			const std::vector<fi::Shop>& p_shops,
			bool p_shop_comments) const;

		std::string get_define(const std::map<byte, std::string>& p_map, byte arg) const;
		std::string get_define(fi::ArgDomain domain, byte arg) const;
	};

}

#endif
