/* Where the running title's own files are.
 *
 * Inside the sandbox the app folder turns up under one of a few names
 * depending on how the title was mounted, and argv[0] is just "eboot.bin",
 * so it has to be found rather than derived. The approach is the one
 * ProsperoStore uses (src/system/app_folder.cpp, GPL-3.0-or-later): try the
 * candidates and keep the one that actually holds this app's eboot.bin.
 *
 * Note that /data is not reachable from a title sandbox without elevated
 * privileges, so files the user adds have to live in the app folder. */
#ifndef APPDIR_H
#define APPDIR_H

/* The app folder without a trailing slash, for example "/app0". Never null;
   falls back to "/app0" when nothing matched. Resolved once. */
const char* app_folder();

#endif
