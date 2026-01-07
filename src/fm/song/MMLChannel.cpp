#include "MMLChannel.h"
#include "./../fm_constants.h"
#include "mml_constants.h"
#include <format>
#include <numeric>
#include <stack>
#include <stdexcept>
#include <tuple>

using byte = unsigned char;

fm::MMLChannel::MMLChannel(int p_song_tempo, int* p_bpm) :
	bpm{ p_bpm }
{
	reset_vm();
	set_qnote_length_from_tempo(p_song_tempo);
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

void fm::MMLChannel::set_qnote_length_from_tempo(int tempo) {
	vm.qnote_length = fm::Fraction(*bpm, tempo);
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
			set_qnote_length_from_tempo(r->tempo);
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
	// total = qnote_length * (dur.musical * 4)
	Fraction total = vm.qnote_length * (dur.musical * Fraction(4, 1));

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

int fm::MMLChannel::byte_to_unsigned_int(byte b) const {
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
	std::string result{ std::format("#{} {{\n", this->name) };
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

// ROM bytecode to MML
void fm::MMLChannel::parse_bytecode(const std::vector<fm::MusicInstruction>& instrs,
	std::size_t entrypoint_idx, std::set<std::size_t> jump_targets) {
	const auto NLENGTHS{ calc_tick_lengths() };
	int octave{ 4 };

	const auto reset = [&octave, this]() -> void {
		octave = 4;
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
			auto snote{ split_note(ob) };
			if (snote.second != octave) {
				octave = snote.second;
				events.push_back(OctaveSetEvent{ octave });
			}
			events.push_back(NoteEvent{ snote.first });
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
			events.push_back(SongTransposeEvent{ byte_to_unsigned_int(instr.operand.value()) });
		}
		else if (ob == c::MSCRIPT_OPCODE_CHANNEL_TRANSPOSE) {
			events.push_back(ChannelTransposeEvent{ byte_to_unsigned_int(instr.operand.value()) });
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

	auto qntmp{ vm.qnote_length };
	int wholenotelength{ 4 * qntmp.extract_whole() };

	for (int i{ 0 }; i < 256; ++i) {
		Fraction r(i, wholenotelength);

		if (c::ALLOWED_FRACTIONS.contains(r)) {

			// Base: 1/N
			if (r.get_num() == 1) {
				result[i] = fm::DefaultLength(r.get_den(), std::nullopt, 0);
			}

			// Dotted: 3/(2N)
			if (r.get_num() == 3 && (r.get_den() % 2 == 0)) {
				result[i] = fm::DefaultLength(r.get_den() / 2, std::nullopt, 1);
			}

			// Dotted: 1/(3N)
			if (r.get_num() == 1 && (r.get_den() % 3 == 0)) {
				result[i] = fm::DefaultLength(r.get_den() / 3, std::nullopt, 0);
			}
		}
	}

	return result;
}

void fm::MMLChannel::add_midi_track(smf::MidiFile& p_midi, int p_pitch_offset) {
	const std::vector<int> duty_cycle_to_instr{ 80, 81, 62, 64 };

	p_midi.addTrack();
	int l_track_no{ p_midi.getTrackCount() - 1 };
	int l_channel_no{ 0 };

	std::set<VMState> vmstates;

	// auto xnote{ vm.qnote_length };
	// auto xtempo{ *bpm / xnote.extract_whole() };

	// p_midi.addTempo(l_track_no, 0, 1000000);
	p_midi.addController(l_track_no, 0, 0, 7, 100);

	p_midi.addTimbre(l_track_no, 0, 0,
		name == "TRI" ? 33 :
		duty_cycle_to_instr.at(vm.st.duty_cycle)
	);

	int ticks{ 0 };
	reset_vm(true);

	// DEBUG: TODO REMOVE
	int debug{ 0 };

	while (vm.pc < events.size()) {
		++debug;
		if (debug > 20000)
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
				12 * (vm.st.octave + 1) + note.pitch + p_pitch_offset + vm.st.pitchoffset, (100 * vm.st.volume) / 15);
			ticks += tickdur.whole;
			p_midi.addNoteOff(l_track_no, ticks, l_channel_no,
				12 * (vm.st.octave + 1) + note.pitch + p_pitch_offset + vm.st.pitchoffset, 0);
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
			vm.st.pitchoffset += cte.semitones;
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
			if (vm.loop_count > lie.iterations)
				vm.pc = vm.loop_end_addr.value();
			else
				++vm.pc;
		}
		else
			++vm.pc;
	}
}

bool fm::VMState::operator<(const fm::VMState& rhs) const {
	return std::tie(pc, loop_count, loop_iters, pitch_offset, volume, duty_cycle,
		push_addr, jsr_addr, loop_addr, loop_end_addr) <
		std::tie(rhs.pc, rhs.loop_count, rhs.loop_iters, rhs.pitch_offset, rhs.volume, rhs.duty_cycle,
			rhs.push_addr, rhs.jsr_addr, rhs.loop_addr, rhs.loop_end_addr);
}
