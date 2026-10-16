#pragma once

// Windows-only: associates the .cgmap extension with this game's .exe, so
// double-clicking a map file launches straight into it (main.cpp already
// handles the resulting command-line argument -- see Menu::StartWithCustomMap).
// Deliberately never called automatically; it writes to the user's own
// registry (HKEY_CURRENT_USER\Software\Classes, no admin rights needed,
// but still a real system-settings change), so it's only ever triggered by
// an explicit button press in Settings.
namespace fileassoc {

// Returns false if the registry write failed for any reason (reported to
// the user rather than silently appearing to succeed).
bool AssociateCgmapFiles();

// Whether the association currently points at this running .exe -- lets
// the Settings button show "ASSOCIATED" vs "ASSOCIATE .cgmap FILES"
// instead of always showing the same label regardless of state.
bool IsAssociated();

} // namespace fileassoc
