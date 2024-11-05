#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sstream>
#include <pthread.h>
#include <cstring>
// #include "DX7.hpp"
// #include "serialization/DX7.hpp"
// #include "cereal/archives/json.hpp"
// #include "cereal/archives/binary.hpp"

int system_user_can_rtprio() {
  struct sched_param schparam;
  memset(&schparam, 0, sizeof(struct sched_param));
  schparam.sched_priority = 60;
  if (0 == sched_setscheduler(0, SCHED_FIFO, &schparam)) {
    printf("success! %d\n", sched_get_priority_max(SCHED_FIFO));
    return 0;
  }

  return 1;
}
int main()
  
{

  return system_user_can_rtprio();

  // std::stringstream ss;
  // DX7Params params;
  // params.init();



  // char cmdStorage[7][256];
  // char* cmds[7];
  // for (int i = 0; i < 7; i++) {
  //   cmds[i] = &cmdStorage[i][0];
  // }
  // params.amyCommands((char**)cmds, 0);

  // int cmdLen = 0;
  // for (int i = 0; i < 7; i++) {
  //   std::cout << cmds[i] << "\n";
  //   cmdLen += strlen(cmds[i]);
  // }

  // {
  //   auto archive = cereal::BinaryOutputArchive(ss);
  //   archive(params);
  // }

  // std::cout << ss.str().size() << ", " << cmdLen << "\n";
  
  
  // // std::cout << strlen(buf) << "\n";

}
