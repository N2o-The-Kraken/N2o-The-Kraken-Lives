# Merge Conflict Status

- Last verification: `python tools/check_merge_conflicts.py`
- Result: **No merge markers detected** in the current working tree.
- Re-run the script after rebasing or pulling upstream changes to confirm the tree stays clean. The checker skips common build
  directories and reports any offending files so they can be resolved before publishing a branch.

If conflicts appear, fix them locally, then re-run the checker before packaging the JUCE SDK or other dependencies from your
Visual Studio environment.
