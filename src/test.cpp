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


int main()
  
{

  double value = 18446744073709551615;

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
