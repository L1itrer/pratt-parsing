#define NOB_IMPLEMENTATION
#include "nob.h"


int main(int argc, char* argv[])
{
  GO_REBUILD_URSELF(argc, argv);
  Cmd cmd_static = {0};
  if (!mkdir_if_not_exists("./out/")) return 1;
  Cmd* cmd = &cmd_static;
#ifdef _WIN32
  cmd_append(cmd, "cl");
  cmd_append(cmd, "-nologo");
  cmd_append(cmd, "-Fo:out\\", "-Fd:out\\", "-Fe:out\\pratt.exe", "-Zi", "-W4");
#else
  cmd_append(cmd, "clang");
  cmd_append(cmd, "-o", "./out/pratt");
  cmd_append(cmd, "-g", "-Wall", "-Wextra");
#endif
  cmd_append(cmd, "./src/pratt.c");
  cmd_append(cmd, "-DPRATT_DEBUG=1");
  if (!cmd_run(cmd)) return 1;
  return 0;
}
