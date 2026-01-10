#include "MMLChannel.h"
#include "./../fm_constants.h"
#include "mml_constants.h"
#include <format>
#include <numeric>
#include <stack>
#include <stdexcept>
#include <tuple>

using byte = unsigned char;

fm::MMLChannel::MMLChannel(fm::Fraction p_song_tempo, int* p_bpm) :
	song_tempo{ p_song_tempo },
	bpm{ p_bpm }
{
	reset_vm();
}

int fm::MMLChannel::get_song_transpose(void) const {
	// return the first song transposition, if any
	for (const auto& ev : events) {
		if (std::holds_alternative<SongTransposeEvent>(ev)) {
			auto& ste = std::get<SongTransposeEvent>(ev);
			return ste.semitones;
		}
	}

	// no song transpositions
	return 0;
}

std::size_t fm::MMLChannel::get_start_index(void) const {
	// return the first start, if any (should be at most one)
	for (std::size_t i{ 0 }; i < events.size(); ++i) {
		if (std::holds_alternative<StartEvent>(events[i])) {
			return i;
		}
	}

	// no explicit start
	return 0;
}

std::size_t fm::MMLChannel::get_label_addr(const std::string& label) {
	for (std::size_t i{ 0 }; i < events.size(); ++i) {
		if (std::holds_alternative<LabelEvent>(events[i])) {
			auto& labe = std::get<LabelEvent>(events[i]);
			if (labe.name == label)
				return i;
		}
	}

	throw std::runtime_error(std::format("Could not find label with name {}", label));
}

void fm::MMLChannel::reset_vm(bool point_at_start) {
	vm.current_index = -1;
	vm.index = 0;
	vm.loop_count = 0;
	vm.loop_iters = 0;
	vm.jsr_addr.reset();
	vm.push_addr.reset();
	vm.loop_addr.reset();
	vm.loop_end_addr.reset();
	vm.st.octave = 4;
	vm.st.ticklength = 0;
	vm.st.default_length = { std::nullopt, 0, 0 };
	vm.st.pitchoffset = 0;
	vm.st.volume = 15;
	vm.st.duty_cycle = 0;
	vm.tempo = song_tempo;

	if (point_at_start) {
		vm.pc = get_start_index();
	}
	else
		vm.pc = 0;
}

const fm::MmlEvent* fm::MMLChannel::current_event(void) const {
	int i = vm.current_index;

	if (i < 0 || i >= events.size())
		return nullptr;

	return &events[i];
}

bool fm::MMLChannel::step(void) {
	if (vm.index >= events.size())
		return false;

	vm.current_index = vm.index;

	const MmlEvent& ev = events[vm.index];

	++vm.index;

	return true;
}

byte fm::MMLChannel::encode_percussion(int p_perc, int p_repeat) const {
	byte result{ static_cast<byte>(p_perc * 16 + p_repeat) };
	if (result == c::HEX_REST || result >= c::HEX_NOTELENGTH_MIN)
		throw std::runtime_error("Percussion byte out of range");
	else
		return result;
}

// add set-length instructions if the input tick length differs from the previous
void fm::MMLChannel::emit_set_length_bytecode_if_necessary(
	std::vector<fm::MusicInstruction>& instrs,
	std::optional<int> length, int dots, std::optional<int> raw,
	bool force) {
	auto dur{ resolve_duration(length, dots, raw) };
	auto ticks{ tick_length(dur) };

	vm.st.fraq += ticks.fraq;
	int carry = vm.st.fraq.extract_whole();
	int final_ticks = ticks.whole + carry;

	if (final_ticks > 255)
		throw std::runtime_error("Tick-length exceeds 255");

	if (force || (final_ticks != vm.st.ticklength)) {
		vm.st.ticklength = final_ticks;

		constexpr byte HEX_NOTELENGTH_MAX{ c::HEX_NOTELENGTH_END - c::HEX_NOTELENGTH_MIN };

		if (final_ticks > 109) {
			fm::MusicInstruction sli{ c::MSCRIPT_OPCODE_SET_LENGTH,
			static_cast<byte>(final_ticks) };
			instrs.push_back(sli);
		}
		else {
			fm::MusicInstruction sli(c::HEX_NOTELENGTH_MIN + static_cast<byte>(final_ticks));
			instrs.push_back(sli);
		}
	}

}

fm::ChannelBytecodeExport fm::MMLChannel::to_bytecode(void) {
	std::vector<fm::MusicInstruction> instrs;

	// label to instruction index the label points to
	std::map<std::string, std::size_t> label_idx;
	// label to all instr indexes jumping to it
	std::map<std::string, std::vector<std::size_t>> jump_targets;

	reset_vm();

	for (std::size_t i{ 0 }; i < events.size(); ++i) {
		const auto& ev{ events[i] };

		// musical note
		if (std::holds_alternative<NoteEvent>(ev)) {
			auto& note = std::get<NoteEvent>(ev);

			emit_set_length_bytecode_if_necessary(instrs, note.length, note.dots, note.raw);
			byte note_byte{ static_cast<byte>(note_index(vm.st.octave, note.pitch)) };

			fm::MusicInstruction notei(note_byte);
			instrs.push_back(notei);
		}
		// percussion note
		else if (std::holds_alternative<PercussionEvent>(ev)) {
			auto& perce = std::get<PercussionEvent>(ev);

			// check the tick length and emit length event if needed
			emit_set_length_bytecode_if_necessary(instrs);

			fm::MusicInstruction perci(encode_percussion(perce.perc_no, perce.repeat));
			instrs.push_back(perci);
		}
		// rest
		else if (std::holds_alternative<RestEvent>(ev)) {
			auto& reste{ std::get<RestEvent>(ev) };

			emit_set_length_bytecode_if_necessary(instrs, reste.length, reste.dots, reste.raw);
			fm::MusicInstruction resti(c::HEX_REST);
			instrs.push_back(resti);
		}
		// length - an abstraction that also exists in Faxanadu's music engine
		else if (std::holds_alternative<LengthEvent>(ev)) {
			auto& lene{ std::get<LengthEvent>(ev) };
			if (lene.raw.has_value()) {
				vm.st.default_length = { std::nullopt, lene.raw.value(), 0 };
			}
			else if (lene.length.has_value()) {
				vm.st.default_length = { lene.length.value(), std::nullopt, lene.dots };
			}
			else
				throw std::runtime_error("Malformed length event");

			emit_set_length_bytecode_if_necessary(instrs, lene.length, lene.dots, lene.raw, true);
		}
		// music opcodes
		else if (std::holds_alternative<SongTransposeEvent>(ev)) {
			auto& ste{ std::get<SongTransposeEvent>(ev) };
			fm::MusicInstruction sti(c::MSCRIPT_OPCODE_GLOBAL_TRANSPOSE,
				int_to_byte(ste.semitones));
			instrs.push_back(sti);
		}
		else if (std::holds_alternative<ChannelTransposeEvent>(ev)) {
			auto& cte{ std::get<ChannelTransposeEvent>(ev) };
			fm::MusicInstruction cti(c::MSCRIPT_OPCODE_CHANNEL_TRANSPOSE,
				int_to_byte(cte.semitones));
			instrs.push_back(cti);
		}
		else if (std::holds_alternative<DetuneEvent>(ev)) {
			auto& dete{ std::get<DetuneEvent>(ev) };
			fm::MusicInstruction deti(c::MSCRIPT_OPCODE_SQ2_DETUNE, int_to_byte(dete.value));
			instrs.push_back(deti);
		}
		else if (std::holds_alternative<EffectEvent>(ev)) {
			auto& effe{ std::get<EffectEvent>(ev) };
			fm::MusicInstruction effi(c::MSCRIPT_OPCODE_SQ_PITCH_EFFECT,
				int_to_byte(effe.value));
			instrs.push_back(effi);
		}
		else if (std::holds_alternative<EnvelopeEvent>(ev)) {
			auto& enve{ std::get<EnvelopeEvent>(ev) };
			fm::MusicInstruction envi(c::MSCRIPT_OPCODE_SQ_ENVELOPE,
				int_to_byte(enve.value));
			instrs.push_back(envi);
		}
		else if (std::holds_alternative<VolumeSetEvent>(ev)) {
			auto& vse{ std::get<VolumeSetEvent>(ev) };
			fm::MusicInstruction vsi(c::MSCRIPT_OPCODE_VOLUME,
				int_to_volume_byte(vse.volume));
			instrs.push_back(vsi);
		}
		else if (std::holds_alternative<PulseEvent>(ev)) {
			auto& pulsee{ std::get<PulseEvent>(ev) };
			fm::MusicInstruction pulsei(c::MSCRIPT_OPCODE_SQ_CONTROL,
				ints_to_sq_control(pulsee.duty_cycle, pulsee.length_counter,
					pulsee.constant_volume, pulsee.volume_period));
			instrs.push_back(pulsei);
		}
		// control flow
		else if (std::holds_alternative<LabelEvent>(ev)) {
			auto& labe = std::get<LabelEvent>(ev);
			if (label_idx.contains(labe.name))
				throw std::runtime_error(std::format("Label redefinition: {}", labe.name));
			else
				label_idx.insert(std::make_pair(labe.name, instrs.size()));
		}
		else if (std::holds_alternative<JSREvent>(ev)) {
			auto& jsre = std::get<JSREvent>(ev);
			jump_targets[jsre.label_name].push_back(instrs.size());
			// will add jump target at the end
			fm::MusicInstruction jsri(c::MSCRIPT_OPCODE_JSR);
			instrs.push_back(jsri);
		}
		else if (std::holds_alternative<ReturnEvent>(ev)) {
			fm::MusicInstruction reti(c::MSCRIPT_OPCODE_RETURN);
			instrs.push_back(reti);
		}
		else if (std::holds_alternative<RestartEvent>(ev)) {
			fm::MusicInstruction resti(c::MSCRIPT_OPCODE_RESTART);
			instrs.push_back(resti);
		}
		else if (std::holds_alternative<NOPEvent>(ev)) {
			fm::MusicInstruction nopi(c::MSCRIPT_OPCODE_NOP);
			instrs.push_back(nopi);
		}
		else if (std::holds_alternative<EndEvent>(ev)) {
			fm::MusicInstruction endi(c::MSCRIPT_OPCODE_END);
			instrs.push_back(endi);
		}
		// loops
		else if (std::holds_alternative<PushAddrEvent>(ev)) {
			fm::MusicInstruction pushaddri(c::MSCRIPT_OPCODE_PUSHADDR);
			instrs.push_back(pushaddri);
		}
		else if (std::holds_alternative<PopAddrEvent>(ev)) {
			fm::MusicInstruction popaddri(c::MSCRIPT_OPCODE_POPADDR);
			instrs.push_back(popaddri);
		}
		else if (std::holds_alternative<BeginLoopEvent>(ev)) {
			auto& bloope = std::get<BeginLoopEvent>(ev);
			fm::MusicInstruction bloopi(c::MSCRIPT_OPCODE_BEGIN_LOOP, static_cast<byte>(bloope.iterations));
			instrs.push_back(bloopi);
		}
		else if (std::holds_alternative<LoopIfEvent>(ev)) {
			auto& lie = std::get<LoopIfEvent>(ev);
			fm::MusicInstruction lii(c::MSCRIPT_OPCODE_LOOPIF,
				static_cast<byte>(lie.iterations));
			instrs.push_back(lii);
		}
		else if (std::holds_alternative<EndLoopEvent>(ev)) {
			fm::MusicInstruction eloopi(c::MSCRIPT_OPCODE_END_LOOP);
			instrs.push_back(eloopi);
		}
		// mml abstractions
		else if (std::holds_alternative<OctaveSetEvent>(ev)) {
			auto& ose = std::get<OctaveSetEvent>(ev);
			vm.st.octave = ose.octave;
		}
		else if (std::holds_alternative<OctaveShiftEvent>(ev)) {
			auto& oshe = std::get<OctaveShiftEvent>(ev);
			vm.st.octave += oshe.amount;
		}
		else if (std::holds_alternative<TempoSetEvent>(ev)) {
			auto& tse = std::get<TempoSetEvent>(ev);
			vm.tempo = tse.tempo;
		}

	}

	// patch jumps - jumps to label @label get the instr idx of @label as target
	for (const auto& kv : label_idx)
		for (const auto& n : jump_targets[kv.first])
			instrs[n].jump_target = kv.second;

	return fm::ChannelBytecodeExport{ instrs, get_start_index() };
}

std::string fm::MMLChannel::get_asm(void) {
	std::string result;
	reset_vm();

	while (step()) {

		const fm::MmlEvent* ev{ current_event() };

		if (auto* n = std::get_if<NoteEvent>(ev)) {
			auto noteresult{ resolve_tie_chain(*n) };

			auto ticks{ tick_length(noteresult.total) };
			int pitch{ noteresult.pitch };

			vm.st.fraq += ticks.fraq;
			int carry = vm.st.fraq.extract_whole();
			int final_ticks = ticks.whole + carry;

			if (final_ticks != vm.st.ticklength) {
				vm.st.ticklength = final_ticks;

				if (final_ticks > 109)
					result += std::format("NoteLength {} ", final_ticks);
				else
					result += std::format("l{} ", final_ticks);
			}

			result += std::format("{} ", note_index_to_str(
				note_index(vm.st.octave, pitch)
			));

		}
		else if (auto* r = std::get_if<LengthEvent>(ev)) {
			if (r->raw.has_value()) {
				vm.st.default_length = { std::nullopt, r->raw.value(), 0 };
			}
			else if (r->length.has_value()) {
				vm.st.default_length = { r->length.value(), std::nullopt, r->dots };
			}
			else
				throw std::runtime_error("Malformed length event");
		}
		else if (auto* r = std::get_if<TempoSetEvent>(ev)) {
			vm.tempo = r->tempo;
		}
		else if (auto* r = std::get_if<RestEvent>(ev)) {
			auto dur{ resolve_duration(r->length, r->dots, r->raw) };
			auto ticks{ tick_length(dur) };

			vm.st.fraq += ticks.fraq;
			int carry = vm.st.fraq.extract_whole();
			int final_ticks = ticks.whole + carry;

			if (final_ticks != vm.st.ticklength) {
				vm.st.ticklength = final_ticks;

				if (final_ticks > 109)
					result += std::format("NoteLength {} ", final_ticks);
				else
					result += std::format("l{} ", final_ticks);
			}

			result += std::format("r ");

		}
		else if (auto* o = std::get_if<OctaveSetEvent>(ev)) {
			vm.st.octave = o->octave;
		}
		else if (auto* v = std::get_if<VolumeSetEvent>(ev)) {
			result += std::format("Volume {} \n", v->volume);
		}
		else if (auto* os = std::get_if<OctaveShiftEvent>(ev)) {
			vm.st.octave += os->amount;
		}
		else if (auto* pusha = std::get_if<PushAddrEvent>(ev)) {
			result += "\nPushAddr\n";
		}
		else if (auto* popa = std::get_if<PopAddrEvent>(ev)) {
			result += "\nPopAddr\n";
		}
		else if (auto* begl = std::get_if<BeginLoopEvent>(ev)) {
			result += std::format("\nBeginLoop {}\n", begl->iterations);
		}
		else if (auto* endl = std::get_if<EndLoopEvent>(ev)) {
			result += "\nEndLoop\n";
		}
		else if (auto* ldef = std::get_if<LabelEvent>(ev)) {
			result += std::format("\n{}:\n", ldef->name);
		}
		else if (auto* jsr = std::get_if<JSREvent>(ev)) {
			result += std::format("\njsr {}\n", jsr->label_name);
		}
		else if (auto* ct = std::get_if<ChannelTransposeEvent>(ev)) {
			result += std::format("\nChanneltranspose {}\n", ct->semitones);
		}
		else if (auto* st = std::get_if<SongTransposeEvent>(ev)) {
			result += std::format("\nGlobalTranspose {}\n", st->semitones);
		}

	}

	return result;
}

fm::TickResult fm::MMLChannel::tick_length(const fm::Duration& dur) const {
	// RAW TICKS: bypass musical math entirely
	if (dur.is_raw()) {
		int ticks = dur.raw.value();

		if (ticks < 1 || ticks > 255)
			throw std::runtime_error(
				std::format("Tick count out of bounds ({})", ticks));

		return fm::TickResult{
			ticks,
			Fraction(0, 1)   // no fractional leftover
		};
	}

	// MUSICAL DURATION
	Fraction qnote = Fraction(*bpm * vm.tempo.get_den(), vm.tempo.get_num());
	Fraction total = qnote * (dur.musical * Fraction(4, 1));

	int whole = total.extract_whole();

	if (whole < 1 || whole > 255)
		throw std::runtime_error(
			std::format("Tick count out of bounds ({})", whole));

	return fm::TickResult{
		whole,
		total   // leftover fractional ticks
	};
}


int fm::MMLChannel::note_index(int octave, int pitch) const {
	int absnote = octave * 12 + pitch;

	if (absnote < 1 || absnote >= 0x80)
		throw std::runtime_error(
			std::format("Note index out of range ({})", absnote));

	return absnote - 23; // because C2 (24) should map to 1
}

std::pair<int, int> fm::MMLChannel::split_note(byte p_note_no) const {
	int note_no{ static_cast<int>(p_note_no) - 1 };
	int octave{ 2 + (note_no / 12) };
	int pitch{ note_no % 12 };

	return std::make_pair(pitch, octave);
}

std::string fm::MMLChannel::note_index_to_str(int p_idx) const {
	const std::vector<std::string> pitchnames{
		"c", "c+", "d", "d+", "e", "f", "f+", "g", "g+", "a", "a+", "b"
	};

	int absidx{ p_idx + 23 };

	return std::format("{}{}", pitchnames[absidx % 12], absidx / 12);
}

std::string fm::MMLChannel::note_no_to_str(int p_note_no) const {
	const std::vector<std::string> pitchnames{
	"c", "c+", "d", "d+", "e", "f", "f+", "g", "g+", "a", "a+", "b"
	};

	if (p_note_no < 0 || p_note_no >= 12)
		throw std::runtime_error(
			std::format("Invalid note number: {} (values must be in the range 0-11)",
				p_note_no));
	else
		return pitchnames[static_cast<std::size_t>(p_note_no)];
}

fm::Duration fm::MMLChannel::resolve_duration(std::optional<int> length,
	int dots, std::optional<int> raw) const {
	if (raw)
		return fm::Duration::from_raw_ticks(raw.value());
	else if (length.has_value() || vm.st.default_length.length.has_value()) {
		// add the default length-dots if the note did not have an explicit musical length
		int base = length.value_or(vm.st.default_length.length.value());
		int final_dots{ length ? dots : dots + vm.st.default_length.dots };
		return Duration::from_length_and_dots(base, final_dots);
	}
	else if (vm.st.default_length.raw.has_value())
		return fm::Duration::from_raw_ticks(vm.st.default_length.raw.value());
	else
		throw std::runtime_error("No note-length available");
}

fm::TieResult fm::MMLChannel::resolve_tie_chain(const NoteEvent& start) {
	int pitch = start.pitch;
	Duration total{ resolve_duration(start.length, start.dots, start.raw) };

	// single note - allow raw ticks
	if (start.tie_to_next == false)
		return fm::TieResult(pitch, total);

	// We assume the VM is currently positioned at 'start'
	while (current_event_is_note_with_tie_next()) {
		step(); // advance VM one event
		const NoteEvent* n = std::get_if<NoteEvent>(current_event());
		if (!n)
			throw std::runtime_error("Tied note not followed by note");
		if (n->raw)
			throw std::runtime_error("Tied note has raw length");
		if (n->pitch != pitch)
			throw std::runtime_error("Tied notes have different pitches");
		total = total + resolve_duration(n->length, n->dots, n->raw);
	}

	return { pitch, total };
}

bool fm::MMLChannel::current_event_is_note_with_tie_next(void) const {
	// Current must be a note
	const NoteEvent* cur = std::get_if<NoteEvent>(current_event());

	if (!cur)
		return false; // Must have tie_next set

	if (!cur->tie_to_next)
		return false; // Peek next event in *execution order*

	return true;
}

byte fm::MMLChannel::ints_to_sq_control(int duty, int envLoop,
	int constVol, int volume) const {
	return static_cast<byte>((duty << 6) | (envLoop << 5) | (constVol << 4) | (volume & 0x0F));
}

byte fm::MMLChannel::int_to_volume_byte(int p_volume) const {
	if (p_volume >= 15)
		return static_cast<byte>(0);
	else if (p_volume < 0)
		return static_cast<byte>(15);
	else
		return static_cast<byte>(15 - p_volume);
}

byte fm::MMLChannel::int_to_byte(int n) const {
	char r{ static_cast<char>(n) };
	return static_cast<byte>(r);
}

int fm::MMLChannel::byte_to_int(byte b) const {
	char r{ static_cast<char>(b) };
	return static_cast<int>(r);
}

int fm::MMLChannel::byte_to_volume(byte b) const {
	if (b >= 15)
		return 0;
	else
		return static_cast<int>(15 - b);
}

std::string fm::MMLChannel::to_string(void) const {
	std::string result{ std::format("{} {{\n", channel_type_to_string()) };
	/*
	const auto LENSTR = [](const fm::DefaultLength& z) -> std::string {
		std::string result;
		if (z.length.has_value())
			result += std::format(" len={} ", z.length.value());
		if (z.raw.has_value())
			result += std::format(" raw={} ", z.raw.value());
		if (z.dots != 0)
			result += std::format(" dots={} ", z.dots);

		result += "\n";
		return result;
		};

	auto lens{ calc_tick_lengths() };
	for (std::size_t i{ 0 }; i < lens.size(); ++i) {
		result += std::format("{}: {}", i, LENSTR(lens[i]));
	}
	*/
	std::optional<int> loopcnt{ std::nullopt };

	for (const auto& ev : events) {

		if (std::holds_alternative<NoteEvent>(ev)) {
			auto& note = std::get<NoteEvent>(ev);

			result += note_no_to_str(note.pitch);
			if (note.raw.has_value())
				result += std::format("#{}", note.raw.value());
			else {
				if (note.length.has_value()) {
					result += std::format("{}", note.length.value());
				}

				for (int i{ 0 }; i < note.dots; ++i)
					result.push_back('.');
			}
			result.push_back(note.tie_to_next ? '&' : ' ');
		}
		else if (std::holds_alternative<PercussionEvent>(ev)) {
			auto& pee = std::get<PercussionEvent>(ev);

			result += std::format("p{}", pee.perc_no);
			if (pee.repeat != 1)
				result += std::format("*{}", pee.repeat);

			result.push_back(' ');
		}
		else if (std::holds_alternative<RestEvent>(ev)) {
			auto& r = std::get<RestEvent>(ev);

			result.push_back('r');

			if (r.raw.has_value())
				result += std::format("#{}", r.raw.value());
			else {
				if (r.length.has_value()) {
					result += std::format("{}", r.length.value());
				}

				for (int i{ 0 }; i < r.dots; ++i)
					result.push_back('.');
			}
			result.push_back(' ');
		}
		else if (std::holds_alternative<OctaveShiftEvent>(ev)) {
			auto& os = std::get<OctaveShiftEvent>(ev);
			if (os.amount > 0)
				result += "> ";
			else
				result += "< ";
		}
		else if (std::holds_alternative<OctaveSetEvent>(ev)) {
			auto& os = std::get<OctaveSetEvent>(ev);
			result += std::format("o{} ", os.octave);
		}
		else if (std::holds_alternative<LengthEvent>(ev)) {
			auto& le = std::get<LengthEvent>(ev);

			if (le.raw.has_value()) {
				result += std::format("l#{}", le.raw.value());
			}
			else if (le.length.has_value()) {
				result += std::format("l{}", le.length.value());
				for (int i{ 0 }; i < le.dots; ++i)
					result += ".";
			}

			result.push_back(' ');
		}
		else if (std::holds_alternative<PushAddrEvent>(ev)) {
			result.push_back('[');
		}
		else if (std::holds_alternative<PopAddrEvent>(ev)) {
			result.push_back(']');
		}
		else if (std::holds_alternative<BeginLoopEvent>(ev)) {
			auto& bl = std::get<BeginLoopEvent>(ev);
			if (loopcnt.has_value())
				throw std::runtime_error("Begin-Loop event starts before the previous loop has ended");
			loopcnt = bl.iterations;
			result.push_back('[');
		}
		else if (std::holds_alternative<EndLoopEvent>(ev)) {
			if (!loopcnt.has_value())
				throw std::runtime_error("End-Loop event does not have a matching Begin-Loop event");

			result += std::format("]{} ", loopcnt.value());
			loopcnt.reset();
		}
		else if (std::holds_alternative<StartEvent>(ev))
			result += std::format("\n!{}\n", c::OPCODE_START);
		else if (std::holds_alternative<EndEvent>(ev))
			result += std::format("\n!{}\n", c::OPCODE_END);
		else if (std::holds_alternative<RestartEvent>(ev))
			result += std::format("\n!{}\n", c::OPCODE_RESTART);
		else if (std::holds_alternative<SongTransposeEvent>(ev)) {
			auto& ste = std::get<SongTransposeEvent>(ev);
			result += std::format("S_{} ", ste.semitones);
		}
		else if (std::holds_alternative<VolumeSetEvent>(ev)) {
			auto& vse = std::get<VolumeSetEvent>(ev);
			result += std::format("v{} ", vse.volume);
		}
		else if (std::holds_alternative<PulseEvent>(ev)) {
			auto& pe = std::get<PulseEvent>(ev);
			result += std::format("\n!{} {} {} {} {}\n", c::OPCODE_PULSE,
				pe.duty_cycle, pe.length_counter, pe.constant_volume, pe.volume_period);
		}
		else if (std::holds_alternative<EnvelopeEvent>(ev)) {
			auto& enve = std::get<EnvelopeEvent>(ev);
			result += std::format("\n!{} {}\n", c::OPCODE_ENVELOPE,
				enve.value);
		}
		else if (std::holds_alternative<ChannelTransposeEvent>(ev)) {
			auto& cte = std::get<ChannelTransposeEvent>(ev);
			result += std::format("_{} ", cte.semitones);
		}
		else if (std::holds_alternative<DetuneEvent>(ev)) {
			auto& dte = std::get<DetuneEvent>(ev);
			result += std::format("\n!{} {}\n", c::OPCODE_DETUNE, dte.value);
		}
		else if (std::holds_alternative<LoopIfEvent>(ev)) {
			auto& lie = std::get<LoopIfEvent>(ev);
			result += std::format("\n!{} {}\n", c::OPCODE_LOOPIF,
				lie.iterations);
		}
		else if (std::holds_alternative<EffectEvent>(ev)) {
			auto& effe = std::get<EffectEvent>(ev);
			result += std::format("\n!{} {}\n", c::OPCODE_EFFECT,
				effe.value);
		}
		else if (std::holds_alternative<JSREvent>(ev)) {
			auto& jsre = std::get<JSREvent>(ev);
			result += std::format("\n!{} {}\n", c::OPCODE_JSR,
				jsre.label_name);
		}
		else if (std::holds_alternative<LabelEvent>(ev)) {
			auto& labe = std::get<LabelEvent>(ev);
			result += std::format("\n{}:\n", labe.name);
		}
		else if (std::holds_alternative<ReturnEvent>(ev)) {
			result += std::format("\n!{}\n", c::OPCODE_RETURN);
		}
		else {
			std::visit([&](auto&& arg) {
				using T = std::decay_t<decltype(arg)>;
				throw std::runtime_error(std::format("Unhandled event type {}", typeid(T).name()));
				},
				ev);
		}
	}

	result += "\n}\n";

	return result;
}

std::string fm::MMLChannel::channel_type_to_string(void) const {
	if (channel_type == fm::ChannelType::sq1)
		return fm::c::CHANNEL_NAMES[0];
	else if (channel_type == fm::ChannelType::sq2)
		return fm::c::CHANNEL_NAMES[1];
	else if (channel_type == fm::ChannelType::tri)
		return fm::c::CHANNEL_NAMES[2];
	else
		return fm::c::CHANNEL_NAMES[3];
}

// ROM bytecode to MML
void fm::MMLChannel::parse_bytecode(const std::vector<fm::MusicInstruction>& instrs,
	std::size_t entrypoint_idx, std::set<std::size_t> jump_targets) {
	const auto NLENGTHS{ calc_tick_lengths() };
	int octave{ 4 };

	const auto reset = [&octave, this]() -> void {
		if (channel_type != fm::ChannelType::noise)
			this->events.push_back(OctaveSetEvent{ octave });
		};

	for (std::size_t i{ 0 }; i < instrs.size(); ++i) {
		if (i == entrypoint_idx) {
			events.push_back(StartEvent{});
			reset();
		}
		if (jump_targets.contains(i)) {
			events.push_back(LabelEvent{ std::format("@label_{}", i) });
			reset();
		}

		const auto& instr{ instrs[i] };
		byte ob{ instr.opcode_byte };
		// length-setting byte
		if (ob >= c::HEX_NOTELENGTH_MIN &&
			ob < c::HEX_NOTELENGTH_END) {
			const auto& leninfo{ (NLENGTHS.at(static_cast<std::size_t>(ob - c::HEX_NOTELENGTH_MIN))) };

			events.push_back(LengthEvent(leninfo.length, leninfo.raw, leninfo.dots));
		}
		// length-setting opcode
		else if (ob == c::MSCRIPT_OPCODE_SET_LENGTH) {
			const auto& leninfo{ (NLENGTHS.at(static_cast<std::size_t>(instr.operand.value()))) };
			events.push_back(LengthEvent(leninfo.length, leninfo.raw, leninfo.dots));
		}
		// rest
		else if (ob == c::HEX_REST) {
			events.push_back(RestEvent{});
		}
		// note
		else if (ob > c::HEX_REST && ob < c::HEX_NOTELENGTH_MIN) {
			// notes for musical channels
			if (channel_type != fm::ChannelType::noise) {

				auto snote{ split_note(ob) };
				if (snote.second != octave) {
					octave = snote.second;
					events.push_back(OctaveSetEvent{ octave });
				}
				events.push_back(NoteEvent{ snote.first });
			}
			// percussion
			else {
				int ptype{ static_cast<int>(ob / 16) };
				int prep{ static_cast<int>(ob % 16) };

				events.push_back(PercussionEvent{ ptype, prep });
			}
		}
		// begin loop
		else if (ob == c::MSCRIPT_OPCODE_BEGIN_LOOP) {
			events.push_back(BeginLoopEvent{ static_cast<int>(instr.operand.value()) });
		}
		// end loop
		else if (ob == c::MSCRIPT_OPCODE_END_LOOP) {
			events.push_back(EndLoopEvent{});
		} // song transpose
		else if (ob == c::MSCRIPT_OPCODE_GLOBAL_TRANSPOSE) {
			events.push_back(SongTransposeEvent{ byte_to_int(instr.operand.value()) });
		}
		else if (ob == c::MSCRIPT_OPCODE_CHANNEL_TRANSPOSE) {
			events.push_back(ChannelTransposeEvent{ byte_to_int(instr.operand.value()) });
		}
		else if (ob == c::MSCRIPT_OPCODE_VOLUME) {
			events.push_back(VolumeSetEvent{ byte_to_volume(instr.operand.value()) });
		}
		else if (ob == c::MSCRIPT_OPCODE_SQ_CONTROL) {
			int ctrl{ static_cast<int>(instr.operand.value()) };
			events.push_back(PulseEvent{
				(ctrl >> 6) & 3,
				(ctrl >> 5) & 1,
				(ctrl >> 4) & 1,
				ctrl & 0x0f
				});
		}
		else if (ob == c::MSCRIPT_OPCODE_SQ_ENVELOPE) {
			events.push_back(EnvelopeEvent{ static_cast<int>(instr.operand.value()) });
		}
		else if (ob == c::MSCRIPT_OPCODE_LOOPIF) {
			events.push_back(LoopIfEvent{ static_cast<int>(instr.operand.value()) });
		}
		else if (ob == c::MSCRIPT_OPCODE_SQ2_DETUNE) {
			events.push_back(DetuneEvent{ static_cast<int>(instr.operand.value()) });
		}
		else if (ob == c::MSCRIPT_OPCODE_END) {
			events.push_back(EndEvent{});
		}
		else if (ob == c::MSCRIPT_OPCODE_SQ_PITCH_EFFECT) {
			events.push_back(EffectEvent{ static_cast<int>(instr.operand.value()) });
		}
		else if (ob == c::MSCRIPT_OPCODE_RESTART) {
			events.push_back(RestartEvent{});
		}
		else if (ob == c::MSCRIPT_OPCODE_RETURN) {
			events.push_back(ReturnEvent{});
		}
		else if (ob == c::MSCRIPT_OPCODE_PUSHADDR) {
			events.push_back(PushAddrEvent{});
		}
		else if (ob == c::MSCRIPT_OPCODE_POPADDR) {
			events.push_back(PopAddrEvent{});
		}
		else if (ob == c::MSCRIPT_OPCODE_NOP) {
			events.push_back(NOPEvent{});
		}
		else if (ob == c::MSCRIPT_OPCODE_JSR) {
			events.push_back(JSREvent{ std::format("@label_{}", instr.jump_target.value()) });
		}
		else
			throw std::runtime_error(std::format("Unhandled opcode {:02x}", ob));
	}

}

// calculate all mappings from byte vals 0-255 to a length string
// we don't try to calculate tempo changes in the bytecode songs
// so this will be fixed
std::vector<fm::DefaultLength> fm::MMLChannel::calc_tick_lengths(void) const {
	std::vector<fm::DefaultLength> result;
	for (int i{ 0 }; i < 256; ++i)
		result.push_back(fm::DefaultLength(std::nullopt, i, 0));

	int qnote_ticks = (*bpm * song_tempo.get_den()) / song_tempo.get_num();
	int whole_note_ticks = 4 * qnote_ticks;

	for (int i{ 0 }; i < 256; ++i) {
		Fraction r(i, whole_note_ticks);

		if (c::ALLOWED_FRACTIONS.contains(r)) {

			// Base: 1/N
			if (r.get_num() == 1) {
				result[i] = fm::DefaultLength(r.get_den(), std::nullopt, 0);
			}

			// Dotted: 3/(2N)
			else if (r.get_num() == 3 && (r.get_den() % 2 == 0)) {
				result[i] = fm::DefaultLength(r.get_den() / 2, std::nullopt, 1);
			}

			// Dotted: 1/(3N)
			else if (r.get_num() == 1 && (r.get_den() % 3 == 0)) {
				result[i] = fm::DefaultLength(r.get_den() / 3, std::nullopt, 0);
			}
		}
	}

	return result;
}

int fm::MMLChannel::add_midi_track(smf::MidiFile& p_midi, int p_pitch_offset,
	int p_max_ticks) {
	const std::vector<int> duty_cycle_to_instr{ 80, 80, 81, 62, 64, 64, 64, 64 };
	const std::vector<int> perc_note_no{ 0, 36, 38, 42, 0, 70, 70, 0 };
	bool is_perc{ channel_type == fm::ChannelType::noise };

	p_midi.addTrack();
	int l_track_no{ p_midi.getTrackCount() - 1 };
	int l_channel_no{ is_perc ? 9 : 0 };

	std::set<VMState> vmstates;

	int default_midi_volume{ 100 };
	if (channel_type == fm::ChannelType::tri)
		default_midi_volume = 70;
	else if (channel_type == fm::ChannelType::noise)
		default_midi_volume = 75;

	p_midi.addController(l_track_no, 0, l_channel_no, 7,
		default_midi_volume
	);

	p_midi.addTimbre(l_track_no, 0, l_channel_no,
		channel_type == fm::ChannelType::tri ? 33 :
		duty_cycle_to_instr.at(vm.st.duty_cycle)
	);

	int ticks{ 1 };
	reset_vm(true);

	// TODO: Find a better solution
	int guard_count{ 0 };

	while (vm.pc < events.size()) {
		++guard_count;
		if (guard_count > 20000)
			break;

		VMState state{ vm.pc,
		vm.loop_count, vm.loop_iters, vm.st.pitchoffset, vm.st.volume, vm.st.duty_cycle,
		vm.push_addr, vm.jsr_addr, vm.loop_addr, vm.loop_end_addr };

		if (vmstates.contains(state))
			break;
		else
			vmstates.insert(state);

		const auto& ev{ events[vm.pc] };

		if (std::holds_alternative<NoteEvent>(ev)) {
			auto& note = std::get<NoteEvent>(ev);
			auto tickdur = tick_length(resolve_duration(note.length, note.dots, note.raw));

			p_midi.addNoteOn(l_track_no, ticks, l_channel_no,
				12 * (vm.st.octave + 1) + note.pitch + p_pitch_offset + vm.st.pitchoffset, (default_midi_volume * vm.st.volume) / 15);
			ticks += tickdur.whole;
			p_midi.addNoteOff(l_track_no, ticks, l_channel_no,
				12 * (vm.st.octave + 1) + note.pitch + p_pitch_offset + vm.st.pitchoffset, 0);
		}
		else if (std::holds_alternative<PercussionEvent>(ev)) {
			auto& perce = std::get<PercussionEvent>(ev);
			auto tickdur = tick_length(resolve_duration(vm.st.default_length.length,
				vm.st.default_length.dots, vm.st.default_length.raw));

			int reps{ perce.repeat == 0 ? 255 : perce.repeat };
			bool l_end_channel{ false };

			for (int rep{ 0 }; rep < reps; ++rep) {
				p_midi.addNoteOn(l_track_no, ticks, l_channel_no,
					perc_note_no.at(perce.perc_no), default_midi_volume);
				ticks += tickdur.whole;

				p_midi.addNoteOff(l_track_no, ticks, l_channel_no,
					perc_note_no.at(perce.perc_no), 0);

				if (ticks > p_max_ticks) {
					l_end_channel = true;
					break;
				}
			}

			if (l_end_channel)
				break;
		}
		else if (std::holds_alternative<LengthEvent>(ev)) {
			auto& le = std::get<LengthEvent>(ev);

			if (le.raw.has_value()) {
				vm.st.default_length = { std::nullopt, le.raw.value(), 0 };
			}
			else if (le.length.has_value()) {
				vm.st.default_length = { le.length.value(), std::nullopt, le.dots };
			}
			else
				throw std::runtime_error("Malformed length event");
		}
		else if (std::holds_alternative<RestEvent>(ev)) {
			auto& re = std::get<RestEvent>(ev);

			auto tickdur = tick_length(resolve_duration(re.length, re.dots, re.raw));
			ticks += tickdur.whole;
		}
		else if (std::holds_alternative<OctaveSetEvent>(ev)) {
			auto& se = std::get<OctaveSetEvent>(ev);
			vm.st.octave = se.octave;
		}
		else if (std::holds_alternative<OctaveShiftEvent>(ev)) {
			auto& ssh = std::get<OctaveShiftEvent>(ev);
			vm.st.octave += ssh.amount;
		}
		else if (std::holds_alternative<ChannelTransposeEvent>(ev)) {
			auto& cte = std::get<ChannelTransposeEvent>(ev);
			vm.st.pitchoffset = cte.semitones;
		}
		else if (std::holds_alternative<VolumeSetEvent>(ev)) {
			auto& vse = std::get<VolumeSetEvent>(ev);
			vm.st.volume = vse.volume;
		}
		else if (std::holds_alternative<PulseEvent>(ev)) {
			auto& pe = std::get<PulseEvent>(ev);
			vm.st.duty_cycle = pe.duty_cycle;
			p_midi.addTimbre(l_track_no, ticks, 0, duty_cycle_to_instr.at(vm.st.duty_cycle));
		}

		if (std::holds_alternative<EndEvent>(ev) ||
			std::holds_alternative<RestartEvent>(ev))
			break;
		else if (std::holds_alternative<JSREvent>(ev)) {
			auto& labe = std::get<JSREvent>(ev);
			if (!vm.jsr_addr.has_value())
				vm.jsr_addr = vm.pc;
			else
				throw std::runtime_error("Invoking JSR with JSR-address already on the stack");
			vm.pc = get_label_addr(labe.label_name);
		}
		else if (std::holds_alternative<ReturnEvent>(ev)) {
			vm.pc = vm.jsr_addr.value() + 1;
			vm.jsr_addr.reset();
		}
		else if (std::holds_alternative<PushAddrEvent>(ev)) {
			vm.push_addr = vm.pc++;
		}
		else if (std::holds_alternative<PopAddrEvent>(ev)) {
			vm.pc = vm.push_addr.value() + 1;
			//vm.push_addr.reset();
		}
		else if (std::holds_alternative<BeginLoopEvent>(ev)) {
			auto& ble = std::get<BeginLoopEvent>(ev);
			vm.loop_addr = vm.pc++;
			vm.loop_iters = ble.iterations;
			vm.loop_count = 0;
		}
		else if (std::holds_alternative<EndLoopEvent>(ev)) {
			auto& ele = std::get<EndLoopEvent>(ev);
			vm.loop_end_addr = vm.pc;
			vm.loop_count++;
			if (vm.loop_count < vm.loop_iters)
				vm.pc = vm.loop_addr.value() + 1;
			else
				++vm.pc;
		}
		else if (std::holds_alternative<LoopIfEvent>(ev)) {
			auto& lie = std::get<LoopIfEvent>(ev);
			if (vm.loop_count >= lie.iterations)
				vm.pc = vm.loop_end_addr.value() + 1;
			else
				++vm.pc;
		}
		else
			++vm.pc;
	}

	return ticks;
}

bool fm::VMState::operator<(const fm::VMState& rhs) const {
	return std::tie(pc, loop_count, loop_iters, pitch_offset, volume, duty_cycle,
		push_addr, jsr_addr, loop_addr, loop_end_addr) <
		std::tie(rhs.pc, rhs.loop_count, rhs.loop_iters, rhs.pitch_offset, rhs.volume, rhs.duty_cycle,
			rhs.push_addr, rhs.jsr_addr, rhs.loop_addr, rhs.loop_end_addr);
}
