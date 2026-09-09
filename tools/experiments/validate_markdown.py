#!/usr/bin/env python3
"""Validate the repository's GitHub Markdown rendering contract.

GitHub officially supports fenced ``math`` blocks for display equations.  This
repository deliberately uses that syntax for every block equation instead of a
three-line ``$$`` construct, whose parsing has proved inconsistent on the
rendered repository pages.

Official syntax reference:
https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/writing-mathematical-expressions
"""

from __future__ import annotations

from pathlib import Path
import re
import subprocess
from urllib.parse import unquote


ROOT = Path(__file__).resolve().parents[2]
MARKDOWN_ROOTS = (
    ROOT / "docs",
    ROOT / "data_origin",
    ROOT / "data_sources",
    ROOT / "experiments",
    ROOT / "paper",
    ROOT / "tools" / "gpu4gst_data",
)
FENCE_OPEN = re.compile(r"^\s*(`{3,}|~{3,})(.*)$")
INLINE_CODE = re.compile(r"`[^`]*`")
INLINE_DOLLAR = re.compile(r"(?<!\\)(?<!\$)\$(?!\$)")
LOCAL_LINK = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
UNSAFE_INLINE_MATH_LEFT_BOUNDARY = re.compile(
    r"[、，；：（）。！？—]\$(?!\$)|(?<!\s)-\$(?!\$)"
)
INLINE_MATH = re.compile(r"(?<!\\)(?<!\$)\$(?!\$)(.*?)(?<!\\)\$(?!\$)")
RAW_INLINE_MATH_STAR = re.compile(r"\*")
FORBIDDEN_GITHUB_MATH_MACRO = re.compile(r"\\(operatorname)\*?(?![A-Za-z])")
FORBIDDEN_GITHUB_CASES = re.compile(r"\\(?:begin|end)\s*\{cases\}")
FORBIDDEN_TEXT_MACRO_UNDERSCORE = re.compile(
    r"\\(?:text|texttt|mathtt)\{[^}\n]*[\\]?_[^}\n]*\}"
)
# 2026-09-09 的真实 GitHub 页面复核中，同一文件的第 737 个及后续
# math-renderer 稳定返回错误，即使表达式只是单个变量。留出安全余量，防止
# 后续文档增长再次越过客户端的单页公式处理边界。
MAX_GITHUB_MATH_EXPRESSIONS_PER_FILE = 700


def markdown_files() -> list[Path]:
    """Return every versioned-document scope without scanning ignored data."""
    files = [ROOT / "README.md", ROOT / "RUN.md"]
    for directory in MARKDOWN_ROOTS:
        if directory.is_dir():
            files.extend(directory.rglob("*.md"))
    return sorted(set(files))


def tracked_paths() -> set[str]:
    """Return exact-case Git paths so local-only links cannot pass on Windows."""
    completed = subprocess.run(
        ["git", "ls-files", "-z"],
        cwd=ROOT,
        check=True,
        capture_output=True,
    )
    return {
        item.decode("utf-8", errors="strict")
        for item in completed.stdout.split(b"\0")
        if item
    }


def check_github_math_macros(
    text: str, relative: Path, line_number: int, failures: list[str]
) -> None:
    """Reject macros confirmed to fail in GitHub's rendered Markdown."""
    for match in FORBIDDEN_GITHUB_MATH_MACRO.finditer(text):
        failures.append(
            f"{relative}:{line_number}: GitHub rejects \\{match.group(1)}; "
            "use a supported spelling such as \\mathrm{name}"
        )
    if FORBIDDEN_GITHUB_CASES.search(text):
        failures.append(
            f"{relative}:{line_number}: GitHub failed to render the cases "
            "environment in this repository; split the branches into "
            "separate fenced math blocks"
        )
    if FORBIDDEN_TEXT_MACRO_UNDERSCORE.search(text):
        failures.append(
            f"{relative}:{line_number}: GitHub may parse an underscore inside "
            "a math text macro outside math mode; keep the code identifier in "
            "Markdown prose or rewrite it as a mathematical symbol"
        )


def main() -> int:
    failures: list[str] = []
    files = markdown_files()
    tracked = tracked_paths()
    discovered = {path.relative_to(ROOT).as_posix() for path in files}
    omitted = sorted(
        item
        for item in tracked
        if item.lower().endswith(".md") and item not in discovered
    )
    for item in omitted:
        failures.append(
            f"{item}: tracked Markdown is outside the validated document set"
        )

    for path in files:
        relative = path.relative_to(ROOT)
        try:
            lines = path.read_text(encoding="utf-8", errors="strict").splitlines()
        except UnicodeError as error:
            failures.append(f"{relative}: invalid UTF-8: {error}")
            continue

        fence_char = ""
        fence_width = 0
        fence_info = ""
        math_has_content = False
        math_blocks = 0
        inline_dollars = 0
        prose_lines: list[str] = []

        for line_number, line in enumerate(lines, start=1):
            stripped = line.strip()
            if fence_char:
                closing = stripped and set(stripped) == {fence_char}
                if closing and len(stripped) >= fence_width:
                    if fence_info == "math" and not math_has_content:
                        failures.append(
                            f"{relative}:{line_number}: empty fenced math block"
                        )
                    fence_char = ""
                    fence_width = 0
                    fence_info = ""
                    math_has_content = False
                elif fence_info == "math":
                    if stripped:
                        math_has_content = True
                    check_github_math_macros(
                        line, relative, line_number, failures
                    )
                continue

            if stripped == "$$":
                failures.append(
                    f"{relative}:{line_number}: use a fenced ```math block, "
                    "not a standalone $$ delimiter"
                )
                continue

            match = FENCE_OPEN.match(line)
            if match:
                marker = match.group(1)
                fence_char = marker[0]
                fence_width = len(marker)
                fence_info = match.group(2).strip()
                math_has_content = False
                if fence_info == "math":
                    math_blocks += 1
                continue

            prose = INLINE_CODE.sub("", line)
            prose_lines.append(prose)
            check_github_math_macros(prose, relative, line_number, failures)
            if UNSAFE_INLINE_MATH_LEFT_BOUNDARY.search(prose):
                failures.append(
                    f"{relative}:{line_number}: GitHub may leave inline math "
                    "literal when its opening $ touches CJK punctuation, an "
                    "em dash, or a "
                    "word hyphen; insert a space before the opening $"
                )
            for inline_match in INLINE_MATH.finditer(prose):
                if RAW_INLINE_MATH_STAR.search(inline_match.group(1)):
                    failures.append(
                        f"{relative}:{line_number}: a raw math star may "
                        "be consumed as Markdown emphasis; write the TeX star command instead"
                    )
            inline_dollars += len(INLINE_DOLLAR.findall(prose))
            if re.search(r"\\[()[\]]", prose):
                failures.append(
                    f"{relative}:{line_number}: use GitHub $...$ inline math, "
                    "not legacy LaTeX delimiters"
                )

            if prose.lstrip().startswith("|"):
                inside_math = False
                for offset, character in enumerate(prose):
                    escaped = offset > 0 and prose[offset - 1] == "\\"
                    if character == "$" and not escaped:
                        inside_math = not inside_math
                    elif character == "|" and inside_math and not escaped:
                        failures.append(
                            f"{relative}:{line_number}: raw | inside table math; "
                            "use \\lvert/\\rvert"
                        )
                        break

        if fence_char:
            failures.append(f"{relative}: unclosed {fence_info or 'code'} fence")
        if inline_dollars % 2:
            failures.append(f"{relative}: unbalanced inline $ delimiters")
        math_expressions = math_blocks + inline_dollars // 2
        if math_expressions > MAX_GITHUB_MATH_EXPRESSIONS_PER_FILE:
            failures.append(
                f"{relative}: {math_expressions} math expressions exceed the "
                f"repository GitHub rendering budget of "
                f"{MAX_GITHUB_MATH_EXPRESSIONS_PER_FILE}; render identifier-only "
                "tokens as code or split non-core material before publishing"
            )

        prose_text = "\n".join(prose_lines)
        for match in LOCAL_LINK.finditer(prose_text):
            target = match.group(1).strip()
            if target.startswith("<") and target.endswith(">"):
                target = target[1:-1]
            if target.startswith(("http://", "https://", "mailto:", "#")):
                continue
            target = target.split("#", 1)[0]
            if not target:
                continue
            destination = (path.parent / unquote(target)).resolve()
            if not destination.exists():
                failures.append(f"{relative}: broken local link: {target}")
                continue
            try:
                git_target = destination.relative_to(ROOT).as_posix()
            except ValueError:
                failures.append(
                    f"{relative}: local link escapes repository: {target}"
                )
                continue
            if destination.is_file():
                published = git_target in tracked
            else:
                prefix = git_target.rstrip("/") + "/"
                published = any(item.startswith(prefix) for item in tracked)
            if not published:
                failures.append(
                    f"{relative}: local link target is not published with exact "
                    f"Git case: {target}"
                )

    if failures:
        for failure in failures:
            print(f"ERROR: {failure}")
        return 1

    print(
        f"Validated {len(files)} GitHub Markdown files: strict UTF-8, "
        "balanced fences/math, safe inline boundaries/macros and expression "
        "budgets, and exact-case tracked links"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
