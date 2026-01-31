#ifndef FB_BSCRIPTREADER_H
#define FB_BSCRIPTREADER_H

#include "./../fe/Config.h"
#include "BScriptOpcode.h"
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace fb {

	enum class SectionType { Defines, BScript };

	class BScriptReader {

		std::pair<std::size_t, std::size_t> bscript_ptr;
		std::size_t bscript_count;

		std::map<byte, fb::BScriptOpcode> opcodes, behavior_ops;
		std::map<std::string, int> defines;
		std::vector<fb::BScriptInstruction> instructions;
		std::vector<std::size_t> ptr_table;

		bool is_label(const std::string& p_asm, const std::vector<std::string>& p_line) const;
		std::string get_label(const std::vector<std::string>& p_line) const;
		bool is_entrypoint(const std::string& p_asm, const std::vector<std::string>& p_line) const;
		std::size_t get_entrypoint(const std::vector<std::string>& p_line) const;

		std::map<fb::ArgDomain, std::string> get_argmap(const std::string& p_asm,
			const std::vector<std::string>& p_line) const;

		std::string get_label_name(const std::string& p_asm, const std::map<fb::ArgDomain, std::string>& p_argmap,
			fb::ArgDomain domain) const;
		int resolve_value(const std::string& p_asm, const std::map<fb::ArgDomain, std::string>& p_argmap,
			fb::ArgDomain domain) const;
		int get_default_value(const std::string& p_asm, fb::ArgDomain domain) const;

	public:
		BScriptReader(const fe::Config& p_config);
		void read_asm_file(const std::string& p_filename,
			const fe::Config& p_config);
		std::vector<byte> to_bytes(void) const;
	};

}

#endif
