#ifndef TUI_HELP_H
#define TUI_HELP_H

/* * Displays help information to the TUI log.
 * If target_cmd is empty (""), prints a general overview.
 * Otherwise, prints specific usage for the target command.
 */
void display_help(const char* target_cmd);

#endif