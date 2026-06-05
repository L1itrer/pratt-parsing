#define NOB_IMPLEMENTATION
#include "nob.h"


int main(int argc, char* argv[])
{
  GO_REBUILD_URSELF(argc, argv);
  Cmd cmd_static = {0};
  if (!mkdir_if_not_exists("./out/")) return 1;
  Cmd* cmd = &cmd_static;
  nob_cc(cmd);
  cmd_append(cmd, "./src/pratt.c");
  cmd_append(cmd, "-Fo:out\\", "-Fd:out\\", "-Fe:out\\pratt.exe", "-Zi", "-W4");
  if (!cmd_run(cmd)) return 1;
  return 0;
}
