#include "MusicOpcode.h"
#include "./../common/klib/Kstring.h"
#include "fm_constants.h"
#include <format>
#include <stdexcept>

fm::MusicOpcode::MusicOpcode(const std::string& p_mnemonic,
	fm::OpcodeType p_opcodetype,
	fm::AudioArgType p_argtype,
	fm::AudioFlow p_flow) :
	m_opcodetype{ p_opcodetype },
	m_mnemonic{ p_mnemonic },
	m_argtype{ p_argtype },
	m_flow{ p_flow }
{
}

fm::MusicOpcode::MusicOpcode(const std::string& p_params) :
	m_opcodetype{ fm::OpcodeType::Opcode },
	m_argtype{ fm::AudioArgType::None },
	m_flow{ fm::AudioFlow::Continue }
{
	auto kvs{ klib::str::extract_keyval_str(p_params) };

	for (const auto& kv : kvs) {
		if (klib::str::to_lower(kv.first) == klib::str::to_lower(c::XML_OPCODE_PARAM_MNEMONIC)) {
			m_mnemonic = kv.second;
		}
		else if (klib::str::to_lower(kv.first) == klib::str::to_lower(c::XML_OPCODE_PARAM_ARG)) {
			m_argtype = string_to_enum_argtype(kv.second);
		}
		else if (klib::str::to_lower(kv.first) == klib::str::to_lower(c::XML_OPCODE_PARAM_FLOW)) {
			m_flow = string_to_enum_flow(kv.second);
		}
		else if (klib::str::to_lower(kv.first) == klib::str::to_lower(c::XML_OPCODE_PARAM_TYPE)) {
			m_opcodetype = string_to_enum_opcodetype(kv.second);
		}
		else
			throw std::runtime_error("Invalid opcode parameter name " + kv.first);
	}

	if (m_mnemonic.empty())
		throw std::runtime_error("Mnemonic for audio opcode not given");
}

std::size_t fm::MusicOpcode::size(void) const {
	std::size_t result{ 1 };

	if (m_argtype == fm::AudioArgType::Byte)
		++result;
	else if (m_argtype == fm::AudioArgType::Address)
		result += 2;

	return result;
}

std::vector<byte> fm::MusicInstruction::get_bytes(const std::vector<fm::MusicOpcode>& p_opcodes) const {
	std::vector<byte> result{ opcode_byte };

	const auto& op{ p_opcodes.at(opcode_byte) };

	if (op.m_argtype == fm::AudioArgType::Byte)
		result.push_back(static_cast<byte>(operand.value()));
	else if (op.m_argtype == fm::AudioArgType::Address) {
		uint16_t opval{ operand.value() };
		result.push_back(static_cast<byte>(opval % 256));
		result.push_back(static_cast<byte>(opval / 256));
	}

	if (op.m_flow == fm::AudioFlow::Jump) {
		uint16_t opval{ static_cast<uint16_t>(jump_target.value()) };
		result.push_back(static_cast<byte>(opval % 256));
		result.push_back(static_cast<byte>(opval / 256));
	}

	return result;
}


std::vector<fm::MusicOpcode> fm::parse_xml_map(const std::map<byte, std::string>& p_map) {
	std::vector<fm::MusicOpcode> result;

	std::map<byte, fm::MusicOpcode> ops;
	for (const auto& kv : p_map)
		ops.insert(std::make_pair(kv.first, fm::MusicOpcode(kv.second)));

	for (byte b{ 0 }; b < 255; ++b)
		if (ops.find(b) == end(ops)) {
			ops.insert(std::make_pair(b,
				fm::MusicOpcode(std::format("${:02x}", b),
					fm::OpcodeType::Note, fm::AudioArgType::None,
					fm::AudioFlow::Continue)
			));
		}

	for (const auto& kv : ops)
		result.push_back(kv.second);

	return result;
}

fm::AudioFlow fm::string_to_enum_flow(const std::string& p_str) {
	if (klib::str::to_lower(p_str) == "jump")
		return fm::AudioFlow::Jump;
	else if (klib::str::to_lower(p_str) == "continue")
		return fm::AudioFlow::Continue;
	else if (klib::str::to_lower(p_str) == "end")
		return fm::AudioFlow::End;
	else
		throw std::runtime_error("Not a valid flow type: " + p_str);
}

fm::AudioArgType fm::string_to_enum_argtype(const std::string& p_str) {
	if (klib::str::to_lower(p_str) == "none")
		return fm::AudioArgType::None;
	else if (klib::str::to_lower(p_str) == "byte")
		return fm::AudioArgType::Byte;
	else if (klib::str::to_lower(p_str) == "addr")
		return fm::AudioArgType::Address;
	else
		throw std::runtime_error("Not a valid flow type: " + p_str);
}

fm::OpcodeType fm::string_to_enum_opcodetype(const std::string& p_str) {
	if (klib::str::to_lower(p_str) == "opcode")
		return fm::OpcodeType::Opcode;
	else if (klib::str::to_lower(p_str) == "note")
		return fm::OpcodeType::Note;
	else
		throw std::runtime_error("Not a valid opcode type: " + p_str);
}
