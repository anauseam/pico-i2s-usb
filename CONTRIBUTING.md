# CONTRIBUTING.md

Thank you for considering a contribution to `pico-i2s-usb`. This document
is intentionally short. The substance of the project lives in the README,
ARCHITECTURE.md, and the `docs/internals/` directory.

## How to build

See the **Building from Source** section of the [README](README.md).
Always verify your change with a clean build:

```bash
cmake --build build --clean-first
```

Incremental builds can mask dependency drift; the clean build is what CI
will run.

## Code style

Formatting is governed by [`.clang-format`](.clang-format) at the repo
root. Before committing:

```bash
clang-format -i src/*.{c,h}
```

The guidelines in [`docs/internals/05-style.md`](docs/internals/05-style.md)
elaborate on language constructs (C11, no VLAs, no `goto`, etc.); read
that file before making non-trivial style changes.

## The docs hierarchy

This project has four kinds of documentation. Knowing where to put a
change matters more than the change itself.

| File                                                                                   | Audience                | What goes here                                                  |
| -------------------------------------------------------------------------------------- | ----------------------- | --------------------------------------------------------------- |
| [README.md](README.md)                                                                 | End users               | How to wire, build, install, configure.                         |
| [ARCHITECTURE.md](ARCHITECTURE.md)                                                     | Maintainers, reviewers  | Design narrative: pipeline, decisions, open observations.       |
| [`docs/internals/`](docs/internals/)                                                   | Maintainers             | Internal architecture, constraints, and hardware contracts.     |
| [`docs/internals/suspected-issues.md`](docs/internals/suspected-issues.md)             | Maintainers             | Descriptive notes on unreproduced defensive code.               |

If you're documenting a fact a user needs, it goes in the README. If
you're documenting a rationale a maintainer needs to understand the
code, it goes in ARCHITECTURE.md. If you're imposing a constraint that
must hold in all future commits, it goes in `docs/internals/`. If you
suspect something is true but have not reproduced it, it goes in
`docs/internals/suspected-issues.md`.

## Pull Request Process

- Open an issue first for non-trivial changes. The maintainer
  appreciates the opportunity to flag scope or alternative approaches
  before you write code.
- Make sure your change aligns with the guidelines in
  [`docs/internals/`](docs/internals/). The guidelines are not arbitrary;
  most are documented hazards. If you believe a constraint is wrong, propose
  changing it in the *same* PR with rationale (and ideally a
  reproduction of the case that motivates the change).
- Update the internal docs when the architecture changes. Adding a new
  module, a new peripheral, a new descriptor topology, or lifting a
  workaround all require touching the corresponding rule file. Sample
  rate / pin / debug toggle changes do not.

## License

By submitting a PR you agree your contribution is licensed under MIT
per [LICENSE](LICENSE).
