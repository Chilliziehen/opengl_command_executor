/**
 * @file NativeDialog.h
 * @brief Platform-native dialog helpers.
 *
 * Currently provides a folder-selection dialog.  Implementations live under
 * src/platform/NativeDialog_<platform>.cpp.
 */
#ifndef NATIVE_DIALOG_H
#define NATIVE_DIALOG_H

#include <string>

namespace platform {

/// Open a native folder-selection dialog and return the chosen path.
/// @param outPath  Receives the UTF-8 absolute path on success; unchanged on
///                 failure or if the user cancelled.
/// @return true if the user selected a folder, false if cancelled or error.
bool OpenFolderDialog(std::string &outPath);

} // namespace platform

#endif // NATIVE_DIALOG_H
