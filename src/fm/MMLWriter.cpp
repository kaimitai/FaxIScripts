#include "MMLWriter.h"
#include <format>
#include <utility>
#include "./../common/klib/Kfile.h"
#include "fm_constants.h"

void fm::MMLWriter::generate_mml_file(const std::string& p_filename,
	const std::map<std::size_t, fm::MusicInstruction>& p_instructions,
	const std::vector<fm::MusicOpcode>& p_opcodes,
	const std::vector<std::size_t>& p_entrypoints,
	const std::set<std::size_t>& p_jump_targets) const {

	const auto& get_next_label = [](std::size_t p_offset,
		int p_lastentry, int& p_lastlabel,
		std::map<std::size_t, std::string>& p_labels) -> std::string {
			auto labiter{ p_labels.find(p_offset) };
			if (labiter != end(p_labels))
				return labiter->second;

			std::string tmp_label{ std::format("@mscript_{:03}_{:02}",
				p_lastentry, p_lastlabel++) };
			p_labels[p_offset] = tmp_label;
			return tmp_label;
		};

	// make a map from offset to vector of entrypoints (tune and channel no combo)
	std::map<std::size_t, std::vector<std::pair<std::size_t, std::size_t>>> l_eps;
	for (std::size_t i{ 0 }; i < p_entrypoints.size(); i += 4)
		for (std::size_t j{ 0 }; j < 4; ++j)
			l_eps[p_entrypoints.at(i + j)].push_back(
				std::make_pair(i / 4, j)
			);

	std::string af{
		" ; MScript MML file extracted by FaxIScripts v0.4\n ; https://github.com/kaimitai/FaxIScripts\n"
	};

	int lastentry{ 0 }, lastlabel{ 0 };
	std::map<std::size_t, std::string> l_labels;
	bool note_last{ false };

	for (const auto& kv : p_instructions) {
		const fm::MusicInstruction& instr{ kv.second };
		std::size_t offset{ kv.first };

		// are we at an entrypoint? output it
		auto ep{ l_eps.find(offset) };
		if (ep != end(l_eps)) {
			af += std::format("\n{}", c::ID_MSCRIPT_ENTRYPOINT);
			for (std::size_t i{ 0 }; i < ep->second.size(); ++i) {
				af += std::format(" {}.{}", ep->second[i].first,
					c::CHANNEL_LABELS.at(ep->second[i].second));
				lastentry = static_cast<int>(ep->second[i].first * 4 + ep->second[i].second);
				lastlabel = 0;
			}
			af += "\n";
		}

		// emit jump labels at this offset
		if (p_jump_targets.find(offset) != end(p_jump_targets)) {
			af += std::format("{}:\n",
				get_next_label(offset, lastentry, lastlabel, l_labels));
		}


		const auto& op{ p_opcodes.at(instr.opcode_byte) };

		if (op.m_opcodetype != fm::OpcodeType::Note && note_last)
			af += "\n    ";

		af += std::format(" {}", op.m_mnemonic);

		if (op.m_argtype == fm::AudioArgType::Byte)
			af += std::format(" {}", instr.operand.value());

		if (op.m_flow == fm::AudioFlow::Jump)
			af += std::format(" {}",
				get_next_label(instr.jump_target.value(),
					lastentry, lastlabel, l_labels)
			);

		if (op.m_opcodetype != fm::OpcodeType::Note)
			af += "\n";
		else
			note_last = true;
	}

	klib::file::write_string_to_file(af, p_filename);
}
