# AI-use log (fill in and keep honest - the brief forbids fabricated interactions)

**Tool / model:** Claude (Anthropic), web chat, used to scaffold the C++ core, tests, CMake and docs in a first working session (date of session: 1 Nov 2026).
Reused/open code: none copied; no third-party libraries yet (standard library only). Licences: N/A so far.

## Representative prompt (real)
"give the detailed project that have been shortlisted to interview try start doing that" (with the assignment PDF attached).

## Real incorrect/unsuitable outputs found during this session (verified by running the code)
1. **Wrong test expectation (AI error):** the first CSV-example test asserted min T >= 20 C and max T <= 105 C. Running the suite failed; the data actually spans 17-188.2 C
   (3 C synthetic radial dip, and interior heating from q). The test, not the solver, was wrong -> corrected after checking the analytical maximum by hand (x ~ 0.42 m).
2. **Tooling error:** shell directory shorthand (`mkdir -p a/{b,c}`) was not expanded by `sh`, so files landed in the wrong place; found by listing the directory; files were moved and the layout re-verified.
## Your own entries (REQUIRED - add yours)
- Your prompts, the outputs you edited, how you verified them (e.g. reproduce the midpoint error 1.25 C by hand: q h^2/(8k)), and at least one challenge you made to the AI.
- Be ready to explain any function in a verification discussion.
