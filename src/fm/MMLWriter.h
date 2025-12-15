#ifndef FM_MMLWRITER_H
#define FM_MMLWRITER_H

#include "MScriptLoader.h"

#include <string>
#include <vector>

namespace fm {

	class MMLWriter {

	public:
		MMLWriter(void) = default;
		void generate_mml_file(const std::string& p_filename,
			const std::map<std::size_t, fm::MusicInstruction>& p_instructions,
			const std::vector<fm::MusicOpcode>& p_opcodes,
			const std::vector<std::size_t>& p_entrypoints,
			const std::set<std::size_t>& p_jump_targets) const;
	};

}

#endif
