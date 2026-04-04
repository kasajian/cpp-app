# AI Agent Guidelines for cpp_app

This file follows the [AGENTS.md open standard](https://agents.md/) and is
recognized automatically by GitHub Copilot, Claude, Cursor, OpenAI Codex,
and other AI coding agents.

The project-agnostic documentation standard this repository uses is defined in
[`docs/AGENT_STANDARDS.md`](docs/AGENT_STANDARDS.md).

This file defines the non-negotiable design principles for this project. It is
written with AI coding agents as the primary audience, but applies equally to
human contributors — these are project invariants, not AI-specific rules.

Agent behavior rules (permitted ops, prohibited ops, working style, conflict
resolution) are in this file. Project-specific technical constraints (static
linking, platform rules, build system, compiler requirements) are in
[`docs/CONSTRAINTS.md`](docs/CONSTRAINTS.md).

## Permitted Operations (No Confirmation Needed)

The following operations may be performed without asking the user for permission:

- **Anonymous web access** — reading public URLs to look up documentation, check
  standards, or retrieve reference material. No authentication, no form submission,
  no writing to external services.
- **Read-only filesystem operations** — reading any file or directory in the
  repository or the local machine.
- **Creating new files or folders** — adding files and directories that do not yet
  exist, including new source files, config files, and documentation.

Operations not listed here fall back to the agent's default behavior for the current
session (which may be fully autonomous, interactive, or anything in between).

## Prohibited Operations (Never Do Without Explicit Instruction)

- **Never push to any remote.** Commit locally and stop. The user will push, or will
  explicitly say to push. This applies to every branch and every remote, including
  `origin`.

## Working Style

For any task that involves a design decision, structural change, or approach with
more than one reasonable option:

1. **Propose first.** Describe what you plan to do and why. If there are trade-offs
   or alternatives worth knowing about, surface them briefly.
2. **Wait for approval.** Do not begin implementation until the user confirms.
3. **Then execute.** Once approved, do the work completely without stopping to
   re-ask about things already decided.

For unambiguous, mechanical tasks (fixing a typo, adding a file the user just
described in detail, running a command) — just do it. Do not ask for permission
on things that have no meaningful alternative.

---

## ⚠️ Handling Conflicts Between User Instructions and This File

When a user instruction conflicts with a rule in this file or any companion file,
**do not automatically comply with the user instruction, and do not automatically
update the file.** Instead:

1. **Stop** and inform the user of the specific conflict — quote or reference the
   rule being violated and which file it appears in.
2. **Ask the user to choose** one of the following:
   - **Comply with the file:** The user reconsiders; the instruction is adjusted to
     align with the existing rule. No file update needed.
   - **Update the file:** The rule itself should change. Update the relevant companion
     file(s) alongside or before the code change.
   - **Intentional exception:** This is a deliberate one-time deviation. In this case:
     - The user must provide the specific reason for the exception.
     - Add a comment directly at the non-compliant change (source comment, CMake
       comment, YAML comment, etc.) stating that it is intentionally non-compliant,
       which rule it violates, and the reason the user gave.
     - Do **not** update the rule in the companion file — the rule still stands for
       all other cases.

This applies to all companion files: `README.md`, `AGENTS.md`, `CLAUDE.md`,
`docs/ARCHITECTURE.md`, `docs/CONSTRAINTS.md`, `docs/AGENT_STANDARDS.md`,
`docs/TEMPLATE_WORKFLOW.md`, and `.github/copilot-instructions.md`.

---

## ⚠️ MANDATORY: Update Documentation With Every Change

**This is not optional. There are no exceptions.**

Any change to source code, `CMakeLists.txt`, `vcpkg.json`, GitHub Actions workflows,
or any other file in the repo triggers this rule. The following documentation files
must be checked and updated as needed:

- **`README.md`**
- **`AGENTS.md`**
- **`CLAUDE.md`**
- **`docs/ARCHITECTURE.md`**
- **`docs/CONSTRAINTS.md`**
- **`docs/AGENT_STANDARDS.md`**
- **`docs/TEMPLATE_WORKFLOW.md`**
- **`.github/copilot-instructions.md`**

This list is not exhaustive. Any file *referenced by* or *linked from* the above
files is also part of the documentation set and must be kept current. If a change
makes a link, example, or description in any referenced file inaccurate, update
that file too.

Do not wait to be asked. Do not skip this step. A commit that changes behaviour
without updating the relevant documentation is incomplete.

### Pre-commit documentation checklist

Before every commit, run through the following mentally:

- [ ] Does `README.md` still accurately describe the build steps, supported platforms,
      and minimum versions?
- [ ] Does `README.md` have accurate unit test instructions for every platform touched?
- [ ] Does `docs/ARCHITECTURE.md` still accurately describe the libraries, CI jobs, and
      design decisions?
- [ ] Does `docs/CONSTRAINTS.md` still accurately reflect the platform rules, build
      system rules, and technical invariants?
- [ ] Does `AGENTS.md` reflect any new conventions or constraints introduced by this
      change?
- [ ] Do `CLAUDE.md` and `.github/copilot-instructions.md` still correctly direct agents
      to `AGENTS.md`?
- [ ] Does `docs/TEMPLATE_WORKFLOW.md` still accurately describe the workflow for
      creating and maintaining tool repos?

---

## Project Constraints

The technical invariants for this project — platform rules, static linking
requirements, build system rules, compiler requirements, CI rules, and what not
to do — are in [`docs/CONSTRAINTS.md`](docs/CONSTRAINTS.md).

Read that file before making any change that touches the build system, CI pipeline,
platform configuration, or dependency list.
