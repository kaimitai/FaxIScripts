
# Introduction to MML (Music Macro Language)

Creating music for **Faxanadu (NES)** presents a unique challenge: the original game uses raw bytecode tailored to a custom music engine to define music sequences, which is difficult and unintuitive for composers. Our goal is to **provide a layer of abstraction** that allows composers to write music using **MML** - a notation-based format familiar to many chiptune composers. This approach makes music creation more accessible and expressive while preserving full compatibility with the NES sound hardware.

The **MML compiler** translates human-readable musical notation into the **bytecode format** required by Faxanadu’s sound engine. This enables composers to focus on musical ideas rather than low-level implementation details.

The descriptions of the Faxanadu music engine opcodes and their arguments in this document are based on the best information currently available from reverse‑engineering the original engine. While every effort has been made to document them accurately, some behaviors may still be incomplete, ambiguous, or engine‑specific in ways that are not yet fully understood.

Composers are encouraged to **experiment freely**:

- Try unusual argument values  
- Combine opcodes in creative ways  
- Explore how the engine responds in different musical contexts  
- Share discoveries so the documentation can continue to improve  

Faxanadu’s sound engine has many quirks, and part of the fun is uncovering them together. If you find behavior that differs from what’s written here, please treat it as an opportunity to refine and expand our collective understanding.

This tool would not exist without **Christian Hammond’s reverse engineering of Faxanadu’s music engine**, which provided the foundation for understanding how the original system works. His disassembly and research can be found here:  
  [https://chipx86.com/faxanadu/](https://chipx86.com/faxanadu/)

---

### Table of Contents

<!--TOC-->
  - [The NES Audio Channels](#the-nes-audio-channels)
  - [Timing & Tempo: Making It Simple for Composers](#timing-tempo-making-it-simple-for-composers)
  - [Fractional Drift & Loops: Why Tempo Choice Matters](#fractional-drift-loops-why-tempo-choice-matters)
  - [CLI Commands for MML Extraction and Injection](#cli-commands-for-mml-extraction-and-injection)
    - [**Extract MML from ROM**](#extract-mml-from-rom)
    - [**Compile and Inject MML into ROM**](#compile-and-inject-mml-into-rom)
    - [**Extract Songs as MIDI from ROM**](#extract-songs-as-midi-from-rom)
    - [**Convert MML to MIDI**](#convert-mml-to-midi)
  - [MML Syntax Basics](#mml-syntax-basics)
  - [MML File Structure](#mml-file-structure)
    - [Song Header](#song-header)
    - [Channels](#channels)
    - [Minimal Example](#minimal-example)
  - [Musical Notation in MML](#musical-notation-in-mml)
    - [Lengths: Musical vs. Raw](#lengths-musical-vs.-raw)
    - [Tied Notes](#tied-notes)
    - [Octave Control](#octave-control)
    - [Percussion Notes (noise channel)](#percussion-notes-noise-channel)
    - [Loops](#loops)
    - [Song Transpose](#song-transpose)
    - [Channel Transpose](#channel-transpose)
    - [Volume Control](#volume-control)
    - [Control-Flow Opcodes](#control-flow-opcodes)
      - [Subroutines](#subroutines)
      - [Conditional Loop Exit: `!endloopif`](#conditional-loop-exit-endloopif)
    - [Effect Opcodes](#effect-opcodes)
      - [SQ2 Detune: `!detune <n>`](#sq2-detune-detune-n)
      - [SQ2 Pitch Effect: `!effect <n>`](#sq2-pitch-effect-effect-n)
      - [Envelope Mode: `!envelope <n>`](#envelope-mode-envelope-n)
      - [Square Wave Channel Control: `!pulse`](#square-wave-channel-control-pulse)
- [Tempo, Tick Math, and Choosing Safe Tempos](#tempo-tick-math-and-choosing-safe-tempos)
  - [Quarter‑notes, ticks, and how tempo works](#quarternotes-ticks-and-how-tempo-works)
  - [Fractional drift and the fractional accumulator](#fractional-drift-and-the-fractional-accumulator)
    - [Correcting drift manually](#correcting-drift-manually)
- [Tempo Coverage Table](#tempo-coverage-table)
- [ROM to MML](#mml-extracted-from-rom)
- [🎧 MIDI Output and How the Playback VM Works](#midi-output-and-how-the-playback-vm-works)
- [Example mml music](#example-mml-song)
<!--/TOC-->

---

## The NES Audio Channels

The NES sound hardware provides **five channels**, but Faxanadu uses **four** for music:

1. **Square Wave 1 (SQ1)**  
   - Primary melodic channel  
   - Supports **duty cycles**, **volume envelopes**, and **pitch bends**  
   - Often used for lead melodies  

2. **Square Wave 2 (SQ2)**  
   - Secondary melodic channel  
   - Similar capabilities to SQ1  
   - Commonly used for harmony or counter-melody  

3. **Triangle Wave (TRI)**  
   - Produces a fixed-volume triangle waveform  
   - No volume control, but supports pitch changes  
   - Typically used for bass lines  

4. **Noise Channel**  
   - Generates pseudo-random noise patterns  
   - Used for percussion (e.g., snare, hi-hats)

*(The NES also has a DPCM channel for sample playback, but Faxanadu does not use it.)*

---



## Timing & Tempo: Making It Simple for Composers

The original Faxanadu music engine is very low-level:  
- It doesn’t know what a “quarter note” or “tempo” is.  
- It only understands **ticks** (tiny time units) and a command called `set-length` that tells the engine how long each note or rest lasts.  
- The engine runs at **60 ticks per second** on NTSC (or **50 ticks per second** on PAL).

That means the original system is basically saying:  
> “Play this note for 30 ticks, then the next for 15 ticks…”  

Not very musical, right?  

---

### What We Changed

In MML, you don’t need to think about ticks at all. Instead, you write music like you normally would:  
- **Set a tempo** (e.g., `t120` for 120 BPM)  
- Use **musical lengths** (quarter notes, dotted halves, triplets, etc.)  

The compiler does the math behind the scenes:  
- It figures out how many ticks each note should last based on your tempo.  
- It inserts the right `set-length` commands automatically.  

So you can focus on **music**, not math.

---

### Example

```mml
t120
l4 c d e f    ; Four quarter notes
l8 g a b c    ; Four eighth notes
```

At 120 BPM:  
- A quarter note lasts **½ second**  
- The compiler converts that to **30 ticks**
- You never have to worry about those numbers - the compiler handles it.

---

### Why This Matters

- **Composers think in beats, not ticks.**  
- MML gives you a familiar musical language.  
- The compiler ensures everything lines, even for dotted notes, triplets, and odd note fractions.

---


## Fractional Drift & Loops: Why Tempo Choice Matters

Behind the scenes, every note length is converted into **ticks**, and ticks are whole numbers.  
When you use **loops** or **subroutines (jsr)** in MML, the compiler calculates tick lengths for each note and rounds them to the nearest integer.  

This works fine for most music, but there’s a catch:  
- If your tempo produces **fractional tick values** (e.g., 12.5 ticks for an eighth note), the compiler rounds them.  
- Over time, these tiny rounding differences can **add up** when the same pattern repeats many times.  
- This is called **fractional drift**—your loop might end slightly early or late compared to other channels.

---

### Why Can’t the Compiler Fix This Automatically?

The compiler does its best:
- It uses an **error accumulator** to spread rounding differences evenly (e.g., alternating 12 and 13 ticks for eighth notes).
- This keeps measures aligned **most of the time**.

But if you loop a section hundreds of times, even small rounding errors can accumulate.  
The engine itself doesn’t know about measures or beats—it just plays ticks.

---

### How to Avoid Drift

Choose tempos that produce **clean tick values** for common note lengths.  
For example:
- At **120 BPM (NTSC)**, a quarter note = 30 ticks (perfect integer).
- At **100 BPM (NTSC)**, a quarter note = 36 ticks (also clean).
- At **125 BPM**, a quarter note = 28.8 ticks (fractional → rounding needed).

We’ll provide a **recommended tempo chart** later in this documentation so you can pick tempos that minimize and eliminate drift.

---

### TL;DR for Composers

- Loops and subroutines repeat set patterns many times, so rounding errors can accumulate.
- Stick to recommended tempos for best results.
- If you use unusual tempos, expect tiny timing differences after long loops—but usually nothing audible unless the loop runs for a very long time.



---

## CLI Commands for MML Extraction and Injection

The `FaxIScripts` CLI application provides several commands to work with MML files and the Faxanadu ROM:

---

### **Extract MML from ROM**
```bash
faxiscripts xmml "faxanadu (u).nes" "faxanadu.mml"
```
**Description:**  
Extracts the music layer from `faxanadu (u).nes` and saves it as an MML file named `faxanadu.mml`.

---

### **Compile and Inject MML into ROM**
```bash
faxiscripts bmml "faxanadu.mml" "faxanadu.nes" -s "faxanadu (u).nes"
```
**Description:**  
Compiles and injects the music layer from `faxanadu.mml` into `faxanadu.nes`.  
- The `-s` option specifies a source ROM (`faxanadu (u).nes`) for reference.  
- If no source ROM is provided, the output file will be patched directly.

---

### **Extract Songs as MIDI from ROM**
```bash
faxiscripts r2m "faxanadu (u).nes" faxanadu
```
**Description:**  
Extracts all songs from `faxanadu (u).nes` into MIDI files named:  
`faxanadu-01.mid` up to `faxanadu-16.mid`.

---

### **Convert MML to MIDI**
```bash
faxiscripts m2m faxanadu.mml faxanadu
```
**Description:**  
Translates all songs in `faxanadu.mml` into MIDI files named:  
`faxanadu-01.mid` up to `faxanadu-16.mid` (depending on the number of songs defined in the MML).  
This command is **independent of any ROM file** and can be used as a general MML renderer.

---


## MML Syntax Basics

When you extract a ROM to MML using `faxiscripts xmml`, the top of the file will include comments like:

```mml
; global transpose for channel sq1: -12 semitones
; global transpose for channel sq2: -12 semitones
; global transpose for channel tri: 12 semitones
; global transpose for channel noise: 127 semitones
```

### What This Means
- These are **global transpositions** applied by the Faxanadu music engine and stored in the ROM.
- **SQ1 and SQ2**: Tuned **down one octave** (`-12 semitones`).  
  - If you write `C` in **octave 4**, the engine plays `C` in **octave 3**.
- **Triangle (TRI)**: Tuned **up one octave** (`+12 semitones`).  
  - `octave 4 C` in MML becomes `octave 5 C` in the engine.
- **Noise**: Has a transpose value (`127 semitones`), likely an unused sentinel.
- Anything after `;` on any line is a **comment** and ignored by the compiler.

---

### Key Points for Composers
- These transpositions are **automatic**; you don’t need to adjust your notation but you need to be aware of them.
- Just write music normally in MML—octaves and notes as you expect.
- Comments (`;`) are safe to add anywhere for your own notes.

---


## MML File Structure

The MML is a **list of songs**, each with **four channels**. A song starts with its header, followed by channel blocks.

### Song Header
- `#song <song number>` — identifies the song.
- `t<tempo>` — default tempo for the whole song (quarter notes per minute).  
  Can be overridden by per-channel tempo commands inside the channel blocks.

### Channels
Four channel blocks follow, each enclosed in `{ }`:
- `#sq1 { ... }`
- `#sq2 { ... }`
- `#tri { ... }`
- `#noise { ... }`

### Minimal Example
```mml
#song 1
t120

#sq1 {
!end
}

#sq2 {
!end
}

#tri {
!end
}

#noise {
!end
}
```

**Notes:**
- The tempo in `t<tempo>` applies to the entire song unless overridden inside a channel.


## Musical Notation in MML

- **Notes:** Use letters `c d e f g a b`.  
  - Sharps: `+` or `#` (e.g., `c+` or `c#`)  
  - Flats: `-` (e.g., `d-`)

- **Rests:** Use `r`.

### Lengths: Musical vs. Raw

- **Musical lengths**:  
  - `l<n>` sets the default note and rest length in musical terms (quarter, eighth, etc.).  
    Example:
    ```mml
    l8 c d e    ; same as c8 d8 e8
    ```
  - Dotted notes: add a dot after the length.  
    Example:
    ```mml
    l4. c       ; dotted quarter note
    ```

- **Raw lengths**:  
  - Use `l~<ticks>` to set the length in **raw ticks**, bypassing tempo calculations.  
    Example:
    ```mml
    l~4 c       ; note lasts exactly 4 ticks
    ```
  - This is useful for precise timing or effects that don’t depend on BPM. Since Faxanadu allows 254 different note lengths, we can't fit them all on a musical grid necessarily. When extracting ROM to MML for example, the decompiler does its best to infer a tempo that puts as many tick lengths as possible on a musical grid, but for some cases it will not be possible - and in these cases we use ~ for raw tick-lengths. They are best avoided by composers in most cases, since they bypass tempo calculations.

**Summary:**  
- `l<n>` → musical length (tempo-based).  
- `l<n>.` → dotted musical length.  
- `l~<ticks>` → raw tick length (absolute timing).


**Summary:**  
- `l<n>` sets the default length for subsequent notes and rests.  
- Individual notes and rests can override the length by appending a number and optional dots.



### Tied Notes

- Use `&` to tie two notes together:
  ```mml
  a4 & a8
  ```
  This means the note `A` lasts for the combined length of a **quarter note + eighth note**.

- The compiler will **collapse ties** into a single note:
  ```mml
  a4 & a8  →  a4.   ; dotted quarter
  ```

#### Rules for Ties
- **Pitch must match** (e.g., `a4 & a8` is valid, but `a4 & b8` is not).
- **Notes must be consecutive** — nothing can come between them.
- Ties work across **length changes** (e.g., `a8 & a16` combines both lengths).

---

**Why use ties?**
- Useful for writing sustained notes that span irregular lengths.
- The compiler optimizes ties into the shortest representation when possible.

### Octave Control

- **Set octave:**  
  ```mml
  o<n>
  ```
  Example:
  ```mml
  o4    ; sets current octave to 4
  ```

- **Shift octave:**  
  - `>` increases the octave by 1  
  - `<` decreases the octave by 1  
  Example:
  ```mml
  o4 c d > e f    ; e and f will play in octave 5
  ```

---

#### Important Note
Octaves are **not part of the Faxanadu engine**. The engine uses **absolute note values**, not octaves.  
We include `o<n>`, `>` and `<` in MML for consistency with standard syntax, but when compiling, these are **collapsed into concrete note numbers**.

**Implication:**  
- In loops or subroutines (`jsr`), the octave cannot change dynamically between iterations because the engine only sees absolute notes.
- **Good practice:** Always set the octave explicitly at the start of each loop or subroutine target:
  ```mml
  [o4 c d e f]2    ; loop starts with explicit octave
  ```

---

### Percussion Notes (noise channel)

Percussion works a little differently from melodic channels:

- **Length is shared:**  
  Individual percussion hits cannot have their own length.  
  Instead, set the lengths using `l<n>` (just like normal notes), then write percussion notes and rests.

- **Percussion notes:**  
  `p0, p1, p2, p3, p4, p5, p6 and p7`  
  (In Faxanadu’s original data, only `p1`, `p2`, and `p3` are used. Perhaps we can think of p1, p2 and p3 as kick, snare and hi-hat)

- **Repeats:**  
  You can repeat a percussion note by adding `*<count>` after it:  
  - `p0*5` → plays `p0` five times  
  - Valid repeat counts: `1–15 and 256`
  - If no repeat is given, the note plays once.

- **Rests:**  
  Use `r` for rests between percussion hits.

**Example:**
```mml
l4 p1 p2 p3 r p2*4
```

This sets length = quarter note, then plays p1, p2, p3, a rest, and p2 four times.

---

### Loops

- Loops are enclosed in square brackets `[...]` followed by an optional repeat count:
  ```mml
  [o4 c d > e f ]4    ; repeat the sequence 4 times
  [o4 c d > e f ]     ; infinite loop
  ```

#### Rules
- If **no counter** is given, the loop is **infinite**.
- Loops **cannot be nested**, except when one is infinite and the other is finite.  
  - This is because under the hood, infinite loops are modeled using `push address` and `pop address and jump`, which behave differently from finite loops.

---


### Song Transpose

- Use `S_<n>` to transpose the entire song by **n semitones** (can be negative):
  ```mml
  S_-12    ; transpose down one octave
  S_5      ; transpose up 5 semitones
  ```

#### Details
- This command affects **all melodic channels** in the song.
- It is **sparingly used** in the original game data.
- Recommended placement: **early in the SQ1 channel**, so it’s clear and applied before any notes.
- Example:
  ```mml
  #sq1 {
    S_-12
    l4 o4 c d e f
    !end
  }
  ```

**Note:**  
This is a global transpose for the song, separate from the ROM’s built-in global transpositions (shown in comments when extracting).


### Channel Transpose

- Use `_<n>` to transpose the **current channel** by **n semitones** (can be negative):
  ```mml
  _-12    ; transpose this channel down one octave
  _5      ; transpose this channel up 5 semitones
  ```

#### Details
- Unlike `S_<n>` (song transpose), this only affects the **channel where it appears**.
- Can be placed anywhere in the channel block, and used in loops to transpose up and down, for example.
- Example:
  ```mml
  #sq2 {
    [o4 c d _-12 e f _0]5
    !end
  }


### Volume Control

- Use `v<n>` to set the **volume** for the current channel:
  ```mml
  v8    ; set volume to 8
  v15   ; maximum volume
  v0    ; silent
  ```

#### Details
- Volume applies **only to square wave channels** (`#sq1` and `#sq2`).
- Valid range:  
  - `0` = silent  
  - `15` = maximum volume
- Example:
  ```mml
  #sq1 {
    v12
    o4 l4 c d e f
  }
  ```


### Control-Flow Opcodes

These special commands control the flow of music in a channel:

- **`!start`**  
  Marks the **entry point** for the music in the current channel.  
  - If omitted, the entry point is assumed to be the **start of the channel block**.  
  - Reason for existence: It’s theoretically possible to have a subroutine with a return before the entry point.
  ```mml
  !start
  ```

- **`!end`**  
  Ends the music for the channel.  
  - **Must be present** if the control flow would otherwise spill out of the channel block. Can be omitted if a channel is in an infinite loop or ends with !restart.
  ```mml
  !end
  ```

- **`!restart`**  
  Restarts the channel from its entry point.  
  - Needed for looping entire songs and their channels.
  ```mml
  !restart
  ```

---

### Subroutines

- **Label definition**  
  Define a label with `@label:` (note the trailing colon):
  ```mml
  @intro:
  ```
  - Labels mark a target location for subroutine calls.

- **Jump to subroutine**  
  Use `!jsr @label` to jump to a label (label name **without** the colon):
  ```mml
  !jsr @intro
  ```
  - Execution continues at `@intro:` until a return.

- **Return from subroutine**  
  Use `!return` to return to the command **after the last `!jsr`**:
  ```mml
  !return
  ```

#### Rule/Constraint
- You **should not** jump to a subroutine **from another subroutine**.  
  - Only **one** return address can be on the stack at any time.
- You can not jump to a label within an other channel block - all channels are self-contained.


### Example: Using Subroutines

```mml
#sq1 {
  !start
  o4 l4
  c d e f
  !jsr @intro    ; jump to subroutine
  g a b
  !end

  @intro:        ; subroutine definition
  o4
  c e g
  !return        ; return to after !jsr
}
```

**Explanation:**
- The main sequence plays `c d e f`, then jumps to `@intro`.
- The subroutine plays `c e g` and returns.
- After returning, the channel continues with `g a b` before ending.


### Conditional Loop Exit: `!endloopif`

`!endloopif <n>` exists a loop if &lt;n&gt; iterations have passed.  
When execution reaches this opcode, it **checks how many iterations have already completed**; if **`<n>` iterations have passed**, it **jumps to the end of the current loop** and continues from there.

#### Correct Usage Example
```mml
#sq1 {
  o4 l4
  ; Repeat the pattern 4 times, but skip the remainder of the last pass:
  [ 
    c d e f
    !endloopif 3   ; When entering the 4th pass (after 3 completed), jump to loop end
    g a             ; Only plays on passes 1–3
  ]4

  !end
}
```

**What happens here:**
- Pass 1: `c d e f`, `!endloopif 3` (condition not met), `g a` plays.
- Pass 2: same as Pass 1.
- Pass 3: same as Pass 1.
- Pass 4: `c d e f`, `!endloopif 3` **triggers** (3 passes have completed), execution **jumps to the end of the loop**, and **`g a` is skipped**. Control continues **after** the loop.

---

#### Rules & Warnings
- Place `!endloopif` **inside the loop**—it evaluates when the opcode is reached.
- It is **only useful** for ending a loop **during its last iteration** (e.g., in a `]4` loop, use `!endloopif 3`).
- Do **not** use it before the loop-end position has been seen at least once (i.e., don’t rely on it in the very first pass).
- Triggering it **after a loop has finished** will cause the **program counter to jump backward**, effectively creating an **infinite loop**.

**Summary:**  
Use `!endloopif <n>` to cleanly skip the tail of a loop during the final pass. Keep it inside the loop body, and select `<n>` as **(repeat count − 1)** for predictable behavior.

### Effect Opcodes


### SQ2 Detune: `!detune <n>`

Applies a **SQ2-only** pitch bias (detune). This subtracts a small value from the **low byte of the SQ2 timer period** before writing to the APU. The value is compared against the computed timer low byte and **clamped** to avoid underflow, producing a **subtle upward pitch shift** when non‑zero.

```mml
#sq2 {
  !start
  o4 l8 c d e f

  !detune 2     ; slight upward detune on SQ2 only
  c d e f

  !detune 0     ; turn off detune
  g a b c
  !end
}
```

**Details**
- **Channel:** Only affects **SQ2**.
- **Effect:** Small upward pitch shift by biasing the **timer low byte** post–note calculation, pre-APU write.
- **Timing:** Not related to note length or envelopes; it’s a **pitch-only** modification.
- **Use case:** Simple **chorus/harmony**-like effect on SQ2.
- **Disable:** `!detune 0` disables the effect.
- **Range:** `n` is an integer; nonzero values increase the bias. (Exact safe range depends on engine clamping—use modest values.)


### SQ2 Pitch Effect: `!effect <n>`

Applies a **special pitch modulation effect** to **SQ2 only**.  
This opcode is extremely rare in the original Faxanadu data (only 3 uses total).

```mml
#sq2 {
  !start
  o4 l8 c d e f

  !effect 2    ; enable subtle pitch modulation
  g a b c

  !effect 0    ; turn off effect
  !end
}
```

#### What It Does
- Introduces a **delayed pitch modulation** after the envelope phase reaches a certain point.

#### Practical Notes
- `!effect 0` disables the effect.
- Non-zero values (e.g., `2`) define the **depth mask**, controlling how far the pitch can shift.
- This is **not related to note length or envelopes**—it’s a **pitch-only modulation** applied after note calculation.

**Summary:**  
`!effect <n>` adds a subtle pitch modulation to SQ2, likely for a vibrato effect.  
Values:
- `0` = off
- `2` = light modulation (as seen in original data)


### Envelope Mode: `!envelope <n>`

Sets the **envelope mode** for **square wave channels** (`#sq1` and `#sq2` only).  
This opcode is **always present at the start of SQ1 and SQ2 channels** in the original game data.

```mml
#sq1 {
  !start
  !envelope 0    ; volume decay
  o4 l8 c d e f

  !envelope 2    ; attack/decay curve
  g a b c
  !end
}
```

#### Supported Values
- `0`: **Volume decay**  
  Smoothly ramps down the volume.
- `1`: **Curve but held**  
  Clamps the envelope into a range (0..15) and slows the decay.
- `2`: **Attack/decay curve**  
  Applies a pre-built pitch envelope: pitches up then down in a set pattern.

---

#### Constants for Readability
You can use predefined constants instead of raw numbers:
```mml
!envelope $ENV_VOLUME_DECAY          ; same as !envelope 0
!envelope $ENV_CURVE_HELD            ; same as !envelope 1
!envelope $ENV_ATTACK_DECAY_CURVE    ; same as !envelope 2
```

**Details**
- Only applies to **SQ1** and **SQ2**.
- Always set at the **start of the channel** before any notes.
- Independent of note length or timing—purely an envelope/pitch behavior.

---

### Square Wave Channel Control: `!pulse`

**Purpose:**  
Configures the Square Wave channel control byte for **SQ1** or **SQ2**. This byte determines the duty cycle, length counter behavior, and volume mode (constant or envelope).

---

#### Byte Composition
The control byte is built from **4 arguments** for the current channel:

| Argument         | Description                                      |
|------------------|--------------------------------------------------|
| **Duty Cycle**   | Waveform shape: 12.5%, 25%, 50%, or 75%         |
| **Length Mode**  | Run or Halt length counter                      |
| **Volume Mode**  | Constant volume or envelope mode                |
| **Volume Value** | 0–15 (lower nibble)                             |

---

##### Constants for MML Parser
Instead of raw numbers, use these constants:

### Duty Cycle
- `$DUTY_12_5` → 0  
- `$DUTY_25` → 1  
- `$DUTY_50` → 2  
- `$DUTY_75` → 3  

### Length Counter
- `$LEN_RUN` → 0  
- `$LEN_HALT` → 1  

### Volume Mode
- `$ENV_MODE` → 0  
- `$CONST_VOL` → 1  

---

### Usage in Script
```mml
!pulse $DUTY_75 $LEN_RUN $CONST_VOL 0
 ```

---

### Composer Notes

The `!pulse` command sets the **basic tone shape** for the square wave channels (SQ1 and SQ2).  
Think of it as choosing the **waveform flavor** and a few playback options:

- **Duty cycle** changes the character of the sound:
  - 12.5% = thin and nasal
  - 25% = bright
  - 50% = balanced
  - 75% = hollow and warm
- **Length counter** and **volume mode** are always set to “run” and “constant” in Faxanadu, so you don’t need to worry about them.
- The last number is the starting volume (0-15), but in practice the game uses its own **software envelopes** to shape the sound dynamically.  
  This means your `!pulse` volume is usually overridden by fades and curves later.

**Tip:** Use `!pulse` mainly to pick the duty cycle for the tone color. The envelope and volume opcodes will handle the actual loudness and expression.


### Opcode: `!nop`

**Purpose:**  
`!nop` stands for “no operation.” It does absolutely nothing when executed.

**Why does it exist?**  
It’s not used anywhere in Faxanadu’s original music data. Most likely, it’s a leftover from Hudson’s shared sound engine, which was adapted for multiple games. Some engines included `NOP` as a placeholder or for alignment purposes, but Faxanadu never needed it.

**Composer Notes:**  
You can safely ignore this opcode. It has no effect on playback and is only here for completeness.

---

# Tempo, Tick Math, and Choosing Safe Tempos

## Quarter‑notes, ticks, and how tempo works
The engine measures time in **ticks**, not **BPM**.  
Internally, one minute is treated as **3600 ticks**, so the number of ticks per quarter‑note is:

T = 3600 / tempo (in quarter-notes per minute)

A tempo of **150 q/min** gives:

T = 3600/150 = 24 ticks per quarter

This value must be an integer for musical durations to land cleanly on the grid. When it isn’t, durations accumulate fractional remainders.

## Very fast tempos: short notes collapse to 0 ticks
At high tempos, T becomes small.  
If T is too small, dividing it into sixteenth, thirty‑second, or dotted values can produce:

- 0‑tick durations (notes that vanish). The compiler will not allow this.

This is why extremely fast tempos must be chosen carefully.

## Very slow tempos: long notes exceed the 255‑tick limit
At slow tempos, T becomes large.  
Whole notes, dotted whole notes, or tied notes can exceed the engine’s maximum duration:

- **255 ticks** (the largest representable length)

When this happens, compilation will also fail - as it is impossible to emit notes that long.

---

## Fractional drift and the fractional accumulator
When T is not divisible by common musical fractions (½, ¼, ⅛, dotted, triplet), durations produce **fractional remainders**. The compiler uses a **fractional accumulator** to keep timing accurate:

- Each note adds its fractional remainder to the accumulator
- When the accumulator crosses 1.0, an extra tick is inserted
- This keeps linear playback correct

However, this system cannot guarantee correctness inside:

- loops
- subroutines
- patterns that repeat many times

Fractional drift inside loops cannot be corrected automatically, since the note lengths remain fixed for each iteration.

Note also that the fractional corrections will increase the size of your bytecode as length-setting code has to be inserted. If a quarter note has tick length 11 for example, the eighth notes will have a tick length of 5.5.

When you then write ```l8 c c``` this will compile to ```l~5 c l~6 c``` since the eighth notes must sum to 11 for channels to stay in sync over time.

### Correcting drift manually
If a looped passage drifts over time, composers can insert:

- synthetic rests (e.g., r64)
- explicit raw ticks (advanced)

If composers don't use any loops or subroutines for a channel, the fractional accumulator will have an easy job correcting the output as it goes along.

## Tempo Coverage Table
It is best to avoid fractional tick lengths for your notes, both for channels to stay in sync, and for bytecode size reasons.

The table below lists tempos that produce clean musical grids. For each tempo:

- **✔️** means the duration fits perfectly (no fractional remainder)
- **❌** means the duration produces fractional drift
- **⛔** means the duration is outside the allowed tick range (too short or too long)

Use these tempos to avoid drift and ensure clean MML output.

| tempo (q/min) | T (ticks/q) | coverage | 1 | 2 | 4 | 8 | 16 | 32 | 1. | 2. | 4. | 8. | 16. | 3 | 6 | 12 | 24 |
|:-------------:|:-----------:|:--------:|:-:|:-:|:-:|:-:|:--:|:--:|:--:|:--:|:--:|:--:|:---:|:-:|:-:|:--:|:--:|
| 150           | 24          | 15       | ✅ | ✅ | ✅ | ✅ | ✅  | ✅  | ✅  | ✅  | ✅  | ✅  | ✅   | ✅ | ✅ | ✅  | ✅  |
| 75            | 48          | 14       | ✅ | ✅ | ✅ | ✅ | ✅  | ✅  | ⛔  | ✅  | ✅  | ✅  | ✅   | ✅ | ✅ | ✅  | ✅  |
| 100           | 36          | 13       | ✅ | ✅ | ✅ | ✅ | ✅  | ❌  | ✅  | ✅  | ✅  | ✅  | ❌   | ✅ | ✅ | ✅  | ✅  |
| 300           | 12          | 13       | ✅ | ✅ | ✅ | ✅ | ✅  | ❌  | ✅  | ✅  | ✅  | ✅  | ❌   | ✅ | ✅ | ✅  | ✅  |
| 60            | 60          | 12       | ✅ | ✅ | ✅ | ✅ | ✅  | ❌  | ⛔  | ✅  | ✅  | ✅  | ❌   | ✅ | ✅ | ✅  | ✅  |
| 85+5/7        | 42          | 11       | ✅ | ✅ | ✅ | ✅ | ❌  | ❌  | ✅  | ✅  | ✅  | ❌  | ❌   | ✅ | ✅ | ✅  | ✅  |
| 90            | 40          | 11       | ✅ | ✅ | ✅ | ✅ | ✅  | ✅  | ✅  | ✅  | ✅  | ✅  | ✅   | ❌ | ❌ | ❌  | ❌  |
| 112+1/2       | 32          | 11       | ✅ | ✅ | ✅ | ✅ | ✅  | ✅  | ✅  | ✅  | ✅  | ✅  | ✅   | ❌ | ❌ | ❌  | ❌  |
| 120           | 30          | 11       | ✅ | ✅ | ✅ | ✅ | ❌  | ❌  | ✅  | ✅  | ✅  | ❌  | ❌   | ✅ | ✅ | ✅  | ✅  |
| 200           | 18          | 11       | ✅ | ✅ | ✅ | ✅ | ❌  | ❌  | ✅  | ✅  | ✅  | ❌  | ❌   | ✅ | ✅ | ✅  | ✅  |
| 225           | 16          | 11       | ✅ | ✅ | ✅ | ✅ | ✅  | ✅  | ✅  | ✅  | ✅  | ✅  | ✅   | ❌ | ❌ | ❌  | ❌  |

150 is the golden tempo for which all normal note lengths resolve to integral tick lengths.

If you can forego triplets or 32nd notes, there are other good options.

85+5/7 and 112+1/2 are very strange tempos indeed, but they too fall neatly on the musical grid.

Note also that you can tie any fraction-free notes together, and the tie will stay fraction-free - but the total length must be at most 255 ticks.

---

## mml extracted from ROM

The Faxanadu music tools provide a **perfect round‑trip at the bytecode level**, not at the MML level.

- **MML → bytecode** is exact and authoritative.  
  The compiler produces the same bytecode the engine would expect, with no ambiguity.

- **bytecode → MML** is a *best‑effort reconstruction*.  
  The extractor attempts to infer:
  - a tempo that explains as many durations as possible,
  - musical note lengths that match the observed tick values,
  - and readable MML structure.

However, the extractor has important limitations:

- It only checks durations against a fixed set of **primitive musical fractions**.  
- It does **not** attempt to interpret long durations as **combinations of tied notes**.  
- It does **not** attempt to **split unusual durations into ties**.  
- Composite durations created by ties in MML become a **single tick length** in the ROM, and may not match any primitive fraction under the inferred tempo.  
- When no clean musical representation exists, the extractor falls back to **raw tick lengths** (`l~<ticks>`).

Because of this, the MML produced from a ROM may differ from the original MML used to create it. This is expected: the ROM does not preserve musical structure, only absolute tick durations.

**Summary:**  
- *Bytecode round‑trips perfectly.*  
- *MML does not, because musical structure is not stored in the ROM.*  
- *For composing new music, always treat your MML source as the master.*

---

## 🎧 MIDI Output and How the Playback VM Works

### 🧩 Per‑channel virtual machine
Each channel is rendered by its **own internal virtual machine**, running independently of the others. This VM executes the fully expanded event stream for that channel:

- **Loops are unrolled**
- **Subroutines (`jsr`) are inlined**
- **All control flow becomes linear**

Because the VM sees a **finite, linear sequence** of events, it never risks running forever. As soon as the channel reaches the end of its expanded event list, the VM reaches equilibrium and stops.

This design guarantees that MIDI export is:

- deterministic  
- bounded  
- free of infinite loops  
- safe for arbitrarily complex MML  

### 🎼 No fractional drift in MIDI
Since the MIDI exporter works on the **fully unrolled** event stream, it can apply fractional‑tick correction continuously:

- Every duration is converted to ticks  
- Any fractional remainder is accumulated  
- Corrections are applied immediately  
- No drift can accumulate across loops or subroutines  

This differs from the in‑engine compiler, which cannot predict how many times a loop will run. The MIDI exporter *can*, because it expands everything first. The result is **perfectly stable timing** in the MIDI output, even for passages that would drift inside the NES engine.

### 🧪 Testing compositions in the real engine
While MIDI export is ideal for checking musical correctness, the **final authority** is always the real engine running inside the ROM.

The easiest way to test your composition in‑game is:

- Patch your MML into the ROM  
- Load the ROM in an emulator
- **Song #1 plays immediately on boot**, because it is the intro theme  

This makes song #1 the most convenient slot for rapid iteration:

- no menu navigation  
- no triggers required  
- instant playback on reset  

For deeper testing, you can assign your composition to any song index, but the intro slot is the fastest workflow during development.

## Example mml song

```
; Athletic Theme

#song 1
t150

#sq1 {
!start
o4 v15
!envelope $ENV_VOLUME_DECAY
!pulse $DUTY_75 $LEN_RUN $CONST_VOL 0
l12>dc24<a>d8c24<a8>dc24<a>d8c24<a8>dc24dl8af24dc2<g+l16ar>c8drfrf4ara+8arf+8drc4&c.g+32ara8gra8grara4grb+8arg8erd4.cr<g+8ar>c8drfrf4ara+8arf+8>dr<a4.ara8dra8b+ra4crarf2.r4<g+8ar>c8drfrf4ara+8arf+8drc4&c.g+32ara8gra8grara4grb+8arg8erd4.cr<g+8ar>c8drfrf4ara+8arf+8>dr<a4.ara8dra8b+ra4crarf2.r8 v12 c8dcrdcrdcrdcrd8crcO4A+O5rcO4A+O5rc<a+4r4rb+a+rb+a+rb+a+rb+a+rb+8a+ra+ara+ara+a4r4r v12 >dcrdcrdcrdcrd8crcO4A+O5rcO4A+O5rc<a+4r4r v15 >>crc8rcr8<a+ra+8ra+r8ara8rar8grc4.<g+8ar>c8drfrf4ara+8arf+8>dr<a4.ara8dra8b+ra4crarf2. v12 rcdefcfgg+afcfcfa8.fcf+df+ab+8af+>d<af+df+af+dgf+gagdgaa+b+a+agfededececega+b+a+agfed< v15 g+8ar>c8dr v15 frf4ar v12 a+8arf+8dr v11 c4&c.g+32ar v10 a8gra8gr v9 ara4gr v8 b+8arg8er v7 d4.cr
!restart
}
#sq2 {
!start
o4 v15 
!pulse $DUTY_50 $LEN_RUN $CONST_VOL 0
!envelope $ENV_VOLUME_DECAY
L4O4 l2ff+gee8l16fra8ar>crc4erf+8drc8<arf+4.>erd8drd8drdrd4dra8erd8cr<a+4.are8fra8ar>crc4erf+8drc8ard4.erd8O4A+O5rd8erc4<grb+ra2.r4e8fra8ar>crc4erf+8drc8<arf+4.>erd8drd8drdrd4dra8erd8cr<a+4.are8fra8ar>crc4erf+8drc8ard4.erd8O4A+O5rd8erc4<grb+ra2.r8 v12 f+8rf+r8f+r8f+r8f+r8r16f+r8gr8gr8g4r4r8er8er8er8er8r16er8fr8f8rf4r4r8 v12 f+r8f+r8f+r8f+r8r16f+r8gr8gr8g4r4r v15 >grg8rgr8frf8rfr8ere8rer8dr<a+4.e8fra8ar>crc4erf+8drc8ard4.erd8O4A+O5rd8erc4<grb+ra2.r1r1r1r1r4e8fra8ar v15 >crc4er v12 f+8drc8<ar v11 f+4.>er v10 d8drd8dr v9 drd4dr v8 a8erd8cr v7 <a+4.ar
!restart
}
#tri {
!start
o4 l8o2f>a12aa24<ff+>a12aa24<f+g>a+12a+a+24<gc>e16ee16e<fb+cb+fb+c16.>c&c32<db+f+b+db+f+16.>c&c32<g>d<d>d<g>d<d16.>d&d32<cb+gb+cb+g16.>c&c32<fb+cb+fb+c16.>c&c32<d>dO2F+O3d<d>d<f+16.>d&d32<g>d<d>d<cb+g16.>c&c32<fb+cb+fb+c16.>c&c32<fb+cb+fb+c16.>c&c32<db+f+b+db+f+16.>c&c32<g>d<d>d<g>d<d16.>d&d32<cb+gb+cb+g16.>c&c32<fb+cb+fb+c16.>c&c32<d>dO2F+O3d<d>d<f+16.>d&d32<g>d<d>d<cb+g16.>c&c32<fb+cb+fb+c16.>c&c32 v12 <d>dO2F+O3d<d>d<f+16.>d&d32<g>d<d>d<g>a+16a+a+16a+<cb+gb+cb+g16.>c&c32<fb+cb+f>a16aa16a<d>dO2F+O3d<d>d<f+16.>d&d32<g>d<d>d<gfed >cc.c.<a+a+.a+.aa.a.gcdefb+cb+fb+c16.>c&c32<d>dO2F+O3d<d>d<f+16.>d&d32<g>d<d>d<cb+g16.>c&c32<facafac16.a&a32fb+cb+fb+c16.>c&c32<db+f+b+db+f+16.>c&c32<g>d<d>d<g>d<d16.>d&d32<cb+gb+cb+g16.>c&c32<fb+cb+ v15 fb+c16.>c&c32 v12 <db+f+b+ v11 db+f+16.>c&c32 <g>d<d>d <g>d<d16.>d&d32 <cb+gb+ cb+g16.>c&c32
!restart
}
#noise {
!start
L4 [[p2 p1 p2 p1 p2 p1 p2 p1 p2 p3 p2 p3]255]
!end
}
```
