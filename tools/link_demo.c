#include <stdio.h>
int main(void)
{
    puts("\033[2J\033[H\033[1;36mAROS XTerm / hyperlinks\033[0m\r");
    puts("Hover: underline + actual target in the window title.\r");
    puts("Ctrl-click opens. Ctrl+Shift-click copies the target.\r\n\r");
    puts("Plain URL: https://aros.org\r");
    puts("OSC 8: \033]8;;https://aros.sourceforge.io/\033\\AROS mainline website\033]8;;\033\\\r");
    puts("Wide label: \033]8;;https://example.org/unicode\033\\中文 😀\033]8;;\033\\\r");
    puts("Wrapped URL: https://example.org/a-long-path/to-demonstrate-link-detection-across-a-terminal-soft-wrap?source=aros-xterm\r");
    puts("Blocked scheme: \033]8;;file:///S/Startup-Sequence\033\\not a clickable file link\033]8;;\033\\\r");
    puts("Local opener check: http://127.0.0.1:9/aros-xterm-test\r\n\r");
    puts("No automatic navigation. A browser and OpenURL preferences are optional.\r");
    return 0;
}
