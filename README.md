# Blackbox

---

Blackbox (bb) is a remote log viewer for C/C++ applications.  It is aimed at multiplayer games, where you might want to be collecting logs from multiple consoles simultaneously.

Blackbox stores logs in a binary format, and tracks source file/line, thread, log category, etc for each log line.

---

Usage
-----
This repository is the main server that (game) clients connect to.  The Visual Studio .sln is in the vs/ subdirectory.  The client lib that integrates into a game client etc is in bbclient/.

To integrate Blackbox, simply include bbclient/include/bb.h and link to bbclient/lib/bbclient_lib_Release_v142/bbclient_lib.lib.  As an alternative to linking to bbclient_lib.lib, you can instead include bbclient's src/*.c in your project, compiled as either C or C++.

Minimal integration looks something like this:

```
#include <stdio.h>
#include "bb.h"

int main(int argc, const char **argv)
{
	BB_INIT("Application Name");
	BB_THREAD_START("Main");

	BB_LOG("Startup", "%s started with %d args\n", argv[0], argc);

	BB_SHUTDOWN();
	return 0;
}
```

Make sure bb.exe is running, to capture the session.  Then run your program.  A view should automatically open in bb.exe's window.  You can show/hide logs based on category and thread, etc.

---

Screenshots
----------

![Main window with a single log view](https://github.com/campbellhome/bbserver_docs/blob/main/screenshots/single_view.png)

![Main window with tiled log views](https://github.com/campbellhome/bbserver_docs/blob/main/screenshots/tiled_views.png)

![Colored logs](https://github.com/campbellhome/bbserver_docs/blob/main/screenshots/colors.png)

---

Credits
-------
Developed by Matt Campbell.  Blackbox was inspired by [Deja Insight](http://www.dejatools.com/dejainsight), which I first used at [Obsidian Entertainment](https://www.obsidian.net/) while working on [Dungeon Siege III](https://en.wikipedia.org/wiki/Dungeon_Siege_III).

Blackbox grew out of a desire to have Deja Insight-like functionality on [Evolve](https://en.wikipedia.org/wiki/Evolve_(video_game)) and other projects at [Turtle Rock Studios](https://www.turtlerockstudios.com/).

Embeds [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut (MIT License).

---

License
-------
Blackbox is licensed under the MIT License; see License.txt for more information.

