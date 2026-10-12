"""The words, the splice and the clean-up of Keyes' line for network co-op's cryo bay
(port/linux/game/coop_cryo_line.c)."""

import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, ROOT, build, enum_with, function, mutated, read, run, structure  # noqa: E402
from embed_assets import CRYO_WORDS, words  # noqa: E402

CASES = ["words", "splice", "filters", "clean", "upsample"]
FUNCTIONS = ("cryo_line_read_le", "cryo_line_words", "cryo_line_splice", "cryo_line_filter_new", "cryo_line_filter_run",
             "cryo_line_clean", "cryo_line_upsample")

# a fault, the function it is in, and the case that must catch it
NEGATIVE_CONTROLS = {
    "crossfade-backwards": (("(sample - *blended) * fade /", "(sample - *blended) * (CRYO_LINE_CROSSFADE - 1 - fade) /"),
                            "cryo_line_splice", "splice"),
    "stereo-words": (("cryo_line_read_le(chunk + 2, 2) == 1", "cryo_line_read_le(chunk + 2, 2) >= 1"), "cryo_line_words",
                     "words"),
    "past-the-end": (("if (chunk_size > size - offset - 8)\n\t\t\treturn NULL;\n", ""), "cryo_line_words", "words"),
    "butterworth-q": (("(2 * index + 1)", "(2 * index)"), "cryo_line_filter_new", "filters"),
    "high-for-low": (("cryo_line_filter_new(low, 4, FALSE, 4500.0", "cryo_line_filter_new(low, 4, TRUE, 4500.0"),
                     "cryo_line_clean", "clean"),
    "gate-opens-early": (("+ 3.0;", "+ 4.0;"), "cryo_line_clean", "clean"),
    "gate-shut": (("gain_sum / CRYO_LINE_GAIN_WINDOW * (sample - low_sample)",
                   "gain_sum / CRYO_LINE_GAIN_WINDOW * 0.0"), "cryo_line_clean", "clean"),
    "full-level": (("? 0.0 : line[frame / 2]", "? 0.0 : 2.0 * line[frame / 2]"), "cryo_line_upsample", "upsample"),
    "imaged": (("cryo_line_filter_run(low, 4, frame % 2", "cryo_line_filter_run(low, 0, frame % 2"),
               "cryo_line_upsample", "upsample"),
}


def generated(fault=None, where=None):
    source = read("port/linux/game/coop_cryo_line.c")
    data = (ROOT / CRYO_WORDS).read_bytes()
    embedded = "static unsigned int coop_cryo_words[] = {\n" + "\n".join(words(data)) + "\n};\n"
    embedded += f"static unsigned long coop_cryo_words_size = {len(data)};\n"
    text = enum_with(source, "CRYO_LINE_RATE") + "\n"
    match = re.search(r"^static struct cryo_line_run\n\{.*?^\};\n", source, re.M | re.S)
    assert match, "cryo_line_runs not found in coop_cryo_line.c"
    text += match.group(0) + structure(source, "cryo_biquad") + "\n"
    for name in FUNCTIONS:
        code = function(source, name)
        if fault and name == where:
            code = mutated(code, *fault)
        text += code + "\n"
    return (("words.inc", embedded), ("under_test.inc", text))


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("coop_cryo_line", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, where, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("coop_cryo_line", generated(fault, where)), case)
    assert status == CHECK_FAILED, f"coop_cryo_line.c with a fault ({control}) passed '{case}': the test cannot see it"
