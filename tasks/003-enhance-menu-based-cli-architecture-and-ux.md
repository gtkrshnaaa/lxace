---
id: "003"
title: "Enhance menu-based CLI architecture and UX"
type: "feature"
status: "done"
created_at: "2026-10-31"
updated_at: "2026-10-31"
---

# [Feature] Enhance menu-based CLI architecture and UX

## Description
Enhance menu-based CLI architecture and UX

## Acceptance Criteria
- [x] Implement flash notification system and engine status badges in `ViewBase` and `CliApp`
- [x] Upgrade `ViewHome` with framed layout, engine metadata (version, port, project), menu shortcuts (1-4, S, P, K, Q), and status notifications
- [x] Upgrade `ViewSettings` to display current parameter values inline on the menu, with validation feedback and shortcut keys
- [x] Upgrade `ViewPrompt` with structured box UI, keyboard navigation hints (Enter to submit, Esc to cancel), and submission flash feedback
- [x] Add unit test suite in `tests/test_cli.cpp` covering menu formatting, state transitions, input validation, and view rendering
- [x] Verify test suite passes 100% inside container (`make test`)
- [x] Verify containerized build succeeds with 50% resource limits (`make build`) and OS installation works

## Context / Notes
- Target files: 
- Dependencies: None
